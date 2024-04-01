#include <unistd.h>
#include <time.h>
#include <iostream>
#include <string>
#include <mutex>
#include <stdio.h>
#include <thread>
#include "StoreMarketDataBin.h"
#include "ConfigFile.h"
#include "SPLog.h"
#include "lz4.h"
#include "sp_tool.h"
#include "reflect.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

extern ConfigFile g_cfgFile;//其他文件的全局变量
extern SPLog g_log; //记录日志的全局变量

static char logbuf[1024];		//写日志的字符串缓冲区，默认初始化为0
static std::mutex g_mutexCbCnt;  //回调次数增加时的线程同步
static std::mutex g_mutexOrderBookFileMap;		//修改线程和orderBook文件映射的互斥锁

//构造函数
StoreMarketDataBin::StoreMarketDataBin()
{
	_snapshotCsvFile = NULL;
	//_tickOrderCsvFile = NULL;
	//_tickExecutionCsvFile = NULL;
	_ordAndExeBinFile = NULL;

	_pArraySnapshot = NULL;
	_pArrayOrdAndExeInfo = NULL;
	//_pArrayTickOrder = NULL;
	//_pArrayTickExecution = NULL;
	//默认时false，如果没启动，析构时不会join
	_logFlag = false;
}

//初始化
int StoreMarketDataBin::BeforeMarket()
{
	//委托簿的线程个数
	string threadNum = g_cfgFile.Get("OrderBookThreadNum");
	if (threadNum == string())
	{
		cerr << "OrderBookThreadNum not found in config file" << endl;
		return -1;
	}
	_threadNumOrderBook = stoi(threadNum);

	//要存储文件的目录
	_folder = g_cfgFile.Get("StoreMarketDataBinFolder");
	if (_folder == string())
	{
		cerr << "StoreMarketDataBinFolder not found in cionfig file" << endl;
		return -1;
	}

	//初始化成员_callBackCount
	_callbackCount = 0;

	//初始化成员变量_data
	time_t tim1 = time(NULL);
	struct tm* tim2 = localtime(&tim1);
	char data[16] = { 0 };
	sprintf(data, "%d%02d%02d", tim2->tm_year + 1900, tim2->tm_mon + 1, tim2->tm_mday);
	_data = data;

	string level = g_cfgFile.Get("compressLevel");
	if (level == string())
	{
		cerr << "StoreMarketDataBin::Init() compressLevel not found in config file" << endl;
	}
	else
	{
		_compressLevel = std::stoi(level);
		printf("StoreMarketDataBin::Init(): compressLevel = %d\n", _compressLevel);
	}

	//打开snapshot.csv文件
	OpenSnapshotCsvFile();
	//打开OrderBook文件
	OpenOrderBookCsvFile();
	//打开tickExection文件
	//OpenTickExecutionCsvFile();
	//打开TickOrder.csv文件
	//OpenTickOrderCsvFile();

	OpenOrdAndExeInfo();

	//开辟空间
	_pArraySnapshot = new std::vector< SnapshotInfo>;
	_pArraySnapshot->resize(ARRAY_LENGTH_SNAPSHOT);
	_idxSnapshot = 0;

	/*_pArrayTickOrder = new std::vector< TickOrderInfo>;
	_pArrayTickOrder->resize(ARRAY_LENGTH_TICKORDER);
	_idxTickOrder = 0;*/

	/*_pArrayTickExecution = new std::vector< TickExecutionInfo>;
	_pArrayTickExecution->resize(ARRAY_LENGTH_TICKEXECUTION);
	_idxTickExecution = 0;*/

	_pArrayOrdAndExeInfo = new std::vector< OrdAndExeInfo>;
	_pArrayOrdAndExeInfo->resize(ARRAY_LENGTH_ORDANDEXECU);
	_idxOrdAndExe = 0;

	for (int i = 0; i < _threadNumOrderBook; i++)
	{
		auto ptr = new std::vector<QuickSnapInfo>;
		ptr->resize(ARRAY_LENGTH_QUICKSNAP);
		_arrsOrderBook.push_back(ptr);

		_idxesOrderBook.push_back(0);
	}

	//启动写日志线程
	_logFlag = true;
	_logThread = std::thread(&StoreMarketDataBin::FuncRecordeCallbackCount, this);

	return 0;
}

