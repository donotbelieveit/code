#include <iostream>
#include <grpcpp/grpcpp.h>
#include "alimama.pb.h"
#include "alimama.grpc.pb.h"
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <filesystem>
#include <unordered_map>
#include <future>
#include <hdfs/hdfs.h>
#include <random>
#include <boost/asio.hpp>
#include <mutex>
#include <etcd/Client.hpp>
#include <string>
#include <cstring> 

namespace fs = std::filesystem;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ClientContext;
using grpc::ClientReader;

using alimama::proto::ModelService;
using alimama::proto::BlockInfo;
using alimama::proto::SliceInfo;
using alimama::proto::Slice2BlockRequest;
using alimama::proto::Slice2BlockResponse;
using alimama::proto::NodeService;
using alimama::proto::LoadAndRemoveRequest;
using alimama::proto::LoadAndRemoveResponse;
using alimama::proto::GetBlockDataRequest;
using alimama::proto::GetBlockDataResponse;
using alimama::proto::SliceRequest;
using alimama::proto::DataInfo;




int NODE_NUM = 6;
std::string WORK_DIR = "/";
int BLOCK_SIZE = 6400000;


bool startsWith(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) {
        return false;
    }
    return (str.compare(0, prefix.length(), prefix) == 0);
}

//从path获取name
// template<typename T>
// char* get_name(T path){
//     std::string tmp = "";
//     if(typeid(path)!=typeid(std::string)){
//         tmp = std::string(path);
//     }else{
//         tmp = path;
//     }
//     std::size_t found = tmp.find_last_of("/");
//     if (found != std::string::npos) {
//         return const_cast<char*>(tmp.substr(found + 1).c_str());
//     }
//     return const_cast<char*>(tmp.c_str());
// }
template <typename T>
const char* get_name(T path) {
    std::string tmp = "";
    if (typeid(path) != typeid(std::string)) {
        tmp = std::string(path);
    } else {
        tmp = path;
    }
    std::size_t found = tmp.find_last_of("/");
    if (found != std::string::npos) {
        std::string fileName = tmp.substr(found + 1);
        char* result = new char[fileName.length() + 1];
        std::strcpy(result, fileName.c_str());
        return result;
    }
    char* result = new char[tmp.length() + 1];
    std::strcpy(result, tmp.c_str());
    return result;
}

void register2etcd(int channel){
    //获取自身ip
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::resolver resolver(io_context);
    boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
    boost::asio::ip::tcp::resolver::iterator it = resolver.resolve(query);
    boost::asio::ip::tcp::resolver::iterator end;

    etcd::Client etcdClient("etcd:2379");
    while (it != end) {
        boost::asio::ip::tcp::endpoint endpoint = *it++;
        std::cout << "Local IP: " << endpoint.address().to_string() << std::endl;

        //注册
        std::string key = "/services/modelservice/" + std::string("scheduler") ;
        std::string value = endpoint.address().to_string()+":"+std::to_string(channel);
        etcd::Response etcdResponse = etcdClient.set(key, value).get();

        if (etcdResponse.is_ok()) {
            std::cout << "Service registered successfully: " << key << " -> " << value << std::endl;
        } else {
            std::cerr << "Failed to register service: " << etcdResponse.error_message() << std::endl;
        }
    }

}
class Scheduler final : public alimama::proto::ModelService::Service{
    public:
        
