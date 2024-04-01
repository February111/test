#include <unistd.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include "TradingStrategy.h"
#include "ConfigFile.h"
#include "reflect.h"
#include "sp_tool.h"

//允许延迟10秒
#define ALLOW_DELAY 10

using std::cerr;
using std::cout;
using std::endl;
using std::istringstream;

//配置信息
extern ConfigFile g_cfgFile;//其他文件的全局变量

//静态全局变量，不同线程回调时，同步要用的锁
static std::mutex g_mutex;//增加交易次数时用到的锁
static std::mutex g_outMutex;//输出时用到的锁
static std::mutex g_taskQueue;//修改任务队列时用到的锁

TradingStrategy::TradingStrategy()
{
	_countTrading = 0;
	_timeSSE = 0;
	_timeSZSE = 0;
	_virtualMarket = false;
	_flagMonit = false;

	//_dir = NULL;
}

//void TradingStrategy::IsVirtualMarket()
//{
//	//必须在init()之前确定是否模拟
//	_virtualMarket = true;
//}

int TradingStrategy::BeforeMarket()
{
	//加载配置信息
	/*ConfigFile cfg;
	if (cfg.Load("../data/config.conf") == false)
	{
		cerr << "TradingStrategy::Init(): load config file failed\n" << endl;
		exit(1);
	}*/

	//判断是不是模拟行情
	string realTime("1");
	string workPattern = g_cfgFile.Get("workPattern");
	if (workPattern == string())
	{
		printf("not get workPattern int config file\n");
		return -1;
	}
	if (workPattern != realTime)
	{
		_virtualMarket = true;
	}

	//交易任务的目录
	string filePath = g_cfgFile.Get("TradingTaskFile");
	if (filePath == string())
	{
		cerr << "TradingStrategy::TradingStrategy(): TradingTaskFile not found in config file" << endl;
		return -1;
	}
	_taskFilePath = filePath;

	cout << "trade task filePath:" << filePath << endl;

	DIR* dirp;
	//打开目录，遍历所有文件
	dirp = opendir(filePath.c_str());
	if (dirp == NULL)
	{
		perror(" TradingStrategy::Init()::opendir");
		return -1;
	}

	struct dirent* entry;
	while ((entry = readdir(dirp)) != NULL)
	{
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
		{
			continue;
		}
		//如果是个目录，就不放进去，直接continue
		if (entry->d_type == DT_DIR)
		{
			continue;
		}

		//文件名放入_allFiles
		_allTaskFiles.emplace(string(entry->d_name));
	}
	closedir(dirp);

	FILE* taskFile;
	string fileName;
	//遍历所有文件，取出任务
	for (auto iter = _allTaskFiles.begin(); iter != _allTaskFiles.end(); ++iter)
	{
		
		int32_t startTim;
		//如果是播放行情，从文件名提取开始时间
		if (_virtualMarket)
		{
			startTim = getStartTimFromFileName(*iter);
			if (startTim == 0)
			{
				cerr << "can't get start time from file name " << *iter << endl;
				continue;
			}

			//如果早于九点半，就改为九点半
			if (startTim < 930)
			{
				startTim = 930;
			}
		}
		else
		{
			//如果是实时行情，从当前时间开始交易，如果未开市，就9：30开始交易
			startTim = GetLocalTm();
			startTim = startTim / 100000;//变成hhmm形式
			if (startTim < 930)
			{
				startTim = 930;
			}
		}

		printf("start time: %d\n", startTim);

		fileName = filePath + "/" + *iter;
		std::cout << "task file name: " << fileName << endl;

		taskFile = fopen(fileName.c_str(), "r");
		if (taskFile == NULL)
		{
			cerr << fileName << " open failed" << endl;
			continue;
		}

		//这个函数里会判断开始时间是否合法，不合法就不读任何数据
		GetTradeTask(taskFile, startTim);

		fclose(taskFile);
	}

	//委托簿的线程个数
	string threadNum = g_cfgFile.Get("OrderBookThreadNum");
	if (threadNum == string())
	{
		cerr << "VirtualMarket::VirutalMarket(): OrderBookThreadNum not found in config file" << endl;
		return -1;
	}
	_threadNumOrderBook = stoi(threadNum);

	//打开写交易结果的文件
	OpenTradeResultFile();

	//如果是实时场景，开启检测新文件的功能
	//监控目录下的文件，有新增的，就读文件添加新任务
	if (_virtualMarket == false)
	{
		_flagMonit = true;
		_threadMonitor = std::thread(&TradingStrategy::MonitorFilesThreadFunc, this);
		printf("_threadMonitor begin\n");
	}

	return 0;
}

