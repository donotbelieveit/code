echo "this is NODE-$NODE_ID"
if [[ "$NODE_ID" == "1" ]]; then
	# 启动节点1对应服务
  ls
  cd model_server
  ./model_server > model_server.log 2>&1 &
  model_server_pid=$!
  cd ../node_server
  ./node_worker $NODE_ID
  sleep 1000
  wait $model_server_pid
else
	# 启动其他节点对应服务
  ls
  cd node_server
  ./node_worker $NODE_ID
  sleep 1000
fi




