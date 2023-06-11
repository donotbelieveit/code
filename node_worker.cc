#include<memory>
#include<thread>
#include<iostream>
#include<string>
#include<dirent.h>
#include"ModelSliceReader.h"
#include"ThreadPool.h"
#include<unordered_map>
#include<vector>


#include "alimama.grpc.pb.h"
#include<grpcpp/grpcpp.h>

using namespace std;

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerAsyncWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::Status;
using grpc::ServerContext;

using alimama::proto::BlockInfo;
using alimama::proto::SliceInfo;
using alimama::proto::Slice2BlockRequest;
using alimama::proto::Slice2BlockResponse;

using alimama::proto::BlockData;
using alimama::proto::GetBlockDataRequest;
using alimama::proto::GetBlockDataResponse;
using alimama::proto::SendCopyRequest;
using alimama::proto::SendCopyResponse;
using alimama::proto::LoadAndRemoveRequest;
using alimama::proto::LoadAndRemoveResponse;

using alimama::proto::NodeService;


string local_path = "./tmp";
// block size is 6.4MB
// int block_size = 1048576;

// 这部分还要改的
int block_size = 64000;
int slice_size = 6400000;
int block_len = slice_size / block_size;
int max_block_num = 6400;

string test_version_path = "/root/testbench/model_2023_03_01_12_30_00";

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
int CalIndex(int slice, int index){
    return slice*block_len+index;
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
            blocks = new char*[max_block_num];
        }

        char* GetBlockData(int slice, int index){
            int block = CalIndex(slice, index);
            int actual_index = block_index[block];
            return blocks[actual_index];
        }

        void LoadBlock(int slice, string version){
            // string filename = local_path + "/" + version + "/";
            string filename = version + "/";

            DIR *dir;
            // dir is not exist, new version is coming, load from hdfs
            if((dir = opendir(filename.c_str())) == NULL){
                DownloadFileFromHDFS(version);
            }

            // int slice_size = 0;


            if(slice>9)
                filename += "model_slice." + to_string(slice);
            else
                filename += "model_slice.0" + to_string(slice);
    
            ModelSliceReader reader;

            if(reader.Load(filename)){
                cout << "Load file" << endl;
                for(int i=0; i < (slice_size / block_size); i++){
                    char* block = new char[block_size];
                    int index = slice*block_len+i;
                    reader.Read(i*block_size, block_size, block);
                    block_index[index] = current_count;
                    blocks[current_count++] = block;
                    // delete block;
                }

                reader.Unload();
            }
        }

        void AddBlock(int slice, int index, string block_data){
            int block_index_ = CalIndex(slice, index);
            char* block_local = new char[block_size];
            memcpy(block_local, block_data.c_str(), block_size);
            block_index[block_index_] = current_count;
            blocks[current_count++] = block_local;
        }

        void RemoveAll(){
            for(int i = 0; i<current_count; i++){
                delete [] blocks[i];
            }
            delete [] blocks;
            block_index.clear();
            cout << "Delect done!" << endl;
        }

    private:
        unordered_map<int, int> block_index;
        char** blocks;
};

// unordered_map<string, BlockInMemory> version_blocks;


// // 加载块到内存内
// void LoadNewBlock2Buf(vector<char*> *buffer, unordered_map<int, int> *index_hash, string version, int slice){
//     // string filename = local_path + "/" + version + "/";
//     string filename = version + "/";

//     DIR *dir;
//     // dir is not exist, new version is coming, load from hdfs
//     if((dir = opendir(filename.c_str())) == NULL){
//         DownloadFileFromHDFS(version);
//     }

//     if(slice>9)
//         filename += "model_slice." + to_string(slice);
//     else
//         filename += "model_slice.0" + to_string(slice);

//     cout << filename << endl;
    
//     ModelSliceReader reader;

//     if(reader.Load(filename)){
//         cout << "Load file" << endl;
//         for(int i=0; i < (slice_size / block_size); i++){
//             char* block = new char[block_size];
//             int index = slice*block_len+i;
//             reader.Read(i*block_size, block_size, block);
//             index_hash->emplace(index, buffer->size());
//             buffer->push_back(block);
//             // delete block;
//         }

//         reader.Unload();
//     }

//     cout << "Load block done!" << endl;
//     cout << "load blocks: " << buffer->size() << endl;
// }

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

