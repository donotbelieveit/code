// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: alimama.proto

#include "alimama.pb.h"
#include "alimama.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/message_allocator.h>
#include <grpcpp/impl/codegen/method_handler.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/server_callback_handlers.h>
#include <grpcpp/impl/codegen/server_context.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace alimama {
namespace proto {

static const char* ModelService_method_names[] = {
  "/alimama.proto.ModelService/Get",
};

std::unique_ptr< ModelService::Stub> ModelService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< ModelService::Stub> stub(new ModelService::Stub(channel));
  return stub;
}

ModelService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_Get_(ModelService_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status ModelService::Stub::Get(::grpc::ClientContext* context, const ::alimama::proto::Request& request, ::alimama::proto::Response* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Get_, context, request, response);
}

void ModelService::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::alimama::proto::Request* request, ::alimama::proto::Response* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, std::move(f));
}

void ModelService::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::alimama::proto::Response* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, std::move(f));
}

void ModelService::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::alimama::proto::Request* request, ::alimama::proto::Response* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, reactor);
}

void ModelService::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::alimama::proto::Response* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::alimama::proto::Response>* ModelService::Stub::AsyncGetRaw(::grpc::ClientContext* context, const ::alimama::proto::Request& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::alimama::proto::Response>::Create(channel_.get(), cq, rpcmethod_Get_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::alimama::proto::Response>* ModelService::Stub::PrepareAsyncGetRaw(::grpc::ClientContext* context, const ::alimama::proto::Request& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::alimama::proto::Response>::Create(channel_.get(), cq, rpcmethod_Get_, context, request, false);
}

ModelService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ModelService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ModelService::Service, ::alimama::proto::Request, ::alimama::proto::Response>(
          [](ModelService::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::alimama::proto::Request* req,
             ::alimama::proto::Response* resp) {
               return service->Get(ctx, req, resp);
             }, this)));
}

ModelService::Service::~Service() {
}

::grpc::Status ModelService::Service::Get(::grpc::ServerContext* context, const ::alimama::proto::Request* request, ::alimama::proto::Response* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


static const char* NodeService_method_names[] = {
  "/alimama.proto.NodeService/GetBlockData",
  "/alimama.proto.NodeService/SendCopy",
  "/alimama.proto.NodeService/LoadAndRemove",
  "/alimama.proto.NodeService/Slice2Block",
};

std::unique_ptr< NodeService::Stub> NodeService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< NodeService::Stub> stub(new NodeService::Stub(channel));
  return stub;
}

NodeService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_GetBlockData_(NodeService_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_SendCopy_(NodeService_method_names[1], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_LoadAndRemove_(NodeService_method_names[2], ::grpc::internal::RpcMethod::SERVER_STREAMING, channel)
  , rpcmethod_Slice2Block_(NodeService_method_names[3], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status NodeService::Stub::GetBlockData(::grpc::ClientContext* context, const ::alimama::proto::GetBlockDataRequest& request, ::alimama::proto::GetBlockDataResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_GetBlockData_, context, request, response);
}

void NodeService::Stub::experimental_async::GetBlockData(::grpc::ClientContext* context, const ::alimama::proto::GetBlockDataRequest* request, ::alimama::proto::GetBlockDataResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_GetBlockData_, context, request, response, std::move(f));
}

void NodeService::Stub::experimental_async::GetBlockData(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::alimama::proto::GetBlockDataResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_GetBlockData_, context, request, response, std::move(f));
}

void NodeService::Stub::experimental_async::GetBlockData(::grpc::ClientContext* context, const ::alimama::proto::GetBlockDataRequest* request, ::alimama::proto::GetBlockDataResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_GetBlockData_, context, request, response, reactor);
}