//从文件名字符串获得开始时间
int32_t TradingStrategy::getStartTimFromFileName(const string& fileName)
{
	//cout << "getStartTimFromFileName fileName: " << fileName << endl;
	//找到下划线
	size_t idx = fileName.find('_', 0);
	if (idx == string::npos)
	{
		return 0;
	}

	string substr = fileName.substr(idx + 1);

	int32_t startTim = 0;
	//字符输入流
	istringstream iss(substr);
	iss >> startTim;

	return startTim;
}

//析构函数
TradingStrategy::~TradingStrategy()
{
	auto iter = _threadWriteFile.begin();
	for (; iter != _threadWriteFile.end(); ++iter)
	{
		fclose(iter->second);
	}

	//关闭目录
	//closedir(_dir);

	//等待线程退出
	//_threadMonitor.join();
}

//从文件读取任务，这个函数里会判断开始时间是否合法，不合法就不读任何数据
void TradingStrategy::GetTradeTask(FILE* taskFile, int32_t startTim)
{
	//printf("startTim : %d\n", startTim);

	if (taskFile == NULL)
	{
		return;
	}

	//判断开始时间合不合法
	if (!((startTim >= 930 && startTim < 1130) || (startTim >= 1300 && startTim <= 1455)))
	{
		cerr << "TradingStrategy::GetTradeTask() : start time invalid " << startTim << endl;
		return;
	}

	uint32_t code;
	uint64_t volume;
	//int32_t flag;
	char flag;
	int32_t finalTim;//在哪个时间之前完成交易任务
	//char security_code[16] = { 0 };
	char buf[64] = { 0 };
	int ret;

	//把首行表头读出来，之后再读数据
	char head[1024];
	fgets(head, sizeof(head), taskFile);
	//printf("get head: %s", head);

	while (feof(taskFile) == false)
	{
		ret = fscanf(taskFile, "%u,", &code);
		if (ret == -1)//文件读完了
		{
			break;
		}

		fscanf(taskFile, "%c,", &flag);
		fscanf(taskFile, "%lu,", &volume);
		fscanf(taskFile, "%d", &finalTim);

		//6位左补0
		//memset(security_code, 0, sizeof(security_code));
		//sprintf(security_code, "%06d", code);
		//printf("TradingStrategy::GetTradeTask(): %06u, %c, %lu, %d\n", code, flag, volume, finalTim);

		
		//截至时间合不合法,交易次数是从开始的这一分钟到结束的时间前一分钟
		if (!((finalTim > 930 && finalTim <= 1130) || (finalTim >= 1300 && finalTim <= 1457)))
		{
			//printf("%u, %lu, %d, %d : ", security_code, volume, flag, finalTim);
			printf("TradingStrategy::GetTradeTask() : final Tim invalid\n");
			continue;
		}

		//根据开始和截止时间，计算在规定交易时间里能进行的交易次数, 
		int32_t cnt = CountOfTrade(startTim, finalTim);
		if (cnt == 0)
		{
			cerr << "trade count = 0" << endl;
			continue;
		}

		//每次交易的数量
		uint64_t onceVolume = volume / cnt;

		//240个交易时间点，从哪一个开始，对应交易任务数组的下标
		int32_t startIdx = GetTradingSequence(startTim);
		if (startIdx == -1)
		{
			cerr << "TradingStrategy::GetTradeTask()：startTim err" << endl;
			return;
		}

		//添加交易任务
		AddTradeTask(code, startIdx, onceVolume, cnt, flag);

		//把一行读完
		fgets(buf, sizeof(buf), taskFile);
	}

	return;
}

/*根据两个时间戳，计算中间有多少个分钟，有多少交易次数，从开始时间的分钟交易，到结束时间-1的分钟结束，
* 比如1130结束其实最后一次交易在1129这一分钟
默认两个时间都合法，所以调用时必须传入合法时间
	时间格式hhmm*/
