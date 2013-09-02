#!/bin/bash
#this is a bash script to read logdata about TCP connection
#2006-4-18,Qiao Junlong,BUAA-CSE
echo "read logdata about queue connection......"
touch queuedata_myred.txt
rm queuedata_myred.txt
touch queuedata_myred.txt
i=0
while [ "$i" -le "30000" ]
do
insmod /root/AQM/myred/queuedata/seqfile_queuedata_myred.ko queue_array_count=$i
cat /proc/data_seq_file >> queuedata_myred.txt
rmmod seqfile_queuedata_myred
i=$[$i + 40]
done
echo "read logdata succed! pelease see queuedata.txt"
