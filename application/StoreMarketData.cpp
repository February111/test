#include <unistd.h>
#include <time.h>
#include <iostream>
#include <string>
#include <mutex>
#include "StoreMarketData.h"
#include "ConfigFile.h"
#include "SPLog.h"
#include "sp_tool.h"
#include "reflect.h"

using std::cerr;
using std::endl;
using std::string;

static std::mutex g_mutexCbCnt;  //回调次数增加时的线程同步
static std::mutex g_mutexOrderBookFileMap;		//修改线程和orderBook文件映射的互斥锁

extern SPLog g_log;				//记录日志的对象
extern ConfigFile g_cfgFile;	//其他文件的全局变量

//初始化
int StoreMarketData::BeforeMarket()
{
	//加载配置文件
	/*ConfigFile cfg;
	if (cfg.Load("../data/config.conf") == false)
	{
		cerr << "load config file failed\n" << endl;
		exit(1);
	}*/

	//委托簿的线程个数
	string threadNum = g_cfgFile.Get("OrderBookThreadNum");
	if (threadNum == string())
	{
		cerr << "OrderBookThreadNum not found in config file" << endl;
		return -1;
	}
	_threadNumOrderBook = stoi(threadNum);

	//要存储文件的目录
	_folder = g_cfgFile.Get("StoreMarketDataFolder");
	if (_folder == string())
	{
		cerr << "StoreMarketDataFolder not found in cionfig file" << endl;
		return -1;
	}

	//分隔符设置为逗号
	_sep = ',';

	//初始化成员_callBackCount
	_callbackCount = 0;



	//初始化成员变量_data
	std::string realTime("1");
	std::string workPattern = g_cfgFile.Get("workPattern");
	if (workPattern == std::string())
	{
		//没有找到
		g_log.WriteLog("not found workPattern in config file");
		return -1;
	}
	if (workPattern == realTime)
	{
		//如果是实时数据
		time_t tim1 = time(NULL);
		struct tm* tim2 = localtime(&tim1);
		char data[16] = { 0 };
		sprintf(data, "%d%02d%02d", tim2->tm_year + 1900, tim2->tm_mon + 1, tim2->tm_mday);
		_data = data;
	}
	else
	{
		
		//是模拟数据
		std::string playDate = g_cfgFile.Get("playFileDate");
		printf("virtual market, playFileDate = %s\n", playDate.c_str());
		if (playDate == std::string())
		{
			//没有找到
			g_log.WriteLog("not found playFileDate in config file");
			return -1;
		}
		_data = playDate;
	}
	
	//打开snapshot.csv文件
	OpenSnapshotCsvFile();
	//打开OrderBook文件
	OpenOrderBookCsvFile();
	//打开ordAndExe.csv
	OpenOrdAndExeCsvFile();

	return 0;
}

//析构函数
StoreMarketData::~StoreMarketData()
{
	//如果文件打开了，析构时关闭
	if (_snapshotCsvFile != NULL)
	{
		fclose(_snapshotCsvFile);
		_snapshotCsvFile = NULL;
	}

	/*if (_tickExecutionCsvFile != NULL)
	{
		fclose(_tickExecutionCsvFile);
		_tickExecutionCsvFile == NULL;
	}

	if (_tickOrderCsvFile != NULL)
	{
		fclose(_tickOrderCsvFile);
		_tickOrderCsvFile == NULL;
	}*/

	if (_ordAndExeCsvFile != NULL)
	{
		fclose(_ordAndExeCsvFile);
		_ordAndExeCsvFile == NULL;
	}

	auto iter = _threadWriteFileOrderBook.begin();
	for (; iter != _threadWriteFileOrderBook.end(); ++iter)
	{
		fclose(iter->second);
	}
}

