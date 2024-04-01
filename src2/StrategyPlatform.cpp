#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include "reflect.h"
#include "MDApplication.h"
#include "ConfigFile.h"
#include "MarketDataSpi.h"
#include "VirtualMarketBin.h"
#include "VirtualMarket.h"
#include "SPLog.h"

using std::cout;
using std::endl;
using std::vector;
using std::string;

//全局变量，配置信息
ConfigFile g_cfgFile;
//全局变量，日志文件
SPLog g_log;

//注册信号回调函数，声明
void SignalRegister();

//按数据品种以及数据类型订阅实时行情数据
int32_t SubscribeWithCategory()
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

	/* 订阅深交所全部股票的现货快照 */
	sub1[0].market = amd::ama::MarketType::kSZSE;
	sub1[0].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub1[0].category_type = amd::ama::SubscribeCategoryType::kStock;
	sub1[0].security_code[0] = '\0';

	/* 订阅上交所全部股票现货快照 */
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

	amd::ama::SubscribeCategoryItem sub3[2];
	memset(sub3, 0, sizeof(sub3));

	/* 订阅上交所全部基金的逐笔委托和逐笔成交数据  */
	sub3[0].market = amd::ama::MarketType::kSSE;
	sub3[0].data_type = amd::ama::SubscribeSecuDataType::kTickOrder
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub3[0].category_type = amd::ama::SubscribeCategoryType::kFund;
	sub3[0].security_code[0] = '\0';
	/* 订阅深交所全部基金的逐笔委托和逐笔成交数据  */
	sub3[1].market = amd::ama::MarketType::kSZSE;
	sub3[1].data_type = amd::ama::SubscribeSecuDataType::kTickOrder
		| amd::ama::SubscribeSecuDataType::kTickExecution;
	sub3[1].category_type = amd::ama::SubscribeCategoryType::kFund;
	sub3[1].security_code[0] = '\0';

	amd::ama::SubscribeCategoryItem sub4[2];
	memset(sub4, 0, sizeof(sub4));

	/* 订阅深交所全部基金的现货快照 */
	sub4[0].market = amd::ama::MarketType::kSZSE;
	sub4[0].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub4[0].category_type = amd::ama::SubscribeCategoryType::kFund;
	sub4[0].security_code[0] = '\0';

	/* 订阅上交所全部基金现货快照 */
	sub4[1].market = amd::ama::MarketType::kSSE;
	sub4[1].data_type = amd::ama::SubscribeSecuDataType::kSnapshot;
	sub4[1].category_type = amd::ama::SubscribeCategoryType::kFund;
	sub4[1].security_code[0] = '\0';

	/* 发起订阅 */
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kSet, sub1, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub2, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub3, 2);
	amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kAdd, sub4, 2);

	return 0;
}