int32_t TradingStrategy::CountOfTrade(int32_t tim1, int32_t tim2)
{
	int32_t hour1, min1, hour2, min2;
	
	hour1 = tim1 / 100;
	min1 = tim1 % 100;

	hour2 = tim2 / 100;
	min2 = tim2 % 100;

	//两个时间的前后顺序
	if (hour2 < hour1 || (hour2 == hour1 && min2 < min1))
	{
		cerr << "end time is earlier than strat time " << endl;
		return 0;
	}

	int32_t ret;
	if (hour1 <= 11 && hour2 >= 14)
	{
		//两个时间中间经过了午休时间
		ret = TimeLag(hour1, min1, 11, 30) + TimeLag(13, 0, hour2, min2);
	}
	else
	{
		ret = TimeLag(hour1, min1, hour2, min2);
	}

	return ret;
}

//计算起始时间相差的分钟数（比如930和931之间相差1），不考虑休市等因素
int32_t TradingStrategy::TimeLag(int32_t hour1, int32_t min1, int32_t hour2, int32_t min2)
{
	int32_t ret = (hour2 - hour1) * 60 + min2 - min1;

	return ret;
}

//把某个证券的交易任务添加到_securityAndTask
void TradingStrategy::AddTradeTask(uint32_t security_code, int32_t startIdx, uint64_t onceVolume, int32_t cnt, char flag)
{
	//printf("startIdx = %d, cnt = %d, flag = %d\n", startIdx, cnt, flag);

	//uint32_t security_code = std::stoi(code);
	int i;

	std::lock_guard<std::mutex> _(g_taskQueue);//修改任务队列时，加锁

	//如果原来没有这个证券的任务数组，新建一个
	if (_securityAndTask.count(security_code) == 0)
	{
		_securityAndTask[security_code] = vector<TradeVolume>(240);
	}

	//从startIdx开始一共cnt个任务节点，添加交易的数量
	if (flag == 'B')//买
	{
		//cout << "flag = 'B'" << endl;
		for (i = startIdx; i < startIdx + cnt; i++)
		{
			
			_securityAndTask[security_code][i]._buyVolume += onceVolume;
			//printf("i = %d, buyvolume = %ld, flagcomplete = %d\n", i, _securityAndTask[security_code][i]._buyVolume, _securityAndTask[security_code][i]._tradeComplete);
		}
	}
	else if (flag == 'S')//卖
	{
		//cout << "flag = 0" << endl;
		for (i = startIdx; i < startIdx + cnt; i++)
		{
			_securityAndTask[security_code][i]._sellVolume += onceVolume;
			//printf("i = %d, sellvolume = %ld, flagcomplete = %d\n", i, _securityAndTask[security_code][i]._sellVolume, _securityAndTask[security_code][i]._tradeComplete);
		}
	}
	
	return;
}

//线程函数，监控目录下的文件，有新文件就提取新任务加入队列
void TradingStrategy::MonitorFilesThreadFunc()
{
	DIR* dirp;
	struct dirent* entry;
	FILE* taskFile;
	int32_t startTim;

	while (_flagMonit)
	{
		dirp = opendir(_taskFilePath.c_str());
		
		while ((entry = readdir(dirp)) != NULL)
		{
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			{
				continue;
			}

			//如果是个目录，就不管，直接continue
			if (entry->d_type == DT_DIR)
			{
				continue;
			}

			//如果是新增的文件
			string fileName(entry->d_name);
			if (_allTaskFiles.count(fileName) == 0)
			{
				//加入_allTaskFiles
				_allTaskFiles.insert(fileName);

				//如果是播放行情，从文件名提取开始时间
				if (_virtualMarket)
				{
					startTim = getStartTimFromFileName(fileName);
					if (startTim == 0)
					{
						cerr << "can't get start time from file name" << endl;
						continue;
					}
				}
				else
				{
					//如果是实时行情，从当前时间的下一分钟开始交易，避免修改正在交易的数据
					//startTim = (_timeSSE / 100000) % 10000;
					startTim = GetLocalTm();
					startTim = startTim / 100000;//变成hhmm形式
					if (startTim % 100 == 59)
					{
						startTim = ((startTim / 100) + 1) * 100;
					}
					else
					{
						startTim++;
					}
				}

				//如果开始时间过早，调整为九点半
				if (startTim < 930)
				{
					startTim = 930;
				}

				printf("new file, start time %d , ", startTim);

				fileName = _taskFilePath + "/" + fileName;
				std::cout << "new fileName: " << fileName << endl;

				//打开文件
				taskFile = fopen(fileName.c_str(), "r");
				if (taskFile == NULL)
				{
					cerr << fileName << " open failed" << endl;
					continue;
				}

				//提取任务
				GetTradeTask(taskFile, startTim);

				fclose(taskFile);
			}
		}

		closedir(dirp);
		
		//每30秒检测一次
		std::this_thread::sleep_for(std::chrono::seconds(30));
	}

	return;
}

