#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <thread>
#include <ama.h>
#include <ama_tools.h>
#include <string>
#include <fstream>
#include <map>
#include "StrategyPlatform.h"
#include <stdlib.h>
#include <climits>
#include <vector>
#include "easycrash_dump.h"
#include <stdio.h>



#ifdef _WIN32
#pragma warning (disable:4996)
#endif

namespace fill
{

using namespace std;

struct ListNode
{
    int val;
    struct ListNode* next;
};

class MergeLists {
public:
    ListNode* mergeTwoLists(ListNode* a, ListNode* b) {
        if ((!a) || (!b)) return a ? a : b;
        ListNode head, * tail = &head, * aPtr = a, * bPtr = b;
        while (aPtr && bPtr) {
            if (aPtr->val < bPtr->val) {
                tail->next = aPtr; aPtr = aPtr->next;
            }
            else {
                tail->next = bPtr; bPtr = bPtr->next;
            }
            tail = tail->next;
        }
        tail->next = (aPtr ? aPtr : bPtr);
        return head.next;
    }

    ListNode* merge(vector <ListNode*>& lists, int l, int r) {
        if (l == r) return lists[l];
        if (l > r) return nullptr;
        int mid = (l + r) >> 1;
        return mergeTwoLists(merge(lists, l, mid), merge(lists, mid + 1, r));
    }

    ListNode* mergeKLists(vector<ListNode*>& lists) {
        return merge(lists, 0, lists.size() - 1);
    }
};

class MediaSortedArrays {
public:
    double findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
        if (nums1.size() > nums2.size()) {
            return findMedianSortedArrays(nums2, nums1);
        }

        int m = nums1.size();
        int n = nums2.size();
        int left = 0, right = m;
        // median1：前一部分的最大值
        // median2：后一部分的最小值
        int median1 = 0, median2 = 0;

        while (left <= right) {
            // 前一部分包含 nums1[0 .. i-1] 和 nums2[0 .. j-1]
            // 后一部分包含 nums1[i .. m-1] 和 nums2[j .. n-1]
            int i = (left + right) / 2;
            int j = (m + n + 1) / 2 - i;

            // nums_im1, nums_i, nums_jm1, nums_j 分别表示 nums1[i-1], nums1[i], nums2[j-1], nums2[j]
            int nums_im1 = (i == 0 ? INT_MIN : nums1[i - 1]);
            int nums_i = (i == m ? INT_MAX : nums1[i]);
            int nums_jm1 = (j == 0 ? INT_MIN : nums2[j - 1]);
            int nums_j = (j == n ? INT_MAX : nums2[j]);

            if (nums_im1 <= nums_j) {
                median1 = max(nums_im1, nums_jm1);
                median2 = min(nums_i, nums_j);
                left = i + 1;
            }
            else {
                right = i - 1;
            }
        }

        return (m + n) % 2 == 0 ? (median1 + median2) / 2.0 : median1;
    }

};

#ifdef _WIN32

#pragma comment(linker, "/defaultlib:dbghelp.lib")
#pragma warning(disable : 4996)

#define STATIC_BLOCK_BEGIN namespace {
#define STATIC_BLOCK_END }

STATIC_BLOCK_BEGIN

STATIC_BLOCK_END

TCHAR EasyCrashDump::s_DumpFileDirectory[MAX_PATH];
LPTOP_LEVEL_EXCEPTION_FILTER EasyCrashDump::m_PreviousFilter;

EasyCrashDump::EasyCrashDump()
{
	m_PreviousFilter = SetUnhandledExceptionFilter(ExceptionFilter);
  SetErrorMode(SEM_NOGPFAULTERRORBOX);
}

EasyCrashDump::~EasyCrashDump()
{
	SetUnhandledExceptionFilter( m_PreviousFilter );
}

void EasyCrashDump::SetDumpFileDirectory(const char* dumpFileDirectory )
{
	_tcscpy(s_DumpFileDirectory, dumpFileDirectory);
}

LONG WINAPI EasyCrashDump::ExceptionFilter(	PEXCEPTION_POINTERS pExceptionInfo )
{
	TCHAR szDumpFileName[MAX_PATH] = {0};
	SYSTEMTIME currentTime;
	GetLocalTime(&currentTime);
	_stprintf(szDumpFileName, _T("%s%04d-%02d-%02d(%02d_%02d_%02d.%03d).dmp"), 
		s_DumpFileDirectory,
		currentTime.wYear,
		currentTime.wMonth,
		currentTime.wDay,
		currentTime.wHour,
		currentTime.wMinute,
		currentTime.wSecond,
		currentTime.wMilliseconds);

	HANDLE hReportFile = CreateFile( szDumpFileName,
		GENERIC_WRITE,
		0,
		0,
		OPEN_ALWAYS,
		FILE_FLAG_WRITE_THROUGH,
		0 );	
	if(hReportFile != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION miniDumpInfo = {0};
		miniDumpInfo.ThreadId = GetCurrentThreadId();
		miniDumpInfo.ExceptionPointers = pExceptionInfo;
		miniDumpInfo.ClientPointers = FALSE;
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hReportFile, MiniDumpNormal, &miniDumpInfo, NULL, NULL);

		CloseHandle(hReportFile);
	}

#define DISABLE_ERROR_POPUP_MESSAGE_BOX
#ifdef DISABLE_ERROR_POPUP_MESSAGE_BOX
	if ( m_PreviousFilter )
		m_PreviousFilter( pExceptionInfo );
	return EXCEPTION_EXECUTE_HANDLER;
#else
	if ( m_PreviousFilter )
		return m_PreviousFilter( pExceptionInfo );
	else
		return EXCEPTION_CONTINUE_SEARCH;
#endif
}
#endif // _WIN32

class ConfigFile
{
public:
    ConfigFile() {}

    bool Load(const std::string &fileName)
    {
        std::ifstream ifs(fileName);
        if (ifs.good() == false)
        {
            std::cout << "ConfigFile::Load() : file not exist" << std::endl;
            return false;
        }

        std::string key;
        std::string value;
        std::string equ;
        //每次读一行
        std::string line;
        while (ifs.good())
        {
            std::getline(ifs, line);
            if (line.size() == 0)
            {
                break;
            }
            
            std::istringstream iss(line);
            iss >> key >> equ >> value;
            if (equ != "=" || value.size() == 0)
            {
                std::cout << "ConfigFile::Load(): config information error" << std::endl;
                return false;
            }
            _config[key] = value;
        }

        return true;
    }
    
    std::string Get(const std::string &key)
    {
        if (_config.count(key) == 0)
        {
            std::cerr << "ConfigFile::Get(const std::string &key):" << "key not found" << std::endl;
            return std::string();
        }
        return _config[key];
    }

    ~ConfigFile() {}

private:
    std::map<std::string, std::string> _config;
};

static std::mutex g_mutex;  //同步cout输出，回调的线程模型请参看开发指南
// 继承IAMDSpi，实现自己的回调处理类
class MySpi01 : public amd::ama::IAMDSpi
{
public:
	// 定义日志回调处理方法
	virtual void OnLog(const int32_t& level,
		const char* log,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA log: " << "    level: " << level << "    log:   " << log << std::endl;
	}

	// 定义监控回调处理方法
	virtual void OnIndicator(const char* indicator,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA indicator: " << indicator << std::endl;
	}

