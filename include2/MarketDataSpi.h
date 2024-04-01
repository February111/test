#pragma once

#include <vector>
#include <map>
#include "ama.h"
#include "sp_type.h"
#include "MDApplication.h"


class MarketDataSpi : public amd::ama::IAMDSpi
{
public:
	//构造函数
	MarketDataSpi();

	// 定义日志回调处理方法
	virtual void OnLog(const int32_t& level,
		const char* log,
		uint32_t len) override;

	// 定义监控回调处理方法
	virtual void OnIndicator(const char* indicator,
		uint32_t len) override;

	// 定义事件回调处理方法  level 对照 EventLevel 数据类型 code 对照 EventCode 数据类型
	virtual void OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len) override;

	// 定义快照数据回调处理方法
	virtual void OnMDSnapshot(amd::ama::MDSnapshot* snapshot, uint32_t cnt) override;

	//
	virtual void OnMDFutureSnapshot(amd::ama::MDFutureSnapshot* snapshots, uint32_t cnt) override;

	//定义指数快照数据回调处理方法
	virtual void OnMDIndexSnapshot(amd::ama::MDIndexSnapshot* snapshots, uint32_t cnt) override;

	//定义逐笔委托数据回调处理方法
	virtual void OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt) override;

	// 定义逐笔成交数据回调处理方法
	virtual void OnMDTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt) override;

	//定义委托簿数据回调处理方法(本地构建模式下非单线程递交, cfg.thread_num为递交委托簿数据线程数, 服务端推送模式下为单线程递交)
	virtual void OnMDOrderBook(std::vector<amd::ama::MDOrderBook>& order_book) override;

	//设置回调函数
	void SetOnMySnapshot(const SnapshotFunc& func)
	{
		_onMySnapshot = func;
	}

	/*void SetOnMyTickOrder(const TickOrderFunc& func)
	{
		_onMyTickOrder = func;
	}

	void SetOnMyTickExecution(const TickExecutionFunc& func)
	{
		_onMyTickExecution = func;
	}*/

	void SetOnOrderAndExecu(const OrderAndExecuFunc& func)
	{
		_onOrderAndExecu = func;
	}

	/*void SetOnMyOrderBook(const OrderBookFunc& func)
	{
		_onMyOrderBook = func;
	}*/

	void SetOnQuickSnap(const QuickSnapFunc& func)
	{
		_onQuickSnap = func;
	}

	//添加应用类对象
	void AddApplication(MDApplication* app);

private:
	void ConvertToSnapshotInfo(const amd::ama::MDSnapshot* snapshot, SnapshotInfo* snapInfo);

	//把tickOrder转化为orderAndExecution
	void AnalyseTickOrder(const amd::ama::MDTickOrder* ord, OrdAndExeInfo* ordAndExe);

	//把tickExecution分析转化位OrdAndExeInfo
	void AnalyseTicExecution(const amd::ama::MDTickExecution* execu, OrdAndExeInfo* ordAndExe);

	//把OrderBook转化为QuickSnapInfo
	void AnalyseOrderBook(const amd::ama::MDOrderBook& orderBook, QuickSnapInfo* quickSap);

	/*更新证券对应的unchangedData，或者检查发现不需要更新，或者传入的数据没有值得更新的
	一旦有更新，就调用应用数组里每个应用对象的OnUnchangedData回调函数*/
	int UpdateUnchangedData(const amd::ama::MDSnapshot& snapshot);

private:
	//自己设置的回调函数
	SnapshotFunc _onMySnapshot;
	OrderAndExecuFunc _onOrderAndExecu;
	QuickSnapFunc _onQuickSnap;

	std::vector<SnapshotInfo> _vecSnapshot;
	std::vector<OrdAndExeInfo> _vecOrdAndExe1;
	std::vector<OrdAndExeInfo> _vecOrdAndExe2;
	//std::vector<QuickSnapInfo> _vecQuickSnap;

	//应用类对象的数组(平台与应用类对象是关联关系）
	std::vector<MDApplication*> _appObjects;

	//证券代码对应的不变数据
	std::map<uint32_t, UnchangedMarketData> _unchangedDatas;
};