// 处理LoadAndRemove rpc请求上下文，服务器流式传输
struct HandleLoadAndRemoveContext : public HandleContextBase {
    LoadAndRemoveRequest    req_;
    LoadAndRemoveResponse   rep_;
    ServerAsyncWriter<LoadAndRemoveResponse>       responder_;
    // 构造函数
    HandleLoadAndRemoveContext() : responder_(&ctx_){}
};



// 处理GetBlockData rpc请求上下文
typedef HandleContext<GetBlockDataRequest, GetBlockDataResponse> HandleGetBlockDataContext;

// 处理SendCopy rpc请求上下文
typedef HandleContext<SendCopyRequest, SendCopyResponse> HandleSendCopyContext;


// typedef HandleContext<LoadAndRemoveRequest, LoadAndRemoveResponse> HandleLoadAndRemoveContext;

// 处理Slice2Block rpc请求上下文
typedef HandleContext<Slice2BlockRequest, Slice2BlockResponse> HandleSlice2BlockContext;

/*
    节点类：每个服务器节点上的基本功能实现
*/
class Node final{
    public:
    // 构造函数 接收每个服务器节点的server端口号以便发送消息
    Node(string name_, vector<pair<string, string>> node_list){
        node_name = name_;
        for(auto p: node_list){
            node_channel[p.first] = p.second;
        }
        current_version = "";
        next_version = "";
    }

