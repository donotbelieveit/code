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
#include <random>
#include <mutex>

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

int NODE_NUM = 6;
std::string WORK_DIR = ".";
int BLOCK_SIZE = 6400000;


class ModelServiceImpl final : public alimama::proto::ModelService::Service {
  Status Get(ServerContext* context, const alimama::proto::Request* request, alimama::proto::Response* response) override {

    // 设置返回结果的状态码
    response->set_status(0);

    // 添加一些 slice_data 到 response
    // response->add_slice_data(data);

    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  ModelServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}
class Scheduler{
    public:
        bool slice2block(std::unique_ptr<alimama::proto::NodeService::Stub>& stub,Slice2BlockRequest& request){
            Slice2BlockResponse response;
            ClientContext context;
            Status status = stub->Slice2Block(&context,request,&response);
            if(status.ok()){
                return true;
            }
            return false;
        }
        std::string get_block_data(){
            std::string data;
            return data;
        }
        void send_load_and_remove(int index,int step,LoadAndRemoveRequest& request){
            LoadAndRemoveResponse response;
            ClientContext context;
            if(step==1)
            Status status = stubs[index]->LoadAndRemove1(&context,request,&response);
            else
            Status status = stubs[index]->LoadAndRemove2(&context,request,&response);
        }
        bool load_and_remove(std::vector<LoadAndRemoveRequest>& request){
            //第一阶段，load第一副本，remove旧版本第二副本
            std::vector<std::thread> threads;
            for(int i=0;i<node_num;i++){
                threads.push_back(std::thread(send_load_and_remove,i,1,request[i]));
            }
            for(auto& thread:threads){
                thread.join();
            }
            threads.clear();
            mtx.lock();
            num_version++;
            switch_step = 1;
            mtx.unlock();
            //第二阶段，send第二副本，remove旧版本第一副本，完成切换
            for(int i=0;i<node_num;i++){
                LoadAndRemoveRequest request;
                threads.push_back(std::thread(send_load_and_remove,i,2,request));
            }
            for(auto& thread:threads){
                thread.join();
            }
            threads.clear();
            mtx.lock();
            num_version--;
            version1 = version2;
            version2 = "";
            block_map1 = block_map2;
            block_map2.clear();
            switch_step = 0;
            mtx.unlock();
        }
        template<typename T>
        void load_model(std::string version_name,std::unordered_map<int, BlockInfo> &block_map,std::vector<T> request){
            while(!fs::exists(fs::path(work_dir+"/"+version_name+"/model.done"))){
                std::this_thread::sleep_for(std::chrono::seconds(10)); 
            }
            std::string model_meta = work_dir+"/"+version_name+"/model.meta";
            std::ifstream file(model_meta);
            auto model_dir = fs::path(work_dir+"/"+version_name);
            int slice_size = -1;
            if (file.is_open()) {
                std::string line;
                std::string tmp;
                std::getline(file, tmp);
                std::getline(file, line);
                size_t slicePos = line.find("slice:");
                size_t sizePos = line.find("size:");

                if (slicePos != std::string::npos && sizePos != std::string::npos) {
                    std::string slice = line.substr(slicePos + 6, sizePos - (slicePos + 6));
                    std::string size = line.substr(sizePos + 5);
                    slice_size = std::stoi(slice);
                }
            } else {
                std::cerr << "Failed to open file: " << version_name << std::endl;
            }
            file.close();
            std::map<int,BlockInfo> block_map_tmp;
            //random(1,node,num)
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(1, node_num);

            //将所有slice分成block
            for(const auto& entry : std::filesystem::directory_iterator(model_dir)) {
                if(entry.path().filename().string().find("model_slice") != std::string::npos){
                    continue;
                }
                std::string slice_name = entry.path().filename().string();
                std::size_t dotPosition = slice_name.find_last_of('.');
                int slice_partition = -1;
                if (dotPosition != std::string::npos && dotPosition < slice_name.length() - 1) {
                    slice_partition = std::stoi(slice_name.substr(dotPosition + 1));
                }
                
                SliceInfo slice_info;
                slice_info.set_slice_partition(slice_partition);
                slice_info.set_version(version_name);
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
                    block_map_tmp[i] = block_info;
                    slice_info.add_block_info()->CopyFrom(block_info);
                }
                request[slice_partition%node_num].add_slice_info()->CopyFrom(slice_info);
            }
            if(num_version==0){
                std::vector<std::thread> threads;
                for(int i=0;i<node_num;i++){
                    threads.push_back(load_model,stubs[i],request[i]);
                }
                for(auto&thread:threads){
                    thread.join();
                }
                threads.clear();
                mtx.lock();
                num_version++;
                version1 = version_name;
                mtx.unlock();
            }else{
                load_and_remove(request);
            }

        }
        void model_monitor(std::string work_dir){
            std::filesystem::path dirPath(work_dir);
            std::filesystem::directory_entry lastEntry;
            while (true) {
                std::filesystem::directory_entry newEntry;
                // 遍历文件夹
                for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                    newEntry = entry;
                    
                    // 检查是否是新文件
                    if (newEntry != lastEntry) {
                        lastEntry = newEntry;

                        // 检查文件类型
                        if (newEntry.is_regular_file()) {
                            if(newEntry.path().filename().string()=="rollback.version"){

                            }else{
                                std::cerr<<newEntry.path().filename().string()<<",wrong name"<<std::endl;
                            }
                            std::ifstream file(entry.path());
                            std::string fileContent((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
                            mtx.lock();
                            if(num_version==1){
                                version2 = fileContent;
                                num_version = 2;
                            }
                            mtx.unlock();
                        } else if (newEntry.is_directory()) {
                            std::cout << "New version detected: " << newEntry.path().filename() << std::endl;
                            // TODO: 处理新文件夹
                            mtx.lock();
                            if(num_version==1){
                                version2 = entry.path().filename().string();
                                num_version = 2;
                            }
                            mtx.unlock();
                        }
                        if(num_version==0){
                            std::vector<Slice2BlockRequest> request(node_num);
                            load_model(version1,block_map1,request);
                        }else{
                            std::vector<LoadAndRemoveRequest> request(node_num);
                            load_model(version2,block_map2,request);
                        }
                    }
                }

                // 等待一段时间再进行下一次检查
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        
        }


        Scheduler(){
            node_num = NODE_NUM;
            num_version = 0;
            work_dir = WORK_DIR;
            block_size = BLOCK_SIZE;
            for(int i = 1; i <= node_num; i++){
                std::string addr = "node-" + std::to_string(i) + ":5001";
                std::unique_ptr<NodeService::Stub> stub(NodeService::NewStub(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));
                stubs.push_back(std::move(stub));
            }
            std::thread model_monitor_thread(model_monitor,work_dir);
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
  RunServer();
  return 0;
}