void InitialConfiguraton(amd::ama::Cfg& cfg)
{
	/*
		通道模式设置及各个通道说明:
		cfg.channel_mode = amd::ama::ChannelMode::kTCP;   ///< TCP 方式计入上游行情系统
		cfg.channel_mode = amd::ama::ChannelMode::kAMI;   ///< AMI 组播方式接入上游行情系统
		cfg.channel_mode = amd::ama::ChannelMode::kRDMA;  ///< 开启硬件加速RDMA通道,抓取网卡数据包数据
		cfg.channel_mode = amd::ama::ChannelMode::kEXA;   ///< 开启硬件加速EXA通道,抓取网卡数据包数据
		cfg.channel_mode = amd::ama::ChannelMode::kPCAP;  ///< 开启硬件加速PCAP通道,抓取网卡数据包数据
		cfg.channel_mode = amd::ama::ChannelMode::kMDDP;  ///< 直接接入交易所网关组播数据，现在只有深圳交易所开通了此服务
		cfg.channel_mode = amd::ama::ChannelMode::kFPGA;  ///< FPGA接入数据
		cfg.channel_mode = amd::ama::ChannelMode::kTCP|amd::ama::ChannelMode::kAMI;  ///< 同时通过TCP方式和AMI组播方式接入上游，通过cfg.ha_mode 设置对应的高可用设置模式
	*/
	cfg.channel_mode = amd::ama::ChannelMode::kTCP;

	cfg.tcp_compress_mode = 0;  //TCP传输数据方式: 0 不压缩 1 华锐自定义压缩 2 zstd压缩(仅TCP模式有效)


	/*
		通道高可用模式设置
		1. cfg.channel_mode 为单通道时,建议设置值为kMasterSlaveA / kMasterSlaveB
		2. cfg.channel_mode 混合开启多个通道时,根据需求设置不同的值
			1) 如果需要多个通道为多活模式接入,请设置kRegularDataFilter值
			2) 如果需要多个通道互为主备接入，请设置值为kMasterSlaveA / kMasterSlaveB,kMasterSlaveA / kMasterSlaveB 差别请参看注释说明
			   通道优先级从高到低依次为 kRDMA/kEXA/kMDDP/kAMI/kTCP/kPCAP
	*/
	cfg.ha_mode = amd::ama::HighAvailableMode::kMasterSlaveA;



	cfg.min_log_level = amd::ama::LogLevel::kInfo; // 设置日志最小级别：Info级（高于等于此级别的日志就可输出了）, AMA内部日志通过 OnLog 回调函数返回

	/*
		设置是否输出监控数据: true(是), false(否), 监控数据通过OnIndicator 回调函数返回
		监控数据格式为json, 主要监控数据包括订阅信息，数据接收数量统计
		数据量统计:包括接收数量和成功下发的数量统计,两者差值为过滤的数据量统计
		eg: "RecvSnapshot": "5926", "SuccessSnapshot": "5925",表示接收了快照数据5926个,成功下发5925个，过滤数据为 5926 - 5925 = 1 个
			过滤的数据有可能为重复数据或者非订阅数据
	*/
	cfg.is_output_mon_data = false;

	/*
		设置逐笔保序开关: true(开启保序功能) , false(关闭保序功能)
		主要校验逐笔成交数据和逐笔委托数据是否有丢失,如果丢失会有告警日志,缓存逐笔数据并等待keep_order_timeout_ms(单位ms)时间等待上游数据重传,
		如果超过此时间,直接下发缓存数据,默认数据已经丢失,如果之后丢失数据过来也会丢弃。
		同时由于深圳和上海交易所都是通道内序号连续,如果打开了保序开关,必须订阅全部代码的逐笔数据,否则一部分序号会被订阅过滤,导致数据超时等待以及丢失告警。
	*/
	cfg.keep_order = true;
	cfg.keep_order_timeout_ms = 3000;


	cfg.is_subscribe_full = false; //设置默认订阅: true 代表默认全部订阅, false 代表默认全部不订阅

	/*
		配置UMS信息:
		username/password 账户名/密码, 一个账户只能保持一个连接接入（注意: 如果需要使用委托簿功能，注意账号需要有委托簿功能权限）
		ums地址配置:
			1) ums地址可以配置1-8个 建议值为2 互为主备, ums_server_cnt 为此次配置UMS地址的个数
			2) ums_servers 为UMS地址信息数据结构:
				local_ip 为本地地址,填0.0.0.0 或者本机ip
				server_ip 为ums服务端地址
				server_port 为ums服务端端口
	*/

	//ConfigFile cfgFile;
	std::string usr;
	std::string pwd;
	std::string ip;
	std::string port;
	//if (cfgFile.Load("../data/config.conf") == true)
	usr = g_cfgFile.Get("usrname");
	pwd = g_cfgFile.Get("password");
	ip = g_cfgFile.Get("ip");
	port = g_cfgFile.Get("port");

	std::istringstream iss(port);
	uint16_t portInt;
	iss >> portInt;

	strcpy(cfg.username, usr.c_str());
	strcpy(cfg.password, pwd.c_str());

	cfg.ums_server_cnt = 2;
	strcpy(cfg.ums_servers[0].local_ip, "0.0.0.0");
	strcpy(cfg.ums_servers[0].server_ip, ip.c_str());

	cfg.ums_servers[0].server_port = portInt;

	strcpy(cfg.ums_servers[1].local_ip, "0.0.0.0");
	strcpy(cfg.ums_servers[1].server_ip, ip.c_str());

	cfg.ums_servers[1].server_port = portInt;


	/*
		业务数据回调接口(不包括 OnIndicator/OnLog等功能数据回调)的线程安全模式设置:
		true: 所有的业务数据接口为接口集线程安全
		false: 业务接口单接口为线程安全,接口集非线程安全
	*/
	cfg.is_thread_safe = false;

	/*
		委托簿前档行情参数设置(仅委托簿版本API有效，若非委托簿版本参数设置无效):
			1) 行情输出设置，包括档位数量以及每档对应的委托队列数量
			2）委托簿计算输出参数设置，包括线程数量以及递交间隔设置
	*/
	cfg.enable_order_book = amd::ama::OrderBookType::kLocalOrderBook; //是否开启委托簿计算功能
	cfg.entry_size = 2;                                    //委托簿输出最大档位数量(递交的委托簿数据档位小于等于entry_size)
	cfg.order_queue_size = 2;                              //每个档位输出的委托队列揭示(最大设置50)
	cfg.thread_num = 3;                                     //构建委托簿的线程数量(递交和构建每一个市场分别开启thread_num个线程)
	cfg.order_book_deliver_interval_microsecond = 0;        /*递交的最小时间间隔(单位:微妙, 委托簿递交间隔大于等于order_book_deliver_interval_microsecond)
															  设置为0时, 委托簿实时下发, 不会等待间隔.
															 */

	return;
}