void StoreMarketData::OnSnapshot(SnapshotInfo* snapInfo, uint32_t cnt)
{
	//printf("StoreMarketData::OnSnapshot() is called\n");

	int callbackCnt;
	//回调次数增加，并记下
	g_mutexCbCnt.lock();
	callbackCnt = ++_callbackCount;
	g_mutexCbCnt.unlock();

	//本地系统时间戳
	int32_t locTim = GetLocalTm();

	for (uint32_t i = 0; i < cnt; ++i)
	{
		snapInfo[i]._numCb = callbackCnt;
		snapInfo[i]._seq = i + 1;
		snapInfo[i]._timLoc = locTim;

		//写入一行数据
		WriteSnapshotToFile(&snapInfo[i], _snapshotCsvFile);//写入snapshotInfo信息
		//WriteAdditionalInfoToFile(callbackCnt, i + 1, locTim, _snapshotCsvFile);//写入补充信息
		WriteEndingToFile(_snapshotCsvFile);//结尾换行
	}

	return;
}


void StoreMarketData::OnOrderAndExecu(OrdAndExeInfo* infos, uint32_t cnt)
{
	int callbackCnt;
	//回调次数增加，并记下
	g_mutexCbCnt.lock();
	callbackCnt = ++_callbackCount;
	g_mutexCbCnt.unlock();

	//本地系统时间戳
	int32_t locTim = GetLocalTm();

	for (uint32_t i = 0; i < cnt; ++i)
	{
		infos[i]._numCb = callbackCnt;
		infos[i]._seq = i + 1;
		infos[i]._timLoc = locTim;
		//写入一行数据，结尾换行
		//WriteTickExecutionToFile(&info[i], _tickExecutionCsvFile);
		WriteOrdAndExeInfoToFile(infos + i, _ordAndExeCsvFile);
		//WriteAdditionalInfoToFile(callbackCnt, i + 1, locTim, _tickExecutionCsvFile);
		WriteEndingToFile(_ordAndExeCsvFile);
	}

	return;
}

void StoreMarketData::OnQuickSnap(QuickSnapInfo* infos, uint32_t cnt)
{
	//printf("StoreMarketData::OnQuickSnap\n");

	uint32_t callbackCnt;
	//回调次数增加，并记下
	g_mutexCbCnt.lock();
	callbackCnt = ++_callbackCount;
	g_mutexCbCnt.unlock();

	//本地系统时间戳
	int32_t locTim = GetLocalTm();

	//本线程id
	std::thread::id thid = std::this_thread::get_id();

	//本线程有没有匹配的文件，如果没有就从队列里出队一个给它，组成映射放入unordered_map
	if (_threadWriteFileOrderBook.count(thid) == 0)
	{
		std::cout << "StoreMarketData::OnQuickSnap: a new thread, thread id = " << thid << std::endl;

		std::lock_guard<std::mutex> _(g_mutexOrderBookFileMap);
		//取出一个文件指针
		if (_orderBookCsvFiles.empty())
		{
			std::cerr << "opened orderbook file pointer is not enough" << std::endl;
			g_log.WriteLog("opened orderbook file pointer is not enough");
			//exit(1);
			return;//既然没有文件了，就不写了，别崩
		}
		FILE* fp = _orderBookCsvFiles.front();
		_orderBookCsvFiles.pop();
		//与当前线程形成映射
		_threadWriteFileOrderBook[thid] = fp;
	}

	//要写的文件
	FILE* writeFile = _threadWriteFileOrderBook[thid];

	for (size_t i = 0; i < cnt; i++)
	{
		infos[i]._cntCb = callbackCnt;
		infos[i]._seq = i + 1;
		infos[i]._tmLoc = locTim;
		//QuickSnapInfo如实写就行，缺少的档位在QuickSnapInfo里已经写成0了
		WriteOrderBookToFile(&infos[i], writeFile);
		
		WriteEndingToFile(writeFile);
	}

	//sleep(1);

	return;
}