//记录回调次数
void StoreMarketDataBin::LogCallbackCount()
{
	//启动写日志线程
	_logFlag = true;
	_logThread = std::thread(&StoreMarketDataBin::FuncRecordeCallbackCount, this);
}

//析构函数
StoreMarketDataBin::~StoreMarketDataBin()
{
	//如果在运行，回收线程
	if (_logFlag)
	{
		_logFlag = false;
		_logThread.join();
		printf("_logThread joined\n");
	}

	size_t length;
	size_t ret;

	//snapshot写入文件
	if (_pArraySnapshot != NULL)
	{
		length = _idxSnapshot;
		//printf("_idxSnapshot = %lu\n", _idxSnapshot);
		sprintf(logbuf, "_idxSnapshot = %lu", _idxSnapshot);
		g_log.WriteLog(logbuf);

		fwrite(&length, sizeof(length), 1, _snapshotCsvFile);

		if (length == 0)
		{
			//printf("did not receive snapshot data\n");
			sprintf(logbuf, "did not receive snapshot data");
			g_log.WriteLog(logbuf);
		}
		else
		{
			//要写入的数据，看看第一个对不对
			/*printf("fisrt snapshot data: mktyp = %d, code = %s, time = %ld, callbackCnt = %u, seq = %u\n",
				_pArraySnapshot->at(0)._snapshot.market_type,
				_pArraySnapshot->at(0)._snapshot.security_code,
				_pArraySnapshot->at(0)._snapshot.orig_time,
				_pArraySnapshot->at(0)._callbackCnt,
				_pArraySnapshot->at(0)._sequence);*/
			//看看最后一个数据对不对
			/*printf("snapshot last one, mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
				_pArraySnapshot->at(_idxSnapshot - 1)._snapshot.market_type,
				_pArraySnapshot->at(_idxSnapshot - 1)._snapshot.security_code,
				_pArraySnapshot->at(_idxSnapshot - 1)._callbackCnt,
				_pArraySnapshot->at(_idxSnapshot - 1)._sequence);*/

			//不压缩，直接写文件
			if (_compressLevel == 0)
			{
				ret = fwrite(&(_pArraySnapshot->at(0)), sizeof(SnapshotInfo), length, _snapshotCsvFile);
				if (ret == 0)
				{
					perror("fwrite snapshot");
				}
				else
				{
					printf("write SnapshotInfo to file, ret = %lu\n", ret);
				}
			}
			else
			{
				//把数据压缩，存入文件
				CompressDataToFile(&(_pArraySnapshot->at(0)), sizeof(SnapshotInfo) * length, _snapshotCsvFile);
			}
		}

		delete _pArraySnapshot;
		fclose(_snapshotCsvFile);
	}

	if (_pArrayOrdAndExeInfo != NULL)
	{
		//tickExecuti
		length = _idxOrdAndExe;
		//printf("_idxOrdAndExe = %lu\n", _idxOrdAndExe);
		sprintf(logbuf, "_idxOrdAndExe = %lu", _idxOrdAndExe);
		g_log.WriteLog(logbuf);

		ret = fwrite(&length, sizeof(length), 1, _ordAndExeBinFile);
		if (ret == 0)
		{
			perror("fwrite ordAndExe length");
		}

		if (length == 0)
		{
			printf("did not receive ordAndExe data\n");
		}
		else
		{
			//要写入的数据，看看第一个对不对
			/*printf("fisrt tickExecution data: mktyp = %d, code = %s, time = %ld, callbackCnt = %u, seq = %u\n",
				_pArrayTickExecution->at(0)._tickExetution.market_type,
				_pArrayTickExecution->at(0)._tickExetution.security_code,
				_pArrayTickExecution->at(0)._tickExetution.exec_time,
				_pArrayTickExecution->at(0)._callbackCnt,
				_pArrayTickExecution->at(0)._sequence);*/

				//看看最后一个数据对不对
				/*printf("tickExecution last one, mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
					_pArrayTickExecution->at(_idxTickExecution - 1)._tickExetution.market_type,
					_pArrayTickExecution->at(_idxTickExecution - 1)._tickExetution.security_code,
					_pArrayTickExecution->at(_idxTickExecution - 1)._callbackCnt,
					_pArrayTickExecution->at(_idxTickExecution - 1)._sequence);*/

			if (_compressLevel == 0)
			{
				//不压缩，直接写文件
				ret = fwrite(&(_pArrayOrdAndExeInfo->at(0)), sizeof(OrdAndExeInfo), length, _ordAndExeBinFile);
				if (ret == 0)
				{
					perror("fwrite _ordAndExeBinFile");
				}
				else
				{
					printf("write _ordAndExeBinFile, ret = %lu\n", ret);
				}
			}
			else
			{
				//把数据压缩，存入文件
				CompressDataToFile(&(_pArrayOrdAndExeInfo->at(0)), sizeof(OrdAndExeInfo) * length, _ordAndExeBinFile);
			}
		}

		delete _pArrayOrdAndExeInfo;
		fclose(_ordAndExeBinFile);
	}

	//OrderBook写文件
	for (size_t i = 0; i < _arrsOrderBook.size(); i++)
	{
		length = _idxesOrderBook[i];
		//printf("_idxesOrderBook[%ld] = %ld\n", i, _idxesOrderBook[i]);
		sprintf(logbuf, "_idxesOrderBook[%ld] = %ld", i, _idxesOrderBook[i]);
		g_log.WriteLog(logbuf);

		fwrite(&length, sizeof(length), 1, _orderBookCsvFiles[i]);

		if (length == 0)
		{
			printf("array %ld did not receive orderBook data\n", i);
		}
		else
		{
			//要写入的数据，看看第一个对不对
			/*printf("fisrt orderBook data: mktyp = %d, code = %s, time = %ld, callbackCnt = %u, seq = %u\n",
				_arrsOrderBook[i]->at(0).market_type,
				_arrsOrderBook[i]->at(0).security_code,
				_arrsOrderBook[i]->at(0).last_tick_time,
				_arrsOrderBook[i]->at(0)._callbackCnt,
				_arrsOrderBook[i]->at(0)._sequence);*/

			//要写入的数据，看看最后一个对不对
			/*printf("last orderBook data: mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
				_arrsOrderBook[i]->at(_idxesOrderBook[i] - 1).market_type,
				_arrsOrderBook[i]->at(_idxesOrderBook[i] - 1).security_code,
				_arrsOrderBook[i]->at(_idxesOrderBook[i] - 1)._callbackCnt,
				_arrsOrderBook[i]->at(_idxesOrderBook[i] - 1)._sequence);*/

			if (_compressLevel == 0)
			{
				//不压缩，直接写文件
				ret = fwrite(&(_arrsOrderBook[i]->at(0)), sizeof(QuickSnapInfo), length, _orderBookCsvFiles[i]);
				if (ret == 0)
				{
					perror("fwrite orderBook");
				}
				else
				{
					printf("write OrderBookInfo to file %lu, ret = %lu\n", i, ret);
				}
			}
			else
			{
				//printf("sizeof(QuickSnapInfo) = %lu\n", sizeof(QuickSnapInfo));
				//把数据压缩，存入文件，
				CompressDataToFile(&(_arrsOrderBook[i]->at(0)), sizeof(QuickSnapInfo) * length, _orderBookCsvFiles[i]);
			}
		}

		delete _arrsOrderBook[i];
		fclose(_orderBookCsvFiles[i]);//文件个数和数组个数是一样的，就算比实际线程个数多了，也无妨，一起释放
	}

	
}