void IsRunning()
{
	fd_set fset;
	struct timeval timeout = { 3, 0 };
	int32_t ret = 0;
	while (true)
	{
		std::cout << "ama is running, input 0 to exit" << std::endl;
		FD_ZERO(&fset);
		FD_SET(STDIN_FILENO, &fset);
		timeout.tv_sec = 3;
		ret = select(1, &fset, NULL, NULL, &timeout);
		if (ret == 0)
		{
			continue;
		}
		else
		{
			char ch = getchar();
			if (ch == '0')
			{
				printf("get 0\n");
				break;
			}
			else
			{
				printf("get not 0\n");
				continue;
			}
		}
	}

	return;
}

int RealTimeData(const SnapshotFunc& spshotFunc, const OrderAndExecuFunc& ordAndExeFunc, const QuickSnapFunc& quickSnapFunc,const vector<MDApplication*>& applObjs)
{
	amd::ama::Cfg cfg; // 准备AMA配置
	InitialConfiguraton(cfg);

	//接收实时行情数据的类
	MarketDataSpi dataPlatform;

	dataPlatform.SetOnMySnapshot(spshotFunc);
	dataPlatform.SetOnOrderAndExecu(ordAndExeFunc);
	dataPlatform.SetOnQuickSnap(quickSnapFunc);

	//给平台添加应用对象
	for (size_t idx = 0; idx < applObjs.size(); ++idx)
	{
		dataPlatform.AddApplication(applObjs[idx]);
	}
	

	/*
		初始化回调以及配置信息, Init函数返回amd::ama::ErrorCode::kSuccess 只能说明配置信息cfg和spi符合配置规范, 后续的鉴权和登陆过程为异步过程,
		如果鉴权和登陆过程遇到环境异常(如网络地址不通, 账号密码错误等), 会通过OnLog/OnEvent两个回调函数返回鉴权和登陆结果
	*/
	if (amd::ama::IAMDApi::Init(&dataPlatform, cfg)
		!= amd::ama::ErrorCode::kSuccess)
	{
		//std::lock_guard<std::mutex> _(g_mutex);
		std::cout << "Init AMA failed" << std::endl;
		amd::ama::IAMDApi::Release();
		return -1;
	}

	//SubscribeWithDataAuth();
	SubscribeWithCategory();
	//SubDerivedData();
	//GetRDIData()
	/*TODO 按照实际需求调用不同的接口函数*/

	/* 保持主线程不退出 */
	IsRunning();

	amd::ama::IAMDApi::Release();

	return 0;
}

int VirtualMakretBinData(const SnapshotFunc& spshotFunc, const OrderAndExecuFunc& ordAndExeFunc, const QuickSnapFunc& quickSnapFunc)
{
	VirtualMarketBin dataPlatform;
	dataPlatform.Init();

	dataPlatform.SetSnapshotCallback(spshotFunc);
	dataPlatform.SetOrdAndExecuCallback(ordAndExeFunc);
	dataPlatform.SetQuickSnapCallback(quickSnapFunc);

	dataPlatform.Play();

	return 0;
}