//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
void StoreMarketData::OpenSnapshotCsvFile()
{
	string fileNameStr = _folder + "/Snapshot" + _data + ".csv";

	const char* filename = fileNameStr.c_str();
	FILE* fp = NULL;
	//如果文件不存在，就新建并写好表头
	fp = fopen(filename, "r");
	if (fp == NULL)
	{
		printf("OpenSnapshotCsvFile: %s does not exist, create a new one\n", filename);
		fp = fopen(filename, "w");
		if (fp == NULL)
		{
			perror("fopen snapshot.csv");
			exit(1);
		}
		//写入表头的一行
		char sep = _sep;
		fprintf(fp, "%s%c", "code", sep);                    // 证券代码
		fprintf(fp, "%s%c", "time", sep);                    // 时间(HHMMSSsss)
		fprintf(fp, "%s%c", "IOPV", sep);
		fprintf(fp, "%s%c", "high_price", sep);
		fprintf(fp, "%s%c", "weighted_avg_offer_price", sep);

		//申卖价格，10档
		for (int i = 10; i >= 1; i--)
		{
			fprintf(fp, "%s%02d%c", "offer_price", i, sep);
		}

		fprintf(fp, "%s%c", "last_price", sep);

		//申买价格，10档
		for (int i = 1; i <= 10; ++i)
		{
			fprintf(fp, "%s%02d%c", "bid_price", i, sep);
		}

		fprintf(fp, "%s%c", "weighted_avg_bid_price", sep);
		fprintf(fp, "%s%c", "low_price", sep);
		fprintf(fp, "%s%c", "total_offer_volume", sep);

		//申卖量，10档
		for (int i = 10; i >= 1; i--)
		{
			fprintf(fp, "%s%02d%c", "offer_volume", i, sep);
		}

		fprintf(fp, "%s%c", "total_volume_trade", sep);

		//申买量，10档
		for (int i = 1; i <= 10; ++i)
		{
			fprintf(fp, "%s%02d%c", "bid_volume", i, sep);
		}

		fprintf(fp, "%s%c", "total_bid_volume", sep);
		fprintf(fp, "%s%c", "num_trades", sep);
		fprintf(fp, "%s%c", "total_value_trade", sep);
		fprintf(fp, "%s%c", "trading_phase_code", sep);
		fprintf(fp, "%s%c", "date", sep);

		//回调次数
		fprintf(fp, "%s%c", "callBack_cnt", sep);
		//第几个数据
		fprintf(fp, "%s%c", "sequence", sep);
		//本地系统时间戳
		fprintf(fp, "%s", "local_time");//最后一列去掉逗号

		fputc('\n', fp);

		fclose(fp);
	}
	else
	{
		printf("OpenSnapshotCsvFile: %s exist, open it\n", filename);
		fclose(fp);
	}

	//追加方式打开文件
	_snapshotCsvFile = fopen(filename, "a");

	return;
}

//打开TickOrder.csv逐笔委托数据文件
//void StoreMarketData::OpenTickOrderCsvFile()
//{
//	std::string fileNameStr = _folder + "/TickOrder" + _data + ".csv";
//
//	const char* filename = fileNameStr.c_str();
//	FILE* fp = NULL;
//	//如果文件不存在，就新建并写好表头
//	fp = fopen(filename, "r");
//	if (fp == NULL)
//	{
//		printf("OpenTickOrderCsvFile: %s does not exist, create a new one\n", filename);
//		
//		fp = fopen(filename, "w");
//		if (fp == NULL)
//		{
//			perror("fopen TickOrder.csv");
//			exit(1);
//		}
//
//		char sep = _sep;
//		fprintf(fp, "%s%c", "market_type", sep);
//		fprintf(fp, "%s%c", "security_code", sep);
//		fprintf(fp, "%s%c", "channel_no", sep);
//		fprintf(fp, "%s%c", "appl_seq_num", sep);
//		fprintf(fp, "%s%c", "order_time", sep);
//		fprintf(fp, "%s%c", "order_price", sep);
//		fprintf(fp, "%s%c", "order_volume", sep);
//		fprintf(fp, "%s%c", "side", sep);
//		fprintf(fp, "%s%c", "order_type", sep);
//		fprintf(fp, "%s%c", "md_stream_id", sep);
//		fprintf(fp, "%s%c", "orig_order_no", sep);
//		fprintf(fp, "%s%c", "biz_index", sep);
//		fprintf(fp, "%s%c", "variety_category", sep);
//		fprintf(fp, "%s%c", "traded_order_volume", sep);
//
//		//回调次数
//		fprintf(fp, "%s%c", "callBack_cnt", sep);
//		//第几个数据
//		fprintf(fp, "%s%c", "sequence", sep);
//		//本地系统时间戳
//		fprintf(fp, "%s%c", "local_time", sep);
//
//		fputc('\n', fp);
//
//		fclose(fp);
//	}
//	else
//	{
//		fclose(fp);
//	}
//
//	//追加方式打开文件
//	_tickOrderCsvFile = fopen(filename, "a");
//
//	return;
//}

