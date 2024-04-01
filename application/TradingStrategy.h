#pragma once

#include <dirent.h>
#include <vector>
#include <array>
#include <deque>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include "ama.h"
#include "sp_type.h"
#include "MDApplication.h"

using std::string;
using std::vector;
using std::unordered_map;
using std::deque;

struct TradeVolume
{
	uint64_t _buyVolume;
	uint64_t _sellVolume;
	bool _tradeComplete;
public:
	//默认构造
	TradeVolume()
	{
		_buyVolume = 0;
		_sellVolume = 0;
		_tradeComplete = false;
	}
	//有参构造
	TradeVolume(int64_t bV, int64_t sV)
		:_buyVolume(bV), _sellVolume(sV)
	{
		_tradeComplete = false;
	}
};

struct TradingResult
{
	//证券代码
	//char _securityCode[amd::ama::ConstField::kSecurityCodeLen];
	uint32_t _securityCode;
	//买还是卖,B买，S卖
	char _buyOrSell;
	//价格
	uint32_t _price;
	//数量
	uint64_t _volumn;
	//是第几次交易
	uint32_t _tradeCount;
	//交易时间
	uint32_t _tradeTime;
	//备注信息
	string _infoTrade;
};

class TradingStrategy : public MDApplication
{
public:
	//构造函数
	TradingStrategy();

	//void IsVirtualMarket();

	//初始化
	//void Init();
	virtual int BeforeMarket() override;

	//QuickSnap数据的回调函数
	virtual void OnQuickSnap(QuickSnapInfo* infos, uint32_t cnt) override;

	//盘后处理
	virtual int AfterMarket() override;

	//析构函数
	virtual ~TradingStrategy() override;

private:
	//从文件读取任务，根据证券代码和开始时间，加入任务数组，
	void GetTradeTask(FILE* fp, int32_t startTim);

	/*根据两个时间戳，计算中间有多少个分钟，有多少交易次数，必须在一定时间范围交易
	时间格式hhmm*/
	int32_t CountOfTrade(int32_t tim1, int32_t tim2);

	//计算两个时间戳之间的时间间隔，返回值是以分钟为单位
	int32_t TimeLag(int32_t hour1, int32_t min1, int32_t hour2, int32_t min2);

	//把某个证券的交易任务添加到_securityAndTask，如果有同一证券的多个任务，进行合并
	void AddTradeTask(uint32_t code, int32_t startIdx, uint64_t onceVolume, int32_t cnt, char flag);

	//线程函数，监控目录下的文件，有新文件就提取新任务加入队列
	void MonitorFilesThreadFunc();

	//退出监控的线程
	void StopMonitoring();

	//根据时间戳获得是第几次交易，返回在布尔数组里的下标(hhmm形式)
	int32_t GetTradingSequence(int32_t tim);

	//执行交易
	//void DoTrade(const string &code, int32_t tradSep, const amd::ama::MDOrderBook& orderBook);
	void DoTrade(const uint32_t& code, int32_t tradSep, const QuickSnapInfo& info);

	//void Buy(const string& code, int32_t trdSeq, const amd::ama::MDOrderBook& orderBook);
	void Buy(const uint32_t& code, int32_t trdSeq, const QuickSnapInfo& orderBook);

	//void Sell(const string& code, int32_t trdSeq, const amd::ama::MDOrderBook& orderBook);
	void Sell(const uint32_t& code, int32_t trdSeq, const QuickSnapInfo& orderBook);

	//void BuyAndSell(const string& code, int32_t trdSeq, const amd::ama::MDOrderBook& orderBook);
	void BuyAndSell(const uint32_t& code, int32_t trdSeq, const QuickSnapInfo& orderBook);

	//打开要写的文件
	void OpenTradeResultFile();

	//把交易结果写入文件
	void WriteTradeResultToFile(const TradingResult& trdRet);

	//从文件名字符串获得开始时间
	int32_t getStartTimFromFileName(const string& fileName);

	//从证券代码获得市场类型
	int32_t GetMarketTypeByCode(uint32_t code);

private:
	//每个证券的交易任务
	//unordered_map<string, vector<TradeVolume>> _securityAndTask;
	unordered_map<uint32_t, vector<TradeVolume>> _securityAndTask;

	//任务文件的目录
	string _taskFilePath;

	//打开的目录
	//DIR* _dir;

	//目录下所有文件名，用来检测有没有新文件
	std::unordered_set<string> _allTaskFiles;

	//每个时间段的交易状态，是否完成交易
	unordered_map<string, vector<bool>> _tradingComplete;

	//最新时间戳
	uint32_t _timeSZSE;
	uint32_t _timeSSE;
	//int32_t _timeMarket;

	//交易次数
	uint32_t _countTrading;
	
	//委托簿的线程总个数
	int32_t _threadNumOrderBook;

	std::queue<FILE*> _orderBookCsvFiles;//存储一开始打开的所有orderBook文件指针，这是给OnOrderBook写的文件
	std::unordered_map<std::thread::id, FILE*> _threadWriteFile;//每个线程匹配一个要写的文件

	//监控的线程
	std::thread _threadMonitor;

	//如果flag为真，监控线程继续循环，为假，退出
	bool _flagMonit;

	//是实时行情还是模拟行情
	bool _virtualMarket;
};

