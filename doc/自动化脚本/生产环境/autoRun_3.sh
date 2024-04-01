#!/bin/bash

#压缩切片后的小文件，放在固定目录，用于传输
mkdtPath="/mnt/dbfile/huangjinpeng/binData/mkdts"

# 获取当前日期
current_date=$(date +%Y%m%d);

#开始路径
startPath=$(pwd)

#日志路径
logPath="$startPath/../data/Log"
#日志文件名
logFile="$logPath/autoRun${current_date}.log"
touch $logFile

#配置文件
config_file="$startPath/../data/config.conf"

#获得存放数据文件的路径
filepath=$(grep "StoreMarketDataBinFolder =" $config_file | cut -d'=' -f2 | tr -d ' ')

#判断是不是周末
if [ `date +\%u` -gt 5 ]; then
	    echo  "today is weekend" >> $logFile
	    exit 0
    else
	    echo  "today is weekday" >> $logFile
fi

# Use the Expect command
expect -c "
# 设置超时时间
set timeout 23400

# 启动你的程序
spawn ./StrategyPlatform

# 模拟用户输入
expect \"you must wait for timeout\"
send \"0\r\"

# 等待程序结束
expect eof
"

#等待程序压缩存文件，程序结束再做下一步
sleep 360
echo " exec over"

fileName1="1MD${current_date}.bin"
fileName2="23MD${current_date}.bin"
fileName3="4MD${current_date}_1.bin"
#程序生成的文件，是否存在，记录日志
if [ -f $filepath/$fileName1 ] && [ -f $filepath/$fileName2 ] && [ -f $filepath/$fileName3 ]; then
	echo "程序生成数据文件 在 ${filepath}" >> $logFile
else
	echo "数据文件 不存在 ${filepath}" >> $logFile
	exit 1
fi

#在文件路径里创建一个日期命名的目录，移动文件进去
mkdir $filepath/$current_date
mv $filepath/*$current_date*.bin $filepath/$current_date

#记录日志
echo "market data files moved to $filepath/$current_date" >> $logFile

#统计数据路径
statisPath=$(grep "StatisticalDataPath =" $config_file | cut -d'=' -f2 | tr -d ' ')
#创建今天的目录
mkdir $statisPath/$current_date

#检测路径下有没有sse和szse目录
# 检查 sse 目录是否存在
if [ -d "$statisPath/sse" ]; then
	    echo "Directory sse exists, move it" >> $logFile
	    #移动
	    mv $statisPath/sse $statisPath/$current_date
    else
	        echo "Directory sse does not exist." >> $logFile
fi

# 检查 szse 目录是否存在
if [ -d "$statisPath/szse" ]; then
	    echo "Directory szse exists, move it" >> $logFile
	    #移动
	    mv $statisPath/szse $statisPath/$current_date
    else
	        echo "Directory szse does not exist." >> $logFile
fi


#切换工作目录，到数据文件所在目录
cd $filepath/$current_date

#在里面压缩文件，得到压缩包
tar -zcvf MarketData.tar.gz *$current_date*.bin

#压缩成功没有，记录日志
if [ -f ./MarketData.tar.gz ]; then
	echo "成功生成压缩包 $filepath/$current_date/Market.tar.gz" >> $logFile
else
	echo "没有生成压缩包 $filepath/$current_date/Market.tar.gz" >> $logFile
	exit 1
fi

#mkdt目录下新建目录
mkdtPath=$mkdtPath/$current_date
mkdir $mkdtPath

#把压缩包切片放在mkdts目录
split -b 200m -d MarketData.tar.gz $mkdtPath/mkdt

#切片成功没有,记录日志
if [ -f $mkdtPath/mkdt00 ]; then
	echo "分片成功 $mkdtPath" >> $logFile
	ls $mkdtPath >> $logFile
else
	echo "分片失败 $mkdtPath"  >> $logFile
	exit 1
fi