//打开TickExecution.csv成交数据文件
//void StoreMarketData::OpenTickExecutionCsvFile()
//{
//	std::string fileNameStr = _folder + "/TickExecution" + _data + ".csv";
//
//	const char* filename = fileNameStr.c_str();
//	FILE* fp = NULL;
//	//如果文件不存在，就新建并写好表头
//	fp = fopen(filename, "r");
//	if (fp == NULL)
//	{
//		printf("OpenTickExecutionCsvFile: %s does not exist, create a new one\n", filename);
//		fp = fopen(filename, "w");
//		if (fp == NULL)
//		{
//			perror("fopen TickExecution.csv");
//			exit(1);
//		}
//
//		char sep = _sep;
//		fprintf(fp, "%s%c", "market_type", sep);
//		fprintf(fp, "%s%c", "security_code", sep);
//		fprintf(fp, "%s%c", "exec_time", sep);
//		fprintf(fp, "%s%c", "channel_no", sep);
//		fprintf(fp, "%s%c", "appl_seq_num", sep);
//		fprintf(fp, "%s%c", "exec_price", sep);
//		fprintf(fp, "%s%c", "exec_volume", sep);
//		fprintf(fp, "%s%c", "value_trade", sep);
//		fprintf(fp, "%s%c", "bid_appl_seq_num", sep);
//		fprintf(fp, "%s%c", "offer_appl_seq_num", sep);
//		fprintf(fp, "%s%c", "side", sep);
//		fprintf(fp, "%s%c", "exec_type", sep);
//		fprintf(fp, "%s%c", "md_stream_id", sep);
//		fprintf(fp, "%s%c", "biz_index", sep);
//		fprintf(fp, "%s%c", "variety_category", sep);
//
//		//回调次数
//		fprintf(fp, "%s%c", "callBack_cnt", sep);
//		//第几个数据
//		fprintf(fp, "%s%c", "sequence", sep);
//		//本地系统时间戳
//		fprintf(fp, "%s%c", "local_time", sep);
//
//		fputc('\n', fp);
//
//		fclose(fp);
//	}
//	else
//	{
//		fclose(fp);
//	}
//
//	//追加方式打开文件
//	_tickExecutionCsvFile = fopen(filename, "a");
//
//	return;
//}

