#!/bin/bash

filename=$1
#"bgfs-storage-1Mwrite-fsync.txt"

echo -e "WORKTH\tRECV\tCREAT\tPROCESS\tOP\tCLEANUP"

# Output in following form
# Message Receive | Create message | Process message | Operation [Write|Fsync] (subset of "process" field) | Cleanup

# FSync operation time
# Read "core" operation and work total
read op wk <<< $(cat $filename | grep -A 3 "FSyncLocal" | awk '/FsyncLocal/ {fs+=$2; fs_cnt+=1} /WorkTotal/ {wk+=$2; wk_cnt+=1} END {print fs/fs_cnt, wk/wk_cnt}')

# Read the rest, print everything
cat $filename | grep -A 3 "FSyncLocal" | awk -v op=$op -v wk=$wk 'BEGIN { OFS="\t"} /Detail/ {recv+=$3; creat+=$5; proc+=$7; clean+=$9; cnt+=1} END {print wk, recv/cnt, creat/cnt, proc/cnt, op, clean/cnt}'

# Write operation time
read op wk <<< $(cat $filename | grep -A 3 "WriteLocalFile" | awk '/RecvWrite/ {tot+=$2; cnt+=1} /WorkTotal/ {wk+=$2; wk_cnt+=1} END {print tot/cnt, wk/wk_cnt}')

cat $filename | grep -A 3 "WriteLocalFile" | awk -v op=$op -v wk=$wk 'BEGIN { OFS="\t"} /Detail/ {recv+=$3; creat+=$5; proc+=$7; clean+=$9; cnt+=1} END {print wk, recv/cnt, creat/cnt, proc/cnt, op, clean/cnt}'
