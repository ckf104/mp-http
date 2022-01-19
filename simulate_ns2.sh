tc qdisc add dev veth2-0 root handle 1:0 tbf rate 0.8mbit burst 20kb limit 300000  
tc qdisc add dev veth2-0 parent 1:0 handle 10:0 netem delay 80ms 10ms 40% loss 0.1 limit 300000 
