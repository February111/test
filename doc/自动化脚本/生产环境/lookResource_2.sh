#!/bin/bash

#当前日期
current_date=$(date +%Y%m%d)
#日志文件路径
logFilePath=./Log
logFile=$logFilePath/resource$current_date.log
touch $logFile

ip_address="172.22.36.17"
port=8200

#判断是不是周末
if [ `date +\%u` -gt 5 ]; then
	    echo  "today is weekend" >> $logFile
	    exit 0
    else
	    echo  "today is weekday" >> $logFile
fi

# 无限循环
while true
do

	# 获取当前的小时和分钟
	current_hour=$(date +%H)
	current_minute=$(date +%M)
	current_second=$(date +%S)

	echo "now time $current_hour:$current_minute:$current_second" >> $logFile

	# 如果当前时间过了15:30，退出循环
	if [ $current_hour -gt 15 ] || ([ $current_hour -eq 15 ] && [ $current_minute -ge 30 ]); then
		echo "当前时间已经超过15:30，退出循环"
		break
	fi

	# 你可以在这里添加其他需要执行的命令
	
	# 获取磁盘使用情况
	disk_info=$(df -h | grep /mnt/tqnas)

	echo "Filesystem                         Size  Used Avail Use% Mounted on" >> $logFile
	echo "$disk_info" >> $logFile
	
	# 获取总内存
	total_mem=$(grep MemTotal /proc/meminfo | awk '{print $2}')

	# 获取可用内存
	avail_mem=$(grep MemAvailable /proc/meminfo | awk '{print $2}')

	# 计算已使用内存
	used_mem=$((total_mem - avail_mem))

	# 计算内存使用率
	mem_usage=$((used_mem * 100 / total_mem))

	# 转换为更易读的格式
	if [ $avail_mem -gt 1048576 ]; then
		mem_readable=$(awk "BEGIN {print $avail_mem / 1048576}")
		mem_readable="${mem_readable}G"
	elif [ $avail_mem -gt 1024 ]; then
		mem_readable=$(awk "BEGIN {print $avail_mem / 1024}")
		mem_readable="${mem_readable}M"
	else
		mem_readable="${avail_mem}K"
	fi

	echo " available_memory ${mem_readable} 内存使用率：$mem_usage%" >> $logFile

	# 使用telnet测试端口
	timeout 5 bash -c "echo > /dev/tcp/$ip_address/$port" >/dev/null 2>&1

	# 检查telnet命令的退出状态
	if [ $? -eq 0 ]; then
	echo " Port $port on $ip_address is open." >> "$logFile"
	else
	echo " Port $port on $ip_address is not open." >> "$logFile"
	fi

	sleep 30

	# 获取总内存
	total_mem=$(grep MemTotal /proc/meminfo | awk '{print $2}')

	# 获取可用内存
	avail_mem=$(grep MemAvailable /proc/meminfo | awk '{print $2}')

	# 计算已使用内存
	used_mem=$((total_mem - avail_mem))

	# 计算内存使用率
	mem_usage=$((used_mem * 100 / total_mem))
	
  	# 转换为更易读的格式
	if [ $avail_mem -gt 1048576 ]; then
		mem_readable=$(awk "BEGIN {print $avail_mem / 1048576}")
		mem_readable="${mem_readable}G"
	elif [ $avail_mem -gt 1024 ]; then
		mem_readable=$(awk "BEGIN {print $avail_mem / 1024}")
		mem_readable="${mem_readable}M"
	else
		mem_readable="${avail_mem}K"
	fi


	echo "after 30 seconds" >> $logFile

	echo " available_memory ${mem_readable} 内存使用率：$mem_usage%" >> $logFile

	# 使用telnet测试端口
	timeout 5 bash -c "echo > /dev/tcp/$ip_address/$port" >/dev/null 2>&1

	# 检查telnet命令的退出状态
	if [ $? -eq 0 ]; then
	echo " Port $port on $ip_address is open." >> "$logFile"
	else
	echo " Port $port on $ip_address is not open." >> "$logFile"
	fi

	sleep 30

done