//退出监控的线程
void TradingStrategy::StopMonitoring()
{
	if (_flagMonit)
	{
		_flagMonit = false;

		_threadMonitor.join();
		printf("_threadMonitor joined\n");
	}
	
	return;
}

//void TradingStrategy::OnOrderBook(std::vector<amd::ama::MDOrderBook>& order_book)
//{
//	int idx;
//	//循环访问多个MDOrderBook元素
//	for (idx = 0; idx < order_book.size(); idx++)
//	{
//		//看时间，比较新的就更新时间戳，比较旧的看情况要不要用
//		int64_t tim = order_book[idx].last_tick_time;
//		if (order_book[idx].market_type == amd::ama::MarketType::kSZSE)
//		{
//			if (tim > _timeSZSE)
//			{
//				_timeSZSE = tim;
//			}
//			else if (_timeSZSE - tim > 1000 * ALLOW_DELAY)
//			{
//				continue;//太旧了不用
//			}
//		}
//		else if (order_book[idx].market_type == amd::ama::MarketType::kSSE)
//		{
//			if (tim > _timeSSE)
//			{
//				_timeSSE = tim;
//			}
//			else if (_timeSSE - tim > 1000 * ALLOW_DELAY)
//			{
//				continue;//太旧了不用
//			}
//		}
//
//		//根据orderbook的时间确定是哪一次交易，返回布尔数组的下标
//		int32_t tickTime = (order_book[idx].last_tick_time / 100000) % 10000;//转化为hhmm形式
//		int32_t tradSeq = GetTradingSequence(tickTime);
//		//cout << " tradeSeq = " << tradSeq << endl;
//		if (tradSeq == -1)
//		{
//			//本次时间戳不在我交易范围内
//			continue;
//		}
//
//		//看当前的MDOrderBook的证券代码，在_tradingStatus里有没有对应的交易任务
//		auto iter = _securityAndTask.begin();
//		for (; iter != _securityAndTask.end(); iter++)
//		{
//			//找到了此条行情对应的待交易证券
//			if (strcmp(iter->first.c_str(), order_book[idx].security_code) == 0)
//			{
//				//printf("get wanted security %s\n", iter->first.c_str());
//				//cout << " tradeSeq = " << tradSeq << endl;
//				
//				//如果交易过了，跳过
//				if (iter->second[tradSeq]._tradeComplete == true)
//				{
//					//printf("this time trade has completed\n");
//					continue;
//				}
//
//				printf("\nwanted security %s\n", iter->first.c_str());
//				cout << " tradeSeq = " << tradSeq << endl;
//				//printf("no traded, first trade\n");
//				
//				//进行交易
//				DoTrade(iter->first, tradSeq, order_book[idx]);
//			}
//		}
//		
//	}
//	
//}