void NodeService::Stub::experimental_async::GetBlockData(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::alimama::proto::GetBlockDataResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_GetBlockData_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::alimama::proto::GetBlockDataResponse>* NodeService::Stub::AsyncGetBlockDataRaw(::grpc::ClientContext* context, const ::alimama::proto::GetBlockDataRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::alimama::proto::GetBlockDataResponse>::Create(channel_.get(), cq, rpcmethod_GetBlockData_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::alimama::proto::GetBlockDataResponse>* NodeService::Stub::PrepareAsyncGetBlockDataRaw(::grpc::ClientContext* context, const ::alimama::proto::GetBlockDataRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::alimama::proto::GetBlockDataResponse>::Create(channel_.get(), cq, rpcmethod_GetBlockData_, context, request, false);
}

::grpc::Status NodeService::Stub::SendCopy(::grpc::ClientContext* context, const ::alimama::proto::SendCopyRequest& request, ::alimama::proto::SendCopyResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_SendCopy_, context, request, response);
}

void NodeService::Stub::experimental_async::SendCopy(::grpc::ClientContext* context, const ::alimama::proto::SendCopyRequest* request, ::alimama::proto::SendCopyResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_SendCopy_, context, request, response, std::move(f));
}

void NodeService::Stub::experimental_async::SendCopy(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::alimama::proto::SendCopyResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_SendCopy_, context, request, response, std::move(f));
}

void NodeService::Stub::experimental_async::SendCopy(::grpc::ClientContext* context, const ::alimama::proto::SendCopyRequest* request, ::alimama::proto::SendCopyResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_SendCopy_, context, request, response, reactor);
}

void NodeService::Stub::experimental_async::SendCopy(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::alimama::proto::SendCopyResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_SendCopy_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::alimama::proto::SendCopyResponse>* NodeService::Stub::AsyncSendCopyRaw(::grpc::ClientContext* context, const ::alimama::proto::SendCopyRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::alimama::proto::SendCopyResponse>::Create(channel_.get(), cq, rpcmethod_SendCopy_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::alimama::proto::SendCopyResponse>* NodeService::Stub::PrepareAsyncSendCopyRaw(::grpc::ClientContext* context, const ::alimama::proto::SendCopyRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::alimama::proto::SendCopyResponse>::Create(channel_.get(), cq, rpcmethod_SendCopy_, context, request, false);
}

::grpc::ClientReader< ::alimama::proto::LoadAndRemoveResponse>* NodeService::Stub::LoadAndRemoveRaw(::grpc::ClientContext* context, const ::alimama::proto::LoadAndRemoveRequest& request) {
  return ::grpc_impl::internal::ClientReaderFactory< ::alimama::proto::LoadAndRemoveResponse>::Create(channel_.get(), rpcmethod_LoadAndRemove_, context, request);
}

void NodeService::Stub::experimental_async::LoadAndRemove(::grpc::ClientContext* context, ::alimama::proto::LoadAndRemoveRequest* request, ::grpc::experimental::ClientReadReactor< ::alimama::proto::LoadAndRemoveResponse>* reactor) {
  ::grpc_impl::internal::ClientCallbackReaderFactory< ::alimama::proto::LoadAndRemoveResponse>::Create(stub_->channel_.get(), stub_->rpcmethod_LoadAndRemove_, context, request, reactor);
}

::grpc::ClientAsyncReader< ::alimama::proto::LoadAndRemoveResponse>* NodeService::Stub::AsyncLoadAndRemoveRaw(::grpc::ClientContext* context, const ::alimama::proto::LoadAndRemoveRequest& request, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc_impl::internal::ClientAsyncReaderFactory< ::alimama::proto::LoadAndRemoveResponse>::Create(channel_.get(), cq, rpcmethod_LoadAndRemove_, context, request, true, tag);
}

::grpc::ClientAsyncReader< ::alimama::proto::LoadAndRemoveResponse>* NodeService::Stub::PrepareAsyncLoadAndRemoveRaw(::grpc::ClientContext* context, const ::alimama::proto::LoadAndRemoveRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncReaderFactory< ::alimama::proto::LoadAndRemoveResponse>::Create(channel_.get(), cq, rpcmethod_LoadAndRemove_, context, request, false, nullptr);
}

::grpc::Status NodeService::Stub::Slice2Block(::grpc::ClientContext* context, const ::alimama::proto::Slice2BlockRequest& request, ::alimama::proto::Slice2BlockResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Slice2Block_, context, request, response);
}

void NodeService::Stub::experimental_async::Slice2Block(::grpc::ClientContext* context, const ::alimama::proto::Slice2BlockRequest* request, ::alimama::proto::Slice2BlockResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Slice2Block_, context, request, response, std::move(f));
}

void NodeService::Stub::experimental_async::Slice2Block(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::alimama::proto::Slice2BlockResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Slice2Block_, context, request, response, std::move(f));
}

