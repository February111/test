#pragma once

#include <stdio.h>
#include <map>
#include <string>
#include <mutex>
#include "ama.h"
#include "sp_type.h"
#include "MDApplication.h"

struct OrderEvaluate
{
	uint64_t _accAmountBuy;				//买的金额(10^3)
	uint64_t _accAmountSell;			//卖的金额
	uint64_t _buyOrderAmt;				//新增买委托金额
	uint64_t _buyOrderVol;				//新增买委托量
	uint64_t _activeBuyOrderAmt;		//积极买委托金额
	uint64_t _activeBuyOrderVol;		//积极买委托量
	uint64_t _passiveBuyOrderAmt;		//消极买委托金额
	uint64_t _passiveBuyOrderVol;		//消极买委托量
	uint64_t _sellOrderAmt;				//新增卖委托金额
	uint64_t _sellOrderVol;				//新增卖委托量
	uint64_t _activeSellOrderAmt;		//新增积极卖委托金额
	uint64_t _activeSellOrderVol;		//新增积极卖委托量
	uint64_t _passiveSellOrderAmt;		//新增消极卖委托金额
	uint64_t _passiveSellOrderVol;		//新增消极卖委托量
	uint64_t _buyOrderCanceledAmt;		//买委托撤单+委托价格低于上个Tick中间价的，金额
	uint64_t _buyOrderCanceledVol;		//买委托撤单+委托价格低于上个Tick中间价的，数量
	uint64_t _sellOrderCanceledAmt;		//卖委托撤单+委托价格高于上个Tick中间价的，金额
	uint64_t _sellOrderCanceledVol;		//卖委托撤单+委托价格高于上个Tick中间价的，数量
	
public:
	//默认构造，把所有值初始化为0
	OrderEvaluate()
	{
		_accAmountBuy = 0;
		_accAmountSell = 0;
		_buyOrderAmt = 0;
		_buyOrderVol = 0;
		_activeBuyOrderAmt = 0;
		_activeBuyOrderVol = 0;
		_passiveBuyOrderAmt = 0;
		_passiveBuyOrderVol = 0;
		_sellOrderAmt = 0;
		_sellOrderVol = 0;
		_activeSellOrderAmt = 0;
		_activeSellOrderVol = 0;
		_passiveSellOrderAmt = 0;
		_passiveSellOrderVol = 0;
		_buyOrderCanceledAmt = 0;
		_buyOrderCanceledVol = 0;
		_sellOrderCanceledAmt = 0;
		_sellOrderCanceledVol = 0;
	}
};

struct ExecutionData
{
	uint32_t _tradeNum;		//交易笔数
	uint32_t _buyTradeNum;	//主买成交的交易笔数
	uint64_t _buyTradeAmt;	//主买成交的金额，（保留三位小数）
	uint64_t _buyTradeVol;	//主买成交的交易量（不保留小数位）
	uint32_t _sellTradeNum;	//主卖成交的交易笔数
	uint64_t _sellTradeAmt;	//主卖成交的金额（保留三位小数）
	uint64_t _sellTradeVol;	//主卖成交的交易量（不保留小数位）

	uint32_t _startPrice;	//开始成交价格（保留三位小数）
	uint32_t _highPrice;	//最高成交价格（保留三位小数）
	uint32_t _lowPrice;		//最低成交价格（保留三位小数）
	uint32_t _endPrice;		//结束价格（保留三位小数）

public:
	//默认构造，把所有值初始化
	ExecutionData()
	{
		_tradeNum = 0;
		_buyTradeNum = 0;
		_buyTradeAmt = 0;
		_buyTradeVol = 0;
		_sellTradeNum = 0;
		_sellTradeAmt = 0;
		_sellTradeVol = 0;
		_startPrice = 0;
		_highPrice = 0;
		_lowPrice = 0;
		_endPrice = 0;
	}
};

class EnhanceMinuteKLine : public MDApplication
{
public:
	//构造函数
	EnhanceMinuteKLine();

	//回调函数，收到snapshot行情时调用
	virtual void OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt) override;

	//回调函数，收到tickExecution行情时使用
	virtual void OnOrderAndExecu(OrdAndExeInfo* execu, uint32_t cnt) override;

private:
	//如果tim2相比tim1跨分钟了，返回1；没有，返回0；是tim1的前一分钟，返回-1
	int32_t CompareTime(uint32_t tim1, uint32_t tim2);

	//把统计好的数据写入文件，文件名根据数据的时间来定
	void WriteDataToFile();

	//写文件的表头
	void WriteHeaderOfFile(FILE* fp);

	//清除map的数据，为了下一分钟重新统计
	void ClearData();

	//orderEvaluate数据写文件
	void WriteOrderEvaluateToFile(const OrderEvaluate& orderEvl, FILE* fp);

	//executionData写文件
	void WriteExecutionDataToFile(const ExecutionData& exeData, FILE* fp);

	//写文件，换行
	void WriteLineFeedToFile(FILE* fp)
	{
		fputc('\n', fp);
	}

	//更新orderEvaluate数据
	void UpdateOrderEvaluateData(const SnapshotInfo &snapshot);

	//更新成交数据
	void UpdateExecutionData(const OrdAndExeInfo& tickExecution);

	//根据snapshot数据计算中间价
	uint32_t GetMidPrice(const SnapshotInfo& snapshot);

private:
	//时间戳，不断更新并判断是否跨分钟
	uint32_t _time;
	
	//证券代码和OrderEvaluate数据的映射
	std::map<uint32_t, OrderEvaluate> _securityAndOrderEvaluate;
	
	//证券代码和成交统计数据的映射
	//std::map<std::string, ExecutionData> _securityAndExecutionData;
	std::map<uint32_t, ExecutionData> _securityAndExecutionData;

	//存储security_code和上一次的snapshot数据，用于计算OrderEvaluate数据
	std::map<uint32_t, SnapshotInfo> _lastSnapshots;

	//要写文件的目录
	std::string _filePath;

	//写csv文件时的分隔符
	char _sep;

	//线程锁，写文件，清空数据，写数据的操作，如果存在多线程，需要加锁
	std::mutex _dataMutex;
};