void TradingStrategy::OnQuickSnap(QuickSnapInfo* infos, uint32_t cnt)
{
	uint32_t idx;
	//循环访问多个QuickSnapInfo元素
	for (idx = 0; idx < cnt; idx++)
	{
		//交易所类型
		/*int32_t marketType = GetMarketTypeByCode(infos[idx]._sc);
		if (marketType == -1)
		{
			printf("code's marketType not valid, %06u\n", infos[idx]._sc);
			return;
		}*/
		uint32_t marketType = infos[idx]._sc / 100000000;

		uint32_t tim = infos[idx]._tm;//类似 143645679
		//看时间，比较新的就更新时间戳，比较旧的看情况要不要用
		if (marketType == 1)//上交所
		{
			if (tim > _timeSSE)
			{
				_timeSSE = tim;
			}
			else if (_timeSSE - tim > 1000 * ALLOW_DELAY)
			{
				continue;//太旧了不用
			}
		}
		else if (marketType == 2)
		{
			if (tim > _timeSZSE)
			{
				_timeSZSE = tim;
			}
			else if (_timeSZSE - tim > 1000 * ALLOW_DELAY)
			{
				continue;//太旧了不用
			}
		}
		else
		{
			printf("market type error, %u\n", infos[idx]._sc);
			return;
		}

		//根据orderbook的时间确定是哪一次交易，返回布尔数组的下标
		int32_t tickTime = infos[idx]._tm / 100000;//转化为hhmm形式
		int32_t tradSeq = GetTradingSequence(tickTime);
		//cout << " tradeSeq = " << tradSeq << endl;
		if (tradSeq == -1)
		{
			//本次时间戳不在我交易范围内
			continue;
		}

		//看当前的MDOrderBook的证券代码，在_tradingStatus里有没有对应的交易任务
		auto iter = _securityAndTask.begin();
		uint32_t realCode;
		for (; iter != _securityAndTask.end(); iter++)
		{
			realCode = infos[idx]._sc % 1000000;
			//找到了此条行情对应的待交易证券
			if (iter->first == realCode)
			{
				//printf("get wanted security %s\n", iter->first.c_str());
				//cout << " tradeSeq = " << tradSeq << endl;

				//如果交易过了，跳过
				if (iter->second[tradSeq]._tradeComplete == true)
				{
					//printf("this time trade has completed\n");
					continue;
				}

				//printf("\nTradingStrategy::OnQuickSnap: wanted security %06u\n", iter->first);
				//cout << " tradeSeq = " << tradSeq << endl;
				//printf("no traded, first trade\n");

				//进行交易
				DoTrade(iter->first, tradSeq, infos[idx]);
			}
		}

	}
}

//执行交易
void TradingStrategy::DoTrade(const uint32_t& code, int32_t trdSeq, const QuickSnapInfo& info)
{
	TradeVolume& trdVol = _securityAndTask[code][trdSeq];

	//如果交易已完成
	/*if (trdVol._tradeComplete)
	{
		return;
	}*/

	//如果只有买任务
	if (trdVol._buyVolume > 0 && trdVol._sellVolume == 0)
	{
		//cout << "buy task" << endl;
		Buy(code, trdSeq, info);
	}
	else if (trdVol._buyVolume == 0 && trdVol._sellVolume > 0)	//只有卖任务
	{
		//cout << "sell task" << endl;
		Sell(code, trdSeq, info);
	}
	else if (trdVol._buyVolume > 0 && trdVol._sellVolume > 0)//买卖任务都有
	{
		//cout << "buyAndSell task" << endl;
		BuyAndSell(code, trdSeq, info);
	}
	else
	{
		//都是0
		//printf("%06u no buy or sell task\n", code);
		trdVol._tradeComplete = true;
		return;
	}

	return;
}

void TradingStrategy::Buy(const uint32_t& code, int32_t trdSeq, const QuickSnapInfo& orderBook)
{
	//cout << "do buy trade" << endl;
	//卖委托簿有没有数据
	if (orderBook._pxAsk01 == 0 && orderBook._pxAsk02 == 0)
	{
		//cout << "no offer order" << endl;
		return;
	}

	uint32_t offerPrice = orderBook._pxAsk01;//行情一档申卖价格
	uint64_t offerVolume = orderBook._volAsk01 * 7 / 10;//行情一档申卖数量乘以0.7

	TradingResult trdRet;
	//strcpy(trdRet._securityCode, code.c_str());//交易证券代码
	trdRet._securityCode = code;
	g_mutex.lock();
	trdRet._tradeCount = ++_countTrading;//交易次数
	g_mutex.unlock();
	trdRet._tradeTime = orderBook._tm;//交易时间
	trdRet._buyOrSell = 'B';	//买或卖
	trdRet._price = offerPrice;	//交易价格

	TradeVolume& trdVol = _securityAndTask[code][trdSeq];
	//如果足够完成本次交易任务
	if (offerVolume >= trdVol._buyVolume)
	{
		trdRet._volumn = trdVol._buyVolume;
	}
	else
	{
		//不够，就把已有的买完，把缺口加给下次交易
		uint64_t deficiency = trdVol._buyVolume - offerVolume;
		trdRet._volumn = offerVolume;

		if (trdSeq + 1 < 240)//避免越界
		{
			_securityAndTask[code][trdSeq + 1]._buyVolume += deficiency;
		}
	}

	//交易完成标志
	trdVol._tradeComplete = true;
	/*这里不加锁，是考虑不会有多个线程执行同一个证券的交易，新增任务的线程也只是增加交易数量不会修改交易完成状态*/

	//交易结果写入文件
	WriteTradeResultToFile(trdRet);

	return;
}