        Status Get(ServerContext* context, const alimama::proto::Request* request, alimama::proto::Response* response) override {

            // 设置返回结果的状态码
            response->set_status(0);

            // 添加一些 slice_data 到 response
            // response->add_slice_data(data);
            std::vector<std::future<std::string>> futures;

            // 遍历 slice_request 数组
            for (const SliceRequest& slice : request->slice_request()) {
                // 启动异步任务，并将 future 对象保存到向量中
                uint64_t slice_partition = slice.slice_partition();
                uint64_t data_start = slice.data_start();
                uint64_t data_len = slice.data_len();
                futures.push_back(std::async(std::launch::async, &Scheduler::get_block_data,this,slice_partition,data_start,data_len));
            }
            
            // 等待所有异步任务完成，并按原顺序将数据写入响应
            for (auto& future : futures) {
                // 获取异步任务的结果
                std::string data = future.get();
                
                // 将数据写入响应的 slice_data 字段
                response->add_slice_data(std::move(data));
            }
            return Status::OK;
        }
        //向各节点发送slice信息加载模型
        bool slice2block(int index,Slice2BlockRequest request){
            Slice2BlockResponse response;   
            ClientContext context;
            Status status = stubs[index]->Slice2Block(&context,request,&response);
            if(status.ok()){
                return true;
            }
            int node = request.slice_info().begin()->slice_partition()%node_num;
            std::cout<<"slice2block error on "<<node<<std::endl;
            return false;
        }
        //向node发送getdata请求
        std::string send_get_block_data(int index,DataInfo info){
            GetBlockDataRequest request;
            request.set_allocated_data_info(&info);
            GetBlockDataResponse response;
            ClientContext context;
            Status status = stubs[index]->GetBlockData(&context,request,&response);
            if(status.ok()){
                return response.block_data();
            }else{
                std::cerr<<"send_get_block_data error on node-"<<index<<std::endl;
            }
            return "";
        }
        std::string get_block_data(int slice_partition,int data_start,int data_len){
            //求下标
            int first_index = data_start%block_size;
            int last_index = (data_start+data_len)%block_size;

            std::vector<std::future<std::string>> futures;


            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(1, 2);

            bool flag = 0;
            mtx.lock();
            //获取版本状态，switchstep为1时，两个版本各存在一个副本，为0时，只存在一个版本
            if(num_version==2){
                if(switch_step==1){
                    flag = 1;
                }
            }
            mtx.unlock();
            for (int i=first_index;i<=last_index;i++) {
                DataInfo datainfo;
                datainfo.set_slice_partition(slice_partition);
                datainfo.set_index(i);
                int index = -1;
                if(flag){
                    //随机从两个版本选择一个
                    if(dist(gen)){
                        datainfo.set_version(version1);
                        index = block_map1[slice_partition*1000+i].node_id1();
                    }else{
                        datainfo.set_version(version2);
                        index = block_map2[slice_partition*1000+i].node_id1();
                    }
                }
                else{
                    //随机从两个副本选择一个
                    datainfo.set_version(version1);
                    if(dist(gen)){
                        index = block_map1[slice_partition*1000+i].node_id1();
                    }else{
                        index = block_map1[slice_partition*1000+i].node_id2();
                    }
                }

                //处理block边界
                if(i==first_index){
                    datainfo.set_start(data_start%block_size);
                    datainfo.set_len(block_size-data_start%block_size);
                }else if(i==last_index){
                    datainfo.set_start(0);
                    datainfo.set_len((data_start+data_len)%block_size);
                }else{
                    datainfo.set_start(0);
                    datainfo.set_len(block_size);
                }
                futures.push_back(std::async(std::launch::async, &Scheduler::send_get_block_data,this,index,datainfo));
            }
            
            // 等待所有异步任务完成，并按原顺序将数据写入响应
            std::string data = "";
            for (auto& future : futures) {
                // 获取异步任务的结果
                data += future.get();
                
                // 将数据写入响应的 slice_data 字段

            }
            return data;
        }
        //向各节点发送load_and_remove请求
        //阶段1，load新版本，删除旧版本第二副本
        //阶段2，send新版本第二副本，删除旧版本
        void send_load_and_remove(int index,int step,Slice2BlockRequest request){
            LoadAndRemoveResponse response;
            ClientContext context;
            Status status;
            if(step==1)
                status = stubs[index]->LoadAndRemove1(&context,request,&response);
            else
                status = stubs[index]->LoadAndRemove2(&context,request,&response);
            if(status.ok()){
                std::cout<<"send_load_and_remove success at "<<step<<" on node-"<<index<<std::endl;
            }else{
                std::cerr<<"send_load_and_remove error at "<<step<<" on node-"<<index<<std::endl;
            }
        }
        bool load_and_remove(std::vector<Slice2BlockRequest>& requests){
            //第一阶段，load第一副本，remove旧版本第二副本
            std::vector<std::thread> threads;
            for(int i=0;i<node_num;i++){
                threads.push_back(std::thread(&Scheduler::send_load_and_remove,this,i,1,requests[i]));
            }
            for(auto& thread:threads){
                thread.join();
            }
            threads.clear();

            //进入switchstep1
            mtx.lock();
            num_version++;
            switch_step = 1;
            mtx.unlock();

            //第二阶段，send第二副本，remove旧版本第一副本，完成切换
            for(int i=0;i<node_num;i++){
                Slice2BlockRequest request;
                threads.push_back(std::thread(&Scheduler::send_load_and_remove,this,i,2,request));
            }
            for(auto& thread:threads){
                thread.join();
            }
            threads.clear();

            //切换完成，更新版本状态
            mtx.lock();
            num_version--;
            version1 = version2;
            version2 = "";
            block_map1 = block_map2;
            block_map2.clear();
            switch_step = 0;
            mtx.unlock();
            return true;
        }
        void load_model(std::string version_name,std::unordered_map<int, BlockInfo> &block_map){
            const char* hdfsUrl = "hdfs://namenode:9000";
            const char* directoryPath = "";  // 要监听的 HDFS 目录路径

            std::vector<Slice2BlockRequest> requests(node_num);

            hdfsFS fs = hdfsConnect(hdfsUrl, 0);
            if (fs == NULL) {
                std::cerr << "Failed to connect to HDFS" << std::endl;
            }
            std::string model_dir = std::string(directoryPath) + "/" + version_name;
            //监听model.done文件    
            while(hdfsExists(fs, (model_dir+"/model.done").c_str())!=0){
                std::cout<<"waiting for model done:"+model_dir+"/model.done"<<std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(10)); 
            }
            std::cout<<"find model done:"+model_dir+"/model.done"<<std::endl;

