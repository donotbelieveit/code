echo "this is NODE-$NODE_ID"
if [[ "$NODE_ID" == "1" ]]; then
	# 启动节点1对应服务
  cd model_server;
  /work/node_worker/node_worker;
  sleep 1000;
else
	# 启动其他节点对应服务
  /work/node_worker/node_worker;
  sleep 1000;
fi