void TradingStrategy::Sell(const uint32_t& code, int32_t trdSeq, const QuickSnapInfo& orderBook)
{
	//cout << "do sell trade" << endl;
	//买委托簿有没有数据
	if (orderBook._pxBid01 == 0 && orderBook._pxBid02 == 0)
	{
		//cout << "no bid order" << endl;
		return;
	}

	uint32_t bidPrice = orderBook._pxBid01;//行情一档申买价格
	uint64_t bidVolume = orderBook._volBid01 * 7 / 10;//行情一档申买数量乘以0.7

	TradingResult trdRet;
	//strcpy(trdRet._securityCode, code.c_str());//交易证券代码
	trdRet._securityCode = code;
	g_mutex.lock();
	trdRet._tradeCount = ++_countTrading;//交易次数
	g_mutex.unlock();
	trdRet._tradeTime = orderBook._tm;//交易时间
	trdRet._buyOrSell = 'S';	//买或卖
	trdRet._price = bidPrice;	//交易价格

	TradeVolume& trdVol = _securityAndTask[code][trdSeq];
	//如果足够完成本次交易
	if (bidVolume >= trdVol._sellVolume)
	{
		trdRet._volumn = trdVol._sellVolume;
	}
	else
	{
		//不够，把能卖的卖完，把缺口加给下次交易
		int64_t deficiency = trdVol._sellVolume - bidVolume;
		trdRet._volumn = bidVolume;

		if (trdSeq + 1 < 240)
		{
			_securityAndTask[code][trdSeq + 1]._sellVolume += deficiency;
		}
	}

	//交易完成标志
	trdVol._tradeComplete = true;

	//交易结果写入文件
	WriteTradeResultToFile(trdRet);

	return;
}

void TradingStrategy::BuyAndSell(const uint32_t& code, int32_t trdSeq, const QuickSnapInfo& orderBook)
{
	//cout << "do buyAndSell trade" << endl;

	uint32_t internMatchPri;//内部撮合的价格
	if (orderBook._pxBid01 != 0 && orderBook._pxAsk01 != 0)
	{
		internMatchPri = (orderBook._pxBid01 + orderBook._pxAsk01) / 2;
	}
	else if (orderBook._pxBid01 == 0 && orderBook._pxAsk01 != 0)
	{
		internMatchPri = orderBook._pxAsk01;
	}
	else if (orderBook._pxBid01 != 0 && orderBook._pxAsk01 == 0)
	{
		internMatchPri = orderBook._pxBid01;
	}
	else
	{
		//bid和offer委托簿都没有数据，这条数据不用了，等下条数据再交易
		//cerr << "bid order and offer order are both empty" << endl;
		return;
	}
	

	TradingResult trdRet1, trdRet2;
	//内部撮合买
	//strcpy(trdRet1._securityCode, code.c_str());//交易证券代码
	trdRet1._securityCode = code;
	g_mutex.lock();
	trdRet1._tradeCount = ++_countTrading;//交易次数
	g_mutex.unlock();
	trdRet1._tradeTime = orderBook._tm;//交易时间
	trdRet1._buyOrSell = 'B';	//买或卖
	trdRet1._price = internMatchPri;	//交易价格
	trdRet1._infoTrade = "internal matching";

	//内部撮合卖
	//strcpy(trdRet2._securityCode, code.c_str());//交易证券代码
	trdRet2._securityCode = code;
	g_mutex.lock();
	trdRet2._tradeCount = ++_countTrading;//交易次数
	g_mutex.unlock();
	trdRet2._tradeTime = orderBook._tm;//交易时间
	trdRet2._buyOrSell = 'S';	//买或卖
	trdRet2._price = internMatchPri;	//交易价格
	trdRet2._infoTrade = "internal matching";

	TradeVolume& trdVol = _securityAndTask[code][trdSeq];
	//如果要买的比卖的多
	if (trdVol._buyVolume > trdVol._sellVolume)
	{
		//两次内部撮合的结果
		trdRet1._volumn = trdVol._sellVolume;
		trdRet2._volumn = trdVol._sellVolume;

		//交易结果写入文件
		WriteTradeResultToFile(trdRet1);
		WriteTradeResultToFile(trdRet2);

		//要执行买交易
		//修改任务数量前加锁
		//g_taskQueue.lock();
		trdVol._buyVolume = trdVol._buyVolume - trdVol._sellVolume;
		trdVol._sellVolume = 0;
		//g_taskQueue.unlock();
		Buy(code, trdSeq, orderBook);
	}
	//卖的比买的多
	else if (trdVol._buyVolume < trdVol._sellVolume)
	{
		//两次内部撮合的结果
		trdRet1._volumn = trdVol._buyVolume;
		trdRet2._volumn = trdVol._buyVolume;

		//交易结果写入文件
		WriteTradeResultToFile(trdRet1);
		WriteTradeResultToFile(trdRet2);

		//执行卖交易
		//g_taskQueue.lock();
		trdVol._sellVolume = trdVol._sellVolume - trdVol._buyVolume;
		trdVol._buyVolume = 0;

		//g_taskQueue.unlock();
		Sell(code, trdSeq, orderBook);
	}
	else
	{
		//买和卖一样多，只有内部撮合，不用额外交易
		//两次内部撮合的结果
		trdRet1._volumn = trdVol._buyVolume;
		trdRet2._volumn = trdVol._buyVolume;

		//交易结果写入文件
		WriteTradeResultToFile(trdRet1);
		WriteTradeResultToFile(trdRet2);

		//修改交易完成标志
		trdVol._tradeComplete = true;
	}
	
}