int VirtualMarketCsvData(const SnapshotFunc& spshotFunc, const OrderAndExecuFunc& ordAndExeFunc, const QuickSnapFunc& quickSnapFunc)
{
	VirtualMarket dataPlatform;
	dataPlatform.Init();

	dataPlatform.SetSnapshotCallback(spshotFunc);
	dataPlatform.SetOrdAndExecuCallback(ordAndExeFunc);
	dataPlatform.SetQuickSnapCallback(quickSnapFunc);

	dataPlatform.Play();

	return 0;
}

int StrategyPlatform()
{
	//加载配置文件
	if (g_cfgFile.Load("../data/config.conf") == false)
	{
		std::cerr << "load config file failed\n" << endl;
		exit(1);
	}

	//信号回调函数注册
	SignalRegister();

	string workPattern = g_cfgFile.Get("workPattern");
	if (workPattern == string())
	{
		std::cerr << "workPattern not found in config file" << endl;
		exit(1);
	}

	int flag = std::stoi(workPattern);
	cout << "workPattern: " << flag << endl;
	int ret = 0;

	//把配置文件里application后面的所有字段放入字符串数组
	vector<string> applName;
	string word;
	string str = g_cfgFile.Get("application");
	if (str == string())
	{
		cout << "config file has no application" << endl;
		return 1;
	}
	std::istringstream iss(str);
	while (std::getline(iss, word, ','))
	{
		applName.push_back(word);
	}

	for (auto& name : applName)
	{
		cout << name << " ";
	}
	cout << endl;

	//应用的类对象
	vector<MDApplication*> applObj;

	int bef;
	//根据应用名，获得对象
	if (applName.empty() == false)
	{
		ReflectFactory* fact = ReflectFactory::GetInstance();
		MDApplication* obj = NULL;
		vector<string>::const_iterator iter;
		for (iter = applName.begin(); iter != applName.end(); ++iter)
		{
			obj = fact->GetAppObject(*iter);
			if (obj != NULL)
			{
				//盘前处理
				bef = obj->BeforeMarket();
				if (bef == 0)
				{
					//盘前处理成功了，放入数组，之后会被调用
					applObj.push_back(obj);
				}
				else
				{
					printf("StrategyPlatform(), beforeMarket() return not 0\n");
				}
			}
			else
			{
				printf("StrategyPlatform(), no reflection for %s\n", iter->c_str());
			}
		}
	}

	//所有的实例都已创建，开始定义回调函数内容
	SnapshotFunc onSnapshot = [&](SnapshotInfo* info, uint32_t cnt) {

		if (applObj.empty() == false)
		{
			for (MDApplication* app : applObj)
			{
				app->OnSnapshot(info, cnt);
			}
		}
	};

	OrderAndExecuFunc onOrdAndExecu = [&](OrdAndExeInfo* info, uint32_t cnt) {

		if (applObj.empty() == false)
		{
			for (MDApplication* app : applObj)
			{
				app->OnOrderAndExecu(info, cnt);
			}
		}
	};

	QuickSnapFunc onQuickSnap = [&](QuickSnapInfo* info, uint32_t cnt) {

		if (applObj.empty() == false)
		{
			for (MDApplication* app : applObj)
			{
				app->OnQuickSnap(info, cnt);
			}
		}
	};

	//1，2，3，代表不同数据来源
	if (flag == 1)
	{
		ret = RealTimeData(onSnapshot, onOrdAndExecu, onQuickSnap, applObj);
	}
	else if (flag == 2)
	{
		ret = VirtualMakretBinData(onSnapshot, onOrdAndExecu, onQuickSnap);
	}
	else if (flag == 3)
	{
		ret = VirtualMarketCsvData(onSnapshot, onOrdAndExecu, onQuickSnap);
	}
	else
	{
		printf("work pattern not valid\n");
		return 1;
	}

	int aft;
	if (applObj.empty() == false)
	{
		for (MDApplication* app : applObj)
		{
			//盘后处理
			aft = app->AfterMarket();
			delete app;
		}
	}

	return ret;
}