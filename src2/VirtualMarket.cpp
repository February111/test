#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <set>
#include "VirtualMarket.h"
#include "ConfigFile.h"

using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::set;
using std::cout;

//读文件读一行的字符串长度
#define LINE_LENGTH 1600
//委托簿数据的档位个数
#define ORDERBOOK_LEVEL_LENGTH 2

extern ConfigFile g_cfgFile;//其他文件的全局变量

VirtualMarket::VirtualMarket()
{

	//回调函数初始化，给它赋一个什么都不做的匿名函数值
	_onSnapshot = [](SnapshotInfo *snapshot, uint32_t cnt) {};
	//_onMDTickOrder = [](amd::ama::MDTickOrder* ticks, uint32_t cnt) {};
	//_onMDTickExecution = [](amd::ama::MDTickExecution* tick, uint32_t cnt) {};
	_onOrdAndExecu = [](OrdAndExeInfo* ordAndExe, uint32_t cnt) {};
	//_onMDOrderBook = [](std::vector<amd::ama::MDOrderBook>& order_book) {};
	_onQuickSnap = [](QuickSnapInfo *info, uint32_t cnt) {};

	//初始化一下flag，默认不播放任何数据
	_playSnapshot = false;
	//_playTickOrder = false;
	//_playTickExecution = false;
	_playOrdAndExecu = false;
	_playOrderBook = false;

	//play时会不断判断最小回调次数，为0的不参与判断
	_callbackCntSnapshot = 0;
	//_callbackCntTickOrder = 0;
	//_callbackCntTickExecution = 0;
	_callbackCntOrdAndExe = 0;
	_callbackCntsOrderBook.clear();
}

//初始化
void VirtualMarket::Init()
{
	//加载配置信息
	/*if (g_cfgFile.Load("../data/config.conf") == false)
	{
		cerr << "VirtualMarket::VirutalMarket(): load config file failed\n" << endl;
		exit(1);
	}*/

	//行情数据文件所在目录
	_filePath = g_cfgFile.Get("VirtualMarketFilePath");
	if (_filePath == string())
	{
		cerr << "VirtualMarket::VirutalMarket(): VirtualMarketFilePath not found in config file" << endl;
		exit(1);
	}
	
	//要播放的文件的日期
	_date = g_cfgFile.Get("playFileDate");
	if (_date == string())
	{
		cerr << "VirtualMarket::VirutalMarket(): playFileDate not found in config file" << endl;
		exit(1);
	}

	string needPlay("1");
	//根据配置文件的信息，判断某类数据要不要播放
	if (g_cfgFile.Get("playSnapshot") == needPlay)
	{
		_playSnapshot = true;
		InitSnapshot();
	}
	else
	{
		//析构的时候遇到NULL指针不再执行fclose和delete
		_snapshotFile = NULL;
		_pArraySnapshot = NULL;
	}

	/*if (g_cfgFile.Get("playTickOrder") == needPlay)
	{
		_playTickOrder = true;
		InitTickOrder();
	}
	else
	{
		_tickOrderFile = NULL;
		_pArrayTickOrder = NULL;
	}*/

	/*if (g_cfgFile.Get("playTickExecution") == needPlay)
	{
		_playTickExecution = true;
		InitTickExecution();
	}
	else
	{
		_tickExecutionFile = NULL;
		_pArrayTickExecution = NULL;
	}*/

	if (g_cfgFile.Get("playOrdAndExecu") == needPlay)
	{
		_playOrdAndExecu = true;
		InitOrdAndExecu();
	}
	else
	{
		_ordAndExecuFile = NULL;
		_pArrayOrdAndExecu = NULL;
	}

	if (g_cfgFile.Get("playQuickSnap") == needPlay)
	{
		_playOrderBook = true;
		InitOrderBook();
	}
	else
	{
		_playOrderBook = false;
		_orderBookFiles.clear();
		_ptrsArrayOrderBook.clear();
		_threadNumOrderBook = 0;//也是为了保证不创建播放orderBook的线程
	}

	return;
}

VirtualMarket::~VirtualMarket()
{
	if (_snapshotFile != NULL)
	{
		fclose(_snapshotFile);
		_snapshotFile == NULL;
	}

	/*if (_tickOrderFile != NULL)
	{
		fclose(_tickOrderFile);
		_tickOrderFile == NULL;
	}*/

	/*if (_tickExecutionFile != NULL)
	{
		fclose(_tickExecutionFile);
		_tickExecutionFile == NULL;
	}*/

	if (_ordAndExecuFile != NULL)
	{
		fclose(_ordAndExecuFile);
		_ordAndExecuFile == NULL;
	}

	for (FILE* fp : _orderBookFiles)
	{
		if (fp != NULL)
		{
			fclose(fp);
		}
	}

	if (_pArraySnapshot != NULL)
	{
		delete _pArraySnapshot;
	}

	/*if (_pArrayTickOrder != NULL)
	{
		delete _pArrayTickOrder;
	}*/

	/*if (_pArrayTickExecution != NULL)
	{
		delete _pArrayTickExecution;
	}*/

	if (_pArrayOrdAndExecu != NULL)
	{
		delete _pArrayOrdAndExecu;
	}
	
	//如果数组里没有元素，自然不会进入循环
	for (size_t i = 0; i < _ptrsArrayOrderBook.size(); i++)
	{
		delete _ptrsArrayOrderBook[i];
	}

}

void VirtualMarket::InitSnapshot()
{
	string snapshotFile = _filePath + "/Snapshot" + _date + ".csv";

	cout << "playFileName: " << snapshotFile << endl;

	_snapshotFile = fopen(snapshotFile.c_str(), "r");
	if (_snapshotFile == NULL)//文件不存在
	{
		cerr << "VirtualMarket::VirutalMarket():" << snapshotFile << " not found" << endl;
		exit(1);
	}

	//把文件光标读到第二行，越过表头
	char line[LINE_LENGTH] = { 0 };
	char* ret;

	ret = fgets(line, sizeof(line), _snapshotFile);
	if (ret == NULL)//空文件
	{
		cerr << "snapshot file is empty" << endl;
		exit(1);
	}

	//给数组申请空间
	_pArraySnapshot = new std::array<SnapshotInfo, ARRAY_LENGTH_SNAPSHOT>;

	//初始化要赋值的数组元素下标
	_writeIndexSnapshot = 0;

	return;
}

//void VirtualMarket::InitTickOrder()
//{
//	string tickOrderFile = _filePath + "/TickOrder" + _date + ".csv";
//
//	cout << "playFileName: " << tickOrderFile << endl;
//
//	_tickOrderFile = fopen(tickOrderFile.c_str(), "r");
//	if (_tickOrderFile == NULL)//文件不存在
//	{
//		cerr << tickOrderFile << " not found" << endl;
//		exit(1);
//	}
//
//	//把文件光标读到第二行，越过表头
//	char line[LINE_LENGTH] = { 0 };
//	char* ret;
//
//	ret = fgets(line, sizeof(line), _tickOrderFile);
//	if (ret == NULL)//空文件
//	{
//		cerr << "tickorder file is empty" << endl;
//		//exit(1);
//	}
//
//	_pArrayTickOrder = new std::array<MDTickOrderSt, ARRAY_LENGTH_TICKORDER>;
//
//	_writeIndexTickOrder = 0;
//
//	return;
//}