//void TradingStrategy::OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt)
//{
//	//cout << "TradingStrategy::OnSnapshot is called" << endl;
//
//	return;
//}

//void TradingStrategy::OnTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
//{
//	//cout << "TradingStrategy::OnTickOrder is called" << endl;
//
//	return;
//}

//void TradingStrategy::OnTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt)
//{
//	//cout << "TradingStrategy::OnTickExecution is called" << endl;
//
//	return;
//}



//930应该返回0, 1459应该返回239
int32_t TradingStrategy::GetTradingSequence(int32_t tim)
{
	//printf("tim = %ld\n", tim);
	int32_t ret;

	//得到时和分
	int32_t hour = tim / 100;
	int32_t min = tim % 100;
	//printf("hour = %d, min = %d\n", hour, min);

	//上午还是下午
	if (tim >= 930 && tim <= 1129)
	{
		if (hour == 9)
		{
			ret = min - 30;
		}
		else if (hour == 10)
		{
			ret = 30 + min;
		}
		else if (hour == 11)
		{
			ret = 90 + min;
		}
	}
	else if (tim >= 1300 && tim <= 1459)
	{
		if (hour == 13)
		{
			ret = 120 + min;
		}
		else if (hour == 14)
		{
			ret = 180 + min;
		}
	}
	else
	{
		//cerr << "TradingStrategy::GetTradingSequence：" << "invalid tim" << tim << endl;
		//exit(1);
		return -1;
	}

	return ret;
}

