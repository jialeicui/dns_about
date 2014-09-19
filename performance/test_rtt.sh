get_rtt()
{
    qps=$1
    res=`./queryperf -s 2.1.1.3 -l60 -T$qps -d query.txt | grep "RTT " | awk -F" " '{print $3}'`

    echo $1,`echo $res | awk -F" " '{print $1 "," $2 "," $3}'`
}

for (( i = 1; i < 16; i++ )); do
    get_rtt `expr $i \* 10000`
done