void StoreMarketData::OpenOrdAndExeCsvFile()
{
	std::string fileNameStr = _folder + "/OrderAndExecution" + _data + ".csv";
	const char* filename = fileNameStr.c_str();
	FILE* fp = NULL;

	fp = fopen(filename, "r");
	if (fp == NULL)
	{
		printf("OpenOrdAndExeCsvFile: %s does not exist, create a new one\n", filename);
		fp = fopen(filename, "w");
		if (fp == NULL)
		{
			perror("fopen OrderAndExecution.csv");
			exit(1);
		}

		char sep = _sep;
		fprintf(fp, "%s%c", "security_code", sep);
		fprintf(fp, "%s%c", "time", sep);
		fprintf(fp, "%s%c", "price", sep);
		fprintf(fp, "%s%c", "volume", sep);
		fprintf(fp, "%s%c", "channel_no", sep);
		fprintf(fp, "%s%c", "id", sep);
		fprintf(fp, "%s%c", "BTag", sep);
		fprintf(fp, "%s%c", "STag", sep);
		fprintf(fp, "%s%c", "BSFlag", sep);
		fprintf(fp, "%s%c", "ADFlag", sep);
		fprintf(fp, "%s%c", "MLFlag", sep);
		//fprintf(fp, "%s%c", "date", sep);

		//回调次数
		fprintf(fp, "%s%c", "callBack_cnt", sep);
		//第几个数据
		fprintf(fp, "%s%c", "sequence", sep);
		//本地系统时间戳
		fprintf(fp, "%s", "local_time");

		fputc('\n', fp);

		fclose(fp);
	}
	else
	{
		fclose(fp);
	}

	//追加方式打开文件
	_ordAndExeCsvFile = fopen(filename, "a");

	return;
}

//打开OrderBook.Csv文件
void StoreMarketData::OpenOrderBookCsvFile()
{
	//循环打开多个文件，给多个线程写入
	for (int32_t fileCount = 1; fileCount <= _threadNumOrderBook; fileCount++)
	{
		std::string fCnt = std::to_string(fileCount);

		std::string fileNameStr = _folder + "/QuickSnap" + _data + "_" + fCnt + ".csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf("OpenOrderBookCsvFile: %s does not exist, create a new one\n", filename);
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen orderbook.csv");
				exit(1);
			}

			char sep = _sep;

			fprintf(fp, "%s%c", "security_code", sep);
			fprintf(fp, "%s%c", "last_tick_time", sep);
			//fprintf(fp, "%s%c", "last_tick_seq", sep);
			
			//买委托簿，2档，每档的委托队列最多有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_volume", sep);
			}
			//卖委托簿，2档，每档的委托队列最多有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_volume", sep);
			}

			fprintf(fp, "%s%c", "total_num_trades", sep);
			fprintf(fp, "%s%c", "total_volume_trade", sep);
			fprintf(fp, "%s%c", "total_value_trade", sep);
			fprintf(fp, "%s%c", "last_price", sep);
			//回调次数
			fprintf(fp, "%s%c", "callBack_cnt", sep);
			//第几个数据
			fprintf(fp, "%s%c", "sequence", sep);
			//本地系统时间戳
			fprintf(fp, "%s", "local_time");//最后一列不带逗号

			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			fclose(fp);
		}

		//追加方式打开文件，放入队列
		FILE* fp1 = fopen(filename, "a");
		_orderBookCsvFiles.push(fp1);
	}

	return;
}

