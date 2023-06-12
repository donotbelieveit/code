#include<memory>
#include<thread>
#include<iostream>
#include<string>
#include<dirent.h>
#include"ModelSliceReader.h"
#include"ThreadPool.h"
#include"sendcopy_client.h"
#include<unordered_map>
#include<vector>
#include<etcd/Client.hpp>
#include<boost/asio.hpp>


#include "alimama.grpc.pb.h"
#include<grpcpp/grpcpp.h>


using namespace std;

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerAsyncWriter;
using grpc::ServerReaderWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::Status;
using grpc::ServerContext;

using alimama::proto::BlockInfo;
using alimama::proto::SliceInfo;
using alimama::proto::DataInfo;
using alimama::proto::Slice2BlockRequest;
using alimama::proto::Slice2BlockResponse;

using alimama::proto::BlockData;
using alimama::proto::GetBlockDataRequest;
using alimama::proto::GetBlockDataResponse;
using alimama::proto::SendCopyRequest;
using alimama::proto::SendCopyResponse;
using alimama::proto::Slice2BlockRequest;
using alimama::proto::LoadAndRemoveResponse;

using alimama::proto::NodeService;


string local_path = "./tmp";

int block_size = 6400000;
// int slice_size = 6400000;
// int block_len = slice_size / block_size;
int max_block_num = 6400;

// string test_version_path = "/root/testbench/model_2023_03_01_12_30_00";

// run a shell command
void Command(string command){
    system(command.c_str());
}


// 从hdfs上下载指定版本文件到本地磁盘
void DownloadFileFromHDFS(string version){
    // 检查是否存在tmp文件夹
    DIR *dir;
    if((dir = opendir(local_path.c_str())) == NULL){
        Command("mkdir -p "+local_path);
    }

    // 检查是否已经存在该版本文件在磁盘上
    if((dir = opendir((local_path + "/" + version).c_str())) != NULL){
        return;
    }
    
    // 将新版本文件从hdfs下载到本地磁盘
    string hdfs_command = "hdfs dfs -fs hdfs://namenode:9000/ -get /" + version + " " + local_path;
    Command(hdfs_command);
}

// 计算指定slice号和slice内index号的数据块在整个版本数据中的实际块号
// 不区分大小数据一律按大数据算，512/6.4=80，一个slice最多包含80个块
int CalIndex(int slice, int index){
    return slice*80+index;
}

/*
    BlockInMemory类：
        保存某个历史版本的数据块信息
        char**: 数据块
        block_index: 由slice和index映射到数据块中的索引
*/
class BlockInMemory{
    public:
        int current_count;

        // 构造函数
        BlockInMemory(){
            current_count = 0;
            blocks = new BlockData*[max_block_num];
        }

        string GetBlockData(int slice, int index){
            int block = CalIndex(slice, index);
            int actual_index = block_index[block];
            BlockData blockdata = *blocks[actual_index];
            return blockdata.data();
        }

        void AddBlock(BlockData blockdata){
            BlockData* bl = new BlockData;
            BlockInfo* bi = new BlockInfo;
            int slice = blockdata.info().slice_partition(), index = blockdata.info().index();
            bl->mutable_info()->CopyFrom(blockdata.info());
            bl->set_data(blockdata.data());
            int block_index_ = CalIndex(slice, index);
            block_index[block_index_] = current_count;
            blocks[current_count++] = bl;
        }

        void RemoveAll(){
            for(int i = 0; i<current_count; i++){
                delete [] blocks[i];
            }
            delete [] blocks;
            block_index.clear();
            current_count = 0;
            cout << "Delect done!" << endl;
        }

        unordered_map<int, int> block_index;
        BlockData** blocks;
        int slice_size;
        // vector<BlockData> blocks;
};


/*
    父结构体，保存共有属性
    type_: 当前对象类型，用于确定是哪个请求
    status_: 记录当前处理状态（1：处理请求构建响应数据；2：发送响应）
    ctx_: rpc上下文
*/
struct HandleContextBase {
    int type_;
    int status_;
    ServerContext   ctx_;
};