            //读取meta文件
            std::string model_meta = model_dir+"/model.meta";
            
            hdfsFile file = hdfsOpenFile(fs, model_meta.c_str(), O_RDONLY, 0, 0, 0);
            if (!file) {
                std::cerr << "Failed to open file: " << model_meta << std::endl;
                hdfsDisconnect(fs);
            }

            const int bufferSize = 1024;
            char buffer[bufferSize];
            std::string secondLine;

            // 读取第二行的slice：xxx，size：xxx
            if (hdfsRead(fs, file, buffer, bufferSize) <= 0) {
                std::cerr << "Failed to read file: " << model_dir << std::endl;
                hdfsCloseFile(fs, file);
                hdfsDisconnect(fs);
            }

            // Read the second line
            tSize bytesRead = hdfsRead(fs, file, buffer, bufferSize);
            if (bytesRead > 0) {
                // Find the newline character
                char* newlinePos = strchr(buffer, '\n');
                if (newlinePos) {
                    *newlinePos = '\0';  // Null-terminate the second line
                    secondLine = buffer;
                }
            }

            hdfsCloseFile(fs, file);

            //读取第二行，获取slice_size
            int slice_size = -1;
            size_t slicePos = secondLine.find("slice:");
            size_t sizePos = secondLine.find("size:");

            if (slicePos != std::string::npos && sizePos != std::string::npos) {
                std::string size = secondLine.substr(sizePos + 5);
                slice_size = std::stoi(size);
            }

            std::unordered_map<int,BlockInfo> block_map_tmp;
            //random(1,node,num)
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(1, node_num);

            //将所有slice分成block
            hdfsFileInfo* fileInfoList = nullptr;
            int numEntries = -1;
            fileInfoList = hdfsListDirectory(fs, model_dir.c_str(), &numEntries);
            if (numEntries < 0) {
                std::cerr << "Failed to list directory: " << directoryPath << std::endl;
                hdfsDisconnect(fs);
            }
            //std::cout << "find slices" <<std::endl;
            for (int i = 0; i < numEntries; ++i) {
                const hdfsFileInfo& fileInfo = fileInfoList[i];
                if(std::string(fileInfo.mName).find("model_slice") == std::string::npos){
                    continue;
                }
                std::string slice_name = std::string(get_name(fileInfo.mName));
                std::size_t dotPosition = slice_name.find_last_of('.');
                int slice_partition = -1;
                if (dotPosition != std::string::npos && dotPosition < slice_name.length() - 1) {
                    std::cout << "slice name: " << slice_name << std::endl;
                    slice_partition = std::stoi(slice_name.substr(dotPosition + 1));
                }
                
                SliceInfo slice_info;
                slice_info.set_slice_partition(slice_partition);
                slice_info.set_version(version_name);
                slice_info.set_slice_size(slice_size);
                for(int i=0;i*block_size<=slice_size;i++){
                    BlockInfo block_info;
                    block_info.set_slice_partition(slice_partition);
                    block_info.set_index(i);
                    int randomNum1 = dist(gen);
                    int randomNum2;
                    do {
                        randomNum2 = dist(gen);
                    } while (randomNum2 == randomNum1);
                    block_info.set_node_id1(randomNum1);
                    block_info.set_node_id2(randomNum2);
                    block_map_tmp[slice_partition*1000+i] = block_info;
                    slice_info.add_block_info()->CopyFrom(block_info);
                }
                requests[slice_partition%node_num].add_slice_info()->CopyFrom(slice_info);
            }