//OnMDSnapShot()中调用的写文件函数
void StoreMarketData::WriteSnapshotToFile(const SnapshotInfo* snapInfo, FILE* fp)
{
	char sep = _sep;

	fprintf(fp, "%u%c", snapInfo->_sc, sep);		//股票代码
	fprintf(fp, "%u%c", snapInfo->_tm, sep);		//时间
	fprintf(fp, "%u%c", snapInfo->_pxIopv, sep);	//ETF净值估计
	fprintf(fp, "%u%c", snapInfo->_pxHigh, sep);	//最高价
	fprintf(fp, "%u%c", snapInfo->_pxAvgAsk, sep);	//加权平均委卖价格
	
	//申卖价
	for (int i = 9; i >= 0; i--)
	{
		fprintf(fp, "%u%c", snapInfo->_pxAsk[i], sep);
	}

	//最新成交价
	fprintf(fp, "%u%c", snapInfo->_pxLast, sep);

	//申买价
	for (int i = 0; i < 10; ++i)
	{
		fprintf(fp, "%u%c", snapInfo->_pxBid[i], sep);
	}
	
	//加权平均委买价格
	fprintf(fp, "%u%c", snapInfo->_pxAvgBid, sep);
	//最低价
	fprintf(fp, "%u%c", snapInfo->_pxLow, sep);
	//委托卖出总量
	fprintf(fp, "%lu%c", snapInfo->_qtyTotAsk, sep);

	//申卖量
	for (int i = 9; i >= 0; i--)
	{
		fprintf(fp, "%lu%c", snapInfo->_qtyAsk[i], sep);
	}

	//成交总量
	fprintf(fp, "%lu%c", snapInfo->_qtyTot, sep);

	//申买量
	for (int i = 0; i < 10; ++i)
	{
		fprintf(fp, "%lu%c", snapInfo->_qtyBid[i], sep);
	}

	//委托买入总量
	fprintf(fp, "%lu%c", snapInfo->_qtyTotBid, sep);
	//成交总笔数
	fprintf(fp, "%u%c", snapInfo->_ctTot, sep);
	//成交总额
	fprintf(fp, "%lf%c", snapInfo->_cnyTot, sep);
	//当前品种(交易)状态
	fprintf(fp, "%s%c", snapInfo->_sta, sep);
	//交易归属日期
	fprintf(fp, "%u%c", snapInfo->_dt, sep);

	fprintf(fp, "%u%c", snapInfo->_numCb, sep);	//回调次数
	fprintf(fp, "%u%c", snapInfo->_seq, sep);	//本次回调中的第几个（从1开始）
	//最后一个字段，不要逗号
	fprintf(fp, "%d", snapInfo->_timLoc);	//收到数据时本地系统时间

	return;
}

//往文件写入一条补充信息，回调次数，本条数据的次序，时间戳
//void  StoreMarketData::WriteAdditionalInfoToFile(uint32_t callbackCnt, uint32_t sequence, int32_t localTime, FILE* fp)
//{
//	char sep = _sep;
//	//回调次数
//	fprintf(fp, "%u%c", callbackCnt, sep);
//	//本组数据的第几个
//	fprintf(fp, "%u%c", sequence, sep);
//	//本地时间戳
//	fprintf(fp, "%d%c", localTime, sep);
//
//	return;
//}

//向文件写一条委托数据
//void StoreMarketData::WriteTickOrderToFile(amd::ama::MDTickOrder* ticks, FILE* fp)
//{
//	char sep = _sep;
//
//	fprintf(fp, "%d%c", ticks->market_type, sep);
//
//	fprintf(fp, "%s%c", ticks->security_code, sep);	//股票代码
//	fprintf(fp, "%d%c", ticks->channel_no, sep);
//	fprintf(fp, "%ld%c", ticks->appl_seq_num, sep);
//	fprintf(fp, "%ld%c", ticks->order_time, sep);
//	fprintf(fp, "%ld%c", ticks->order_price, sep);
//	fprintf(fp, "%ld%c", ticks->order_volume, sep);
//	fprintf(fp, "%hhu%c", ticks->side, sep);
//	fprintf(fp, "%hhu%c", ticks->order_type, sep);
//	fprintf(fp, "%s%c", ticks->md_stream_id, sep);
//	fprintf(fp, "%ld%c", ticks->orig_order_no, sep);
//	fprintf(fp, "%ld%c", ticks->biz_index, sep);
//	fprintf(fp, "%hhu%c", ticks->variety_category, sep);
//	fprintf(fp, "%ld%c", ticks->traded_order_volume, sep);
//
//	return;
//}