void StoreMarketDataBin::OnSnapshot(SnapshotInfo* snapInfo, uint32_t cnt)
{
	uint32_t callbackCnt;
	//回调次数增加，并记下
	g_mutexCbCnt.lock();
	callbackCnt = ++_callbackCount;
	g_mutexCbCnt.unlock();

	//本地系统时间戳
	int32_t locTim = GetLocalTm();

	for (uint32_t i = 0; i < cnt; ++i)
	{
		//判断idx有没有越界
		if (_idxSnapshot >= _pArraySnapshot->size())
		{
			//扩容
			_pArraySnapshot->resize(2 * _pArraySnapshot->size());

			//记录日志
			string loginfo = "ARRAY_LENGTH_SNAPSHOT is not enough， resize again";
			cout << loginfo << endl;
			g_log.WriteLog(loginfo.c_str());
		}

		//存入快照数据
		memcpy(&(_pArraySnapshot->at(_idxSnapshot)), snapInfo + i, sizeof(SnapshotInfo));
		//存入补充信息
		_pArraySnapshot->at(_idxSnapshot)._numCb = callbackCnt;
		_pArraySnapshot->at(_idxSnapshot)._seq = i + 1;
		_pArraySnapshot->at(_idxSnapshot)._timLoc = locTim;

		//下标自增
		_idxSnapshot++;
	}

	return;
}