//void VirtualMarket::InitTickExecution()
//{
//	string tickExecutionFile = _filePath + "/TickExecution" + _date + ".csv";
//
//	cout << "playFileName: " << tickExecutionFile << endl;
//
//	_tickExecutionFile = fopen(tickExecutionFile.c_str(), "r");
//	if (_tickExecutionFile == NULL)
//	{
//		cerr << tickExecutionFile << " not found" << endl;
//		exit(1);
//	}
//
//	//把文件光标读到第二行，越过表头
//	char line[LINE_LENGTH] = { 0 };
//	char* ret;
//
//	ret = fgets(line, sizeof(line), _tickExecutionFile);
//	if (ret == NULL)//空文件
//	{
//		cerr << "tickexecution file is empty" << endl;
//		//exit(1);
//	}
//
//	_pArrayTickExecution = new std::array<MDTickExecutionSt, ARRAY_LENGTH_TICKEXECUTION>;
//
//	_writeIndexTickExecution = 0;
//
//	return;
//}

void VirtualMarket::InitOrdAndExecu()
{
	string fileName = _filePath + "/OrderAndExecution" + _date + ".csv";

	cout << "playFileName: " << fileName << endl;

	_ordAndExecuFile = fopen(fileName.c_str(), "r");
	if (_ordAndExecuFile == NULL)
	{
		cerr << fileName << " not found" << endl;
		exit(1);
	}

	//把文件光标读到第二行，越过表头
	char line[LINE_LENGTH] = { 0 };
	char* ret;

	ret = fgets(line, sizeof(line), _ordAndExecuFile);
	if (ret == NULL)//空文件
	{
		cerr << "OrdAndExecu file is empty" << endl;
		exit(1);
	}

	//开辟空间
	_pArrayOrdAndExecu = new std::vector<OrdAndExeInfo>;
	_pArrayOrdAndExecu->resize(ARRAY_LENGTH_ORDANDEXECU);

	_writeIndexOrdAndExecu = 0;

	return;
}

void VirtualMarket::InitOrderBook()
{
	//委托簿的线程个数
	string threadNum = g_cfgFile.Get("OrderBookThreadNum");
	if (threadNum == string())
	{
		cerr << "VirtualMarket::VirutalMarket(): OrderBookThreadNum not found in config file" << endl;
		exit(1);
	}
	_threadNumOrderBook = stoi(threadNum);

	string orderBookFile;
	int i;
	for (i = 1; i <= _threadNumOrderBook; i++)
	{
		orderBookFile = _filePath + "/QuickSnap" + _date + "_" + std::to_string(i) + ".csv";
		
		cout << "playFileName: " << orderBookFile << endl;

		FILE* fp = fopen(orderBookFile.c_str(), "r");
		if (fp == NULL)
		{
			cerr << orderBookFile << "not found" << endl;
			exit(1);
		}
		//文件指针放入数组
		_orderBookFiles.emplace_back(fp);
	}

	//把文件光标读到第二行，越过表头
	char line[LINE_LENGTH] = { 0 };
	char* ret;

	for (i = 0; i < _threadNumOrderBook; i++)
	{
		ret = fgets(line, sizeof(line), _orderBookFiles[i]);
		if (ret == NULL)//空文件
		{
			cerr << "QuickSnap file is empty" << endl;
			exit(1);
		}
	}

	for (i = 0; i < _threadNumOrderBook; i++)
	{
		auto ptr = new std::array<QuickSnapInfo, ARRAY_LENGTH_QUICKSNAP>;
		_ptrsArrayOrderBook.emplace_back(ptr);
	}

	for (i = 0; i < _threadNumOrderBook; i++)
	{
		_writeIndexesOrderBook.emplace_back(0);
	}

	//扩展orderBook相关数组的长度
	_dataBeginsOrderBook.resize(_threadNumOrderBook);
	_dataCntsOrderBook.resize(_threadNumOrderBook);
	//回调次数数组设置长度
	_callbackCntsOrderBook.resize(_threadNumOrderBook);
	_tempCallbackCntsOrderBook.resize(_threadNumOrderBook);

	return;
}

void VirtualMarket::SetSnapshotCallback(const SnapshotFunc& snapshotCallback)
{
	_onSnapshot = snapshotCallback;
}

void VirtualMarket::SetOrdAndExecuCallback(const OrderAndExecuFunc& ordAndExeCallback)
{
	_onOrdAndExecu = ordAndExeCallback;
}

//void VirtualMarket::SetOrderBookCallback(const OrderBookFunc& orderBookCallback)
//{
//	_onMDOrderBook = orderBookCallback;
//}

void VirtualMarket::SetQuickSnapCallback(const QuickSnapFunc& func)
{
	_onQuickSnap = func;
}

void VirtualMarket::Play()
{
	_runThreadNum = 0;
	_waitThreadNum = 0;
	vector<std::thread> threadVec;

	//创建提取数据触发回调的线程
	if (_playSnapshot)
	{
		threadVec.emplace_back(std::thread(&VirtualMarket::ThreadFuncSnapshot, this));
		_runThreadNum++;
	}
	
	/*if (_playTickOrder)
	{
		threadVec.emplace_back(std::thread(&VirtualMarket::ThreadFuncTickOrder, this));
		_runThreadNum++;
	}*/

	/*if (_playTickExecution)
	{
		threadVec.emplace_back(std::thread(&VirtualMarket::ThreadFuncTickExecution, this));
		_runThreadNum++;
	}*/

	if (_playOrdAndExecu)
	{
		threadVec.emplace_back(std::thread(&VirtualMarket::ThreadFuncOrdAndExe, this));
		_runThreadNum++;
	}

	if (_playOrderBook)
	{
		for (int i = 0; i < _threadNumOrderBook; i++)
		{
			threadVec.emplace_back(std::thread(&VirtualMarket::ThreadFuncOrderBook, this, i));
			_runThreadNum++;
		}
	}
	

	printf("PlayThreadNum = %u\n", _runThreadNum);

	/*一直循环直到所有线程都读完第一次数据，开始等待*/
	while (1)
	{
		if (_runThreadNum != _waitThreadNum)
		{
			//printf("continue\n");
			continue;
		}

		//如果锁没有被释放，说明子线程里某些原子操作还未完成
		if (_cvMutex.try_lock() == false)
		{
			continue;
		}
		else
		{
			_cvMutex.unlock();
		}

		/*所有线程都在等待条件变量了*/
		//cout << "all waiting" << endl;

		//更新最小回调次序
		_minCallbackCnt =  MinCallbackCnt();

		//唤醒所有线程
		_condVal.notify_all();

		//只需要唤醒一次
		break;

		//sleep(1);
	}

	//等待所有线程结束
	for (size_t i = 0; i < threadVec.size(); i++)
	{
		threadVec[i].join();
		printf("thread joined\n");
	}

	printf("all thread over\n");

	return;
}