	// 定义事件回调处理方法  level 对照 EventLevel 数据类型 code 对照 EventCode 数据类型
	virtual void OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len) override
	{
		std::cout << "AMA event: " << event_msg << std::endl;
	}

	// 定义快照数据回调处理方法
	virtual void OnMDSnapshot(amd::ama::MDSnapshot* snapshot,
		uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			WriteSnapshotToFile(&snapshot[i], _snapshotCsvFile);
			//std::cout << amd::ama::Tools::Serialize(snapshot[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshot);
	}
	//
	virtual void OnMDFutureSnapshot(amd::ama::MDFutureSnapshot* snapshots, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
			std::cout << snapshots->security_code << "==" << snapshots->last_price << "==" << snapshots->total_volume_trade << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}
	//定义指数快照数据回调处理方法
	virtual void OnMDIndexSnapshot(amd::ama::MDIndexSnapshot* snapshots, uint32_t cnt)
	{

		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}

	//定义逐笔委托数据回调处理方法
	virtual void OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(ticks[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(ticks);
	}

	// 定义逐笔成交数据回调处理方法
	virtual void OnMDTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(tick[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(tick);
	}

	//定义委托簿数据回调处理方法(本地构建模式下非单线程递交, cfg.thread_num为递交委托簿数据线程数, 服务端推送模式下为单线程递交)
	virtual void OnMDOrderBook(std::vector<amd::ama::MDOrderBook>& order_book)
	{
		std::lock_guard<std::mutex> _(g_mutex);
		//std::cout << "order_book size = " << order_book.size() << std::endl;

		for (size_t i = 0; i < order_book.size(); i++)
		{
			//往OrderBook.csv文件写一行数据
			WriteOrderBookToFile(&order_book[i], _orderBookCsvFile);
			/*std::cout << "i = " << i << ",";
			if (order_book[i].bid_order_book.size() > 0)
			{
				std::cout << "bid_order_book.size()=";
				std::cout << order_book[i].bid_order_book.size() << ","
					<< "bid_order_price01:" << order_book[i].bid_order_book[0].price << ","
					<< "bid_order_volum01:" << order_book[i].bid_order_book[0].volume << ","
					<< "channel_no:" << order_book[i].channel_no << ","
					<< "last_price:" << order_book[i].last_price << ","
					<< "offer_order_book.size()=" << order_book[i].offer_order_book.size();
			}
			std::cout << std::endl;*/
		}
		//不需要手动释放内存
	}

	//MySpi初始化
	void Init()
	{
		//打开snapshot.csv文件
		OpenSnapshotCsvFile();
		//打开OrderBook.csv文件
		OpenOrderBookCsvFile();
	}

	//析构函数
	~MySpi01()
	{
		//如果文件打开了，析构时关闭
		if (_snapshotCsvFile != NULL)
		{
			fclose(_snapshotCsvFile);
			_snapshotCsvFile = NULL;
		}

		if (_orderBookCsvFile != NULL)
		{
			fclose(_orderBookCsvFile);
			_orderBookCsvFile == NULL;
		}
	}

private:
	//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
	void OpenSnapshotCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/Snapshot.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf(": %s does exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen snapshot.csv");
				return;
			}
			/*时间，股票代码，买一价格，卖一价格，买一量，卖一量，还有总成交量，总成交金额,总成交笔数*/
			fprintf(fp, "%-10s%c", "market", sep);
			fprintf(fp, "%-10s%c", "code", sep);
			fprintf(fp, "%-18s%c", "time", sep);
			fprintf(fp, "%-15s%c", "last_price", sep);
			fprintf(fp, "%-15s%c", "bid_price01", sep);
			fprintf(fp, "%-15s%c", "bid_volume01", sep);
			fprintf(fp, "%-15s%c", "offer_price01", sep);
			fprintf(fp, "%-15s%c", "offer_volume01", sep);
			fprintf(fp, "%-10s%c", "num_trades", sep);
			fprintf(fp, "%-18s%c", "total_volume_trade", sep);
			fprintf(fp, "%-16s%c", "total_value_trade", sep);
			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			//printf("MarketDat.csv exist\n");
			fclose(fp);
		}

		//追加方式打开文件
		_snapshotCsvFile = fopen(filename, "a");
	}

	//OnMDSnapShot()中调用的写文件函数
	void WriteSnapshotToFile(amd::ama::MDSnapshot* snapshot, FILE* fp)
	{
		char sep = ',';
		char market[20] = { 0 };
		//市场类型
		memset(market, 0, sizeof(market));
		switch (snapshot->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%-10s,", market);

		//股票代码
		fprintf(fp, "%-10s%c", snapshot->security_code, sep);
		//时间
		fprintf(fp, "%-18lld%c", snapshot->orig_time, sep);
		//最新价
		fprintf(fp, "%-15lld%c", snapshot->last_price, sep);
		//买一价格
		fprintf(fp, "%-15lld%c", snapshot->bid_price[0], sep);
		//买一量
		fprintf(fp, "%-15lld%c", snapshot->bid_volume[0], sep);
		//卖一价格
		fprintf(fp, "%-15lld%c", snapshot->offer_price[0], sep);
		//卖一量
		fprintf(fp, "%-15lld%c", snapshot->offer_volume[0], sep);
		//成交笔数
		fprintf(fp, "%-10lld%c", snapshot->num_trades, sep);
		//成交总量
		fprintf(fp, "%-18lld%c", snapshot->total_volume_trade, sep);
		//成交总金额
		fprintf(fp, "%-16lld", snapshot->total_value_trade);
		fputc('\n', fp);

		return;
	}

	//打开OrderBook.Csv文件
	void OpenOrderBookCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/OrderBook.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf("OpenOrderBookCsvFile: %s does not exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen orderbook.csv");
				return;
			}
			
			fprintf(fp, "%s%c", "channel_no", sep);
			fprintf(fp, "%s%c", "market_type", sep);
			fprintf(fp, "%s%c", "security_code", sep);
			fprintf(fp, "%s%c", "last_tick_time", sep);
			fprintf(fp, "%s%c", "last_snapshot_time", sep);
			fprintf(fp, "%s%c", "last_tick_seq", sep);
			//买委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue", sep);
			}
			//卖委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue", sep);
			}
			fprintf(fp, "%s%c", "total_num_trades", sep);
			fprintf(fp, "%s%c", "total_volume_trade", sep);
			fprintf(fp, "%s%c", "total_value_trade", sep);
			fprintf(fp, "%s%c", "last_price", sep);

			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			fclose(fp);
		}

		//追加方式打开文件
		_orderBookCsvFile = fopen(filename, "a");
	}

	////往OrderBook.csv文件写一行数据
	void WriteOrderBookToFile(amd::ama::MDOrderBook* orderBook, FILE* fp)
	{
		int32_t bid_order_size = orderBook->bid_order_book.size();
		int32_t offer_order_size = orderBook->offer_order_book.size();
		//买委托簿为空，退出
		if (bid_order_size <= 0)
		{
			return;
		}
		char sep = ',';
		
		fprintf(fp, "%d%c", orderBook->channel_no, sep);
		//市场类型
		char market[10] = { 0 };
		memset(market, 0, sizeof(market));
		switch (orderBook->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%s,", market);
		
		fprintf(fp, "%s%c", orderBook->security_code, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_snapshot_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_seq, sep);

		//买委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{	
			if (i < bid_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->bid_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		//卖委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{
			if (i < offer_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->offer_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		fprintf(fp, "%lld%c", orderBook->total_num_trades, sep);
		fprintf(fp, "%lld%c", orderBook->total_volume_trade, sep);
		fprintf(fp, "%lld%c", orderBook->total_value_trade, sep);
		fprintf(fp, "%lld%c", orderBook->last_price, sep);

		fputc('\n', fp);
		return;
	}

	


private:
	FILE* _snapshotCsvFile;
	FILE* _orderBookCsvFile;
};


//按数据品种以及数据类型订阅实时行情数据
int32_t SubscribeWithCategory01()
{
	/*
	按品种类型订阅信息设置:
	1. 订阅信息分三个维度 market:市场, data_type:证券数据类型, category_type:品种类型, security_code:证券代码
	2. 订阅操作有三种:
		kSet 设置订阅, 覆盖所有订阅信息
		kAdd 增加订阅, 在前一个基础上增加订阅信息
		kDel 删除订阅, 在前一个基础上删除订阅信息
		kCancelAll 取消所有订阅信息
	*/
	amd::ama::SubscribeCategoryItem sub1[2];
	memset(sub1, 0, sizeof(sub1));

	/* 订阅深交所全部证券代码的股票现货快照 */
	sub1[0].market = amd::ama::MarketType::kSZSE;
	sub1[0].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[0].security_code[0] = '\0';

	/* 订阅上交所全部证券代码的股票现货快照 */
	sub1[1].market = amd::ama::MarketType::kSSE;
	sub1[1].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[1].security_code[0] = '\0';

	amd::ama::SubscribeCategoryItem sub2[2];
	memset(sub2, 0, sizeof(sub2));

	/* 订阅上交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[0].market = amd::ama::MarketType::kSSE;
	sub2[0].data_type = amd::ama::SubscribeSecuDataType::kTickOrder 
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[0].security_code[0] = '\0';
	/* 订阅深交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[1].market = amd::ama::MarketType::kSZSE;
	sub2[1].data_type = amd::ama::SubscribeSecuDataType::kTickOrder
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[1].security_code[0] = '\0';
	//strcpy(sub2[1].security_code, "600004");

	/* 发起订阅 */
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kSet, sub1, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub2, 2);

	return 0;
}

//衍生数据订阅
int32_t SubDerivedData01()
{
	amd::ama::SubscribeDerivedDataItem sub[2];
	memset(sub, 0, sizeof(sub));

	//订阅深交所000001/上交所60000证券代码
	sub[0].market = amd::ama::MarketType::kSZSE;
	strcpy(sub[0].security_code, "000001");
	//sub[0].security_code[0] = '\0';

	sub[1].market = amd::ama::MarketType::kSSE;
	strcpy(sub[1].security_code, "600000");
	//sub[1].security_code[0] = '\0';

	//发起订阅委托簿类型的数据
	return amd::ama::IAMDApi::SubscribeDerivedData(amd::ama::SubscribeType::kSet,
		amd::ama::SubscribeDerivedDataType::kOrderBook, sub, 2);
}

// 继承IAMDSpi，实现自己的回调处理类
class MySpi02 : public amd::ama::IAMDSpi
{
public:
	// 定义日志回调处理方法
	virtual void OnLog(const int32_t& level,
		const char* log,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA log: " << "    level: " << level << "    log:   " << log << std::endl;
	}

	// 定义监控回调处理方法
	virtual void OnIndicator(const char* indicator,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA indicator: " << indicator << std::endl;
	}

	// 定义事件回调处理方法  level 对照 EventLevel 数据类型 code 对照 EventCode 数据类型
	virtual void OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len) override
	{
		std::cout << "AMA event: " << event_msg << std::endl;
	}

	// 定义快照数据回调处理方法
	virtual void OnMDSnapshot(amd::ama::MDSnapshot* snapshot,
		uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			WriteSnapshotToFile(&snapshot[i], _snapshotCsvFile);
			//std::cout << amd::ama::Tools::Serialize(snapshot[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshot);
	}
	//
	virtual void OnMDFutureSnapshot(amd::ama::MDFutureSnapshot* snapshots, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
			std::cout << snapshots->security_code << "==" << snapshots->last_price << "==" << snapshots->total_volume_trade << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}
	//定义指数快照数据回调处理方法
	virtual void OnMDIndexSnapshot(amd::ama::MDIndexSnapshot* snapshots, uint32_t cnt)
	{

		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}

	//定义逐笔委托数据回调处理方法
	virtual void OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(ticks[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(ticks);
	}

	// 定义逐笔成交数据回调处理方法
	virtual void OnMDTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(tick[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(tick);
	}

	//定义委托簿数据回调处理方法(本地构建模式下非单线程递交, cfg.thread_num为递交委托簿数据线程数, 服务端推送模式下为单线程递交)
	virtual void OnMDOrderBook(std::vector<amd::ama::MDOrderBook>& order_book)
	{
		std::lock_guard<std::mutex> _(g_mutex);
		//std::cout << "order_book size = " << order_book.size() << std::endl;

		for (size_t i = 0; i < order_book.size(); i++)
		{
			//往OrderBook.csv文件写一行数据
			WriteOrderBookToFile(&order_book[i], _orderBookCsvFile);
			/*std::cout << "i = " << i << ",";
			if (order_book[i].bid_order_book.size() > 0)
			{
				std::cout << "bid_order_book.size()=";
				std::cout << order_book[i].bid_order_book.size() << ","
					<< "bid_order_price01:" << order_book[i].bid_order_book[0].price << ","
					<< "bid_order_volum01:" << order_book[i].bid_order_book[0].volume << ","
					<< "channel_no:" << order_book[i].channel_no << ","
					<< "last_price:" << order_book[i].last_price << ","
					<< "offer_order_book.size()=" << order_book[i].offer_order_book.size();
			}
			std::cout << std::endl;*/
		}
		//不需要手动释放内存
	}

	//MySpi初始化
	void Init()
	{
		//打开snapshot.csv文件
		OpenSnapshotCsvFile();
		//打开OrderBook.csv文件
		OpenOrderBookCsvFile();
	}

	//析构函数
	~MySpi02()
	{
		//如果文件打开了，析构时关闭
		if (_snapshotCsvFile != NULL)
		{
			fclose(_snapshotCsvFile);
			_snapshotCsvFile = NULL;
		}

		if (_orderBookCsvFile != NULL)
		{
			fclose(_orderBookCsvFile);
			_orderBookCsvFile == NULL;
		}
	}

private:
	//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
	void OpenSnapshotCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/Snapshot.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf(": %s does exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen snapshot.csv");
				return;
			}
			/*时间，股票代码，买一价格，卖一价格，买一量，卖一量，还有总成交量，总成交金额,总成交笔数*/
			fprintf(fp, "%-10s%c", "market", sep);
			fprintf(fp, "%-10s%c", "code", sep);
			fprintf(fp, "%-18s%c", "time", sep);
			fprintf(fp, "%-15s%c", "last_price", sep);
			fprintf(fp, "%-15s%c", "bid_price01", sep);
			fprintf(fp, "%-15s%c", "bid_volume01", sep);
			fprintf(fp, "%-15s%c", "offer_price01", sep);
			fprintf(fp, "%-15s%c", "offer_volume01", sep);
			fprintf(fp, "%-10s%c", "num_trades", sep);
			fprintf(fp, "%-18s%c", "total_volume_trade", sep);
			fprintf(fp, "%-16s%c", "total_value_trade", sep);
			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			//printf("MarketDat.csv exist\n");
			fclose(fp);
		}

		//追加方式打开文件
		_snapshotCsvFile = fopen(filename, "a");
	}

	//OnMDSnapShot()中调用的写文件函数
	void WriteSnapshotToFile(amd::ama::MDSnapshot* snapshot, FILE* fp)
	{
		char sep = ',';
		char market[20] = { 0 };
		//市场类型
		memset(market, 0, sizeof(market));
		switch (snapshot->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%-10s,", market);

		//股票代码
		fprintf(fp, "%-10s%c", snapshot->security_code, sep);
		//时间
		fprintf(fp, "%-18lld%c", snapshot->orig_time, sep);
		//最新价
		fprintf(fp, "%-15lld%c", snapshot->last_price, sep);
		//买一价格
		fprintf(fp, "%-15lld%c", snapshot->bid_price[0], sep);
		//买一量
		fprintf(fp, "%-15lld%c", snapshot->bid_volume[0], sep);
		//卖一价格
		fprintf(fp, "%-15lld%c", snapshot->offer_price[0], sep);
		//卖一量
		fprintf(fp, "%-15lld%c", snapshot->offer_volume[0], sep);
		//成交笔数
		fprintf(fp, "%-10lld%c", snapshot->num_trades, sep);
		//成交总量
		fprintf(fp, "%-18lld%c", snapshot->total_volume_trade, sep);
		//成交总金额
		fprintf(fp, "%-16lld", snapshot->total_value_trade);
		fputc('\n', fp);

		return;
	}

	//打开OrderBook.Csv文件
	void OpenOrderBookCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/OrderBook.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf("OpenOrderBookCsvFile: %s does not exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen orderbook.csv");
				return;
			}
			
			fprintf(fp, "%s%c", "channel_no", sep);
			fprintf(fp, "%s%c", "market_type", sep);
			fprintf(fp, "%s%c", "security_code", sep);
			fprintf(fp, "%s%c", "last_tick_time", sep);
			fprintf(fp, "%s%c", "last_snapshot_time", sep);
			fprintf(fp, "%s%c", "last_tick_seq", sep);
			//买委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue", sep);
			}
			//卖委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue", sep);
			}
			fprintf(fp, "%s%c", "total_num_trades", sep);
			fprintf(fp, "%s%c", "total_volume_trade", sep);
			fprintf(fp, "%s%c", "total_value_trade", sep);
			fprintf(fp, "%s%c", "last_price", sep);

			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			fclose(fp);
		}

		//追加方式打开文件
		_orderBookCsvFile = fopen(filename, "a");
	}

	////往OrderBook.csv文件写一行数据
	void WriteOrderBookToFile(amd::ama::MDOrderBook* orderBook, FILE* fp)
	{
		int32_t bid_order_size = orderBook->bid_order_book.size();
		int32_t offer_order_size = orderBook->offer_order_book.size();
		//买委托簿为空，退出
		if (bid_order_size <= 0)
		{
			return;
		}
		char sep = ',';
		
		fprintf(fp, "%d%c", orderBook->channel_no, sep);
		//市场类型
		char market[10] = { 0 };
		memset(market, 0, sizeof(market));
		switch (orderBook->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%s,", market);
		
		fprintf(fp, "%s%c", orderBook->security_code, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_snapshot_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_seq, sep);

		//买委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{	
			if (i < bid_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->bid_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		//卖委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{
			if (i < offer_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->offer_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		fprintf(fp, "%lld%c", orderBook->total_num_trades, sep);
		fprintf(fp, "%lld%c", orderBook->total_volume_trade, sep);
		fprintf(fp, "%lld%c", orderBook->total_value_trade, sep);
		fprintf(fp, "%lld%c", orderBook->last_price, sep);

		fputc('\n', fp);
		return;
	}

	


private:
	FILE* _snapshotCsvFile;
	FILE* _orderBookCsvFile;
};


//按数据品种以及数据类型订阅实时行情数据
int32_t SubscribeWithCategory02()
{
	/*
	按品种类型订阅信息设置:
	1. 订阅信息分三个维度 market:市场, data_type:证券数据类型, category_type:品种类型, security_code:证券代码
	2. 订阅操作有三种:
		kSet 设置订阅, 覆盖所有订阅信息
		kAdd 增加订阅, 在前一个基础上增加订阅信息
		kDel 删除订阅, 在前一个基础上删除订阅信息
		kCancelAll 取消所有订阅信息
	*/
	amd::ama::SubscribeCategoryItem sub1[2];
	memset(sub1, 0, sizeof(sub1));

	/* 订阅深交所全部证券代码的股票现货快照 */
	sub1[0].market = amd::ama::MarketType::kSZSE;
	sub1[0].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[0].security_code[0] = '\0';

	/* 订阅上交所全部证券代码的股票现货快照 */
	sub1[1].market = amd::ama::MarketType::kSSE;
	sub1[1].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[1].security_code[0] = '\0';

	amd::ama::SubscribeCategoryItem sub2[2];
	memset(sub2, 0, sizeof(sub2));

	/* 订阅上交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[0].market = amd::ama::MarketType::kSSE;
	sub2[0].data_type = amd::ama::SubscribeSecuDataType::kTickOrder 
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[0].security_code[0] = '\0';
	/* 订阅深交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[1].market = amd::ama::MarketType::kSZSE;
	sub2[1].data_type = amd::ama::SubscribeSecuDataType::kTickOrder
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[1].security_code[0] = '\0';
	//strcpy(sub2[1].security_code, "600004");

	/* 发起订阅 */
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kSet, sub1, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub2, 2);

	return 0;
}

//衍生数据订阅
int32_t SubDerivedData02()
{
	amd::ama::SubscribeDerivedDataItem sub[2];
	memset(sub, 0, sizeof(sub));

	//订阅深交所000001/上交所60000证券代码
	sub[0].market = amd::ama::MarketType::kSZSE;
	strcpy(sub[0].security_code, "000001");
	//sub[0].security_code[0] = '\0';

	sub[1].market = amd::ama::MarketType::kSSE;
	strcpy(sub[1].security_code, "600000");
	//sub[1].security_code[0] = '\0';

	//发起订阅委托簿类型的数据
	return amd::ama::IAMDApi::SubscribeDerivedData(amd::ama::SubscribeType::kSet,
		amd::ama::SubscribeDerivedDataType::kOrderBook, sub, 2);
}

// 继承IAMDSpi，实现自己的回调处理类
class MySpi03 : public amd::ama::IAMDSpi
{
public:
	// 定义日志回调处理方法
	virtual void OnLog(const int32_t& level,
		const char* log,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA log: " << "    level: " << level << "    log:   " << log << std::endl;
	}

	// 定义监控回调处理方法
	virtual void OnIndicator(const char* indicator,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA indicator: " << indicator << std::endl;
	}

	// 定义事件回调处理方法  level 对照 EventLevel 数据类型 code 对照 EventCode 数据类型
	virtual void OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len) override
	{
		std::cout << "AMA event: " << event_msg << std::endl;
	}

	// 定义快照数据回调处理方法
	virtual void OnMDSnapshot(amd::ama::MDSnapshot* snapshot,
		uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			WriteSnapshotToFile(&snapshot[i], _snapshotCsvFile);
			//std::cout << amd::ama::Tools::Serialize(snapshot[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshot);
	}
	//
	virtual void OnMDFutureSnapshot(amd::ama::MDFutureSnapshot* snapshots, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
			std::cout << snapshots->security_code << "==" << snapshots->last_price << "==" << snapshots->total_volume_trade << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}
	//定义指数快照数据回调处理方法
	virtual void OnMDIndexSnapshot(amd::ama::MDIndexSnapshot* snapshots, uint32_t cnt)
	{

		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}

	//定义逐笔委托数据回调处理方法
	virtual void OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(ticks[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(ticks);
	}

	// 定义逐笔成交数据回调处理方法
	virtual void OnMDTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(tick[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(tick);
	}

	//定义委托簿数据回调处理方法(本地构建模式下非单线程递交, cfg.thread_num为递交委托簿数据线程数, 服务端推送模式下为单线程递交)
	virtual void OnMDOrderBook(std::vector<amd::ama::MDOrderBook>& order_book)
	{
		std::lock_guard<std::mutex> _(g_mutex);
		//std::cout << "order_book size = " << order_book.size() << std::endl;

		for (size_t i = 0; i < order_book.size(); i++)
		{
			//往OrderBook.csv文件写一行数据
			WriteOrderBookToFile(&order_book[i], _orderBookCsvFile);
			/*std::cout << "i = " << i << ",";
			if (order_book[i].bid_order_book.size() > 0)
			{
				std::cout << "bid_order_book.size()=";
				std::cout << order_book[i].bid_order_book.size() << ","
					<< "bid_order_price01:" << order_book[i].bid_order_book[0].price << ","
					<< "bid_order_volum01:" << order_book[i].bid_order_book[0].volume << ","
					<< "channel_no:" << order_book[i].channel_no << ","
					<< "last_price:" << order_book[i].last_price << ","
					<< "offer_order_book.size()=" << order_book[i].offer_order_book.size();
			}
			std::cout << std::endl;*/
		}
		//不需要手动释放内存
	}

	//MySpi初始化
	void Init()
	{
		//打开snapshot.csv文件
		OpenSnapshotCsvFile();
		//打开OrderBook.csv文件
		OpenOrderBookCsvFile();
	}

	//析构函数
	~MySpi03()
	{
		//如果文件打开了，析构时关闭
		if (_snapshotCsvFile != NULL)
		{
			fclose(_snapshotCsvFile);
			_snapshotCsvFile = NULL;
		}

		if (_orderBookCsvFile != NULL)
		{
			fclose(_orderBookCsvFile);
			_orderBookCsvFile == NULL;
		}
	}

private:
	//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
	void OpenSnapshotCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/Snapshot.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf(": %s does exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen snapshot.csv");
				return;
			}
			/*时间，股票代码，买一价格，卖一价格，买一量，卖一量，还有总成交量，总成交金额,总成交笔数*/
			fprintf(fp, "%-10s%c", "market", sep);
			fprintf(fp, "%-10s%c", "code", sep);
			fprintf(fp, "%-18s%c", "time", sep);
			fprintf(fp, "%-15s%c", "last_price", sep);
			fprintf(fp, "%-15s%c", "bid_price01", sep);
			fprintf(fp, "%-15s%c", "bid_volume01", sep);
			fprintf(fp, "%-15s%c", "offer_price01", sep);
			fprintf(fp, "%-15s%c", "offer_volume01", sep);
			fprintf(fp, "%-10s%c", "num_trades", sep);
			fprintf(fp, "%-18s%c", "total_volume_trade", sep);
			fprintf(fp, "%-16s%c", "total_value_trade", sep);
			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			//printf("MarketDat.csv exist\n");
			fclose(fp);
		}

		//追加方式打开文件
		_snapshotCsvFile = fopen(filename, "a");
	}

	//OnMDSnapShot()中调用的写文件函数
	void WriteSnapshotToFile(amd::ama::MDSnapshot* snapshot, FILE* fp)
	{
		char sep = ',';
		char market[20] = { 0 };
		//市场类型
		memset(market, 0, sizeof(market));
		switch (snapshot->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%-10s,", market);

		//股票代码
		fprintf(fp, "%-10s%c", snapshot->security_code, sep);
		//时间
		fprintf(fp, "%-18lld%c", snapshot->orig_time, sep);
		//最新价
		fprintf(fp, "%-15lld%c", snapshot->last_price, sep);
		//买一价格
		fprintf(fp, "%-15lld%c", snapshot->bid_price[0], sep);
		//买一量
		fprintf(fp, "%-15lld%c", snapshot->bid_volume[0], sep);
		//卖一价格
		fprintf(fp, "%-15lld%c", snapshot->offer_price[0], sep);
		//卖一量
		fprintf(fp, "%-15lld%c", snapshot->offer_volume[0], sep);
		//成交笔数
		fprintf(fp, "%-10lld%c", snapshot->num_trades, sep);
		//成交总量
		fprintf(fp, "%-18lld%c", snapshot->total_volume_trade, sep);
		//成交总金额
		fprintf(fp, "%-16lld", snapshot->total_value_trade);
		fputc('\n', fp);

		return;
	}

	//打开OrderBook.Csv文件
	void OpenOrderBookCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/OrderBook.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf("OpenOrderBookCsvFile: %s does not exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen orderbook.csv");
				return;
			}
			
			fprintf(fp, "%s%c", "channel_no", sep);
			fprintf(fp, "%s%c", "market_type", sep);
			fprintf(fp, "%s%c", "security_code", sep);
			fprintf(fp, "%s%c", "last_tick_time", sep);
			fprintf(fp, "%s%c", "last_snapshot_time", sep);
			fprintf(fp, "%s%c", "last_tick_seq", sep);
			//买委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue", sep);
			}
			//卖委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue", sep);
			}
			fprintf(fp, "%s%c", "total_num_trades", sep);
			fprintf(fp, "%s%c", "total_volume_trade", sep);
			fprintf(fp, "%s%c", "total_value_trade", sep);
			fprintf(fp, "%s%c", "last_price", sep);

			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			fclose(fp);
		}

		//追加方式打开文件
		_orderBookCsvFile = fopen(filename, "a");
	}

	////往OrderBook.csv文件写一行数据
	void WriteOrderBookToFile(amd::ama::MDOrderBook* orderBook, FILE* fp)
	{
		int32_t bid_order_size = orderBook->bid_order_book.size();
		int32_t offer_order_size = orderBook->offer_order_book.size();
		//买委托簿为空，退出
		if (bid_order_size <= 0)
		{
			return;
		}
		char sep = ',';
		
		fprintf(fp, "%d%c", orderBook->channel_no, sep);
		//市场类型
		char market[10] = { 0 };
		memset(market, 0, sizeof(market));
		switch (orderBook->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%s,", market);
		
		fprintf(fp, "%s%c", orderBook->security_code, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_snapshot_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_seq, sep);

		//买委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{	
			if (i < bid_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->bid_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		//卖委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{
			if (i < offer_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->offer_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		fprintf(fp, "%lld%c", orderBook->total_num_trades, sep);
		fprintf(fp, "%lld%c", orderBook->total_volume_trade, sep);
		fprintf(fp, "%lld%c", orderBook->total_value_trade, sep);
		fprintf(fp, "%lld%c", orderBook->last_price, sep);

		fputc('\n', fp);
		return;
	}

	


private:
	FILE* _snapshotCsvFile;
	FILE* _orderBookCsvFile;
};


//按数据品种以及数据类型订阅实时行情数据
int32_t SubscribeWithCategory03()
{
	/*
	按品种类型订阅信息设置:
	1. 订阅信息分三个维度 market:市场, data_type:证券数据类型, category_type:品种类型, security_code:证券代码
	2. 订阅操作有三种:
		kSet 设置订阅, 覆盖所有订阅信息
		kAdd 增加订阅, 在前一个基础上增加订阅信息
		kDel 删除订阅, 在前一个基础上删除订阅信息
		kCancelAll 取消所有订阅信息
	*/
	amd::ama::SubscribeCategoryItem sub1[2];
	memset(sub1, 0, sizeof(sub1));

	/* 订阅深交所全部证券代码的股票现货快照 */
	sub1[0].market = amd::ama::MarketType::kSZSE;
	sub1[0].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[0].security_code[0] = '\0';

	/* 订阅上交所全部证券代码的股票现货快照 */
	sub1[1].market = amd::ama::MarketType::kSSE;
	sub1[1].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[1].security_code[0] = '\0';

	amd::ama::SubscribeCategoryItem sub2[2];
	memset(sub2, 0, sizeof(sub2));

	/* 订阅上交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[0].market = amd::ama::MarketType::kSSE;
	sub2[0].data_type = amd::ama::SubscribeSecuDataType::kTickOrder 
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[0].security_code[0] = '\0';
	/* 订阅深交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[1].market = amd::ama::MarketType::kSZSE;
	sub2[1].data_type = amd::ama::SubscribeSecuDataType::kTickOrder
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[1].security_code[0] = '\0';
	//strcpy(sub2[1].security_code, "600004");

	/* 发起订阅 */
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kSet, sub1, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub2, 2);

	return 0;
}

//衍生数据订阅
int32_t SubDerivedData03()
{
	amd::ama::SubscribeDerivedDataItem sub[2];
	memset(sub, 0, sizeof(sub));

	//订阅深交所000001/上交所60000证券代码
	sub[0].market = amd::ama::MarketType::kSZSE;
	strcpy(sub[0].security_code, "000001");
	//sub[0].security_code[0] = '\0';

	sub[1].market = amd::ama::MarketType::kSSE;
	strcpy(sub[1].security_code, "600000");
	//sub[1].security_code[0] = '\0';

	//发起订阅委托簿类型的数据
	return amd::ama::IAMDApi::SubscribeDerivedData(amd::ama::SubscribeType::kSet,
		amd::ama::SubscribeDerivedDataType::kOrderBook, sub, 2);
}

// 继承IAMDSpi，实现自己的回调处理类
class MySpi04 : public amd::ama::IAMDSpi
{
public:
	// 定义日志回调处理方法
	virtual void OnLog(const int32_t& level,
		const char* log,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA log: " << "    level: " << level << "    log:   " << log << std::endl;
	}

	// 定义监控回调处理方法
	virtual void OnIndicator(const char* indicator,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA indicator: " << indicator << std::endl;
	}

	// 定义事件回调处理方法  level 对照 EventLevel 数据类型 code 对照 EventCode 数据类型
	virtual void OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len) override
	{
		std::cout << "AMA event: " << event_msg << std::endl;
	}

	// 定义快照数据回调处理方法
	virtual void OnMDSnapshot(amd::ama::MDSnapshot* snapshot,
		uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			WriteSnapshotToFile(&snapshot[i], _snapshotCsvFile);
			//std::cout << amd::ama::Tools::Serialize(snapshot[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshot);
	}
	//
	virtual void OnMDFutureSnapshot(amd::ama::MDFutureSnapshot* snapshots, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
			std::cout << snapshots->security_code << "==" << snapshots->last_price << "==" << snapshots->total_volume_trade << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}
	//定义指数快照数据回调处理方法
	virtual void OnMDIndexSnapshot(amd::ama::MDIndexSnapshot* snapshots, uint32_t cnt)
	{

		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}

	//定义逐笔委托数据回调处理方法
	virtual void OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(ticks[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(ticks);
	}

	// 定义逐笔成交数据回调处理方法
	virtual void OnMDTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(tick[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(tick);
	}

	//定义委托簿数据回调处理方法(本地构建模式下非单线程递交, cfg.thread_num为递交委托簿数据线程数, 服务端推送模式下为单线程递交)
	virtual void OnMDOrderBook(std::vector<amd::ama::MDOrderBook>& order_book)
	{
		std::lock_guard<std::mutex> _(g_mutex);
		//std::cout << "order_book size = " << order_book.size() << std::endl;

		for (size_t i = 0; i < order_book.size(); i++)
		{
			//往OrderBook.csv文件写一行数据
			WriteOrderBookToFile(&order_book[i], _orderBookCsvFile);
			/*std::cout << "i = " << i << ",";
			if (order_book[i].bid_order_book.size() > 0)
			{
				std::cout << "bid_order_book.size()=";
				std::cout << order_book[i].bid_order_book.size() << ","
					<< "bid_order_price01:" << order_book[i].bid_order_book[0].price << ","
					<< "bid_order_volum01:" << order_book[i].bid_order_book[0].volume << ","
					<< "channel_no:" << order_book[i].channel_no << ","
					<< "last_price:" << order_book[i].last_price << ","
					<< "offer_order_book.size()=" << order_book[i].offer_order_book.size();
			}
			std::cout << std::endl;*/
		}
		//不需要手动释放内存
	}

	//MySpi初始化
	void Init()
	{
		//打开snapshot.csv文件
		OpenSnapshotCsvFile();
		//打开OrderBook.csv文件
		OpenOrderBookCsvFile();
	}

	//析构函数
	~MySpi04()
	{
		//如果文件打开了，析构时关闭
		if (_snapshotCsvFile != NULL)
		{
			fclose(_snapshotCsvFile);
			_snapshotCsvFile = NULL;
		}

		if (_orderBookCsvFile != NULL)
		{
			fclose(_orderBookCsvFile);
			_orderBookCsvFile == NULL;
		}
	}

private:
	//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
	void OpenSnapshotCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/Snapshot.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf(": %s does exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen snapshot.csv");
				return;
			}
			/*时间，股票代码，买一价格，卖一价格，买一量，卖一量，还有总成交量，总成交金额,总成交笔数*/
			fprintf(fp, "%-10s%c", "market", sep);
			fprintf(fp, "%-10s%c", "code", sep);
			fprintf(fp, "%-18s%c", "time", sep);
			fprintf(fp, "%-15s%c", "last_price", sep);
			fprintf(fp, "%-15s%c", "bid_price01", sep);
			fprintf(fp, "%-15s%c", "bid_volume01", sep);
			fprintf(fp, "%-15s%c", "offer_price01", sep);
			fprintf(fp, "%-15s%c", "offer_volume01", sep);
			fprintf(fp, "%-10s%c", "num_trades", sep);
			fprintf(fp, "%-18s%c", "total_volume_trade", sep);
			fprintf(fp, "%-16s%c", "total_value_trade", sep);
			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			//printf("MarketDat.csv exist\n");
			fclose(fp);
		}

		//追加方式打开文件
		_snapshotCsvFile = fopen(filename, "a");
	}

	//OnMDSnapShot()中调用的写文件函数
	void WriteSnapshotToFile(amd::ama::MDSnapshot* snapshot, FILE* fp)
	{
		char sep = ',';
		char market[20] = { 0 };
		//市场类型
		memset(market, 0, sizeof(market));
		switch (snapshot->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%-10s,", market);

		//股票代码
		fprintf(fp, "%-10s%c", snapshot->security_code, sep);
		//时间
		fprintf(fp, "%-18lld%c", snapshot->orig_time, sep);
		//最新价
		fprintf(fp, "%-15lld%c", snapshot->last_price, sep);
		//买一价格
		fprintf(fp, "%-15lld%c", snapshot->bid_price[0], sep);
		//买一量
		fprintf(fp, "%-15lld%c", snapshot->bid_volume[0], sep);
		//卖一价格
		fprintf(fp, "%-15lld%c", snapshot->offer_price[0], sep);
		//卖一量
		fprintf(fp, "%-15lld%c", snapshot->offer_volume[0], sep);
		//成交笔数
		fprintf(fp, "%-10lld%c", snapshot->num_trades, sep);
		//成交总量
		fprintf(fp, "%-18lld%c", snapshot->total_volume_trade, sep);
		//成交总金额
		fprintf(fp, "%-16lld", snapshot->total_value_trade);
		fputc('\n', fp);

		return;
	}

	//打开OrderBook.Csv文件
	void OpenOrderBookCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/OrderBook.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf("OpenOrderBookCsvFile: %s does not exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen orderbook.csv");
				return;
			}
			
			fprintf(fp, "%s%c", "channel_no", sep);
			fprintf(fp, "%s%c", "market_type", sep);
			fprintf(fp, "%s%c", "security_code", sep);
			fprintf(fp, "%s%c", "last_tick_time", sep);
			fprintf(fp, "%s%c", "last_snapshot_time", sep);
			fprintf(fp, "%s%c", "last_tick_seq", sep);
			//买委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue", sep);
			}
			//卖委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue", sep);
			}
			fprintf(fp, "%s%c", "total_num_trades", sep);
			fprintf(fp, "%s%c", "total_volume_trade", sep);
			fprintf(fp, "%s%c", "total_value_trade", sep);
			fprintf(fp, "%s%c", "last_price", sep);

			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			fclose(fp);
		}

		//追加方式打开文件
		_orderBookCsvFile = fopen(filename, "a");
	}

	////往OrderBook.csv文件写一行数据
	void WriteOrderBookToFile(amd::ama::MDOrderBook* orderBook, FILE* fp)
	{
		int32_t bid_order_size = orderBook->bid_order_book.size();
		int32_t offer_order_size = orderBook->offer_order_book.size();
		//买委托簿为空，退出
		if (bid_order_size <= 0)
		{
			return;
		}
		char sep = ',';
		
		fprintf(fp, "%d%c", orderBook->channel_no, sep);
		//市场类型
		char market[10] = { 0 };
		memset(market, 0, sizeof(market));
		switch (orderBook->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%s,", market);
		
		fprintf(fp, "%s%c", orderBook->security_code, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_snapshot_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_seq, sep);

		//买委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{	
			if (i < bid_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->bid_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		//卖委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{
			if (i < offer_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->offer_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		fprintf(fp, "%lld%c", orderBook->total_num_trades, sep);
		fprintf(fp, "%lld%c", orderBook->total_volume_trade, sep);
		fprintf(fp, "%lld%c", orderBook->total_value_trade, sep);
		fprintf(fp, "%lld%c", orderBook->last_price, sep);

		fputc('\n', fp);
		return;
	}

	


private:
	FILE* _snapshotCsvFile;
	FILE* _orderBookCsvFile;
};


//按数据品种以及数据类型订阅实时行情数据
int32_t SubscribeWithCategory04()
{
	/*
	按品种类型订阅信息设置:
	1. 订阅信息分三个维度 market:市场, data_type:证券数据类型, category_type:品种类型, security_code:证券代码
	2. 订阅操作有三种:
		kSet 设置订阅, 覆盖所有订阅信息
		kAdd 增加订阅, 在前一个基础上增加订阅信息
		kDel 删除订阅, 在前一个基础上删除订阅信息
		kCancelAll 取消所有订阅信息
	*/
	amd::ama::SubscribeCategoryItem sub1[2];
	memset(sub1, 0, sizeof(sub1));

	/* 订阅深交所全部证券代码的股票现货快照 */
	sub1[0].market = amd::ama::MarketType::kSZSE;
	sub1[0].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[0].security_code[0] = '\0';

	/* 订阅上交所全部证券代码的股票现货快照 */
	sub1[1].market = amd::ama::MarketType::kSSE;
	sub1[1].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[1].security_code[0] = '\0';

	amd::ama::SubscribeCategoryItem sub2[2];
	memset(sub2, 0, sizeof(sub2));

	/* 订阅上交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[0].market = amd::ama::MarketType::kSSE;
	sub2[0].data_type = amd::ama::SubscribeSecuDataType::kTickOrder 
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[0].security_code[0] = '\0';
	/* 订阅深交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[1].market = amd::ama::MarketType::kSZSE;
	sub2[1].data_type = amd::ama::SubscribeSecuDataType::kTickOrder
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[1].security_code[0] = '\0';
	//strcpy(sub2[1].security_code, "600004");

	/* 发起订阅 */
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kSet, sub1, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub2, 2);

	return 0;
}

//衍生数据订阅
int32_t SubDerivedData04()
{
	amd::ama::SubscribeDerivedDataItem sub[2];
	memset(sub, 0, sizeof(sub));

	//订阅深交所000001/上交所60000证券代码
	sub[0].market = amd::ama::MarketType::kSZSE;
	strcpy(sub[0].security_code, "000001");
	//sub[0].security_code[0] = '\0';

	sub[1].market = amd::ama::MarketType::kSSE;
	strcpy(sub[1].security_code, "600000");
	//sub[1].security_code[0] = '\0';

	//发起订阅委托簿类型的数据
	return amd::ama::IAMDApi::SubscribeDerivedData(amd::ama::SubscribeType::kSet,
		amd::ama::SubscribeDerivedDataType::kOrderBook, sub, 2);
}

// 继承IAMDSpi，实现自己的回调处理类
class MySpi05 : public amd::ama::IAMDSpi
{
public:
	// 定义日志回调处理方法
	virtual void OnLog(const int32_t& level,
		const char* log,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA log: " << "    level: " << level << "    log:   " << log << std::endl;
	}

	// 定义监控回调处理方法
	virtual void OnIndicator(const char* indicator,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA indicator: " << indicator << std::endl;
	}

	// 定义事件回调处理方法  level 对照 EventLevel 数据类型 code 对照 EventCode 数据类型
	virtual void OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len) override
	{
		std::cout << "AMA event: " << event_msg << std::endl;
	}

	// 定义快照数据回调处理方法
	virtual void OnMDSnapshot(amd::ama::MDSnapshot* snapshot,
		uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			WriteSnapshotToFile(&snapshot[i], _snapshotCsvFile);
			//std::cout << amd::ama::Tools::Serialize(snapshot[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshot);
	}
	//
	virtual void OnMDFutureSnapshot(amd::ama::MDFutureSnapshot* snapshots, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
			std::cout << snapshots->security_code << "==" << snapshots->last_price << "==" << snapshots->total_volume_trade << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}
	//定义指数快照数据回调处理方法
	virtual void OnMDIndexSnapshot(amd::ama::MDIndexSnapshot* snapshots, uint32_t cnt)
	{

		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}

	//定义逐笔委托数据回调处理方法
	virtual void OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(ticks[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(ticks);
	}

	// 定义逐笔成交数据回调处理方法
	virtual void OnMDTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(tick[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(tick);
	}

	//定义委托簿数据回调处理方法(本地构建模式下非单线程递交, cfg.thread_num为递交委托簿数据线程数, 服务端推送模式下为单线程递交)
	virtual void OnMDOrderBook(std::vector<amd::ama::MDOrderBook>& order_book)
	{
		std::lock_guard<std::mutex> _(g_mutex);
		//std::cout << "order_book size = " << order_book.size() << std::endl;

		for (size_t i = 0; i < order_book.size(); i++)
		{
			//往OrderBook.csv文件写一行数据
			WriteOrderBookToFile(&order_book[i], _orderBookCsvFile);
			/*std::cout << "i = " << i << ",";
			if (order_book[i].bid_order_book.size() > 0)
			{
				std::cout << "bid_order_book.size()=";
				std::cout << order_book[i].bid_order_book.size() << ","
					<< "bid_order_price01:" << order_book[i].bid_order_book[0].price << ","
					<< "bid_order_volum01:" << order_book[i].bid_order_book[0].volume << ","
					<< "channel_no:" << order_book[i].channel_no << ","
					<< "last_price:" << order_book[i].last_price << ","
					<< "offer_order_book.size()=" << order_book[i].offer_order_book.size();
			}
			std::cout << std::endl;*/
		}
		//不需要手动释放内存
	}

	//MySpi初始化
	void Init()
	{
		//打开snapshot.csv文件
		OpenSnapshotCsvFile();
		//打开OrderBook.csv文件
		OpenOrderBookCsvFile();
	}

	//析构函数
	~MySpi05()
	{
		//如果文件打开了，析构时关闭
		if (_snapshotCsvFile != NULL)
		{
			fclose(_snapshotCsvFile);
			_snapshotCsvFile = NULL;
		}

		if (_orderBookCsvFile != NULL)
		{
			fclose(_orderBookCsvFile);
			_orderBookCsvFile == NULL;
		}
	}

private:
	//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
	void OpenSnapshotCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/Snapshot.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf(": %s does exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen snapshot.csv");
				return;
			}
			/*时间，股票代码，买一价格，卖一价格，买一量，卖一量，还有总成交量，总成交金额,总成交笔数*/
			fprintf(fp, "%-10s%c", "market", sep);
			fprintf(fp, "%-10s%c", "code", sep);
			fprintf(fp, "%-18s%c", "time", sep);
			fprintf(fp, "%-15s%c", "last_price", sep);
			fprintf(fp, "%-15s%c", "bid_price01", sep);
			fprintf(fp, "%-15s%c", "bid_volume01", sep);
			fprintf(fp, "%-15s%c", "offer_price01", sep);
			fprintf(fp, "%-15s%c", "offer_volume01", sep);
			fprintf(fp, "%-10s%c", "num_trades", sep);
			fprintf(fp, "%-18s%c", "total_volume_trade", sep);
			fprintf(fp, "%-16s%c", "total_value_trade", sep);
			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			//printf("MarketDat.csv exist\n");
			fclose(fp);
		}

		//追加方式打开文件
		_snapshotCsvFile = fopen(filename, "a");
	}

	//OnMDSnapShot()中调用的写文件函数
	void WriteSnapshotToFile(amd::ama::MDSnapshot* snapshot, FILE* fp)
	{
		char sep = ',';
		char market[20] = { 0 };
		//市场类型
		memset(market, 0, sizeof(market));
		switch (snapshot->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%-10s,", market);

		//股票代码
		fprintf(fp, "%-10s%c", snapshot->security_code, sep);
		//时间
		fprintf(fp, "%-18lld%c", snapshot->orig_time, sep);
		//最新价
		fprintf(fp, "%-15lld%c", snapshot->last_price, sep);
		//买一价格
		fprintf(fp, "%-15lld%c", snapshot->bid_price[0], sep);
		//买一量
		fprintf(fp, "%-15lld%c", snapshot->bid_volume[0], sep);
		//卖一价格
		fprintf(fp, "%-15lld%c", snapshot->offer_price[0], sep);
		//卖一量
		fprintf(fp, "%-15lld%c", snapshot->offer_volume[0], sep);
		//成交笔数
		fprintf(fp, "%-10lld%c", snapshot->num_trades, sep);
		//成交总量
		fprintf(fp, "%-18lld%c", snapshot->total_volume_trade, sep);
		//成交总金额
		fprintf(fp, "%-16lld", snapshot->total_value_trade);
		fputc('\n', fp);

		return;
	}

	//打开OrderBook.Csv文件
	void OpenOrderBookCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/OrderBook.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf("OpenOrderBookCsvFile: %s does not exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen orderbook.csv");
				return;
			}
			
			fprintf(fp, "%s%c", "channel_no", sep);
			fprintf(fp, "%s%c", "market_type", sep);
			fprintf(fp, "%s%c", "security_code", sep);
			fprintf(fp, "%s%c", "last_tick_time", sep);
			fprintf(fp, "%s%c", "last_snapshot_time", sep);
			fprintf(fp, "%s%c", "last_tick_seq", sep);
			//买委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue", sep);
			}
			//卖委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue", sep);
			}
			fprintf(fp, "%s%c", "total_num_trades", sep);
			fprintf(fp, "%s%c", "total_volume_trade", sep);
			fprintf(fp, "%s%c", "total_value_trade", sep);
			fprintf(fp, "%s%c", "last_price", sep);

			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			fclose(fp);
		}

		//追加方式打开文件
		_orderBookCsvFile = fopen(filename, "a");
	}

	////往OrderBook.csv文件写一行数据
	void WriteOrderBookToFile(amd::ama::MDOrderBook* orderBook, FILE* fp)
	{
		int32_t bid_order_size = orderBook->bid_order_book.size();
		int32_t offer_order_size = orderBook->offer_order_book.size();
		//买委托簿为空，退出
		if (bid_order_size <= 0)
		{
			return;
		}
		char sep = ',';
		
		fprintf(fp, "%d%c", orderBook->channel_no, sep);
		//市场类型
		char market[10] = { 0 };
		memset(market, 0, sizeof(market));
		switch (orderBook->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%s,", market);
		
		fprintf(fp, "%s%c", orderBook->security_code, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_snapshot_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_seq, sep);

		//买委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{	
			if (i < bid_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->bid_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		//卖委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{
			if (i < offer_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->offer_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		fprintf(fp, "%lld%c", orderBook->total_num_trades, sep);
		fprintf(fp, "%lld%c", orderBook->total_volume_trade, sep);
		fprintf(fp, "%lld%c", orderBook->total_value_trade, sep);
		fprintf(fp, "%lld%c", orderBook->last_price, sep);

		fputc('\n', fp);
		return;
	}

	


private:
	FILE* _snapshotCsvFile;
	FILE* _orderBookCsvFile;
};


//按数据品种以及数据类型订阅实时行情数据
int32_t SubscribeWithCategory05()
{
	/*
	按品种类型订阅信息设置:
	1. 订阅信息分三个维度 market:市场, data_type:证券数据类型, category_type:品种类型, security_code:证券代码
	2. 订阅操作有三种:
		kSet 设置订阅, 覆盖所有订阅信息
		kAdd 增加订阅, 在前一个基础上增加订阅信息
		kDel 删除订阅, 在前一个基础上删除订阅信息
		kCancelAll 取消所有订阅信息
	*/
	amd::ama::SubscribeCategoryItem sub1[2];
	memset(sub1, 0, sizeof(sub1));

	/* 订阅深交所全部证券代码的股票现货快照 */
	sub1[0].market = amd::ama::MarketType::kSZSE;
	sub1[0].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[0].security_code[0] = '\0';

	/* 订阅上交所全部证券代码的股票现货快照 */
	sub1[1].market = amd::ama::MarketType::kSSE;
	sub1[1].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[1].security_code[0] = '\0';

	amd::ama::SubscribeCategoryItem sub2[2];
	memset(sub2, 0, sizeof(sub2));

	/* 订阅上交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[0].market = amd::ama::MarketType::kSSE;
	sub2[0].data_type = amd::ama::SubscribeSecuDataType::kTickOrder 
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[0].security_code[0] = '\0';
	/* 订阅深交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[1].market = amd::ama::MarketType::kSZSE;
	sub2[1].data_type = amd::ama::SubscribeSecuDataType::kTickOrder
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[1].security_code[0] = '\0';
	//strcpy(sub2[1].security_code, "600004");

	/* 发起订阅 */
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kSet, sub1, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub2, 2);

	return 0;
}

//衍生数据订阅
int32_t SubDerivedData05()
{
	amd::ama::SubscribeDerivedDataItem sub[2];
	memset(sub, 0, sizeof(sub));

	//订阅深交所000001/上交所60000证券代码
	sub[0].market = amd::ama::MarketType::kSZSE;
	strcpy(sub[0].security_code, "000001");
	//sub[0].security_code[0] = '\0';

	sub[1].market = amd::ama::MarketType::kSSE;
	strcpy(sub[1].security_code, "600000");
	//sub[1].security_code[0] = '\0';

	//发起订阅委托簿类型的数据
	return amd::ama::IAMDApi::SubscribeDerivedData(amd::ama::SubscribeType::kSet,
		amd::ama::SubscribeDerivedDataType::kOrderBook, sub, 2);
}

// 继承IAMDSpi，实现自己的回调处理类
class MySpi06 : public amd::ama::IAMDSpi
{
public:
	// 定义日志回调处理方法
	virtual void OnLog(const int32_t& level,
		const char* log,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA log: " << "    level: " << level << "    log:   " << log << std::endl;
	}

	// 定义监控回调处理方法
	virtual void OnIndicator(const char* indicator,
		uint32_t len) override
	{
		std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "AMA indicator: " << indicator << std::endl;
	}

	// 定义事件回调处理方法  level 对照 EventLevel 数据类型 code 对照 EventCode 数据类型
	virtual void OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len) override
	{
		std::cout << "AMA event: " << event_msg << std::endl;
	}

	// 定义快照数据回调处理方法
	virtual void OnMDSnapshot(amd::ama::MDSnapshot* snapshot,
		uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			WriteSnapshotToFile(&snapshot[i], _snapshotCsvFile);
			//std::cout << amd::ama::Tools::Serialize(snapshot[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshot);
	}
	//
	virtual void OnMDFutureSnapshot(amd::ama::MDFutureSnapshot* snapshots, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
			std::cout << snapshots->security_code << "==" << snapshots->last_price << "==" << snapshots->total_volume_trade << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}
	//定义指数快照数据回调处理方法
	virtual void OnMDIndexSnapshot(amd::ama::MDIndexSnapshot* snapshots, uint32_t cnt)
	{

		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(snapshots);
	}

	//定义逐笔委托数据回调处理方法
	virtual void OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(ticks[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(ticks);
	}

	// 定义逐笔成交数据回调处理方法
	virtual void OnMDTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt) override
	{
		for (uint32_t i = 0; i < cnt; ++i)
		{
			std::lock_guard<std::mutex> _(g_mutex);
			//std::cout << amd::ama::Tools::Serialize(tick[i]) << std::endl;
		}

		/* 手动释放数据内存, 否则会造成内存泄露 */
		amd::ama::IAMDApi::FreeMemory(tick);
	}

	//定义委托簿数据回调处理方法(本地构建模式下非单线程递交, cfg.thread_num为递交委托簿数据线程数, 服务端推送模式下为单线程递交)
	virtual void OnMDOrderBook(std::vector<amd::ama::MDOrderBook>& order_book)
	{
		std::lock_guard<std::mutex> _(g_mutex);
		//std::cout << "order_book size = " << order_book.size() << std::endl;

		for (size_t i = 0; i < order_book.size(); i++)
		{
			//往OrderBook.csv文件写一行数据
			WriteOrderBookToFile(&order_book[i], _orderBookCsvFile);
			/*std::cout << "i = " << i << ",";
			if (order_book[i].bid_order_book.size() > 0)
			{
				std::cout << "bid_order_book.size()=";
				std::cout << order_book[i].bid_order_book.size() << ","
					<< "bid_order_price01:" << order_book[i].bid_order_book[0].price << ","
					<< "bid_order_volum01:" << order_book[i].bid_order_book[0].volume << ","
					<< "channel_no:" << order_book[i].channel_no << ","
					<< "last_price:" << order_book[i].last_price << ","
					<< "offer_order_book.size()=" << order_book[i].offer_order_book.size();
			}
			std::cout << std::endl;*/
		}
		//不需要手动释放内存
	}

	//MySpi初始化
	void Init()
	{
		//打开snapshot.csv文件
		OpenSnapshotCsvFile();
		//打开OrderBook.csv文件
		OpenOrderBookCsvFile();
	}

	//析构函数
	~MySpi06()
	{
		//如果文件打开了，析构时关闭
		if (_snapshotCsvFile != NULL)
		{
			fclose(_snapshotCsvFile);
			_snapshotCsvFile = NULL;
		}

		if (_orderBookCsvFile != NULL)
		{
			fclose(_orderBookCsvFile);
			_orderBookCsvFile == NULL;
		}
	}

private:
	//打开snapshot.csv文件，文件指针赋给成员变量_snapshotCsvFile
	void OpenSnapshotCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/Snapshot.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf(": %s does exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen snapshot.csv");
				return;
			}
			/*时间，股票代码，买一价格，卖一价格，买一量，卖一量，还有总成交量，总成交金额,总成交笔数*/
			fprintf(fp, "%-10s%c", "market", sep);
			fprintf(fp, "%-10s%c", "code", sep);
			fprintf(fp, "%-18s%c", "time", sep);
			fprintf(fp, "%-15s%c", "last_price", sep);
			fprintf(fp, "%-15s%c", "bid_price01", sep);
			fprintf(fp, "%-15s%c", "bid_volume01", sep);
			fprintf(fp, "%-15s%c", "offer_price01", sep);
			fprintf(fp, "%-15s%c", "offer_volume01", sep);
			fprintf(fp, "%-10s%c", "num_trades", sep);
			fprintf(fp, "%-18s%c", "total_volume_trade", sep);
			fprintf(fp, "%-16s%c", "total_value_trade", sep);
			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			//printf("MarketDat.csv exist\n");
			fclose(fp);
		}

		//追加方式打开文件
		_snapshotCsvFile = fopen(filename, "a");
	}

	//OnMDSnapShot()中调用的写文件函数
	void WriteSnapshotToFile(amd::ama::MDSnapshot* snapshot, FILE* fp)
	{
		char sep = ',';
		char market[20] = { 0 };
		//市场类型
		memset(market, 0, sizeof(market));
		switch (snapshot->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%-10s,", market);

		//股票代码
		fprintf(fp, "%-10s%c", snapshot->security_code, sep);
		//时间
		fprintf(fp, "%-18lld%c", snapshot->orig_time, sep);
		//最新价
		fprintf(fp, "%-15lld%c", snapshot->last_price, sep);
		//买一价格
		fprintf(fp, "%-15lld%c", snapshot->bid_price[0], sep);
		//买一量
		fprintf(fp, "%-15lld%c", snapshot->bid_volume[0], sep);
		//卖一价格
		fprintf(fp, "%-15lld%c", snapshot->offer_price[0], sep);
		//卖一量
		fprintf(fp, "%-15lld%c", snapshot->offer_volume[0], sep);
		//成交笔数
		fprintf(fp, "%-10lld%c", snapshot->num_trades, sep);
		//成交总量
		fprintf(fp, "%-18lld%c", snapshot->total_volume_trade, sep);
		//成交总金额
		fprintf(fp, "%-16lld", snapshot->total_value_trade);
		fputc('\n', fp);

		return;
	}

	//打开OrderBook.Csv文件
	void OpenOrderBookCsvFile()
	{
        ConfigFile cfgFile;
        std::string path;
        if (cfgFile.Load("../data/config.conf") == true)
        {
            path = cfgFile.Get("filePath");
        }
        std::string fileNameStr = path + "/OrderBook.csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf("OpenOrderBookCsvFile: %s does not exist, create a new one\n", filename);
			char sep = ',';
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen orderbook.csv");
				return;
			}
			
			fprintf(fp, "%s%c", "channel_no", sep);
			fprintf(fp, "%s%c", "market_type", sep);
			fprintf(fp, "%s%c", "security_code", sep);
			fprintf(fp, "%s%c", "last_tick_time", sep);
			fprintf(fp, "%s%c", "last_snapshot_time", sep);
			fprintf(fp, "%s%c", "last_tick_seq", sep);
			//买委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "bid_order", i, "_queue", sep);
			}
			//卖委托簿，2档，每档的委托队列有2个数据
			for (int i = 1; i <= 2; i++)
			{
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_price", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_volume", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue_size", sep);
				fprintf(fp, "%s%02d%s%c", "offer_order", i, "_queue", sep);
			}
			fprintf(fp, "%s%c", "total_num_trades", sep);
			fprintf(fp, "%s%c", "total_volume_trade", sep);
			fprintf(fp, "%s%c", "total_value_trade", sep);
			fprintf(fp, "%s%c", "last_price", sep);

			fputc('\n', fp);

			fclose(fp);
		}
		else
		{
			fclose(fp);
		}

		//追加方式打开文件
		_orderBookCsvFile = fopen(filename, "a");
	}

	////往OrderBook.csv文件写一行数据
	void WriteOrderBookToFile(amd::ama::MDOrderBook* orderBook, FILE* fp)
	{
		int32_t bid_order_size = orderBook->bid_order_book.size();
		int32_t offer_order_size = orderBook->offer_order_book.size();
		//买委托簿为空，退出
		if (bid_order_size <= 0)
		{
			return;
		}
		char sep = ',';
		
		fprintf(fp, "%d%c", orderBook->channel_no, sep);
		//市场类型
		char market[10] = { 0 };
		memset(market, 0, sizeof(market));
		switch (orderBook->market_type)
		{
		case amd::ama::MarketType::kNEEQ:
			strcpy(market, "NEEQ");
			break;
		case amd::ama::MarketType::kSHFE:
			strcpy(market, "SHFE");
			break;
		case amd::ama::MarketType::kCFFEX:
			strcpy(market, "CFFEX");
			break;
		case amd::ama::MarketType::kDCE:
			strcpy(market, "DCE");
			break;
		case amd::ama::MarketType::kCZCE:
			strcpy(market, "CZCE");
			break;
		case amd::ama::MarketType::kINE:
			strcpy(market, "INE");
			break;
		case amd::ama::MarketType::kSSE:
			strcpy(market, "SSE");
			break;
		case amd::ama::MarketType::kSZSE:
			strcpy(market, "SZSE");
			break;
		case amd::ama::MarketType::kHKEx:
			strcpy(market, "HKEx");
			break;
		case amd::ama::MarketType::kCFETS:
			strcpy(market, "CFETS");
			break;
		default:
			break;
		}
		fprintf(fp, "%s,", market);
		
		fprintf(fp, "%s%c", orderBook->security_code, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_snapshot_time, sep);
		fprintf(fp, "%lld%c", orderBook->last_tick_seq, sep);

		//买委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{	
			if (i < bid_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->bid_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->bid_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		//卖委托簿最多2档
		for (int32_t i = 0; i < 2; i++)
		{
			if (i < offer_order_size)
			{
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].price, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].volume, sep);
				fprintf(fp, "%lld%c", orderBook->offer_order_book[i].order_queue_size, sep);

				//委托队列里有order_queue_size个数据
				std::string orderQueue;
				for (int j = 0; j < orderBook->offer_order_book[i].order_queue_size; j++)
				{
					orderQueue += std::to_string(orderBook->bid_order_book[i].order_queue[j]);
					orderQueue += " ";
				}
				fprintf(fp, "%s%c", orderQueue.c_str(), sep);
			}
			else
			{
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
				fprintf(fp, "%lld%c", 0ll, sep);
			}
		}

		fprintf(fp, "%lld%c", orderBook->total_num_trades, sep);
		fprintf(fp, "%lld%c", orderBook->total_volume_trade, sep);
		fprintf(fp, "%lld%c", orderBook->total_value_trade, sep);
		fprintf(fp, "%lld%c", orderBook->last_price, sep);

		fputc('\n', fp);
		return;
	}

	