//void StoreMarketDataBin::OnMyTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
//{
//	uint32_t callbackCnt;
//	//回调次数增加，并记下
//	g_mutexCbCnt.lock();
//	callbackCnt = ++_callbackCount;
//	g_mutexCbCnt.unlock();
//
//	//本地系统时间戳
//	int32_t locTim = GetLocalTm();
//
//	for (uint32_t i = 0; i < cnt; ++i)
//	{
//		//判断idx有没有越界
//		if (_idxTickOrder >= _pArrayTickOrder->size())
//		{
//			//扩容
//			_pArrayTickOrder->resize(2 * _pArrayTickOrder->size());
//
//			//记录日志
//			string loginfo = "ARRAY_LENGTH_TICKORDER is not enough， resize again";
//			cerr << loginfo << endl;
//			g_log.WriteLog(loginfo.c_str());
//		}
//
//		//放入数组
//		memcpy(&_pArrayTickOrder->at(_idxTickOrder)._tickOrder, ticks + i, sizeof(amd::ama::MDTickOrder));
//		_pArrayTickOrder->at(_idxTickOrder)._callbackCnt = callbackCnt;
//		_pArrayTickOrder->at(_idxTickOrder)._sequence = i + 1;
//		_pArrayTickOrder->at(_idxTickOrder)._localTime = locTim;
//
//		/*printf("mktype = %d, code = %s, cbCnt = %d, seq = %u\n",
//			_pArrayTickOrder->at(_idxTickOrder)._tickOrder.market_type,
//			_pArrayTickOrder->at(_idxTickOrder)._tickOrder.security_code,
//			callbackCnt,
//			i + 1);*/
//
//		_idxTickOrder++;
//
//		//std::this_thread::sleep_for(std::chrono::seconds(1));//睡1秒
//	}
//
//	return;
//}

//void StoreMarketDataBin::OnMyTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt)
//{
//	//printf("StoreMarketDataBin::OnMyTickExecution\n");
//
//	uint32_t callbackCnt;
//	//回调次数增加，并记下
//	g_mutexCbCnt.lock();
//	callbackCnt = ++_callbackCount;
//	g_mutexCbCnt.unlock();
//
//	//本地系统时间戳
//	int32_t locTim = GetLocalTm();
//
//	for (uint32_t i = 0; i < cnt; ++i)
//	{
//		//printf("_idxTickExecution = %ld\n", _idxTickExecution);
//
//		//判断idx有没有越界
//		if (_idxTickExecution >= _pArrayTickExecution->size())
//		{
//			//扩容
//			_pArrayTickExecution->resize(2 * _pArrayTickExecution->size());
//
//			//记录日志
//			string loginfo = "ARRAY_LENGTH_TICKEXECUTION is not enough， resize again";
//			cerr << loginfo << endl;
//			g_log.WriteLog(loginfo.c_str());
//		}
//
//		//复制成交数据
//		memcpy(&(_pArrayTickExecution->at(_idxTickExecution)._tickExetution), tick + i, sizeof(amd::ama::MDTickExecution));
//		//写入补充信息
//		_pArrayTickExecution->at(_idxTickExecution)._callbackCnt = callbackCnt;
//		_pArrayTickExecution->at(_idxTickExecution)._sequence = i + 1;
//		_pArrayTickExecution->at(_idxTickExecution)._localTime = locTim;
//
//		/*printf("mktp = %d, code = %s, time = %ld\n", 
//			_pArrayTickExecution->at(_idxTickExecution)._tickExetution.market_type,
//			_pArrayTickExecution->at(_idxTickExecution)._tickExetution.security_code,
//			_pArrayTickExecution->at(_idxTickExecution)._tickExetution.exec_time);*/
//
//		//下标自增
//		_idxTickExecution++;
//		
//	}
//
//	//std::this_thread::sleep_for(std::chrono::seconds(1));//睡1秒
//
//	return;
//}