//提取snapshot数据的线程
void VirtualMarket::ThreadFuncSnapshot()
{
	//要传递数据的起始位置和数据个数
	_dataBeginSnapshot = 0;
	_dataCntSnapshot = 0;

	bool ret;
	uint32_t cbCnt;
	//读取第一批snapshot数据
	//第一行数据
	ret = ReadOneSnapshot(_pArraySnapshot->at(_writeIndexSnapshot), _callbackCntSnapshot);
	//没读到，以后不参与循环
	if (ret == false)
	{
		cerr << "snapshot file has head but has no data" << endl;
		_callbackCntSnapshot = 0;
	}
	else
	{
		++_writeIndexSnapshot;
		++_dataCntSnapshot;
		//读文件，一直到callbackCnt不同了，或者没数据了
		while (1)
		{
			//如果读到数据，放入数组
			ret = ReadOneSnapshot(_pArraySnapshot->at(_writeIndexSnapshot), cbCnt);
			//没数据了
			if (ret == false)
			{
				_tempCallbackCntSnapshot = 0;
				break;
			}
			++_writeIndexSnapshot;

			//读到数据属于下次回调的数据
			if (cbCnt != _callbackCntSnapshot)
			{
				//缓存记录下来
				_tempCallbackCntSnapshot = cbCnt;
				break;
			}

			//读到数据且是同一批回调的数据
			++_dataCntSnapshot;
		}
	}

	//等待条件变量，等待被唤醒
	std::unique_lock<std::mutex> lock(_cvMutex);
	_waitThreadNum++;	//增加等待线程个数
	_condVal.wait(lock);
	//被唤醒了
	_waitThreadNum--;	//减少等待线程个数
	lock.unlock();//手动解锁
	

	//如果_callbackCnt被置为0，代表没有数据了，结束线程
	while (_callbackCntSnapshot != 0)
	{
		//检查自己是不是最小的回调次序，是就触发回调函数
		if (_callbackCntSnapshot == _minCallbackCnt)
		{
			//printf("_callbackCntSnapshot = %u\n", _callbackCntSnapshot);

			_onSnapshot(&(_pArraySnapshot->at(_dataBeginSnapshot)), _dataCntSnapshot);
			
			try
			{
				//更新下一组数据和回调次数
				UpdateSnapshotData();
			}
			catch (std::out_of_range& out)
			{
				cerr << out.what() << endl;
				break;
			}
		}
		//没有轮到此线程触发回调，继续下轮等待
	}

	printf("threadSnapshot end\n");
	
	_mutexThreadNum.lock();
	_runThreadNum--;
	_mutexThreadNum.unlock();

	return;
}

//void VirtualMarket::ThreadFuncTickOrder()
//{
//	//要提取的数据在数组中的起始位置和数据个数
//	_dataBeginTickOrder = 0;
//	_dataCntTickOrder = 0;
//
//	bool ret;
//	uint32_t cbCnt;
//	//读取第一批tickOrder数据
//	//第一行数据
//	ret = ReadOneTickOrder(_pArrayTickOrder->at(_writeIndexTickOrder), _callbackCntTickOrder);
//	//没读到，以后不参与循环
//	if (ret == false)
//	{
//		cerr << "tickorder file has head but has no data" << endl;
//		_callbackCntTickOrder = 0;
//	}
//	else
//	{
//		++_writeIndexTickOrder;
//		++_dataCntTickOrder;
//		//读文件，一直到callbackCnt不同了，或者没数据了
//		while (1)
//		{
//			//如果读到数据，放入数组
//			ret = ReadOneTickOrder(_pArrayTickOrder->at(_writeIndexTickOrder), cbCnt);
//			//没数据了
//			if (ret == false)
//			{
//				_tempCallbackCntTickOrder = 0;
//				break;
//			}
//			++_writeIndexTickOrder;
//
//			//读到数据属于下次回调的数据
//			if (cbCnt != _callbackCntTickOrder)
//			{
//				//缓存记录下来
//				_tempCallbackCntTickOrder = cbCnt;
//				break;
//			}
//
//			//读到数据且是同一批回调的数据
//			++_dataCntTickOrder;
//		}
//	}
//
//	//等待条件变量，等待被唤醒
//	std::unique_lock<std::mutex> lock(_cvMutex);
//	_waitThreadNum++;	//增加等待线程个数
//	_condVal.wait(lock);
//
//	_waitThreadNum--;	//减少等待线程个数
//	lock.unlock();//手动解锁
//
//	//如果_callbackCnt被置为0，代表没有数据了，结束线程
//	while (_callbackCntTickOrder != 0)
//	{
//		//检查自己是不是最小的回调次序，是就触发回调函数
//		if (_callbackCntTickOrder == _minCallbackCnt)
//		{
//			//printf("_callbackCntTickOrder = %u\n", _callbackCntTickOrder);
//
//			//调用回调函数
//			_onMDTickOrder(&(_pArrayTickOrder->at(_dataBeginTickOrder)), _dataCntTickOrder);
//
//			try
//			{
//				//更新下一组数据和回调次数
//				UpdateTickOrderData();
//			}
//			catch (std::out_of_range& out)
//			{
//				cerr << out.what() << endl;
//				break;
//			}
//		}
//		//如果没有触发，继续下轮等待
//	}
//
//	printf("threadTickOrder end\n");
//
//	_mutexThreadNum.lock();
//	_runThreadNum--;
//	_mutexThreadNum.unlock();
//}

//提取tickExecution数据的线程
//void VirtualMarket::ThreadFuncTickExecution()
//{
//	_dataBeginTickExecution = 0;
//	_dataCntTickExecution = 0;
//
//	bool ret;
//	uint32_t cbCnt;
//	//读取第一批tickExecution数据
//	//第一行数据
//	ret = ReadOneTickExecution(_pArrayTickExecution->at(_writeIndexTickExecution), _callbackCntTickExecution);
//	//没读到，以后不参与循环
//	if (ret == false)
//	{
//		cerr << "tickexecution file has head but has no data" << endl;
//		_callbackCntTickExecution = 0;
//	}
//	else
//	{
//		++_writeIndexTickExecution;
//		++_dataCntTickExecution;
//		//读文件，一直到callbackCnt不同了，或者没数据了
//		while (1)
//		{
//			//如果读到数据，放入数组
//			ret = ReadOneTickExecution(_pArrayTickExecution->at(_writeIndexTickExecution), cbCnt);
//			//没数据了
//			if (ret == false)
//			{
//				_tempCallbackCntTickExecution = 0;
//				break;
//			}
//			++_writeIndexTickExecution;
//
//			//读到数据属于下次回调的数据
//			if (cbCnt != _callbackCntTickExecution)
//			{
//				//缓存记录下来
//				_tempCallbackCntTickExecution = cbCnt;
//				break;
//			}
//
//			++_dataCntTickExecution;
//		}
//	}
//
//	//等待条件变量，等待被唤醒
//	std::unique_lock<std::mutex> lock(_cvMutex);
//	_waitThreadNum++;	//增加等待线程个数
//	_condVal.wait(lock);
//
//	_waitThreadNum--;	//减少等待线程个数
//	lock.unlock();//手动解锁
//
//	//如果_callbackCnt被置为0，代表没有数据了，结束线程
//	while (_callbackCntTickExecution != 0)
//	{
//		//检查自己是不是最小的回调次序，是就触发回调函数
//		if (_callbackCntTickExecution == _minCallbackCnt)
//		{
//			//printf("_callbackCntTickExecution = %u\n", _callbackCntTickExecution);
//
//			_onMDTickExecution(&(_pArrayTickExecution->at(_dataBeginTickExecution)), _dataCntTickExecution);
//
//			try
//			{
//				UpdateTickExecutionData();
//			}
//			catch (std::out_of_range& out)
//			{
//				cerr << out.what() << endl;
//				break;
//			}
//		}
//		//没有轮到此线程触发回调，继续下轮等待
//	}
//
//	printf("threadTickExecution end\n");
//
//	_mutexThreadNum.lock();
//	_runThreadNum--;
//	_mutexThreadNum.unlock();
//
//	return;
//}

