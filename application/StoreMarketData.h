#pragma once

#include <queue>
#include <unordered_map>
#include <thread>
#include <string>
#include "ama.h"
#include "sp_type.h"
#include "MDApplication.h"

class StoreMarketData : public MDApplication
{
public:
	//初始化
	virtual int BeforeMarket() override;

	//析构函数
	virtual ~StoreMarketData() override;

	//收到数据时调用的回调函数
	virtual void OnSnapshot(SnapshotInfo* snapInfo, uint32_t cnt) override;

	virtual void OnOrderAndExecu(OrdAndExeInfo* ordAndExe, uint32_t cnt) override;

	//void OnMyOrderBook(std::vector<amd::ama::MDOrderBook>& order_book);
	virtual void OnQuickSnap(QuickSnapInfo* infos, uint32_t cnt) override;

private:
	//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
	void OpenSnapshotCsvFile();

	void OpenOrdAndExeCsvFile();

	//打开OrderBook.Csv文件
	void OpenOrderBookCsvFile();

	//OnMDSnapShot()中调用的写文件函数
	void WriteSnapshotToFile(const SnapshotInfo* snapInfo, FILE* fp);

	//往文件写入一条补充信息，回调次数，本条数据的次序，时间戳
	//void WriteAdditionalInfoToFile(uint32_t callbackCnt, uint32_t sequence, int32_t localTime, FILE* fp);

	//文件结尾换行
	void WriteEndingToFile(FILE* fp)
	{
		fputc('\n', fp);
	}

	//往文件写入一条OrderBook数据
	//void WriteOrderBookToFile(amd::ama::MDOrderBook* orderBook, FILE* fp);
	void WriteOrderBookToFile(QuickSnapInfo* info, FILE* fp);

	//向文件写一条成交数据
	//void WriteTickExecutionToFile(amd::ama::MDTickExecution* tick, FILE* fp);

	//写入ordAndExeInfo数据
	void WriteOrdAndExeInfoToFile(const OrdAndExeInfo* info, FILE* fp);

	//向文件写一条委托数据
	//void WriteTickOrderToFile(amd::ama::MDTickOrder* ticks, FILE* fp);

private:
	//委托簿的线程总个数
	int32_t _threadNumOrderBook;

	//存储数据的文件夹路径
	std::string _folder;

	//csv文件的分隔符
	char _sep;

	//文件指针
	FILE* _snapshotCsvFile;
	//FILE* _tickExecutionCsvFile;
	//FILE* _tickOrderCsvFile;
	FILE* _ordAndExeCsvFile;
	std::queue<FILE*> _orderBookCsvFiles;//存储一开始打开的所有orderBook文件指针
	std::unordered_map<std::thread::id, FILE*> _threadWriteFileOrderBook;//每个线程匹配一个要写的文件

	std::string _data;			//日期，用来组成文件名
	uint32_t _callbackCount;	//回调次数
};

