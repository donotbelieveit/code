PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`
PROTOS_PATH = ./

CXX = g++
CPPFLAGS += -Ofast `pkg-config --cflags protobuf grpc`  -I${HADOOP_HOME}/include
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++`\
           -pthread\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
					 -lboost_log\
           -ldl
LDFLAGS += -lpthread -letcd-cpp-api -lprotobuf -lgrpc++ -lgrpc -lz -lcpprest -lssl -lcrypto -lboost_system  -L${HADOOP_HOME}/lib/native/ -lhdfs -lmodel_slice_reader


.PRECIOUS: %.grpc.pb.cc
%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

node_worker: alimama.pb.o alimama.grpc.pb.o node_worker.o
	${CXX} -std=c++17 ${CPPFLAGS} $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o *.pb.cc *.pb.h node_worker