void VirtualMarket::ThreadFuncOrdAndExe()
{
	_dataBeginOrdAndExe = 0;
	_dataCntOrdAndExe = 0;

	bool ret;
	uint32_t cbCnt;
	//读取第一批tickExecution数据
	//第一行数据
	ret = ReadOneOrdAndExe(_pArrayOrdAndExecu->at(_writeIndexOrdAndExecu), _callbackCntOrdAndExe);
	//没读到，以后不参与循环
	if (ret == false)
	{
		cerr << "OrdAndExecu file has head but has no data" << endl;
		_callbackCntOrdAndExe = 0;
	}
	else
	{
		++_writeIndexOrdAndExecu;
		++_dataCntOrdAndExe;
		//读文件，一直到callbackCnt不同了，或者没数据了
		while (1)
		{
			//如果读到数据，放入数组
			ret = ReadOneOrdAndExe(_pArrayOrdAndExecu->at(_writeIndexOrdAndExecu), cbCnt);
			//没数据了
			if (ret == false)
			{
				_tempCallbackCntOrdAndExe = 0;
				break;
			}
			++_writeIndexOrdAndExecu;

			//读到数据属于下次回调的数据
			if (cbCnt != _callbackCntOrdAndExe)
			{
				//缓存记录下来
				_tempCallbackCntOrdAndExe = cbCnt;
				break;
			}

			++_dataCntOrdAndExe;
		}
	}

	//等待条件变量，等待被唤醒
	std::unique_lock<std::mutex> lock(_cvMutex);
	_waitThreadNum++;	//增加等待线程个数
	_condVal.wait(lock);

	_waitThreadNum--;	//减少等待线程个数
	lock.unlock();//手动解锁

	//如果_callbackCnt被置为0，代表没有数据了，结束线程
	while (_callbackCntOrdAndExe != 0)
	{
		//检查自己是不是最小的回调次序，是就触发回调函数
		if (_callbackCntOrdAndExe == _minCallbackCnt)
		{
			//printf("_callbackCntTickExecution = %u\n", _callbackCntTickExecution);

			_onOrdAndExecu(&(_pArrayOrdAndExecu->at(_dataBeginOrdAndExe)), _dataCntOrdAndExe);

			try
			{
				//UpdateTickExecutionData();
				UpdateOrdAndExeData();
			}
			catch (std::out_of_range& out)
			{
				cerr << out.what() << endl;
				break;
			}
		}
		//没有轮到此线程触发回调，继续下轮等待
	}

	printf("threadOrdAndExe end\n");

	_mutexThreadNum.lock();
	_runThreadNum--;
	_mutexThreadNum.unlock();

	return;
}

//提取orderBook数据的线程，有多个，需要区分,传入的是orderBook文件指针数组(_orderBookFiles)的下标
void VirtualMarket::ThreadFuncOrderBook(int32_t idx)
{
	_dataBeginsOrderBook[idx] = 0;
	_dataCntsOrderBook[idx] = 0;

	bool ret;
	uint32_t cbCnt;
	//读取第一批OrderBook数据
	//第一行数据
	ret = ReadOneOrderBook(_orderBookFiles[idx], _ptrsArrayOrderBook[idx]->at(_writeIndexesOrderBook[idx]), _callbackCntsOrderBook[idx]);
	//没读到，以后不参与循环
	if (ret == false)
	{
		cerr << "QuickSnap file has head but has no data" << endl;
		_callbackCntsOrderBook[idx] = 0;
	}
	else
	{
		++_writeIndexesOrderBook[idx];
		++_dataCntsOrderBook[idx];
		//读文件，一直到callbackCnt不同了，或者没数据了
		while (1)
		{
			//如果读到数据，放入数组
			ret = ReadOneOrderBook(_orderBookFiles[idx], _ptrsArrayOrderBook[idx]->at(_writeIndexesOrderBook[idx]), cbCnt);
			//没数据了
			if (ret == false)
			{
				_tempCallbackCntsOrderBook[idx] = 0;
				break;
			}
			++_writeIndexesOrderBook[idx];

			//读到数据属于下次回调的数据
			if (cbCnt != _callbackCntsOrderBook[idx])
			{
				//缓存记录下来
				_tempCallbackCntsOrderBook[idx] = cbCnt;
				break;
			}

			++_dataCntsOrderBook[idx];
		}
	}

	//等待条件变量，等待被唤醒
	std::unique_lock<std::mutex> lock(_cvMutex);
	_waitThreadNum++;	//增加等待线程个数
	_condVal.wait(lock);

	_waitThreadNum--;	//减少等待线程个数
	lock.unlock();//手动解锁

	//如果_callbackCnt被置为0，代表没有数据了，结束线程
	while (_callbackCntsOrderBook[idx] != 0)
	{
		//检查自己是不是最小的回调次序，是就触发回调函数
		if (_callbackCntsOrderBook[idx] == _minCallbackCnt)
		{
			//printf("_callbackCntsOrderBook[%d] = %u\n", idx, _callbackCntsOrderBook[idx]);

			/*vector<MDOrderBookSt> vec(_ptrsArrayOrderBook[idx]->begin() + _dataBeginsOrderBook[idx],
				_ptrsArrayOrderBook[idx]->begin() + _dataBeginsOrderBook[idx] + _dataCntsOrderBook[idx]);

			_onMDOrderBook(vec);*/

			_onQuickSnap(&(_ptrsArrayOrderBook[idx]->at(_dataBeginsOrderBook[idx])), _dataCntsOrderBook[idx]);

			try
			{
				//更新下一组数据和回调次数
				UpdateOrderBookData(idx);
			}
			catch (std::out_of_range& out)
			{
				cerr << "quickSnap error:" << out.what() << endl;
				break;
			}
		}
		//没有轮到此线程触发回调，继续下轮等待
	}

	printf("threadOrderBook %d end\n", idx);

	_mutexThreadNum.lock();
	_runThreadNum--;
	_mutexThreadNum.unlock();

	return;
}