    ~Node() {
        server_->Shutdown();
        cq_ptr->Shutdown();
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
   // 没写完 待改 还没有发送块
    Status Slice2Block(ServerContext* context, const Slice2BlockRequest* request, Slice2BlockResponse* responde) {
        int slice;
        string version = request->slice_info(0).version();
        if(current_version == ""){
            current_version = version;
        }else if(current_version != version){
            next_version = version;
        }
        for(int i = 0; i < request->slice_info_size(); i++){
            SliceInfo sliceinfo = request->slice_info(i);
            BlockInMemory slice_block;
            slice = sliceinfo.slice_partition();
            slice_block.LoadBlock(slice, version);
            blocks_buffer[slice] = slice_block;
        }
        responde->set_ok(true);
        return Status::OK;
    }

    /*
    BlockInfo{
        int slice_partition: 该块属于哪一个分片数据
        int index: 该块在这个分片数据中的索引号
        int node_id1: 该块第一个副本所在的服务器节点号
        int node_id2: 该块第二个副本所在的服务器节点号
    }

    BlockData{
        int slice_partition: 该块属于的数据分片号
        int block_index: 该块在该数据分片中的索引号
        string node1: 该块第一个副本所在的服务器节点号
        string node2: 该块第二个副本所在的服务器节点号
        char* data: 块本体数据
        string version: 该块属于哪个版本
    }

    GetBlockDataRequest{
        BlockInfo[] block_info: 需要读取的数据块的信息
    }

    GetBlockDataResponse{
        BlockData[] block_data: 读取到的数据块本体和其他信息
    }

    该rpc调用完成根据接收到的data_info读取块，并将读取到的块数据返回给客户端
    */
    Status GetBlockData(ServerContext* context, const GetBlockDataRequest* request, GetBlockDataResponse* responde) {
        int block_index, copy_num_, slice, index;
        char* block;
        for(int i=0; i<request->block_info_size(); i++){
            BlockInfo blockinfo = request->block_info(i);
            BlockData blockdata;
            slice = blockinfo.slice_partition();
            index = blockinfo.index();
            blockdata.set_slice_partition(slice);
            blockdata.set_block_index(index);
            blockdata.set_node1("node" + to_string(blockinfo.node_id1()));
            blockdata.set_node2("node" + to_string(blockinfo.node_id2()));
            blockdata.set_version(current_version);
            block_index = CalIndex(slice, index);
            copy_num_ = copy_num[current_version][block_index];
            if(copy_num_ == 1){
                block = blocks_copy1[current_version].GetBlockData(slice, index);
            }else{
                block = blocks_copy2[current_version].GetBlockData(slice, index);
            }
            blockdata.set_data(block);
            responde->add_block_data()->CopyFrom(blockdata);
        }
        return Status::OK;
    }

    /*
    SendCopyRequest{
        BlockData block_data: 需要发送副本的数据块信息
    }

    BlockData{
        int slice_partition: 该块属于的数据分片号
        int block_index: 该块在该数据分片中的索引号
        string node1: 该块第一个副本所在的服务器节点号
        string node2: 该块第二个副本所在的服务器节点号
        char* data: 块本体数据
        string version: 该块属于哪个版本
    }

    SendCopyResponse{
        bool ok: 发送副本是否成功
    }

    该rpc调用是在服务器节点分割完数据切片后将某个数据块发送到指定的服务器节点上进行保存
    */
    Status SendCopy(ServerContext* context, const SendCopyRequest* request, SendCopyResponse* responde) {
        // node1 == 自身node号，存储在第一个副本；否则存储在第二个副本
        BlockData blockdata = request->block_data();
        int block_index = CalIndex(blockdata.slice_partition(), blockdata.block_index());
        if(node_name == blockdata.node1()){
            blocks_copy1[blockdata.version()].AddBlock(blockdata.slice_partition(), blockdata.block_index(), blockdata.data());
            copy_num[blockdata.version()][block_index] = 1;
        }else{
            blocks_copy2[blockdata.version()].AddBlock(blockdata.slice_partition(), blockdata.block_index(), blockdata.data());
            copy_num[blockdata.version()][block_index] = 2;
        }
        responde->set_ok(true);
        return Status::OK;
    }

    /*
    LoadAndRemoveRequest{
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
    Status LoadAndRemove(ServerContext* context, const LoadAndRemoveRequest* request, ServerAsyncWriter<LoadAndRemoveResponse>* responde) {
        for(int i=0;i<request->slice_info_size();i++){

        }
        LoadAndRemoveResponse* rep_ = new LoadAndRemoveResponse;
        rep_->set_ok(true);
        // responde->Write((const LoadAndRemoveResponse)rep_);
        // 系统等待1s
        sleep(1);

        // 删除旧版本第二个副本内容
        blocks_copy2[current_version].RemoveAll();
        

        sleep(1);
        blocks_copy1[current_version].RemoveAll();
        current_version = next_version;
        return Status::OK;
    }

    void Run(){
        ServerBuilder builder;
        string server_address("0.0.0.0:" + node_channel[node_name]);
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(service_);

        cq_ptr = builder.AddCompletionQueue();

        server_ = builder.BuildAndStart();

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
            HandleLoadAndRemoveContext* hlrc = new HandleLoadAndRemoveContext;
            hlrc->type_ = 4;        // 设置类型4，加载新版本和卸载旧版本信息
            hlrc->status_ = 1;

            service_->RequestSlice2Block(&hs2bc->ctx_, &hs2bc->req_, &hs2bc->responder_, cq_ptr.get(), cq_ptr.get(), hs2bc);
            service_->RequestSendCopy(&hscc->ctx_, &hscc->req_, &hscc->responder_, cq_ptr.get(), cq_ptr.get(), hscc);
            service_->RequestGetBlockData(&hgbc->ctx_, &hgbc->req_, &hgbc->responder_, cq_ptr.get(), cq_ptr.get(), hgbc);
            service_->RequestLoadAndRemove(&hlrc->ctx_, &hlrc->req_, &hlrc->responder_, cq_ptr.get(), cq_ptr.get(), hlrc);
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
                        delete (HandleLoadAndRemoveContext*)hcb;
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
                    HandleLoadAndRemoveContext* hlrc = new HandleLoadAndRemoveContext;
                    hlrc->status_ = 1;
                    hlrc->type_ = 4;
                    service_->RequestLoadAndRemove(&hlrc->ctx_, &hlrc->req_, &hlrc->responder_, cq_ptr.get(), cq_ptr.get(), hlrc);
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
                        HandleLoadAndRemoveContext* hc = (HandleLoadAndRemoveContext*)hcb;
                        Status status = LoadAndRemove(&hc->ctx_, &hc->req_, &hc->responder_);
                        hc->status_ = 2;
                        // hc->responder_.Finish();
                    }
                    break;
                }
            });
        }
    }

    private:
        map<string, string> node_channel;
        string node_name;

        NodeService::AsyncService* service_;
        unique_ptr<Server> server_;
        unique_ptr<ServerCompletionQueue> cq_ptr;

        // 副本1保存的块数据
        unordered_map<string, BlockInMemory> blocks_copy1;
        // 副本2保存的块数据
        unordered_map<string, BlockInMemory> blocks_copy2;
        // 服务器节点暂时保存的分割好但还没发送往各自服务器节点的slice块数据
        unordered_map<int, BlockInMemory> blocks_buffer;
        // 映射申请的某一块数据是属于服务器上保存的第一副本还是第二副本
        unordered_map<string, unordered_map<int, int>> copy_num;
        // 目前的版本号
        string current_version;
        string next_version;
};

// int main(int argc, char** argv){
//     // old_version = new unordered_map<pair<int, int>, char*>();
//     // auto old_version = new vector<char*>();
    

//     return 0;
// }