template<typename RequestType, typename ReplyType>
/*
    req_: 接受客户端发送的请求
    rep_: 发送响应给客户端
    responder_: 发送到客户端的方法对象
*/
struct HandleContext : public HandleContextBase {
    RequestType     req_;
    ReplyType       rep_;
    ServerAsyncResponseWriter<ReplyType> responder_;
    // 构造函数
    HandleContext() : responder_(&ctx_){}
};


// 处理GetBlockData rpc请求上下文
typedef HandleContext<GetBlockDataRequest, GetBlockDataResponse> HandleGetBlockDataContext;

// 处理SendCopy rpc请求上下文
typedef HandleContext<SendCopyRequest, SendCopyResponse> HandleSendCopyContext;

// 处理LoadAndRemove请求
typedef HandleContext<Slice2BlockRequest, LoadAndRemoveResponse> HandleLoadAndRemove1Context;
typedef HandleContext<Slice2BlockRequest, LoadAndRemoveResponse> HandleLoadAndRemove2Context;

// 处理Slice2Block rpc请求上下文
typedef HandleContext<Slice2BlockRequest, Slice2BlockResponse> HandleSlice2BlockContext;

/*
    节点类：每个服务器节点上的基本功能实现
*/
class Node final{
    public:
    // 构造函数 接收每个服务器节点的server端口号以便发送消息
    Node(int name_){
        node_name = name_;
        string target_address;
        for(int i=1; i<=6; i++){
            if(i == name_){
                continue;
            }else{
                target_address = "node" + to_string(i) + ":" + server_port;
                SendCopyClient client(grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));
                clients.emplace(i, client);
                cout << "Add client connnect to server " << i << endl;
            }
        }
        current_version = "";
        next_version = "";
    }

    ~Node() {
        server_->Shutdown();
        cq_ptr->Shutdown();
    }

    void Login(){
        //获取自身ip
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
        boost::asio::ip::tcp::resolver::iterator it = resolver.resolve(query);
        boost::asio::ip::tcp::resolver::iterator end;

        etcd::Client etcdClient("etcd:2379");
        while (it != end) {
            boost::asio::ip::tcp::endpoint endpoint = *it++;
            cout << "Local IP: " << endpoint.address().to_string() << std::endl;

            //注册
            string key = "/services/" + std::string("node_worker") ;
            string value = endpoint.address().to_string()+":"+ server_port;
            etcd::Response etcdResponse = etcdClient.set(key, value).get();

            if (etcdResponse.is_ok()) {
                cout << "Service registered successfully: " << key << " -> " << value << std::endl;
            } else {
                cerr << "Failed to register service: " << etcdResponse.error_message() << std::endl;
            
            }

        }
    }

    /*
    BlockInfo{
        int slice_partition: 该块属于哪一个分片数据
        int index: 该块在这个分片数据中的索引号
        int node_id1: 该块第一个副本所在的服务器节点号
        int node_id2: 该块第二个副本所在的服务器节点号
    }

    SliceInfo{
        string      version: 加载指定版本的数据
        int         slice_partition: 加载指定的分片文件
        BlockInfo[] block_info: 记录块信息，指定该块将被发送到哪些节点上进行保存
    }

    Slice2BlockRequest{
        SliceInfo[] slice_info: 一系列待处理的slice分片数据信息
    }

    Slice2BlockResponse{
        bool ok: 是否完成分块操作
    }

    该rpc调用完成加载指定version和分片的模型数据，并将该分片分成固定大小的块发送到不同的
    服务器节点上进行保存
    */
    Status Slice2Block(ServerContext* context, const Slice2BlockRequest* request, Slice2BlockResponse* responde) {
        int slice;
        string version = request->slice_info(0).version();
        if(current_version == ""){
            current_version = version;
        }else if(current_version != version){
            next_version = version;
        }

        // 分割slice成块
        vector<thread> threads;
        for(int i = 0; i < request->slice_info_size(); i++){
            SliceInfo sliceinfo = request->slice_info(i);
            thread t(&Node::LoadAndSend, this, sliceinfo, true);
            threads.push_back(t);
        }

        for(int i = 0; i<request->slice_info_size();i++){
            threads[i].join();
        }

        responde->set_ok(true);
        return Status::OK;
    }

    /*
        加载某个slice并切块和发送到指定节点
    */
    void LoadAndSend(SliceInfo sliceinfo, bool sendcopy2){
        string version = sliceinfo.version();
        int slice = sliceinfo.slice_partition();
        // string filename = local_path + "/" + version + "/";
        string filename = version + "/";

        DIR *dir;
        // dir is not exist, new version is coming, load from hdfs
        if((dir = opendir(filename.c_str())) == NULL){
            DownloadFileFromHDFS(version);
        }

        if(slice>9)
            filename += "model_slice." + to_string(slice);
        else
            filename += "model_slice.0" + to_string(slice);
    
        ModelSliceReader reader;
        vector<thread> send_threads;

        // 创建多个线程，每个线程上运行一个client异步监听
        for(int i=1; i<=6;i++){
            if(i != node_name){
                thread t = thread(&SendCopyClient::AsyncCompleteRpc, &clients[i]);
                send_threads.push_back(t);
            }
        }

        if(reader.Load(filename)){
            cout << "Load file: " << filename << endl;
            for(int i=0; i < sliceinfo.block_info_size(); i++){
                BlockInfo blockinfo = sliceinfo.block_info(i);
                char* block = new char[block_size];
                reader.Read(i*block_size, block_size, block);
                BlockData blockdata;
                blockdata.mutable_info()->CopyFrom(blockinfo);
                blockdata.set_data(block);

                int block_index = CalIndex(blockinfo.slice_partition(), blockinfo.index());
                
                // 调用client发送块，如果是当前节点的块，直接保存到内存
                // 发送副本1
                if(blockinfo.node_id1() == node_name){
                    blocks_copy1[version].AddBlock(blockdata);
                    copy_num[version][block_index] = 1;
                }else{
                    clients[blockinfo.node_id1()].SendCopy(blockdata);
                }

                // 发送副本2
                if(sendcopy2){
                    if(blockinfo.node_id2() == node_name){
                        blocks_copy2[version].AddBlock(blockdata);
                        copy_num[version][block_index] = 2;
                    }else{
                        clients[blockinfo.node_id2()].SendCopy(blockdata);
                    }
                }

                delete block;
            }

            for(int i=0; i<send_threads.size(); i++){
                send_threads[i].join();
            }

            reader.Unload();
        }
    }

    /*
    DataInfo{
        int slice_partition: 分片号
        int index: 分片内块索引
        string version: 版本号
        int start: 读取的数据起始点
        int len: 读取的数据长度
    }

    GetBlockDataRequest{
        DataInfo block_info: 需要读取的数据块的信息
    }

    GetBlockDataResponse{
        string block_data: 读取到的数据块本体
    }

    该rpc调用完成根据接收到的data_info读取块，并将读取到的块数据返回给客户端
    */
    Status GetBlockData(ServerContext* context, const GetBlockDataRequest* request, GetBlockDataResponse* responde) {
        int block_index, copy_num_, slice, index;
        string block, version;
        DataInfo di = request->data_info();
        slice = di.slice_partition();
        index = di.index();
        version = di.version();
        block_index = CalIndex(slice, index);
        if(copy_num[version][block_index] == 1){
            // current_count = 0, 数据已删除或是尚未加载进来
            if(blocks_copy1[version].current_count == 0){
                cout << "Didnot read from " << version << " slice " << slice << " index " << index << endl;
                responde->set_block_data("");
                return Status::OK;
            }
            block = blocks_copy1[version].GetBlockData(slice, index);
        }else{
            if(blocks_copy2[version].current_count == 0){
                cout << "Didnot read from " << version << " slice " << slice << " index " << index << endl;
                responde->set_block_data("");
                return Status::OK;
            }
            block = blocks_copy2[version].GetBlockData(slice, index);
        }
        string block_final = block.substr(di.start(), di.len());
        responde->set_block_data(block_final);
        return Status::OK;
    }

    /*
    SendCopyRequest{
        BlockData block_data: 需要发送副本的数据块信息
    }

    BlockData{
        BlockInfo info: 块信息
        char* data: 块本体数据
    }

    SendCopyResponse{
        bool ok: 发送副本是否成功
    }

    该rpc调用是在服务器节点分割完数据切片后将某个数据块发送到指定的服务器节点上进行保存
    */
    Status SendCopy(ServerContext* context, const SendCopyRequest* request, SendCopyResponse* responde) {
        // node1 == 自身node号，存储在第一个副本；否则存储在第二个副本
        BlockData blockdata = request->block_data();
        int block_index = CalIndex(blockdata.info().slice_partition(), blockdata.info().index());
        if(node_name == blockdata.info().node_id1()){
            blocks_copy1[blockdata.info().version()].AddBlock(blockdata);
            copy_num[blockdata.info().version()][block_index] = 1;
        }else{
            blocks_copy2[blockdata.info().version()].AddBlock(blockdata);
            copy_num[blockdata.info().version()][block_index] = 2;
        }
        responde->set_ok(true);
        return Status::OK;
    }

    /*
    Slice2BlockRequest{
        SliceInfo[] slice_info: 待加载的新版本分片slice信息
    }

    LoadAndRemoveResponse{
        bool ok: 加载新版本以及删除旧副本是否成功
    }
    该rpc调用是接受到版本切换的请求时向内存中加载新的版本块和删除旧的版本块
    整个流程：
        1、load新版本的slice并向指定节点发送第一个副本
        2、所有节点都收到各自的块后，该节点向pd发送第一个ok信号，等待1s
        3、pd将包含旧版本第二副本的所有请求回复
        4、node将自身存储的旧版本第二副本删除
        5、node节点将第二副本发送给指定节点
        6、发送完成后发送第二个ok信号给pd，等待1s
        7、pd将所有包含旧版本的请求回复
        8、系统进入新版本
    */
    Status LoadAndRemove1(ServerContext* context, const Slice2BlockRequest* request, LoadAndRemoveResponse* responde) {
        next_version = request->slice_info(0).version();

        vector<thread> threads;
        for(int i = 0; i < request->slice_info_size(); i++){
            SliceInfo sliceinfo = request->slice_info(i);
            // sendcopy2 == false，暂时不发送第二个副本
            thread t(&Node::LoadAndSend, this, sliceinfo, false);
            threads.push_back(t);
        }

        for(int i = 0; i<request->slice_info_size();i++){
            threads[i].join();
        }

        cout << "Load new version " << next_version << " copy1 done!" << endl;
        responde->set_ok(true);
        return Status::OK;
    }

    Status LoadAndRemove2(ServerContext* context, const Slice2BlockRequest* request, LoadAndRemoveResponse* responde) {
        BlockInMemory bm = blocks_copy1[next_version];

        vector<thread> send_threads;
        // 创建多个线程，每个线程上运行一个client异步监听
        for(int i=1; i<=6;i++){
            if(i != node_name){
                thread t = thread(&SendCopyClient::AsyncCompleteRpc, &clients[i]);
                send_threads.push_back(t);
            }
        }

        for(int i = 0; i<send_threads.size(); i++){
            send_threads[i].join();
        }

        // 读取副本1的block信息中保存的第二个节点号，发送副本
        for(int i=0; i<bm.current_count; i++){
            clients[bm.blocks[i]->info().node_id2()].SendCopy(*(bm.blocks[i]));
        }

        cout << "Load new version " << next_version << " copy2 done!" << endl;
        responde->set_ok(true);
        return Status::OK;
    }

    void Remove(int copy_id){
        // 删除副本1的时候，此时节点中只剩新版本的两个副本，进入新的版本
        if(copy_id == 1){
            blocks_copy1[current_version].RemoveAll();
            current_version = next_version;
            cout << "Transfer to new version " << current_version << endl;
        }else{
            blocks_copy2[current_version].RemoveAll();
        }
    }

    void Run(){
        ServerBuilder builder;
        string server_address("0.0.0.0:" + server_port);
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(service_);

        cout << "Listening at " << server_address << endl;

        cq_ptr = builder.AddCompletionQueue();

        server_ = builder.BuildAndStart();

        // 注册到etcd
        Login();

        // 进入死循环等待请求并处理之前先注册一下请求
        {
            HandleSlice2BlockContext* hs2bc = new HandleSlice2BlockContext;
            hs2bc->type_ = 1;       // 设置类型1， 切割slice rpc调用
            hs2bc->status_ = 1;     // 设置状态为1，表示为请求处理阶段
            HandleSendCopyContext* hscc = new HandleSendCopyContext;
            hscc->type_ = 2;        // 设置类型2， 发送副本rpc调用
            hscc->status_ = 1;
            HandleGetBlockDataContext* hgbc = new HandleGetBlockDataContext;
            hgbc->type_ = 3;        // 设置类型3， 获取某个块rpc调用
            hgbc->status_ = 1;
            HandleLoadAndRemove1Context* hlr1c = new HandleLoadAndRemove1Context;
            hlr1c->type_ = 4;        // 设置类型4，发送新版本的第一个副本和删除旧版本的第二个副本
            hlr1c->status_ = 1;
            HandleLoadAndRemove2Context* hlr2c = new HandleLoadAndRemove2Context;
            hlr2c->type_ = 5;        // 设置类型5，发送新版本第二个副本和删除旧版本的第一个副本
            hlr2c->status_ = 1;

            service_->RequestSlice2Block(&hs2bc->ctx_, &hs2bc->req_, &hs2bc->responder_, cq_ptr.get(), cq_ptr.get(), hs2bc);
            service_->RequestSendCopy(&hscc->ctx_, &hscc->req_, &hscc->responder_, cq_ptr.get(), cq_ptr.get(), hscc);
            service_->RequestGetBlockData(&hgbc->ctx_, &hgbc->req_, &hgbc->responder_, cq_ptr.get(), cq_ptr.get(), hgbc);
            service_->RequestLoadAndRemove1(&hlr1c->ctx_, &hlr1c->req_, &hlr1c->responder_, cq_ptr.get(), cq_ptr.get(), hlr1c);
            service_->RequestLoadAndRemove2(&hlr2c->ctx_, &hlr2c->req_, &hlr2c->responder_, cq_ptr.get(), cq_ptr.get(), hlr2c);
        }

        // 创建线程池
        ThreadPool pool(5);

        while(true){
            // 已经注册了请求处理，这里阻塞从完成队列中取出一个请求进行处理
            HandleContextBase* hcb = NULL;
            bool ok = false;
            GPR_ASSERT(cq_ptr->Next((void**)&hcb, &ok));
            GPR_ASSERT(ok);

            /*
                根据tag分辨是哪一种请求
                第一次注册请求处理时使用的是对象地址
                直接从map中取出进行判断
            */
           int type = hcb->type_;
           // status为2时，响应已经发送
           if(hcb->status_ == 2){
                switch (type)
                {
                    case 1:
                    {
                        delete (HandleSlice2BlockContext*)hcb;
                    }
                    break;
                    case 2:
                    {
                        delete (HandleSendCopyContext*)hcb;
                    }
                    break;
                    case 3:
                    {
                        delete (HandleGetBlockDataContext*)hcb;
                    }
                    break;
                    case 4:
                    {
                        delete (HandleLoadAndRemove1Context*)hcb;
                    }
                    break;
                    case 5:
                    {
                        delete (HandleLoadAndRemove2Context*)hcb;
                    }
                    break;
                }
                continue;
           }

           // 重新创建一个请求处理上下文对象（以便接收下一个请求进行处理）
            switch (type){
                case 1: {
                    HandleSlice2BlockContext* hsbc = new HandleSlice2BlockContext;
                    hsbc->status_ = 1;
                    hsbc->type_ = 1;
                    service_->RequestSlice2Block(&hsbc->ctx_, &hsbc->req_, &hsbc->responder_, cq_ptr.get(), cq_ptr.get(), hsbc);
                }
                break;
                case 2: {
                    HandleSendCopyContext* hscc = new HandleSendCopyContext;
                    hscc->status_ = 1;
                    hscc->type_ = 2;
                    service_->RequestSendCopy(&hscc->ctx_, &hscc->req_, &hscc->responder_, cq_ptr.get(), cq_ptr.get(), hscc);
                }
                break;
                case 3: {
                    HandleGetBlockDataContext* hgbc = new HandleGetBlockDataContext;
                    hgbc->status_ = 1;
                    hgbc->type_ = 3;
                    service_->RequestGetBlockData(&hgbc->ctx_, &hgbc->req_, &hgbc->responder_, cq_ptr.get(), cq_ptr.get(), hgbc);
                }
                break;
                case 4: {
                    HandleLoadAndRemove1Context* hlr1c = new HandleLoadAndRemove1Context;
                    hlr1c->status_ = 1;
                    hlr1c->type_ = 4;
                    service_->RequestLoadAndRemove1(&hlr1c->ctx_, &hlr1c->req_, &hlr1c->responder_, cq_ptr.get(), cq_ptr.get(), hlr1c);
                }
                break;
                case 5: {
                    HandleLoadAndRemove2Context* hlr2c = new HandleLoadAndRemove2Context;
                    hlr2c->status_ = 1;
                    hlr2c->type_ = 5;
                    service_->RequestLoadAndRemove2(&hlr2c->ctx_, &hlr2c->req_, &hlr2c->responder_, cq_ptr.get(), cq_ptr.get(), hlr2c);
                }
                break;
            }

            pool.enqueue([type, hcb, this](){
                // 根据type执行相应的操作
                switch (type)
                {
                    case 1: {
                        HandleSlice2BlockContext* hc = (HandleSlice2BlockContext*)hcb;
                        Status status = Slice2Block(&hc->ctx_, &hc->req_, &hc->rep_);
                        // 执行完操作将状态改为发送响应
                        hc->status_ = 2;
                        // 调用responder_进行异步响应发送
                        hc->responder_.Finish(hc->rep_, status, hcb); 
                    }
                    break;
                    case 2: {
                        HandleSendCopyContext* hc = (HandleSendCopyContext*)hcb;
                        Status status = SendCopy(&hc->ctx_, &hc->req_, &hc->rep_);
                        hc->status_ = 2;
                        hc->responder_.Finish(hc->rep_, status, hcb);
                    }
                    break;
                    case 3: {
                        HandleGetBlockDataContext* hc = (HandleGetBlockDataContext*)hcb;
                        Status status = GetBlockData(&hc->ctx_, &hc->req_, &hc->rep_);
                        hc->status_ = 2;
                        hc->responder_.Finish(hc->rep_, status, hcb);
                    }
                    break;
                    case 4: {
                        HandleLoadAndRemove1Context* hc = (HandleLoadAndRemove1Context*)hcb;
                        Status status = LoadAndRemove1(&hc->ctx_, &hc->req_, &hc->rep_);
                        hc->status_ = 2;
                        hc->responder_.Finish(hc->rep_, status, hcb);
                        sleep(1);
                        // 删除副本2
                        Remove(2);
                    }
                    break;
                    case 5: {
                        HandleLoadAndRemove2Context* hc = (HandleLoadAndRemove2Context*)hcb;
                        Status status = LoadAndRemove2(&hc->ctx_, &hc->req_, &hc->rep_);
                        hc->status_ = 2;
                        hc->responder_.Finish(hc->rep_, status, hcb);
                        sleep(1);
                        // 删除副本1
                        Remove(1);
                    }
                    break;
                }
            });
        }
    }

    private:
        string server_port = "5001";
        // string client_port = "50052";
        int node_name;

        // 多个客户端链接到不同的服务器
        unordered_map<int, SendCopyClient> clients;

        NodeService::AsyncService* service_;
        unique_ptr<Server> server_;
        unique_ptr<ServerCompletionQueue> cq_ptr;

        // 副本1保存的块数据
        unordered_map<string, BlockInMemory> blocks_copy1;
        // 副本2保存的块数据
        unordered_map<string, BlockInMemory> blocks_copy2;

        // 映射申请的某一块数据是属于服务器上保存的第一副本还是第二副本
        unordered_map<string, unordered_map<int, int>> copy_num;
        // 目前的版本号
        string current_version;
        string next_version;
};

int main(int argc, char** argv){
    // 初始化服务器节点
    // 必须指定节点号
    int node_number;
    if(argc>1){
        node_number = atoi(argv[1]);
        Node node(node_number);
        node.Run();
    }else{
        cout << "Node number is required." << endl;
    }

    return 0;
}