//void VirtualMarket::FirstGetData()
//{
//
//	//要传递数据的起始位置和数据个数
//	_dataBeginSnapshot = 0;
//	_dataCntSnapshot = 0;
//	_dataBeginTickOrder = 0;
//	_dataCntTickOrder = 0;
//	_dataBeginTickExecution = 0;
//	_dataCntTickExecution = 0;
//	//_dataBeginOrderBook = 0;
//	//_dataCntorderBook = 0;
//	int i;
//	for (i = 0; i < _threadNumOrderBook; i++)
//	{
//		_dataBeginsOrderBook.emplace_back(0);
//		_dataCntsOrderBook.emplace_back(0);
//	}
//
//	bool ret;
//	uint32_t cbCnt;
//	//读取第一批snapshot数据
//	//第一行数据
//	ret = ReadOneSnapshot(_pArraySnapshot->at(_writeIndexSnapshot), _callbackCntSnapshot);
//	//没读到，以后不参与play()里的循环
//	if (ret == false)
//	{
//		cerr << "snapshot file has head but has no data" << endl;
//		_callbackCntSnapshot = 0;
//
//	}
//	else
//	{
//		++_writeIndexSnapshot;
//		++_dataCntSnapshot;
//		//读文件，一直到callbackCnt不同了，或者没数据了
//		while (1)
//		{
//			//如果读到数据，放入数组
//			ret = ReadOneSnapshot(_pArraySnapshot->at(_writeIndexSnapshot), cbCnt);
//			//没数据了
//			if (ret == false)
//			{
//				_tempCallbackCntSnapshot = 0;
//				break;
//			}
//			++_writeIndexSnapshot;
//
//			//读到数据属于下次回调的数据
//			if (cbCnt != _callbackCntSnapshot)
//			{
//				//缓存记录下来
//				_tempCallbackCntSnapshot = cbCnt;
//				break;
//			}
//
//			//读到数据且是同一批回调的数据
//			++_dataCntSnapshot;
//		}
//	}
//
//	//传递时
//
//	//读取第一批tickOrder数据
//	//第一行数据
//	ret = ReadOneTickOrder(_pArrayTickOrder->at(_writeIndexTickOrder), _callbackCntTickOrder);
//	//没读到，以后不参与play()里的循环
//	if (ret == false)
//	{
//		cerr << "tickorder file has head but has no data" << endl;
//		_callbackCntTickOrder = 0;
//	}
//	else
//	{
//		++_writeIndexTickOrder;
//		++_dataCntTickOrder;
//		//读文件，一直到callbackCnt不同了，或者没数据了
//		while (1)
//		{
//			//如果读到数据，放入数组
//			ret = ReadOneTickOrder(_pArrayTickOrder->at(_writeIndexTickOrder), cbCnt);
//			//没数据了
//			if (ret == false)
//			{
//				_tempCallbackCntTickOrder = 0;
//				break;
//			}
//			++_writeIndexTickOrder;
//
//			//读到数据属于下次回调的数据
//			if (cbCnt != _callbackCntTickOrder)
//			{
//				//缓存记录下来
//				_tempCallbackCntTickOrder = cbCnt;
//				break;
//			}
//
//			++_dataCntTickOrder;
//		}
//	}
//
//	//读取第一批tickExecution数据
//	//第一行数据
//	ret = ReadOneTickExecution(_pArrayTickExecution->at(_writeIndexTickExecution), _callbackCntTickExecution);
//	//没读到，以后不参与play()里的循环
//	if (ret == false)
//	{
//		cerr << "tickexecution file has head but has no data" << endl;
//		_callbackCntTickExecution = 0;
//	}
//	else
//	{
//		++_writeIndexTickExecution;
//		++_dataCntTickExecution;
//		//读文件，一直到callbackCnt不同了，或者没数据了
//		while (1)
//		{
//			//如果读到数据，放入数组
//			ret = ReadOneTickExecution(_pArrayTickExecution->at(_writeIndexTickExecution), cbCnt);
//			//没数据了
//			if (ret == false)
//			{
//				_tempCallbackCntTickExecution = 0;
//				break;
//			}
//			++_writeIndexTickExecution;
//
//			//读到数据属于下次回调的数据
//			if (cbCnt != _callbackCntTickExecution)
//			{
//				//缓存记录下来
//				_tempCallbackCntTickExecution = cbCnt;
//				break;
//			}
//
//			++_dataCntTickExecution;
//		}
//	}
//
//	//回调次数数组设置长度
//	_callbackCntsOrderBook.resize(_threadNumOrderBook);
//	_tempCallbackCntsOrderBook.resize(_threadNumOrderBook);
//
//	for (i = 0; i < _threadNumOrderBook; i++)
//	{
//		//读取第一批OrderBook数据
//		//第一行数据
//		ret = ReadOneOrderBook(_orderBookFiles[i], _ptrsArrayOrderBook[i]->at(_writeIndexesOrderBook[i]), _callbackCntsOrderBook[i]);
//		//没读到，以后不参与play()里的循环
//		if (ret == false)
//		{
//			cerr << "orderbook file has head but has no data" << endl;
//			_callbackCntsOrderBook[i] = 0;
//		}
//		else
//		{
//			++_writeIndexesOrderBook[i];
//			++_dataCntsOrderBook[i];
//			//读文件，一直到callbackCnt不同了，或者没数据了
//			while (1)
//			{
//				//如果读到数据，放入数组
//				ret = ReadOneOrderBook(_orderBookFiles[i], _ptrsArrayOrderBook[i]->at(_writeIndexesOrderBook[i]), cbCnt);
//				//没数据了
//				if (ret == false)
//				{
//					_tempCallbackCntsOrderBook[i] = 0;
//					break;
//				}
//				++_writeIndexesOrderBook[i];
//
//				//读到数据属于下次回调的数据
//				if (cbCnt != _callbackCntsOrderBook[i])
//				{
//					//缓存记录下来
//					_tempCallbackCntsOrderBook[i] = cbCnt;
//					break;
//				}
//
//				++_dataCntsOrderBook[i];
//			}
//		}
//	}
//
//	return;
//}