void NodeService::Stub::experimental_async::Slice2Block(::grpc::ClientContext* context, const ::alimama::proto::Slice2BlockRequest* request, ::alimama::proto::Slice2BlockResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Slice2Block_, context, request, response, reactor);
}

void NodeService::Stub::experimental_async::Slice2Block(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::alimama::proto::Slice2BlockResponse* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Slice2Block_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::alimama::proto::Slice2BlockResponse>* NodeService::Stub::AsyncSlice2BlockRaw(::grpc::ClientContext* context, const ::alimama::proto::Slice2BlockRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::alimama::proto::Slice2BlockResponse>::Create(channel_.get(), cq, rpcmethod_Slice2Block_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::alimama::proto::Slice2BlockResponse>* NodeService::Stub::PrepareAsyncSlice2BlockRaw(::grpc::ClientContext* context, const ::alimama::proto::Slice2BlockRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::alimama::proto::Slice2BlockResponse>::Create(channel_.get(), cq, rpcmethod_Slice2Block_, context, request, false);
}

NodeService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      NodeService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< NodeService::Service, ::alimama::proto::GetBlockDataRequest, ::alimama::proto::GetBlockDataResponse>(
          [](NodeService::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::alimama::proto::GetBlockDataRequest* req,
             ::alimama::proto::GetBlockDataResponse* resp) {
               return service->GetBlockData(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      NodeService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< NodeService::Service, ::alimama::proto::SendCopyRequest, ::alimama::proto::SendCopyResponse>(
          [](NodeService::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::alimama::proto::SendCopyRequest* req,
             ::alimama::proto::SendCopyResponse* resp) {
               return service->SendCopy(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      NodeService_method_names[2],
      ::grpc::internal::RpcMethod::SERVER_STREAMING,
      new ::grpc::internal::ServerStreamingHandler< NodeService::Service, ::alimama::proto::LoadAndRemoveRequest, ::alimama::proto::LoadAndRemoveResponse>(
          [](NodeService::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::alimama::proto::LoadAndRemoveRequest* req,
             ::grpc_impl::ServerWriter<::alimama::proto::LoadAndRemoveResponse>* writer) {
               return service->LoadAndRemove(ctx, req, writer);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      NodeService_method_names[3],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< NodeService::Service, ::alimama::proto::Slice2BlockRequest, ::alimama::proto::Slice2BlockResponse>(
          [](NodeService::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::alimama::proto::Slice2BlockRequest* req,
             ::alimama::proto::Slice2BlockResponse* resp) {
               return service->Slice2Block(ctx, req, resp);
             }, this)));
}

NodeService::Service::~Service() {
}

::grpc::Status NodeService::Service::GetBlockData(::grpc::ServerContext* context, const ::alimama::proto::GetBlockDataRequest* request, ::alimama::proto::GetBlockDataResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status NodeService::Service::SendCopy(::grpc::ServerContext* context, const ::alimama::proto::SendCopyRequest* request, ::alimama::proto::SendCopyResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status NodeService::Service::LoadAndRemove(::grpc::ServerContext* context, const ::alimama::proto::LoadAndRemoveRequest* request, ::grpc::ServerWriter< ::alimama::proto::LoadAndRemoveResponse>* writer) {
  (void) context;
  (void) request;
  (void) writer;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status NodeService::Service::Slice2Block(::grpc::ServerContext* context, const ::alimama::proto::Slice2BlockRequest* request, ::alimama::proto::Slice2BlockResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace alimama
}  // namespace proto

