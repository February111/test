#!/bin/bash

# 获取当前日期
current_date=$(date +%Y%m%d);

#日志文件
logPath="../data/Log"
logFile="$logPath/autoRun${current_date}.log"

#分片文件位置
compressFilePath="/mnt/disk4/StrategyPlatformData/shengchan/mkdts"
#单独的日期路径
compressFilePath=$compressFilePath/$current_date

#解压缩后的文件放的目录
filePath="/mnt/disk4/StrategyPlatformData/shengchan/binData"

#判断是不是周末
if [ `date +\%u` -gt 5 ]; then
	echo  "today is weekend" >> $logFile
	exit 0
else
	echo  "today is weekday" >> $logFile
fi

#创造以日期命名的目录
mkdir $filePath/$current_date
filePath=$filePath/$current_date

#切片文件是否存在，记录日志
if [ -f $compressFilePath/mkdt00 ]; then
	echo "分片文件 存在 $compressFilePath" >> $logFile
else
	echo "分片文件 不存在。$compressFilePath" >> $logFile
	exit 1
fi

#合卷
cat $compressFilePath/mkdt* > $filePath/MarketData.tar.gz
#解压缩
tar zxvf $filePath/MarketData.tar.gz -C $filePath

#解压缩是否成功，解出来的文件名如下
fileName1="1MD${current_date}.bin"
fileName2="23MD${current_date}.bin"
fileName3="4MD${current_date}_1.bin"
fileName4="4MD${current_date}_6.bin"

#echo "$filePath/$fileName1"

if [ -f $filePath/$fileName1 ] && [ -f $filePath/$fileName2 ] && [ -f $filePath/$fileName3 ] && [ -f $filePath/$fileName4 ]; then
	echo "成功解压数据文件 在 ${filePath}" >> $logFile
	ls  $filePath >> $logFile
else
	echo "数据文件解压失败" >> $logFile
	ls  $filePath >> $logFile
	exit 1
fi

#配置文件
configFile="../data/config.conf"

#修改播放数据的路径
sed -i "s|VirtualMarketBinFilePath = .*|VirtualMarketBinFilePath = $filePath|" $configFile

# 修改"playFileDate"之后的日期字符串
sed -i "/^playFileDate/ s/\([0-9]\{4\}[0-9]\{2\}[0-9]\{2\}\)/$current_date/g" $configFile

#修改播放的数据
# 匹配到以"play*"开头的一行，并修改"="后面的内容
sed -i "/^playSnapshot/ s/=\(.*\)/= 1/g" $configFile
sed -i "/^playOrdAndExecu/ s/=\(.*\)/= 1/g" $configFile
sed -i "/^playQuickSnap/ s/=\(.*\)/= 1/g" $configFile

#修改执行的功能
sed -i "/^workPattern/ s/=\(.*\)/= 2/g" $configFile
sed -i 's/^application = .*/application = 2,3,4/' "$configFile"

# 执行可执行文件
./StrategyPlatform
#echo "work 203"

#等待程序执行完,等20分钟
sleep 1200

#csv文件目录
csvFilePath=$(grep "StoreMarketDataFolder =" $configFile | cut -d'=' -f2 | tr -d ' ')

#检查是否生成csv文件
fileName1="Snapshot${current_date}.csv"
fileName2="OrderAndExecution${current_date}.csv"
fileName3="QuickSnap${current_date}_1.csv"
#程序生成的文件，是否存在，记录日志
if [ -f $csvFilePath/$fileName1 ] && [ -f $csvFilePath/$fileName2 ] && [ -f $csvFilePath/$fileName3 ]; then
	echo "程序生成csv数据文件 在 ${csvFilePath}" >> $logFile
	#在文件路径里创建一个日期命名的目录，移动文件进去
	mkdir $csvFilePath/$current_date
	mv $csvFilePath/*$current_date*.csv $csvFilePath/$current_date
	#记录日志
	echo "csv data files moved to $csvFilePath/$current_date" >> $logFile
else
	echo "csv数据文件 不存在 ${csvFilePath}" >> $logFile
	
fi

#交易结果文件路径
tradeResultPath=$(grep "TradeResultFilePath =" $configFile | cut -d'=' -f2 | tr -d ' ')

file_found=false
#检测是否有结果文件，记录日志
for file in "$tradeResultPath"/*; do
	# 检查文件名
	if [[ $(basename "$file") == TradeResult_1.csv ]]; then
		file_found=true
		# 跳出循环
		break
	fi
done

# 检查标志是否为false，如果为false，则打印文件不存在的消息
if [[ $file_found == false ]]; then
	echo "File TradeResult_1.csv does not exist in $tradeResultPath" >> $logFile
else
	echo "File TradeResult_1.csv found in $tradeResultPath" >> $logFile
	#新建一个日期文件，移动进去
	resultToday="$tradeResultPath/$current_date"
	mkdir $resultToday
	mv $tradeResultPath/TradeResult* $resultToday
	echo "File TradeResult* moved to $resultToday" >> $logFile
fi


#k线文件路径
klinePath=$(grep "KLineFilePath =" $configFile | cut -d'=' -f2 | tr -d ' ')

file_found=false
#检测是否有k线文件，记录日志
for file in "$klinePath"/*; do
	# 检查文件名是否以KLine.csv结尾
	if [[ $(basename "$file") == *KLine.csv ]]; then
		file_found=true
		# 跳出循环
		break
	fi
done

# 检查标志是否为false，如果为false，则文件不存在
if [[ $file_found == false ]]; then
	echo "File *KLine.csv does not exist in $klinePath" >> $logFile
else
	echo "File *KLine.csv found in $klinePath" >> $logFile
	#新建一个日期文件，移动进去
	klineToday="$klinePath/$current_date"
	mkdir $klineToday
	mv $klinePath/*KLine.csv $klineToday
	echo "File *KLine.csv moved to $klineToday" >> $logFile
fi