void VirtualMarket::UpdateSnapshotData()
{

	//_tempCallbackCnt若为0，代表上次就读到了文件尾，没数据了
	if (_tempCallbackCntSnapshot == 0)
	{
		_callbackCntSnapshot = 0;//代表不再参与play()里下次循环

		/*更新最小回调次数，让其他线程接管播放，不更新的话,minCnt还是当前线程本次循环时的回调次数
		  其他线程永远无法匹配到最小回调次数*/
		_minCallbackCnt = MinCallbackCnt();

		printf("_callbackCntSnapshot = 0, _writeIndexSnapshot = %lu;\n", _writeIndexSnapshot);

		return;
	}

	//记录回调次序
	_callbackCntSnapshot = _tempCallbackCntSnapshot;

	//更新最小回调次数
	_minCallbackCnt = MinCallbackCnt();

	//本次数据的起始位置，在要写的位置前一个
	_dataBeginSnapshot = _writeIndexSnapshot - 1;
	//初始化数据个数
	_dataCntSnapshot = 1;

	uint32_t cbCnt;
	bool ret;
	//读文件，一直到callbackCnt不同了，或者没数据了
	while (1)
	{
		//如果读到数据，放入数组
		ret = ReadOneSnapshot(_pArraySnapshot->at(_writeIndexSnapshot), cbCnt);
		//没读到
		if (ret == false)
		{
			_tempCallbackCntSnapshot = 0;

			//printf("_tempCallbackCntSnapshot = 0;\n");
			//printf("_callbackCntSnapshot = %u, _dataCntSnapshot = %u\n", _callbackCntSnapshot, _dataCntSnapshot);


			break;
		}
		++_writeIndexSnapshot;

		//读到了，但回调次数变了
		if (cbCnt != _callbackCntSnapshot)
		{
			//缓存记录下来
			_tempCallbackCntSnapshot = cbCnt;
			break;
		}

		//如果读到的是同一批次数据，更新数据个数
		++_dataCntSnapshot;
	}

	return;
}

bool VirtualMarket::ReadOneSnapshot(SnapshotInfo& snapshot, uint32_t& cnt)
{
	//到了文件尾
	if (feof(_snapshotFile))
	{
		return false;
	}

	//uint32_t levelLen = amd::ama::ConstField::kPositionLevelLen;
	char sep = ',';

	int32_t ret = 0;
	//文件格式化读入
	ret = fscanf(_snapshotFile, "%u,", &snapshot._sc);
	if (ret == -1)
	{
		return false;
	}

	fscanf(_snapshotFile, "%u,%u,%u,%u,",
		&snapshot._tm,
		&snapshot._pxIopv,
		&snapshot._pxHigh,
		&snapshot._pxAvgAsk
	);

	//申卖价
	for (int i = 9; i >= 0; i--)
	{
		fscanf(_snapshotFile, "%u,", &snapshot._pxAsk[i]);
	}

	//最新成交价
	fscanf(_snapshotFile, "%u,", &snapshot._pxLast);

	//申买价
	for (int i = 0; i < 10; i++)
	{
		fscanf(_snapshotFile, "%u,", &snapshot._pxBid[i]);
	}

	fscanf(_snapshotFile, "%u,%u,%lu,",
		&snapshot._pxAvgBid,
		&snapshot._pxLow,
		&snapshot._qtyTotAsk
	);

	//申卖量
	for (int i = 9; i >= 0; i--)
	{
		fscanf(_snapshotFile, "%lu,", &snapshot._qtyAsk[i]);
	}

	//成交总量
	fscanf(_snapshotFile, "%lu,", &snapshot._qtyTot);

	//申买量
	for (int i = 0; i < 10; i++)
	{
		fscanf(_snapshotFile, "%lu,", &snapshot._qtyBid[i]);
	}

	fscanf(_snapshotFile, "%lu,%u,%lf,",
		&snapshot._qtyTotBid,
		&snapshot._ctTot,
		&snapshot._cnyTot);

	FGet_str(_snapshotFile, snapshot._sta, sep);

	fscanf(_snapshotFile, "%u,", &snapshot._dt);

	//回调次数
	fscanf(_snapshotFile, "%u", &cnt);

	//把本行剩下的读完
	char str[100] = { 0 };
	fgets(str, sizeof(str), _snapshotFile);

	return true;
}

//void VirtualMarket::UpdateTickOrderData()
//{
//	//_tempCallbackCnt若为0，代表上次就读到了文件尾，没数据了
//	if (_tempCallbackCntTickOrder == 0)
//	{
//		_callbackCntTickOrder = 0;
//
//		//更新最小回调次数，让其他线程接管播放
//		_minCallbackCnt = MinCallbackCnt();
//
//		//printf("_callbackCntTickOrder = 0;\n");
//
//		return;
//	}
//
//	/*记录回调次序；*/
//	_callbackCntTickOrder = _tempCallbackCntTickOrder;
//
//	//更新最小回调次数
//	_minCallbackCnt = MinCallbackCnt();
//
//	/*本次数据的起始位置，在要写的位置前一个，因为上次更新已经写进一个了
//	  数据个数初始化为1*/
//	_dataBeginTickOrder = _writeIndexTickOrder - 1;
//	_dataCntTickOrder = 1;
//
//	uint32_t cbCnt;
//	bool ret;
//	//读文件里数据
//	while (1)
//	{
//		//如果读到数据，放入数组
//		ret = ReadOneTickOrder(_pArrayTickOrder->at(_writeIndexTickOrder), cbCnt);
//		//没数据了
//		if (ret == false)
//		{
//			_tempCallbackCntTickOrder = 0;
//
//			//printf("_tempCallbackCntTickOrder = 0;\n");
//			//printf("_callbackCntTickOrder = %u, _dataCntTickOrder = %u\n", _callbackCntTickOrder, _dataCntTickOrder);
//
//			break;
//		}
//		++_writeIndexTickOrder;
//
//		if (cbCnt != _callbackCntTickOrder)
//		{
//			_tempCallbackCntTickOrder = cbCnt;
//			break;
//		}
//
//		//如果读到的是同一批次数据，更新数据个数
//		_dataCntTickOrder++;
//	}
//
//	return;
//}

//bool VirtualMarket::ReadOneTickOrder(MDTickOrderSt& tickOrder, uint32_t& cnt)
//{
//	//到了文件尾
//	if (feof(_tickOrderFile))
//	{
//		return false;
//	}
//
//	char sep = ',';
//	int32_t ret = 0;
//
//	ret = fscanf(_tickOrderFile, "%d,", &tickOrder.market_type);
//	if (ret == -1)
//	{
//		return false;
//	}
//	FGet_str(_tickOrderFile, tickOrder.security_code, sep);
//
//	fscanf(_tickOrderFile, "%d,%ld,%ld,%ld,%ld,%hhu,%hhu,",
//		&tickOrder.channel_no,
//		&tickOrder.appl_seq_num,
//		&tickOrder.order_time,
//		&tickOrder.order_price,
//		&tickOrder.order_volume,
//		&tickOrder.side,
//		&tickOrder.order_type);
//
//	FGet_str(_tickOrderFile, tickOrder.md_stream_id, sep);
//
//	fscanf(_tickOrderFile, "%ld,%ld,%hhu,%ld,",
//		&tickOrder.orig_order_no,
//		&tickOrder.biz_index,
//		&tickOrder.variety_category,
//		&tickOrder.traded_order_volume);
//
//	//回调次数
//	fscanf(_tickOrderFile, "%u,", &cnt);
//
//	//把本行剩下的读完
//	char str[100] = { 0 };
//	fgets(str, sizeof(str), _tickOrderFile);
//
//	return true;
//}



