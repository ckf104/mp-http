DELAY_MS=25
RATE_MBIT=4
BUF_PKTS=10
BDP_BYTES=$(echo "(1.5*$DELAY_MS/1000.0)*($RATE_MBIT*1000000.0/8.0)" | bc -q -l)
BDP_PKTS=$(echo "$BDP_BYTES/1500" | bc -q)
LIMIT_PKTS=$(echo "$BDP_PKTS+$BUF_PKTS" | bc -q)

tc qdisc $1 dev veth1-0 root handle 1:0 tbf rate ${RATE_MBIT}Mbit burst 20k limit ${LIMIT_PKTS}
tc qdisc $1 dev veth1-0 parent 1:0 handle 10:0 netem delay ${DELAY_MS}ms 5ms loss 0.1% limit ${LIMIT_PKTS}