void StoreMarketDataBin::OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt)
{
	//printf("StoreMarketDataBin::OnMyTickExecution\n");

	uint32_t callbackCnt;
	//回调次数增加，并记下
	g_mutexCbCnt.lock();
	callbackCnt = ++_callbackCount;
	g_mutexCbCnt.unlock();

	//本地系统时间戳
	int32_t locTim = GetLocalTm();

	for (uint32_t i = 0; i < cnt; ++i)
	{
		//printf("_idxTickExecution = %ld\n", _idxTickExecution);

		//判断idx有没有越界
		if (_idxOrdAndExe >= _pArrayOrdAndExeInfo->size())
		{
			//扩容
			_pArrayOrdAndExeInfo->resize(2 * _pArrayOrdAndExeInfo->size());

			//记录日志
			string loginfo = "ARRAY_LENGTH_ORDANDEXECU is not enough， resize again";
			cerr << loginfo << endl;
			g_log.WriteLog(loginfo.c_str());
		}

		//写入补充信息
		info[i]._numCb = callbackCnt;
		info[i]._seq = i + 1;
		info[i]._timLoc = locTim;
		//复制成交数据
		memcpy(&(_pArrayOrdAndExeInfo->at(_idxOrdAndExe)), info + i, sizeof(OrdAndExeInfo));
		

		/*printf("mktp = %d, code = %s, time = %ld\n",
			_pArrayTickExecution->at(_idxTickExecution)._tickExetution.market_type,
			_pArrayTickExecution->at(_idxTickExecution)._tickExetution.security_code,
			_pArrayTickExecution->at(_idxTickExecution)._tickExetution.exec_time);*/

			//下标自增
		_idxOrdAndExe++;

	}

	//std::this_thread::sleep_for(std::chrono::seconds(1));//睡1秒

	return;
}