//void VirtualMarket::UpdateTickExecutionData()
//{
//	//_tempCallbackCnt若为0，代表上次就读到了文件尾，没数据了
//	if (_tempCallbackCntTickExecution == 0)
//	{
//		_callbackCntTickExecution = 0;
//
//		//更新最小回调次数，让其他线程接管播放
//		_minCallbackCnt = MinCallbackCnt();
//
//		//printf("_callbackCntTickExecution = 0;\n");
//
//		return;
//	}
//
//	/*记录回调次序；*/
//	_callbackCntTickExecution = _tempCallbackCntTickExecution;
//
//	//更新最小回调次数
//	_minCallbackCnt = MinCallbackCnt();
//
//	//本次数据的起始位置，在要写的位置前一个，因为上次更新已经写进一个了
//	//数据个数初始化为1
//	_dataBeginTickExecution = _writeIndexTickExecution - 1;
//	_dataCntTickExecution = 1;
//
//	uint32_t cbCnt;
//	bool ret;
//	while (1)
//	{
//		ret = ReadOneTickExecution(_pArrayTickExecution->at(_writeIndexTickExecution), cbCnt);
//		if (ret == false)
//		{
//			_tempCallbackCntTickExecution = 0;
//
//			//printf("_tempCallbackCntTickExecution = 0;\n");
//			//printf("_callbackCntTickExecution = %u, _dataCntTickExecution = %u\n", _callbackCntTickExecution, _dataCntTickExecution);
//
//			break;
//		}
//		_writeIndexTickExecution++;
//
//		if (cbCnt != _callbackCntTickExecution)
//		{
//			_tempCallbackCntTickExecution = cbCnt;
//			break;
//		}
//
//		_dataCntTickExecution++;
//	}
//
//	return;
//}

void VirtualMarket::UpdateOrdAndExeData()
{
	//_tempCallbackCnt若为0，代表上次就读到了文件尾，没数据了
	if (_tempCallbackCntOrdAndExe == 0)
	{
		_callbackCntOrdAndExe = 0;

		//更新最小回调次数，让其他线程接管播放
		_minCallbackCnt = MinCallbackCnt();

		printf("_callbackCntOrderAndExe = 0;_writeIndexOrdAndExecu = %lu\n", _writeIndexOrdAndExecu);

		return;
	}

	/*记录回调次序；*/
	_callbackCntOrdAndExe = _tempCallbackCntOrdAndExe;

	//更新最小回调次数
	_minCallbackCnt = MinCallbackCnt();

	//本次数据的起始位置，在要写的位置前一个，因为上次更新已经写进一个了
	//数据个数初始化为1
	_dataBeginOrdAndExe = _writeIndexOrdAndExecu - 1;
	_dataCntOrdAndExe = 1;

	uint32_t cbCnt;
	bool ret;
	while (1)
	{
		//判断越界
		if (_writeIndexOrdAndExecu >= _pArrayOrdAndExecu->size())
		{
			printf("ARRAY_LENGTH_ORDANDEXECU is not enough, resize again\n");
			_pArrayOrdAndExecu->resize(2 * _pArrayOrdAndExecu->size());
		}

		ret = ReadOneOrdAndExe(_pArrayOrdAndExecu->at(_writeIndexOrdAndExecu), cbCnt);
		if (ret == false)
		{
			_tempCallbackCntOrdAndExe = 0;

			//printf("_tempCallbackCntTickExecution = 0;\n");
			//printf("_callbackCntTickExecution = %u, _dataCntTickExecution = %u\n", _callbackCntTickExecution, _dataCntTickExecution);

			break;
		}
		_writeIndexOrdAndExecu++;

		if (cbCnt != _callbackCntOrdAndExe)
		{
			_tempCallbackCntOrdAndExe = cbCnt;
			break;
		}

		_dataCntOrdAndExe++;
	}

	return;
}

//bool VirtualMarket::ReadOneTickExecution(MDTickExecutionSt& tickExecu, uint32_t& cnt)
//{
//	//到了文件尾
//	if (feof(_tickExecutionFile))
//	{
//		return false;
//	}
//
//	char sep = ',';
//	int32_t ret = 0;
//
//	ret = fscanf(_tickExecutionFile, "%d,", &tickExecu.market_type);
//	if (ret == -1)
//	{
//		return false;
//	}
//	FGet_str(_tickExecutionFile, tickExecu.security_code, sep);
//
//	fscanf(_tickExecutionFile, "%ld,%d,%ld,%ld,%ld,%ld,%ld,%ld,%hhu,%hhu,",
//		&tickExecu.exec_time,
//		&tickExecu.channel_no,
//		&tickExecu.appl_seq_num,
//		&tickExecu.exec_price,
//		&tickExecu.exec_volume,
//		&tickExecu.value_trade,
//		&tickExecu.bid_appl_seq_num,
//		&tickExecu.offer_appl_seq_num,
//		&tickExecu.side,
//		&tickExecu.exec_type
//	);
//
//	FGet_str(_tickExecutionFile, tickExecu.md_stream_id, sep);
//
//	fscanf(_tickExecutionFile, "%ld,%hhu,", &tickExecu.biz_index, &tickExecu.variety_category);
//
//	//回调次数
//	fscanf(_tickExecutionFile, "%u,", &cnt);
//
//	//把本行剩下的读完
//	char str[100] = { 0 };
//	fgets(str, sizeof(str), _tickExecutionFile);
//
//	return true;
//}

bool VirtualMarket::ReadOneOrdAndExe(OrdAndExeInfo& info, uint32_t& cnt)
{
	//到了文件尾
	if (feof(_ordAndExecuFile))
	{
		return false;
	}

	int32_t ret = 0;

	ret = fscanf(_ordAndExecuFile, "%u,", &info._sc);
	if (ret == -1)
	{
		return false;
	}
	//FGet_str(_tickExecutionFile, tickExecu.security_code, sep);

	fscanf(_ordAndExecuFile, "%u,%u,%u,%hu,%u,%u,%u,%c,%c,%c,",
		&info._tm,
		&info._px,
		&info._qty,
		&info._ch,
		&info._id,
		&info._BTag,
		&info._STag,
		&info._BSFlag,
		&info._ADFlag,
		&info._MLFlag
	);

	//回调次数
	fscanf(_ordAndExecuFile, "%u", &cnt);

	//把本行剩下的读完
	char str[100] = { 0 };
	fgets(str, sizeof(str), _ordAndExecuFile);

	return true;
}