//向文件写一条成交数据
//void StoreMarketData::WriteTickExecutionToFile(amd::ama::MDTickExecution* tick, FILE* fp)
//{
//	char sep = _sep;
//
//	fprintf(fp, "%d%c", tick->market_type, sep);//市场类型直接写整型数字
//
//	fprintf(fp, "%s%c", tick->security_code, sep);	//股票代码
//	fprintf(fp, "%ld%c", tick->exec_time, sep);//时间
//	fprintf(fp, "%d%c", tick->channel_no, sep);
//	fprintf(fp, "%ld%c", tick->appl_seq_num, sep);
//	fprintf(fp, "%ld%c", tick->exec_price, sep);
//	fprintf(fp, "%ld%c", tick->exec_volume, sep);
//	fprintf(fp, "%ld%c", tick->value_trade, sep);
//	fprintf(fp, "%ld%c", tick->bid_appl_seq_num, sep);
//	fprintf(fp, "%ld%c", tick->offer_appl_seq_num, sep);
//	fprintf(fp, "%hhu%c", tick->side, sep);
//	fprintf(fp, "%hhu%c", tick->exec_type, sep);
//	fprintf(fp, "%s%c", tick->md_stream_id, sep);
//	fprintf(fp, "%ld%c", tick->biz_index, sep);
//	fprintf(fp, "%hhu%c", tick->variety_category, sep);
//
//	return;
//}

//写入ordAndExeInfo数据
void StoreMarketData::WriteOrdAndExeInfoToFile(const OrdAndExeInfo* info, FILE* fp)
{
	fprintf(fp, "%u%c", info->_sc, _sep);
	fprintf(fp, "%u%c", info->_tm, _sep);
	fprintf(fp, "%u%c", info->_px, _sep);
	fprintf(fp, "%u%c", info->_qty, _sep);
	fprintf(fp, "%hu%c", info->_ch, _sep);
	fprintf(fp, "%u%c", info->_id, _sep);
	fprintf(fp, "%u%c", info->_BTag, _sep);
	fprintf(fp, "%u%c", info->_STag, _sep);
	fprintf(fp, "%c%c", info->_BSFlag, _sep);
	fprintf(fp, "%c%c", info->_ADFlag, _sep);
	fprintf(fp, "%c%c", info->_MLFlag, _sep);
	//fprintf(fp, "%u%c", info->_dt, _sep);

	fprintf(fp, "%u%c", info->_numCb, _sep);
	fprintf(fp, "%u%c", info->_seq, _sep);
	//最后一个字段，不要逗号
	fprintf(fp, "%d", info->_timLoc);
}