//void StoreMarketDataBin::OnMyOrderBook(std::vector<amd::ama::MDOrderBook>& order_book)
//{
//	uint32_t callbackCnt;
//	//回调次数增加，并记下
//	g_mutexCbCnt.lock();
//	callbackCnt = ++_callbackCount;
//	g_mutexCbCnt.unlock();
//
//	//本地系统时间戳
//	int32_t locTim = GetLocalTm();
//
//	//本线程id
//	std::thread::id thid = std::this_thread::get_id();
//
//	//本线程有没有匹配
//	if (_threadAndOrderBookArray.count(thid) == 0)
//	{
//		std::cout << "StoreMarketDataBin::OnMyOrderBook：a new thread, thread id = " << thid << std::endl;
//
//		std::lock_guard<std::mutex> _(g_mutexOrderBookFileMap);
//		//已经有多少个线程匹配到了数组
//		size_t sz = _threadAndOrderBookArray.size();
//
//		//printf("sz = %ld\n", sz);
//
//		//如果数组已经被匹配完了
//		if (sz == _arrsOrderBook.size())
//		{
//			//记录信息，本次回调直接返回
//			printf("StoreMarketDataBin::OnMyOrderBook：real thread num is more than array num\n");
//			g_log.WriteLog("StoreMarketDataBin::OnMyOrderBook：real thread num is more than array num\n");
//			
//			return;
//		}
//
//		//最新的线程匹配数组
//		_threadAndOrderBookArray[thid] = sz;
//
//	}
//
//	//要写的数组
//	size_t thrd = _threadAndOrderBookArray[thid];
//
//	for (size_t i = 0; i < order_book.size(); i++)
//	{
//		//printf("_idxesOrderBook[%lu] = %lu\n", thrd, _idxesOrderBook[thrd]);
//
//		//判断idx有没有越界
//		if (_idxesOrderBook[thrd] >= _arrsOrderBook[thrd]->size())
//		{
//			//扩容
//			_arrsOrderBook[thrd]->resize(2 * _arrsOrderBook[thrd]->size());
//
//			//记录日志
//			string loginfo = "ARRAY_LENGTH_QUICKSNAP is not enough， resize again";
//			cout << loginfo << endl;
//			g_log.WriteLog(loginfo.c_str());
//		}
//
//		//存入委托簿数据
//		OrderBookInfo& info = _arrsOrderBook[thrd]->at(_idxesOrderBook[thrd]);
//		
//		info.channel_no = order_book[i].channel_no;
//		info.market_type = order_book[i].market_type;
//		strcpy(info.security_code, order_book[i].security_code);
//		info.last_tick_time = order_book[i].last_tick_time;
//		info.last_snapshot_time = order_book[i].last_snapshot_time;
//		info.last_tick_seq = order_book[i].last_tick_seq;
//
//		if (order_book[i].bid_order_book.size() >= 1)
//		{
//			info._bidPrice01 = order_book[i].bid_order_book[0].price;
//			info._bidVolume01 = order_book[i].bid_order_book[0].volume;
//
//			if (order_book[i].bid_order_book.size() >= 2)
//			{
//				info._bidPrice02 = order_book[i].bid_order_book[1].price;
//				info._bidVolume02 = order_book[i].bid_order_book[1].volume;
//			}
//		}
//		
//		if (order_book[i].offer_order_book.size() >= 1)
//		{
//			info._offerPrice01 = order_book[i].offer_order_book[0].price;
//			info._offerVolume01 = order_book[i].offer_order_book[0].volume;
//
//			if (order_book[i].offer_order_book.size() >= 2)
//			{
//				info._offerPrice02 = order_book[i].offer_order_book[1].price;
//				info._offerVolume02 = order_book[i].offer_order_book[1].volume;
//			}
//		}
//
//		info.total_num_trades = order_book[i].total_num_trades;
//		info.total_volume_trade = order_book[i].total_volume_trade;
//		info.total_value_trade = order_book[i].total_value_trade;
//		info.last_price = order_book[i].last_price;
//
//		//存入补充信息
//		info._callbackCnt = callbackCnt;
//		info._sequence = i + 1;
//		info._localTime = locTim;
//
//		//下标自增
//		_idxesOrderBook[thrd]++;
//	}
//
//	//std::this_thread::sleep_for(std::chrono::seconds(1));//睡1秒
//
//	return;
//}

void StoreMarketDataBin::OnQuickSnap(QuickSnapInfo* info, uint32_t cnt)
{
	uint32_t callbackCnt;
	//回调次数增加，并记下
	g_mutexCbCnt.lock();
	callbackCnt = ++_callbackCount;
	g_mutexCbCnt.unlock();

	//本地系统时间戳
	int32_t locTim = GetLocalTm();

	//本线程id
	std::thread::id thid = std::this_thread::get_id();

	//本线程有没有匹配
	if (_threadAndOrderBookArray.count(thid) == 0)
	{
		std::cout << "StoreMarketDataBin::OnQuickSnap：a new thread, thread id = " << thid << std::endl;

		std::lock_guard<std::mutex> _(g_mutexOrderBookFileMap);
		//已经有多少个线程匹配到了数组
		size_t sz = _threadAndOrderBookArray.size();

		//printf("sz = %ld\n", sz);

		//如果数组已经被匹配完了
		if (sz == _arrsOrderBook.size())
		{
			//记录信息，本次回调直接返回
			printf("StoreMarketDataBin::OnMyOrderBook：real thread num is more than array num\n");
			g_log.WriteLog("StoreMarketDataBin::OnMyOrderBook：real thread num is more than array num\n");

			return;
		}

		//最新的线程匹配数组
		_threadAndOrderBookArray[thid] = sz;

	}

	//要写的数组
	size_t thrd = _threadAndOrderBookArray[thid];

	for (size_t i = 0; i < cnt; i++)
	{
		//printf("_idxesOrderBook[%lu] = %lu\n", thrd, _idxesOrderBook[thrd]);

		//判断idx有没有越界
		if (_idxesOrderBook[thrd] >= _arrsOrderBook[thrd]->size())
		{
			//扩容
			_arrsOrderBook[thrd]->resize(2 * _arrsOrderBook[thrd]->size());

			//记录日志
			string loginfo = "ARRAY_LENGTH_QUICKSNAP is not enough， resize again";
			cout << loginfo << endl;
			g_log.WriteLog(loginfo.c_str());
		}

		//存入委托簿数据
		//QuickSnapInfo& info = _arrsOrderBook[thrd]->at(_idxesOrderBook[thrd]);
		
		//补充信息
		info[i]._cntCb = callbackCnt;
		info[i]._seq = i + 1;
		info[i]._tmLoc = locTim;

		//存入数组
		memcpy(&(_arrsOrderBook[thrd]->at(_idxesOrderBook[thrd])), info + i, sizeof(QuickSnapInfo));

		//下标自增
		_idxesOrderBook[thrd]++;
	}

	//std::this_thread::sleep_for(std::chrono::seconds(1));//睡1秒

	return;
}