void VirtualMarket::UpdateOrderBookData(size_t idx)
{
	if (_tempCallbackCntsOrderBook[idx] == 0)
	{
		_callbackCntsOrderBook[idx] = 0;

		//更新最小回调次数，让其他线程接管播放
		_minCallbackCnt = MinCallbackCnt();

		printf("_callbackCntOrderBook = 0, _writeIndexesOrderBook[%lu] = %lu, data num = %lu;\n", idx, _writeIndexesOrderBook[idx], _writeIndexesOrderBook[idx]);

		return;
	}

	/*记录回调次序；
	本次数据的起始位置，在要写的位置前一个，因为上次更新已经写进一个了
	数据个数初始化为1*/
	_callbackCntsOrderBook[idx] = _tempCallbackCntsOrderBook[idx];

	//更新最小回调次数
	_minCallbackCnt = MinCallbackCnt();

	_dataBeginsOrderBook[idx] = _writeIndexesOrderBook[idx] - 1;
	_dataCntsOrderBook[idx] = 1;

	uint32_t cbCnt;
	bool ret;
	//从文件读数据
	while (1)
	{
		ret = ReadOneOrderBook(_orderBookFiles[idx], _ptrsArrayOrderBook[idx]->at(_writeIndexesOrderBook[idx]), cbCnt);
		//读不到数据
		if (ret == false)
		{
			_tempCallbackCntsOrderBook[idx] = 0;

			//printf("_tempCallbackCntOrderBook = 0;\n");
			//printf("_callbackCntOrderBook = %u, _dataCntorderBook = %u\n", _callbackCntOrderBook, _dataCntorderBook);

			break;
		}
		_writeIndexesOrderBook[idx]++;

		//读到了下次回调的数据
		if (cbCnt != _callbackCntsOrderBook[idx])
		{
			_tempCallbackCntsOrderBook[idx] = cbCnt;
			break;
		}

		//读到同一批数据
		_dataCntsOrderBook[idx]++;
	}

	return;
}

//bool VirtualMarket::ReadOneOrderBook(FILE* _orderBookFile, MDOrderBookSt& orderBook, uint32_t& cnt)
//{
//	//到了文件尾
//	if (feof(_orderBookFile))
//	{
//		return false;
//	}
//
//	int32_t i, j;
//	amd::ama::MDOrderBookItem orderBookItem;//委托簿xxx_order_book的数组元素
//	char sep = ',';
//	int32_t ret = 0;
//	//从文件读数据
//	ret = fscanf(_orderBookFile, "%d,%d,", &orderBook.channel_no, &orderBook.market_type);
//	if (ret == -1)
//	{
//		return false;
//	}
//
//	FGet_str(_orderBookFile, orderBook.security_code, sep);
//
//	fscanf(_orderBookFile, "%ld,%ld,%ld,",
//		&orderBook.last_tick_time, &orderBook.last_snapshot_time, &orderBook.last_tick_seq);
//
//	//printf("first get data orderbook last_tick_time = %ld, last_snapshot_time = %ld, last_tick_seq = %ld\n", orderBook.last_tick_time, orderBook.last_snapshot_time, orderBook.last_tick_seq);
//
//	//买委托簿
//	for (i = 0; i < ORDERBOOK_LEVEL_LENGTH; ++i)
//	{
//		memset(&orderBookItem, 0, sizeof(orderBookItem));
//
//		fscanf(_orderBookFile, "%ld,%ld,",
//			&orderBookItem.price,
//			&orderBookItem.volume
//		);
//		//printf("first get data orderbook bid_order_book item, price = %ld, volume = %ld, queue_size = %ld\n", orderBookItem.price, orderBookItem.volume, orderBookItem.order_queue_size);
//
//		//文件表结构固定，必须读两档委托两个orderBookItem，但如果是个被0填充的委托簿数据项，就不加入数组
//		if (!(orderBookItem.price == 0 && orderBookItem.volume == 0))
//		{
//			orderBook.bid_order_book.emplace_back(orderBookItem);
//		}
//	}
//
//	//卖委托簿
//	for (i = 0; i < ORDERBOOK_LEVEL_LENGTH; ++i)
//	{
//		memset(&orderBookItem, 0, sizeof(orderBookItem));
//
//		fscanf(_orderBookFile, "%ld,%ld,",
//			&orderBookItem.price,
//			&orderBookItem.volume
//		);
//
//		//文件表结构固定，必须读两档委托两个orderBookItem，但如果是个被0填充的委托簿数据项，就不加入数组
//		if (!(orderBookItem.price == 0 && orderBookItem.volume == 0))
//		{
//			orderBook.offer_order_book.emplace_back(orderBookItem);
//		}
//
//	}
//
//	fscanf(_orderBookFile, "%ld,%ld,%ld,%ld,",
//		&orderBook.total_num_trades,
//		&orderBook.total_volume_trade,
//		&orderBook.total_value_trade,
//		&orderBook.last_price);
//
//	//回调次数
//	fscanf(_orderBookFile, "%u", &cnt);
//
//	//把本行剩下的读完
//	char str[100] = { 0 };
//	fgets(str, sizeof(str), _orderBookFile);
//
//	return true;
//}

bool VirtualMarket::ReadOneOrderBook(FILE* _orderBookFile, QuickSnapInfo& orderBook, uint32_t& cnt)
{
	//到了文件尾
	if (feof(_orderBookFile))
	{
		return false;
	}

	int32_t ret = 0;
	//从文件读数据
	ret = fscanf(_orderBookFile, "%u,%u,", &orderBook._sc, &orderBook._tm);
	if (ret == -1)
	{
		return false;
	}

	fscanf(_orderBookFile, "%u,%lu,%u,%lu,",
		&orderBook._pxBid01, &orderBook._volBid01, &orderBook._pxBid02, &orderBook._volBid02);

	//printf("first get data orderbook last_tick_time = %ld, last_snapshot_time = %ld, last_tick_seq = %ld\n", orderBook.last_tick_time, orderBook.last_snapshot_time, orderBook.last_tick_seq);

	fscanf(_orderBookFile, "%u,%lu,%u,%lu,",
		&orderBook._pxAsk01, &orderBook._volAsk01, &orderBook._pxAsk02, &orderBook._volAsk02);

	fscanf(_orderBookFile, "%u,%lu,%lf,%lu,",
		&orderBook._ctTot,
		&orderBook._qtyTot,
		&orderBook._cnyTot,
		&orderBook._pxLast);

	//回调次数
	fscanf(_orderBookFile, "%u", &cnt);

	//把本行剩下的读完
	char str[100] = { 0 };
	fgets(str, sizeof(str), _orderBookFile);

	return true;
}

uint32_t VirtualMarket::MinCallbackCnt()
{
	set<uint32_t> cnts;

	if (_callbackCntSnapshot > 0)
	{
		cnts.insert(_callbackCntSnapshot);
	}


	if (_callbackCntOrdAndExe > 0)
	{
		cnts.insert(_callbackCntOrdAndExe);
	}

	for (size_t i = 0; i < _callbackCntsOrderBook.size(); i++)
	{
		if (_callbackCntsOrderBook[i] > 0)
		{
			cnts.insert(_callbackCntsOrderBook[i]);
		}
	}

	uint32_t ret;

	if (cnts.empty() == false)
	{
		ret = *(cnts.begin());
		//printf("minCallback = %u\n", ret);
	}
	else
	{
		cerr << "VirtualMarket::MinCallbackCnt(): all callbackCnt are 0" << endl;
		ret = 0;
	}

	return ret;
}

//从文件中读字符串，读到分隔符sep为止，并且下次再读文件是从分隔符之后开始
void VirtualMarket::FGet_str(FILE* fp, char* str, char sep)
{
	int i = 0;
	while (1)
	{
		fscanf(fp, "%c", &str[i]);
		if (str[i] == sep)
		{
			str[i] = '\0';
			break;
		}
		i++;
	}
}