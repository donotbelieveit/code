#include<iostream>
#include<memory>
#include<string>

#include<grpcpp/grpcpp.h>
#include<grpc/support/log.h>
#include<thread>

#include"alimama.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

using alimama::proto::NodeService;
using alimama::proto::SendCopyRequest;
using alimama::proto::SendCopyResponse;

using alimama::proto::BlockData;

using namespace std;


class SendCopyClient {
    public:
        explicit SendCopyClient(shared_ptr<Channel> channel): stub_(NodeService::NewStub(channel)){}

        void SendCopy(const BlockData& blockdata) {
            SendCopyRequest request;
            // request赋值
            request.mutable_block_data()->CopyFrom(blockdata);

            AsyncClientCall* call = new AsyncClientCall;

            call->response_reader = stub_->PrepareAsyncSendCopy(&call->context, request, &cq_);

            call->response_reader->StartCall();

            call->response_reader->Finish(&call->reply, &call->status, (void*)call);
        }

        void AsyncCompleteRpc() {
            void* got_tag;
            bool ok = false;

            while (cq_.Next(&got_tag, &ok))
            {
                AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);

                GPR_ASSERT(ok);

                if(call->status.ok())
                    cout << "Copy has been send!" << endl;
                else
                    cout << "SendCopy failed!" << endl;

                delete call;
            }
        }

    private:
        struct AsyncClientCall {
            SendCopyResponse reply;

            ClientContext context;

            Status status;

            unique_ptr<ClientAsyncResponseReader<SendCopyResponse>> response_reader;
        };

        unique_ptr<NodeService::Stub> stub_;

        CompletionQueue cq_;
};