private:
	FILE* _snapshotCsvFile;
	FILE* _orderBookCsvFile;
};


//按数据品种以及数据类型订阅实时行情数据
int32_t SubscribeWithCategory06()
{
	/*
	按品种类型订阅信息设置:
	1. 订阅信息分三个维度 market:市场, data_type:证券数据类型, category_type:品种类型, security_code:证券代码
	2. 订阅操作有三种:
		kSet 设置订阅, 覆盖所有订阅信息
		kAdd 增加订阅, 在前一个基础上增加订阅信息
		kDel 删除订阅, 在前一个基础上删除订阅信息
		kCancelAll 取消所有订阅信息
	*/
	amd::ama::SubscribeCategoryItem sub1[2];
	memset(sub1, 0, sizeof(sub1));

	/* 订阅深交所全部证券代码的股票现货快照 */
	sub1[0].market = amd::ama::MarketType::kSZSE;
	sub1[0].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[0].security_code[0] = '\0';

	/* 订阅上交所全部证券代码的股票现货快照 */
	sub1[1].market = amd::ama::MarketType::kSSE;
	sub1[1].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[1].security_code[0] = '\0';

	amd::ama::SubscribeCategoryItem sub2[2];
	memset(sub2, 0, sizeof(sub2));

	/* 订阅上交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[0].market = amd::ama::MarketType::kSSE;
	sub2[0].data_type = amd::ama::SubscribeSecuDataType::kTickOrder 
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[0].security_code[0] = '\0';
	/* 订阅深交所全部股票的逐笔委托和逐笔成交数据  */
	sub2[1].market = amd::ama::MarketType::kSZSE;
	sub2[1].data_type = amd::ama::SubscribeSecuDataType::kTickOrder
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub2[1].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub2[1].security_code[0] = '\0';
	//strcpy(sub2[1].security_code, "600004");

	/* 发起订阅 */
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kSet, sub1, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub2, 2);

	return 0;
}

//衍生数据订阅
int32_t SubDerivedData06()
{
	amd::ama::SubscribeDerivedDataItem sub[2];
	memset(sub, 0, sizeof(sub));

	//订阅深交所000001/上交所60000证券代码
	sub[0].market = amd::ama::MarketType::kSZSE;
	strcpy(sub[0].security_code, "000001");
	//sub[0].security_code[0] = '\0';

	sub[1].market = amd::ama::MarketType::kSSE;
	strcpy(sub[1].security_code, "600000");
	//sub[1].security_code[0] = '\0';

	//发起订阅委托簿类型的数据
	return amd::ama::IAMDApi::SubscribeDerivedData(amd::ama::SubscribeType::kSet,
		amd::ama::SubscribeDerivedDataType::kOrderBook, sub, 2);
}


}//end of namespace fill

int main()
{
	int ret;
	ret = StrategyPlatform();

	return ret;
}