//往文件写入一条OrderBook数据
//void StoreMarketData::WriteOrderBookToFile(amd::ama::MDOrderBook* orderBook, FILE* fp)
//{
//	size_t bid_order_size = orderBook->bid_order_book.size();
//	size_t offer_order_size = orderBook->offer_order_book.size();
//
//	char sep = _sep;
//
//	fprintf(fp, "%d%c", orderBook->channel_no, sep);
//
//	fprintf(fp, "%d%c", orderBook->market_type, sep);//市场类型直接写整型数字
//
//	fprintf(fp, "%s%c", orderBook->security_code, sep);
//	fprintf(fp, "%ld%c", orderBook->last_tick_time, sep);
//	fprintf(fp, "%ld%c", orderBook->last_snapshot_time, sep);
//	fprintf(fp, "%ld%c", orderBook->last_tick_seq, sep);
//
//	//买委托簿最多2档
//	for (size_t i = 0; i < 2; i++)
//	{
//		if (i < bid_order_size)
//		{
//			//printf("bid_order_size = %u, i = %u\n", bid_order_size, i);
//
//			fprintf(fp, "%ld%c", orderBook->bid_order_book[i].price, sep);
//			fprintf(fp, "%ld%c", orderBook->bid_order_book[i].volume, sep);
//			//fprintf(fp, "%ld%c", orderBook->bid_order_book[i].order_queue_size, sep);
//
//			//委托队列里有order_queue_size个数据
//			/*std::string orderQueue;
//			for (int j = 0; j < orderBook->bid_order_book[i].order_queue_size; j++)
//			{
//				printf("order_queue_size = %ld, j = %d\n", orderBook->bid_order_book[i].order_queue_size, j);
//
//				orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
//				orderQueue += " ";
//			}
//			fprintf(fp, "%s%c", orderQueue.c_str(), sep);*/
//		}
//		else
//		{
//			fprintf(fp, "%ld%c", 0l, sep);
//			fprintf(fp, "%ld%c", 0l, sep);
//			//fprintf(fp, "%ld%c", 0l, sep);
//			//fprintf(fp, "%ld%c", 0l, sep);
//		}
//	}
//
//	//卖委托簿最多2档
//	for (size_t i = 0; i < 2; i++)
//	{
//		if (i < offer_order_size)
//		{
//			//printf("offer_order_size = %u, i = %u\n", offer_order_size, i);
//
//			fprintf(fp, "%ld%c", orderBook->offer_order_book[i].price, sep);
//			fprintf(fp, "%ld%c", orderBook->offer_order_book[i].volume, sep);
//			//fprintf(fp, "%ld%c", orderBook->offer_order_book[i].order_queue_size, sep);
//
//			//委托队列里有order_queue_size个数据
//			//std::string orderQueue;
//			//for (int j = 0; j < orderBook->offer_order_book[i].order_queue_size; j++)
//			//{
//			//	//printf("order_queue_size = %ld, j = %d\n", orderBook->offer_order_book[i].order_queue_size, j);
//
//			//	orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
//			//	orderQueue += " ";
//			//}
//			//fprintf(fp, "%s%c", orderQueue.c_str(), sep);
//		}
//		else
//		{
//			fprintf(fp, "%ld%c", 0l, sep);
//			fprintf(fp, "%ld%c", 0l, sep);
//			//fprintf(fp, "%ld%c", 0l, sep);
//			//fprintf(fp, "%ld%c", 0l, sep);
//		}
//	}
//
//	fprintf(fp, "%ld%c", orderBook->total_num_trades, sep);
//	fprintf(fp, "%ld%c", orderBook->total_volume_trade, sep);
//	fprintf(fp, "%ld%c", orderBook->total_value_trade, sep);
//	fprintf(fp, "%ld%c", orderBook->last_price, sep);
//
//	return;
//}

void StoreMarketData::WriteOrderBookToFile(QuickSnapInfo* info, FILE* fp)
{
	char sep = _sep;

	fprintf(fp, "%u%c", info->_sc, sep);
	fprintf(fp, "%u%c", info->_tm, sep);
	
	//fprintf(fp, "%ld%c", info->last_tick_seq, sep);

	//买委托簿最多2档
	fprintf(fp, "%u%c", info->_pxBid01, sep);
	fprintf(fp, "%lu%c", info->_volBid01, sep);
	fprintf(fp, "%u%c", info->_pxBid02, sep);
	fprintf(fp, "%lu%c", info->_volBid02, sep);

	//卖委托簿最多2档
	fprintf(fp, "%u%c", info->_pxAsk01, sep);
	fprintf(fp, "%lu%c", info->_volAsk01, sep);
	fprintf(fp, "%u%c", info->_pxAsk02, sep);
	fprintf(fp, "%lu%c", info->_volAsk02, sep);

	fprintf(fp, "%u%c", info->_ctTot, sep);
	fprintf(fp, "%lu%c", info->_qtyTot, sep);
	fprintf(fp, "%.5lf%c", info->_cnyTot, sep);
	fprintf(fp, "%lu%c", info->_pxLast, sep);

	//补充信息
	fprintf(fp, "%u%c", info->_cntCb, sep);
	fprintf(fp, "%u%c", info->_seq, sep);
	//最后一个字段，不要逗号
	fprintf(fp, "%d", info->_tmLoc);

	return;
}

//创建实例的函数
MDApplication* CreateStoreCsv()
{
	return new StoreMarketData();
}

//注册到反射里，key为2
RegisterAction g_regActStoreCsv("2", CreateStoreCsv);