void TradingStrategy::OpenTradeResultFile()
{
	/*ConfigFile cfgFile;
	std::string path;
	if (cfgFile.Load("../data/config.conf") == true)
	{
		path = cfgFile.Get("TradeResultFilePath");
		if (path == string())
		{
			cerr << "cfgFile.Get(TradeResultFilePath) not found" << endl;
		}
	}*/

	std::string path;
	path = g_cfgFile.Get("TradeResultFilePath");
	if (path == string())
	{
		cerr << "cfgFile.Get(TradeResultFilePath) not found" << endl;
	}

	//循环打开多个文件，给多个线程写入
	for (int32_t fileCount = 1; fileCount <= _threadNumOrderBook; fileCount++)
	{
		std::string fCnt = std::to_string(fileCount);

		std::string fileNameStr = path + "/TradeResult" + "_" + fCnt + ".csv";

		const char* filename = fileNameStr.c_str();
		FILE* fp = NULL;
		//如果文件不存在，就新建并写好表头
		fp = fopen(filename, "r");
		if (fp == NULL)
		{
			printf("%s does not exist, create a new one\n", filename);
			fp = fopen(filename, "w");
			if (fp == NULL)
			{
				perror("fopen tradeResult.csv");
				exit(1);
			}

			char sep = ',';
			fprintf(fp, "%s%c", "SecurityCode", sep);
			fprintf(fp, "%s%c", "BuyOrSell", sep);
			fprintf(fp, "%s%c", "Price", sep);
			fprintf(fp, "%s%c", "Volume", sep);
			fprintf(fp, "%s%c", "TradeCount", sep);
			fprintf(fp, "%s%c", "TradeTime", sep);
			//fprintf(fp, "%s%c", "note", sep);
			fprintf(fp, "%s", "note");

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
}

//把交易结果写入文件
void TradingStrategy::WriteTradeResultToFile(const TradingResult& trdRet)
{
	/*cout << trdRet._securityCode << "," 
		<< trdRet._buyOrSell << "," 
		<< trdRet._price << "," 
		<< trdRet._volumn << ","
		<< trdRet._tradeCount << "," 
		<< trdRet._tradeTime  << ","
		<< trdRet._infoTrade << ","
		<< endl;*/
	//本线程id
	std::thread::id thid = std::this_thread::get_id();

	//本线程有没有匹配的文件，如果没有就从队列里出队一个给它，组成映射放入unordered_map
	if (_threadWriteFile.count(thid) == 0)
	{
		std::lock_guard<std::mutex> _(g_outMutex);
		std::cout << "TradingStrategy::WriteTradeResultToFile(): a new thread, thread id = " << thid << std::endl;
		if (_orderBookCsvFiles.empty())
		{
			std::cerr << "TradingStrategy::WriteTradeResultToFile: opened tradeResult file pointer is not enough" << std::endl;
			exit(1);
		}
		FILE* fptm = _orderBookCsvFiles.front();
		_orderBookCsvFiles.pop();
		_threadWriteFile[thid] = fptm;
	}

	//要写的文件
	FILE* fp = _threadWriteFile[thid];

	char sep = ',';
	//cout << trdRet._securityCode << " " << trdRet._buyOrSell << " " << trdRet._price << " " << trdRet._volumn << " " << trdRet._tradeCount << " " << trdRet._tradeTime << endl;

	int ret;
	ret = fprintf(fp, "%06u%c", trdRet._securityCode, sep);
	//printf("ret = %d\n", ret);
	fprintf(fp, "%c%c", trdRet._buyOrSell, sep);
	fprintf(fp, "%u%c", trdRet._price, sep);
	fprintf(fp, "%lu%c", trdRet._volumn, sep);
	fprintf(fp, "%u%c", trdRet._tradeCount, sep);
	fprintf(fp, "%u%c", trdRet._tradeTime, sep);
	if (trdRet._infoTrade.size() != 0)
	{
		fprintf(fp, "%s", trdRet._infoTrade.c_str());//最后一列不要加逗号了
		//fprintf(fp, "%s%c", trdRet._infoTrade.c_str(), sep);//最后一列
	}

	fputc('\n', fp);

	//fflush(fp);
}

//从证券代码获得市场类型
int32_t TradingStrategy::GetMarketTypeByCode(uint32_t code)
{
	//int ret = -1;
	////只订阅了股票和基金，还是好分辨的
	//char sc[10] = { 0 };
	//sprintf(sc, "%06u", code);
	////上交所
	//if (sc[0] == '5' || sc[0] == '6' || sc[0] == '9')
	//{
	//	ret = amd::ama::MarketType::kSSE;
	//}
	//else if (sc[0] == '0' || sc[0] == '1' || sc[0] == '2' || sc[0] == '3')
	//{
	//	ret = amd::ama::MarketType::kSZSE;
	//}

	int ret = 0;
	uint32_t flag = 0;
	flag = code / 100000000;
	if (flag == 1)
	{
		ret = amd::ama::MarketType::kSSE;
	}
	else if (flag == 2)
	{
		ret = amd::ama::MarketType::kSZSE;
	}

	return ret;
}

//盘后处理
int TradingStrategy::AfterMarket()
{
	StopMonitoring();

	return 0;
}

//创建实例的函数
MDApplication* CreateTradeStra()
{
	return new TradingStrategy();
}

//注册到反射里，key为3
RegisterAction g_regActTradeStrategy("3", CreateTradeStra);