            hdfsFreeFileInfo(fileInfoList, numEntries);
            hdfsDisconnect(fs);
            block_map = block_map_tmp;
            //将block分配给node
            if(num_version==0){
                std::vector<std::thread> threads;
                for(int i=0;i<node_num;i++){
                    threads.push_back(std::thread(&Scheduler::slice2block,this,i,requests[i]));
                }
                for(auto&thread:threads){
                    thread.join();
                }
                threads.clear();


                mtx.lock();
                num_version++;
                std::cout<<"finished first load"<<std::endl;
                version1 = version_name;
                mtx.unlock();

                
            }else{
                load_and_remove(requests);
            }

            

        }
        void model_monitor(std::string work_dir){
            const char* hdfsUrl = "hdfs://namenode:9000";
            const char* directoryPath = "/";  // 要监听的 HDFS 目录路径
            std::string prefix1 = "model";
            std::string prefix2 = "rollback";

            hdfsFS fs = hdfsConnect(hdfsUrl, 0);
            if (fs == NULL) {
                std::cerr << "Failed to connect to HDFS" << std::endl;
            }

            // 存储上次检查的文件列表
            std::vector<std::string> previousFiles;

            while (true) {
                int numEntries = 0;
                hdfsFileInfo* fileInfo = hdfsListDirectory(fs, directoryPath, &numEntries);
                if (fileInfo == NULL) {
                    std::cerr << "Failed to list directory: " << directoryPath << std::endl;
                    hdfsDisconnect(fs);
                }

                std::vector<std::string> currentFiles;

                for (int i = 0; i < numEntries; ++i) {
                    const char* fileNameChars = get_name(fileInfo[i].mName);
                    std::string fileName(fileNameChars);
                    std::cout << "current file names: " << fileName << std::endl;
                    if(!(startsWith(fileName,prefix1)||startsWith(fileName,prefix2))){
                        continue;
                    }

                    currentFiles.push_back(fileName);

                    // 检查当前文件是否为新文件
                    if (std::find(previousFiles.begin(), previousFiles.end(), fileName) == previousFiles.end()) {
                        std::cout << "Found new file: " << fileName << std::endl;
                        std::string version_name = "";
                        if(fileInfo[i].mKind == kObjectKindDirectory){
                            std::cout << "New version detected: " << fileName << std::endl;
                            // TODO: 处理新文件夹
                            version_name = std::string(fileName);
                        }else{
                            std::string filePath = std::string(directoryPath) + "/" + std::string(fileName);
                            hdfsFile file = hdfsOpenFile(fs, filePath.c_str(), O_RDONLY, 0, 0, 0);
                            if (!file) {
                                std::cerr << "Failed to open file: " << filePath << std::endl;
                                hdfsDisconnect(fs);
                            }

                            const int bufferSize = 1024;
                            char buffer[bufferSize];
                            std::string version_name;

                            tSize bytesRead;
                            while ((bytesRead = hdfsRead(fs, file, buffer, bufferSize)) > 0) {
                                version_name.append(buffer, bytesRead);
                            }

                            hdfsCloseFile(fs, file);
                        }
                        if(num_version==0){
                            load_model(version_name,block_map1);
                        }else{
                            load_model(version_name,block_map2);
                        }
                        // 在这里处理新文件的逻辑
                    }
                }

                hdfsFreeFileInfo(fileInfo, numEntries);

                previousFiles = currentFiles;

                std::this_thread::sleep_for(std::chrono::seconds(10));  // 休眠 10 秒后重新轮询目录
            }

            hdfsDisconnect(fs);


            //....................../

        
        }


        Scheduler(){
            node_num = NODE_NUM;
            num_version = 0;
            work_dir = WORK_DIR;
            block_size = BLOCK_SIZE;
            //添加各个node服务的stub
            for(int i = 1; i <= node_num; i++){
                std::string addr = "node-" + std::to_string(i) + ":5001";
                std::unique_ptr<NodeService::Stub> stub(NodeService::NewStub(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));
                stubs.push_back(std::move(stub));
            }
            std::thread model_monitor_thread(&Scheduler::model_monitor,this,work_dir);
            while(true){
                std::this_thread::sleep_for(std::chrono::seconds(10));
                mtx.lock();
                if(num_version==1){
                    std::cout<<version1<<" load already"<<std::endl;
                    mtx.unlock();
                    break;
                }
                std::cout<<version1<<" not ready yet, waiting ..."<<std::endl;
                mtx.unlock();
            }
            



        }
    private:
        std::mutex mtx;
        std::string work_dir="";
        int node_num;
        int num_version;
        int block_size;
        std::string version1 = "";
        std::string version2 = "";
        std::unordered_map<int, BlockInfo> block_map1,block_map2;
        std::vector<std::unique_ptr<NodeService::Stub>> stubs;
        int switch_step = 0;
};

int main() {
    setenv("LIBHDFS_OPTS", "-Dfile.encoding=UTF-8", 1);
    Scheduler service = Scheduler();
    std::string server_address("node-1:4567");
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
   //注册
    register2etcd(4567);
    server->Wait();
    return 0;
}
