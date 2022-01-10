tc qdisc add dev veth1-0 root handle 1:0 tbf rate 4mbit burst 20kb limit 300000  
tc qdisc add dev veth1-0 parent 1:0 handle 10:0 netem delay 100ms 20ms 30% loss 0.1 limit 300000 