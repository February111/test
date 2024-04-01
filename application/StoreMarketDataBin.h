#pragma once

#include <thread>
#include <unordered_map>
#include <queue>
#include <vector>
#include <thread>
#include "ama.h"
#include "sp_type.h"
#include "MDApplication.h"

class StoreMarketDataBin: public MDApplication
{
public:
	//构造函数
	StoreMarketDataBin();

	//初始化
	//void Init();
	virtual int BeforeMarket() override;

	//析构函数
	virtual ~StoreMarketDataBin() override;

	//收到数据时调用的回调函数
	virtual void OnSnapshot(SnapshotInfo* snapInfo, uint32_t cnt) override;

	//void OnMyTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt);

	//void OnMyTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt);

	virtual void OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt) override;

	//void OnMyOrderBook(std::vector<amd::ama::MDOrderBook>& order_book);

	virtual void OnQuickSnap(QuickSnapInfo* info, uint32_t cnt) override;

	//记录回调次数
	void LogCallbackCount();

private:
	//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
	void OpenSnapshotCsvFile();

	//打开TickOrder.csv逐笔委托数据文件
	//void OpenTickOrderCsvFile();

	//打开TickExecution.csv成交数据文件
	//void OpenTickExecutionCsvFile();

	void OpenOrdAndExeInfo();

	//打开OrderBook.Csv文件
	void OpenOrderBookCsvFile();

	//把数据压缩，存入文件
	void CompressDataToFile(const void *pData, size_t dataLen, FILE *fp);

	//写日志的线程函数
	void FuncRecordeCallbackCount();

private:
	//委托簿的线程总个数
	int32_t _threadNumOrderBook;

	//存储数据的文件夹路径
	std::string _folder;

	//文件指针
	FILE* _snapshotCsvFile;
	//FILE* _tickExecutionCsvFile;
	//FILE* _tickOrderCsvFile;
	FILE* _ordAndExeBinFile;
	std::vector<FILE*> _orderBookCsvFiles;//存储一开始打开的所有orderBook文件指针
	
	//数组，存储收到的数据，最后写入文件
	std::vector< SnapshotInfo> *_pArraySnapshot;
	//std::vector< TickExecutionInfo> *_pArrayTickExecution;
	//std::vector< TickOrderInfo> *_pArrayTickOrder;
	std::vector< OrdAndExeInfo>* _pArrayOrdAndExeInfo;
	//std::vector<std::vector<OrderBookInfo> *> _arrsOrderBook;
	std::vector<std::vector<QuickSnapInfo>*> _arrsOrderBook;

	size_t _idxSnapshot;
	//size_t _idxTickOrder;//tickOrder数组下一个要写的位置下标
	//size_t _idxTickExecution;
	size_t _idxOrdAndExe;
	std::vector<size_t> _idxesOrderBook;

	std::unordered_map<std::thread::id, size_t> _threadAndOrderBookArray;//每个线程匹配一个索引，代表要存的数组，最后要写的文件

	std::string _data;			//日期，用来组成文件名

	uint32_t _callbackCount;	//回调次数

	int _compressLevel;//压缩等级

	bool _logFlag;	//写日志的标志
	std::thread _logThread; //写日志的线程
};