//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
void StoreMarketDataBin::OpenSnapshotCsvFile()
{
	string fileNameStr = _folder + "/" + SNAPSHOT_FILE_NAME + _data + ".bin";

	const char* filename = fileNameStr.c_str();

	//二进制写方式打开文件
	_snapshotCsvFile = fopen(filename, "wb");
	if (_snapshotCsvFile == NULL)
	{
		perror("fopen snapshot file");
		exit(1);
	}

	printf("StoreMarketDataBin open file %s\n", filename);

	return;
}

//打开TickOrder.csv逐笔委托数据文件
//void StoreMarketDataBin::OpenTickOrderCsvFile()
//{
//	std::string fileNameStr = _folder + "/" + TICKORDER_FILE_NAME + _data + ".bin";
//
//	const char* filename = fileNameStr.c_str();
//	FILE* fp = NULL;
//	fp = fopen(filename, "wb");//写模式打开二进制文件，没有就创建
//	if (fp == NULL)
//	{
//		perror("StoreMarketDataBin::OpenTickOrderCsvFile fopen ");
//		exit(1);
//	}
//
//	_tickOrderCsvFile = fp;
//
//	printf("StoreMarketDataBin: open file %s\n", filename);
//
//	return;
//}

void StoreMarketDataBin::OpenOrdAndExeInfo()
{
	std::string fileNameStr = _folder + "/" + ORDANDEXECU_FILE_NAME + _data + ".bin";

	const char* filename = fileNameStr.c_str();
	FILE* fp = NULL;
	fp = fopen(filename, "wb");//写模式打开二进制文件，没有就创建
	if (fp == NULL)
	{
		perror("StoreMarketDataBin::OpenOrdAndExeInfo fopen ");
		exit(1);
	}

	_ordAndExeBinFile = fp;

	printf("StoreMarketDataBin: open file %s\n", filename);

	return;
}

//打开TickExecution.csv成交数据文件
//void StoreMarketDataBin::OpenTickExecutionCsvFile()
//{
//	std::string fileNameStr = _folder + "/" + TICKEXECUTION_FILE_NAME + _data + ".bin";
//
//	const char* filename = fileNameStr.c_str();
//	
//	_tickExecutionCsvFile = fopen(filename, "wb");
//	if (_tickExecutionCsvFile == NULL)
//	{
//		perror("fopen tickExecu file");
//		exit(1);
//	}
//
//	printf("StoreMarketDataBin: open file %s\n", filename);
//
//	return;
//}

//打开OrderBook.Csv文件
void StoreMarketDataBin::OpenOrderBookCsvFile()
{
	//循环打开多个文件，给多个线程写入
	for (int32_t fileCount = 1; fileCount <= _threadNumOrderBook; fileCount++)
	{
		std::string fCnt = std::to_string(fileCount);

		std::string fileNameStr = _folder + "/" + QUICKSNAP_FILE_NAME + _data + "_" + fCnt + ".bin";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//打开文件，指针放入数组
		fp = fopen(filename, "wb");
		if (fp == NULL)
		{
			perror("fopen orderbook file");
			exit(1);
		}

		printf("StoreMarketDataBin: open file %s\n", filename);

		_orderBookCsvFiles.push_back(fp);
	}

	return;
}

//把数据压缩，存入文件
void StoreMarketDataBin::CompressDataToFile(const void* pData, size_t dataLen, FILE* fp)
{
	//printf("sizeof(QuickSnapInfo) * length, dataLen = %lu\n", dataLen);

	//分片压缩，每片的上限CHUNK_SIZE,开辟空间存储压缩后数据
	int lenCompressed = LZ4_compressBound(COMPRESS_CHUNK_SIZE);
	//printf("StoreMarketDataBin::CompressDataToFile: lenCompressed(dstCapacity) = %d\n", lenCompressed);

	//开辟空间存储压缩后数据
	char* bufCompressed = (char*)malloc(lenCompressed);
	if (bufCompressed == NULL)
	{
		perror("malloc bufCompressed");
		return;
	}

	int chunkSrcLen;
	int ret;
	int compressedSize;
	const char* source = (const char*)pData;
	//循环，分片压缩，直到最后一片
	for (size_t i = 0; i < dataLen; i += chunkSrcLen)
	{
		//清理缓冲区
		memset(bufCompressed, 0, lenCompressed);

		//压缩source中从i起的的数据，剩下数据足够多的话一次压缩CHUNK_SIZE,不够的话就把剩下压缩完
		if (dataLen - i >= COMPRESS_CHUNK_SIZE)
		{
			//压缩CHUNK_SIZE大小的数据
			chunkSrcLen = COMPRESS_CHUNK_SIZE;
		}
		else
		{
			//压缩剩下的所有数据
			chunkSrcLen = dataLen - i;
		}

		//从source+i开始，压缩chunkSrcLen大小的数据，到bufCompressed,压缩数据量返回
		//printf("before compress, chunkSrcLen = %d\n", chunkSrcLen);
		sprintf(logbuf, "before compress, chunkSrcLen = %d", chunkSrcLen);
		g_log.WriteLog(logbuf);

		ret = LZ4_compress_fast(source + i, bufCompressed, chunkSrcLen, lenCompressed, _compressLevel);
		//ret = LZ4_compress_default(source + i, bufCompressed, chunkSrcLen, lenCompressed);
		if (ret <= 0)
		{
			printf("StoreMarketDataBin::CompressDataToFile: compress failed, ret = %d\n", ret);
			return;
		}
		else
		{
			//printf("StoreMarketDataBin::CompressDataToFile：compressedSize = %d\n", ret);
			sprintf(logbuf, "compressedSize = %d", ret);
			g_log.WriteLog(logbuf);
		}

		compressedSize = ret;
		//插入标志和数据长度
		/*ret = fwrite(CHUNK_MARK, sizeof(CHUNK_MARK), 1, fp);
		if (ret == 0)
		{
			perror("fwrite CHUNK_MARK");
		}*/
		ret = fwrite(&compressedSize, sizeof(compressedSize), 1, fp);
		if (ret == 0)
		{
			perror("fwrite compressedSize");
			return;
		}
		//插入对应长度的数据
		ret = fwrite(bufCompressed, 1, compressedSize, fp);
		if (ret < compressedSize)
		{
			perror("fwrite bufCompressed");
			return;
		}
		//printf("fwrite compressed data to file, ret = %d\n", ret);
		sprintf(logbuf, "fwrite compressed data to file, ret = %d", ret);
		g_log.WriteLog(logbuf);
	}

	//printf("all data written to file\n");
	sprintf(logbuf, "all data written to file\n");
	g_log.WriteLog(logbuf);

	//释放空间
	free(bufCompressed);

	return;
}

//写日志的线程函数
void StoreMarketDataBin::FuncRecordeCallbackCount()
{
	char buf[1024] = { 0 };

	while (_logFlag)
	{
		sleep(10);

		memset(buf, 0, 1024);
		sprintf(buf, "StoreMarketDataBin callback count: %u", _callbackCount);
		g_log.WriteLog(buf);
		g_log.FlushLogFile();
		//printf("log callback count\n");
	}

	return;
}

//创建实例的函数
MDApplication* CreateStoreBin()
{
	return new StoreMarketDataBin();
}

//注册到反射里，key为1
RegisterAction g_regActStoreBin("1", CreateStoreBin);