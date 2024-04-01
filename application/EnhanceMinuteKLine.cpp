#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <set>
#include "EnhanceMinuteKLine.h"
#include "ConfigFile.h"
#include "reflect.h"

#define	PRICE_MULTIPLE	1000		//Price的扩大倍数
#define QTY_MULTIPLE	1			//Qty的扩大倍数
#define AMT_MULTIPLE	1000		//Amt的扩大倍数
//价格保留3位小数，数量不保留小数，金额保留三位小数

using std::string;
using std::set;
using std::pair;

extern ConfigFile g_cfgFile;//其他文件的全局变量

//构造函数
EnhanceMinuteKLine::EnhanceMinuteKLine()
{
	//初始化时间戳
	_time = 0;

	//打开配置文件
	/*ConfigFile cfg;
	if (cfg.Load("../data/config.conf") == false)
	{
		std::cerr << "EnhanceMinuteKLine::EnhanceMinuteKLine(): load config file failed\n" << std::endl;
		exit(1);
	}*/

	//获得写数据文件的目录
	_filePath = g_cfgFile.Get("KLineFilePath");
	if (_filePath == std::string())
	{
		std::cerr << "KLineFilePath not found in config file" << std::endl;
		exit(1);
	}

	//设置写csv文件的分隔符
	_sep = ',';
}

//回调函数，收到snapshot行情时调用
void EnhanceMinuteKLine::OnSnapshot(SnapshotInfo* snapInfo, uint32_t cnt)
{
	int cmpRet = CompareTime(_time, snapInfo[0]._tm );

	//数据时间戳在最新时间戳的上一分钟，不使用
	if (cmpRet < 0)
	{
		return;
	}

	//如果跨分钟了
	if (_time != 0 && cmpRet > 0)
	{
		_dataMutex.lock();
		
		//写文件，map重置清空
		WriteDataToFile();
		ClearData();

		_dataMutex.unlock();
	}

	//更新时间戳
	_time = snapInfo[0]._tm;

	//更新数据
	for (uint32_t i = 0; i < cnt; i++)
	{
		_dataMutex.lock();
		UpdateOrderEvaluateData(snapInfo[i]);
		_dataMutex.unlock();
	}
}


//回调函数，收到tickExecution行情时使用
void EnhanceMinuteKLine::OnOrderAndExecu(OrdAndExeInfo* ticks, uint32_t cnt)
{
	

	//printf("EnhanceMinuteKLine::OnTickExecution： ticks[0].security_code = %s, cnt = %u\n", ticks[0].security_code, cnt);

	int cmpRet = CompareTime(_time, ticks[0]._tm);

	//数据时间戳在最新时间戳的上一分钟，不使用
	if (cmpRet < 0)
	{
		return;
	}

	//如果跨分钟了
	if (_time != 0 && cmpRet > 0)
	{
		_dataMutex.lock();
		//写文件，map重置清空
		WriteDataToFile();
		ClearData();
		_dataMutex.unlock();
	}

	//更新时间戳
	_time = ticks[0]._tm;

	//更新统计数据
	for (uint32_t i = 0; i < cnt; i++)
	{
		_dataMutex.lock();
		UpdateExecutionData(ticks[i]);
		_dataMutex.unlock();
	}
}


//如果tim2相比tim1跨分钟了，返回1；没有，返回0；是tim1的前一分钟，返回-1
int32_t EnhanceMinuteKLine::CompareTime(uint32_t tim1, uint32_t tim2)
{
	uint32_t hour1, hour2;	//时
	uint32_t min1, min2;	//分
	uint32_t temp;

	temp = tim1 / 100000;
	min1 = temp % 100;
	temp = tim1 / 10000000;
	hour1 = temp % 100;

	temp = tim2 / 100000;
	min2 = temp % 100;
	temp = tim2 / 10000000;
	hour2 = temp % 100;

	if (hour1 == hour2)
	{
		if (min2 > min1)
		{
			return 1;
		}
		else if (min2 == min1)
		{
			return 0;
		}
		else
		{
			return -1;
		}
	}
	else if (hour1 < hour2)
	{
		return 1;
	}
	else
	{
		return -1;
	}
}

//把统计好的数据写入文件，文件名根据数据的时间来定，写完后关闭文件
void EnhanceMinuteKLine::WriteDataToFile()
{
	//用时和分组成文件名
	uint32_t hhmm = (_time / 100000) % 10000;
	std::string fileName = _filePath + "/" + std::to_string(hhmm) + "EnhancedMinuteKLine.csv";

	//写方式打开，没有就新建文件，有就清空
	FILE* fp = fopen(fileName.c_str(), "w");
	if (fp == NULL)
	{
		perror("fopen minuteKLine.csv");
		return;
	}
	//写文件的表头字段
	WriteHeaderOfFile(fp);

	//统计所有证券代码
	set<uint32_t> allCodes;
	for (pair<const uint32_t, OrderEvaluate>& elem : _securityAndOrderEvaluate)
	{
		allCodes.insert(elem.first);
	}
	for (pair<const uint32_t, ExecutionData> &elem : _securityAndExecutionData)
	{
		allCodes.insert(elem.first);
	}

	//写数据
	for (const uint32_t& code : allCodes)
	{
		//写证券代码
		fprintf(fp, "%06u%c", code, _sep);

		//写OrderEvaluate数据
		WriteOrderEvaluateToFile(_securityAndOrderEvaluate[code], fp);

		//写ExecutionData数据
		WriteExecutionDataToFile(_securityAndExecutionData[code], fp);
		
		//换行
		WriteLineFeedToFile(fp);
	}
	
	//关闭文件
	fclose(fp);
	
	return;
}

//写文件的表头
void EnhanceMinuteKLine::WriteHeaderOfFile(FILE* fp)
{
	fprintf(fp, "%s%c", "security_code", _sep);
	fprintf(fp, "%s%c", "accAmountBuy", _sep);
	fprintf(fp, "%s%c", "accAmountSell", _sep);
	fprintf(fp, "%s%c", "buyOrderAmt", _sep);
	fprintf(fp, "%s%c", "buyOrderVol", _sep);
	fprintf(fp, "%s%c", "activeBuyOrderAmt", _sep);
	fprintf(fp, "%s%c", "activeBuyOrderVol", _sep);
	fprintf(fp, "%s%c", "passiveBuyOrderAmt", _sep);
	fprintf(fp, "%s%c", "passiveBuyOrderVol", _sep);
	fprintf(fp, "%s%c", "sellOrderAmt", _sep);
	fprintf(fp, "%s%c", "sellOrderVol", _sep);
	fprintf(fp, "%s%c", "activeSellOrderAmt", _sep);
	fprintf(fp, "%s%c", "activeSellOrderVol", _sep);
	fprintf(fp, "%s%c", "passiveSellOrderAmt", _sep);
	fprintf(fp, "%s%c", "passiveSellOrderVol", _sep);
	fprintf(fp, "%s%c", "buyOrderCanceledAmt", _sep);
	fprintf(fp, "%s%c", "buyOrderCanceledVol", _sep);
	fprintf(fp, "%s%c", "sellOrderCanceledAmt", _sep);
	fprintf(fp, "%s%c", "sellOrderCanceledVol", _sep);

	fprintf(fp, "%s%c", "tradeNum", _sep);
	fprintf(fp, "%s%c", "buyTradeNum", _sep);
	fprintf(fp, "%s%c", "buyTradeAmt", _sep);
	fprintf(fp, "%s%c", "buyTradeVol", _sep);
	fprintf(fp, "%s%c", "sellTradeNum", _sep);
	fprintf(fp, "%s%c", "sellTradeAmt", _sep);
	fprintf(fp, "%s%c", "sellTradeVol", _sep);

	fprintf(fp, "%s%c", "startPrice", _sep);
	fprintf(fp, "%s%c", "highPrice", _sep);
	fprintf(fp, "%s%c", "lowPrice", _sep);
	fprintf(fp, "%s%c", "endPrice", _sep);

	//换行
	WriteLineFeedToFile(fp);

	return;
}

//orderEvaluate数据写文件
void EnhanceMinuteKLine::WriteOrderEvaluateToFile(const OrderEvaluate& orderEvl, FILE* fp)
{
	fprintf(fp, "%lu%c", orderEvl._accAmountBuy, _sep);
	fprintf(fp, "%lu%c", orderEvl._accAmountSell, _sep);
	fprintf(fp, "%lu%c", orderEvl._buyOrderAmt, _sep);
	fprintf(fp, "%lu%c", orderEvl._buyOrderVol, _sep);
	fprintf(fp, "%lu%c", orderEvl._activeBuyOrderAmt, _sep);
	fprintf(fp, "%lu%c", orderEvl._activeBuyOrderVol, _sep);
	fprintf(fp, "%lu%c", orderEvl._passiveBuyOrderAmt, _sep);
	fprintf(fp, "%lu%c", orderEvl._passiveBuyOrderVol, _sep);
	fprintf(fp, "%lu%c", orderEvl._sellOrderAmt, _sep);
	fprintf(fp, "%lu%c", orderEvl._sellOrderVol, _sep);
	fprintf(fp, "%lu%c", orderEvl._activeSellOrderAmt, _sep);
	fprintf(fp, "%lu%c", orderEvl._activeSellOrderVol, _sep);
	fprintf(fp, "%lu%c", orderEvl._passiveSellOrderAmt, _sep);
	fprintf(fp, "%lu%c", orderEvl._passiveSellOrderVol, _sep);
	fprintf(fp, "%lu%c", orderEvl._buyOrderCanceledAmt, _sep);
	fprintf(fp, "%lu%c", orderEvl._buyOrderCanceledVol, _sep);
	fprintf(fp, "%lu%c", orderEvl._sellOrderCanceledAmt, _sep);
	fprintf(fp, "%lu%c", orderEvl._sellOrderCanceledVol, _sep);
}

//executionData写文件
void EnhanceMinuteKLine::WriteExecutionDataToFile(const ExecutionData& exeData, FILE* fp)
{
	fprintf(fp, "%u%c", exeData._tradeNum, _sep);
	fprintf(fp, "%u%c", exeData._buyTradeNum, _sep);
	fprintf(fp, "%lu%c", exeData._buyTradeAmt, _sep);
	fprintf(fp, "%lu%c", exeData._buyTradeVol, _sep);
	fprintf(fp, "%u%c", exeData._sellTradeNum, _sep);
	fprintf(fp, "%lu%c", exeData._sellTradeAmt, _sep);
	fprintf(fp, "%lu%c", exeData._sellTradeVol, _sep);

	fprintf(fp, "%u%c", exeData._startPrice, _sep);
	fprintf(fp, "%u%c", exeData._highPrice, _sep);
	fprintf(fp, "%u%c", exeData._lowPrice, _sep);
	fprintf(fp, "%u%c", exeData._endPrice, _sep);
}

//清除map的数据，为了下一分钟重新统计
void EnhanceMinuteKLine::ClearData()
{
	_securityAndOrderEvaluate.clear();
	_securityAndExecutionData.clear();
	_lastSnapshots.clear();
}

//更新orderEvaluate数据
void EnhanceMinuteKLine::UpdateOrderEvaluateData(const SnapshotInfo& snapInfo)
{
	//如果没有上一次snapshot数据，就不计算，并把这次的snapshot存起来
	uint32_t code = snapInfo._sc % 1000000;
	if (_lastSnapshots.count(code) == 0)
	{
		_lastSnapshots[code] = snapInfo;
		return;
	}

	OrderEvaluate* pOrderEvl = &(_securityAndOrderEvaluate[code]);
	//有上次数据数据，统计OrderEvaluate的值
	SnapshotInfo* sp1 = &(_lastSnapshots[code]);//上次数据
	const SnapshotInfo* sp2 = &snapInfo;//本次数据

	uint32_t levelLen = amd::ama::ConstField::kPositionLevelLen;//档位个数
	int flag = 1;//遇到两个相同价格挂单数量一致，修改flag
	uint32_t idx1 = 0;
	uint32_t idx2 = 0;
	uint32_t price;		//价格
	uint64_t volume;	//数量
	uint64_t amt;		//金额
	//遍历ask数据
	while (idx1 < levelLen && idx2 < levelLen)
	{
		//考虑其中有价格数量全为0的档位
		//两个都是全0
		if ((sp1->_pxAsk[idx1] == 0 && sp1->_qtyAsk[idx1] == 0) &&
			(sp2->_pxAsk[idx2] == 0 && sp2->_qtyAsk[idx2] == 0))
		{
			//后面没数据了
			break;
		}
		else if ((sp1->_pxAsk[idx1] == 0 && sp1->_qtyAsk[idx1] == 0) && !(sp2->_pxAsk[idx2] == 0 && sp2->_qtyAsk[idx2] == 0))
		{
			//sp1当前档位是全0，sp2的档位是新增的，统计新增卖委托
			price = sp2->_pxAsk[idx2];	//价格
			volume = sp2->_qtyAsk[idx2];	//数量
			amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额

			//新增卖委托金额
			pOrderEvl->_sellOrderAmt += amt;
			//新增卖委托量
			pOrderEvl->_sellOrderVol += volume;

			//上个tick中间价
			uint32_t lastMidPrice = GetMidPrice(*sp1);
			if (price < lastMidPrice)
			{
				//新增卖委托金额，积极委托
				pOrderEvl->_activeSellOrderAmt += amt;
				//新增卖委托量，积极委托
				pOrderEvl->_activeSellOrderVol += volume;
			}
			else if (price > lastMidPrice)
			{
				//新增卖委托金额，消极委托
				pOrderEvl->_passiveSellOrderAmt += amt;
				//新增卖委托量，积极委托
				pOrderEvl->_passiveSellOrderVol += volume;
			}
			//idx2继续后移
			idx2++;
			continue;
		}
		else if (!(sp1->_pxAsk[idx1] == 0 && sp1->_qtyAsk[idx1] == 0) && (sp2->_pxAsk[idx2] == 0 && sp2->_qtyAsk[idx2] == 0))
		{
			//sp1当前档位不是全0，sp2当前档位是全0
			price = sp1->_pxAsk[idx1];	//价格
			volume = sp1->_qtyAsk[idx1];	//数量
			amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额
			//是被买完了
			if (flag == 1)
			{
				//更新买金额
				pOrderEvl->_accAmountBuy += amt;
			}
			//是自己撤单，撤完了
			else if (flag == 0)
			{
				//上个tick中间价
				int64_t lastMidPrice = GetMidPrice(*sp1);
				//如果高于上个tick中间价
				if (price > lastMidPrice)
				{
					//更新卖委托撤单金额
					pOrderEvl->_sellOrderCanceledAmt += amt;
					//更新卖委托撤单数量
					pOrderEvl->_sellOrderCanceledVol += volume;
				}
			}

			//idx1继续后移
			idx1++;
			continue;
		}

		//两个指针指向的档位价格一样
		if (sp1->_pxAsk[idx1] == sp2->_pxAsk[idx2])
		{
			//比较数量，如果减少了
			if (sp2->_qtyAsk[idx2] < sp1->_qtyAsk[idx1])
			{
				//价格
				price = sp1->_pxAsk[idx1];
				//数量
				volume = sp1->_qtyAsk[idx1] - sp2->_qtyAsk[idx2];
				//金额
				amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;
				
				if (flag == 1)
				{
					//主买，金额增加	
					pOrderEvl->_accAmountBuy += amt;
				}
				else if (flag == 0)
				{
					//卖委托，撤单了
					//上个tick中间价
					uint32_t lastMidPrice = GetMidPrice(*sp1);
					//如果高于上个tick中间价
					if (price > lastMidPrice)
					{
						//更新卖委托撤单金额
						pOrderEvl->_sellOrderCanceledAmt += amt;
						//更新卖委托撤单数量
						pOrderEvl->_sellOrderCanceledVol += volume;
					}
				}
			}
			//卖委托增加了
			else if (sp2->_qtyAsk[idx2] > sp1->_qtyAsk[idx1])
			{
				price = sp1->_pxAsk[idx1];	//价格	
				volume = sp2->_qtyAsk[idx2] - sp1->_qtyAsk[idx1];	//数量
				amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;//金额

				//新增卖委托金额
				pOrderEvl->_sellOrderAmt += amt;
				//新增卖委托量
				pOrderEvl->_sellOrderVol += volume;

				//上个tick中间价
				uint32_t lastMidPrice = GetMidPrice(*sp1);
				if (price < lastMidPrice)
				{
					//新增卖委托金额，积极委托
					pOrderEvl->_activeSellOrderAmt += amt;
					//新增卖委托量，积极委托
					pOrderEvl->_activeSellOrderVol += volume;
				}
				else if (price > lastMidPrice)
				{
					//新增卖委托金额，消极委托
					pOrderEvl->_passiveSellOrderAmt += amt;
					//新增卖委托量，积极委托
					pOrderEvl->_passiveSellOrderVol += volume;
				}
			}
			else//数量相等，修改flag
			{
				flag = 0;
			}

			//两个指针都向后移，统计下个档位
			idx1++;
			idx2++;
		}
		else if (sp1->_pxAsk[idx1] > sp2->_pxAsk[idx2])
		{
			//sp1的更大，那么sp2这边这个是新增的卖委托

			price = sp2->_pxAsk[idx2];	//价格
			volume = sp2->_qtyAsk[idx2];	//数量
			amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额

			//新增卖委托金额
			pOrderEvl->_sellOrderAmt += amt;
			//新增卖委托量
			pOrderEvl->_sellOrderVol += volume;

			//上个tick中间价
			int64_t lastMidPrice = GetMidPrice(*sp1);
			if (price < lastMidPrice)
			{
				//新增卖委托金额，积极委托
				pOrderEvl->_activeSellOrderAmt += amt;
				//新增卖委托量，积极委托
				pOrderEvl->_activeSellOrderVol += volume;
			}
			else if (price > lastMidPrice)
			{
				//新增卖委托金额，消极委托
				pOrderEvl->_passiveSellOrderAmt += amt;
				//新增卖委托量，积极委托
				pOrderEvl->_passiveSellOrderVol += volume;
			}

			//移动idx2，看下一次是否两个指针指向价格相等
			idx2++;
		}
		else if (sp1->_pxAsk[idx1] < sp2->_pxAsk[idx2])
		{
			//sp2的价格更高，说明sp1那边的价位的卖委托，上次有，现在没了
			price = sp1->_pxAsk[idx1];	//价格
			volume = sp1->_qtyAsk[idx1];	//数量
			amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额
			//是被买完了
			if (flag == 1)
			{
				//更新买金额
				pOrderEvl->_accAmountBuy += amt;
			}
			//是自己撤单，撤完了
			else if (flag == 0)
			{
				//上个tick中间价
				uint32_t lastMidPrice = GetMidPrice(*sp1);
				//如果高于上个tick中间价
				if (price > lastMidPrice)
				{
					//更新卖委托撤单金额
					pOrderEvl->_sellOrderCanceledAmt += amt;
					//更新卖委托撤单数量
					pOrderEvl->_sellOrderCanceledVol += volume;
				}
			}

			//向后移动idx1
			idx1++;
		}
	}

	flag = 1;
	idx1 = 0;
	idx2 = 0;
	//遍历bid数据
	while (idx1 < levelLen && idx2 < levelLen)
	{
		//如果sp1当前档位价格和数量全是0，同时sp2当前档位价格数量全是0，那后面就没数据了，退出
		if ((sp1->_pxBid[idx1] == 0 && sp1->_qtyBid[idx1] == 0) && (sp2->_pxBid[idx2] == 0 && sp2->_qtyBid[idx2] == 0))
		{
			break;
		}
		else if (!(sp1->_pxBid[idx1] == 0 && sp1->_qtyBid[idx1] == 0) && (sp2->_pxBid[idx2] == 0 && sp2->_qtyBid[idx2] == 0))
		{
			//sp1当前档位不为0，sp2为0，说明是sp1的减少了，判断是主卖成交了，还是买委托撤单了
			price = sp1->_pxBid[idx1];	//价格
			volume = sp1->_qtyBid[idx1];	//数量
			amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额

			if (flag == 1)
			{
				//主卖成交了,更新卖的金额
				pOrderEvl->_accAmountSell += amt;
			}
			else if (flag == 0)
			{
				//买委托撤单了
				//上个tick的中间价
				uint32_t lastMidPrice = GetMidPrice(*sp1);

				//低于上个tick中间价
				if (price < lastMidPrice)
				{
					//更新买委托撤单的金额
					pOrderEvl->_buyOrderCanceledAmt += amt;
					//更新买委托撤单的数量
					pOrderEvl->_buyOrderCanceledVol += volume;
				}
			}

			//继续遍历sp1的下个档位
			idx1++;
			continue;
		}
		else if ((sp1->_pxBid[idx1] == 0 && sp1->_qtyBid[idx1] == 0) && !(sp2->_pxBid[idx2] == 0 && sp2->_qtyBid[idx2] == 0))
		{
			//sp1当前档位为0，sp2不为0，说明sp2的档位是新增的买委托，统计买委托数据
			price = sp2->_pxBid[idx1];	//价格
			volume = sp2->_qtyBid[idx2];	//数量
			amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额

			//新增买委托金额
			pOrderEvl->_buyOrderAmt += amt;
			//新增买委托量
			pOrderEvl->_buyOrderVol += volume;

			//上个tick的中间价
			uint32_t lastMidPrice = GetMidPrice(*sp1);

			if (price > lastMidPrice)
			{
				//新增买委托金额，积极委托
				pOrderEvl->_activeBuyOrderAmt += amt;
				//新增买委托量，积极委托
				pOrderEvl->_activeBuyOrderVol += volume;
			}
			else if (price < lastMidPrice)
			{
				//新增买委托金额，消极委托
				pOrderEvl->_passiveBuyOrderAmt += amt;
				//新增买委托量，消极委托
				pOrderEvl->_passiveBuyOrderVol += volume;
			}

			//遍历sp2的下一个档位
			idx2++;
			continue;
		}

		//两个指针指向的档位价格一样
		if (sp1->_pxBid[idx1] == sp2->_pxBid[idx2])
		{
			//比较数量，如果买委托减少了
			if (sp1->_qtyBid[idx1] > sp2->_qtyBid[idx2])
			{
				price = sp1->_pxBid[idx1];	//价格
				volume = sp1->_qtyBid[idx1] - sp2->_qtyBid[idx2];	//减少的数量
				amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额

				//是因为有主动卖所以减少
				if (flag == 1)
				{
					//更新卖的金额
					pOrderEvl->_accAmountSell += amt;
				}
				else if (flag == 0)//因为自己撤了买委托，所以减少
				{
					//上个tick的中间价
					uint32_t lastMidPrice = GetMidPrice(*sp1);
					
					//低于上个tick中间价
					if (price < lastMidPrice)
					{
						//更新买委托撤单的金额
						pOrderEvl->_buyOrderCanceledAmt += amt;
						//更新买委托撤单的数量
						pOrderEvl->_buyOrderCanceledVol += volume;
					}
				}
			}
			else if (sp1->_qtyBid[idx1] < sp2->_qtyBid[idx2])//买委托增加了
			{
				price = sp1->_pxBid[idx1];	//价格
				volume = sp2->_qtyBid[idx2] - sp1->_qtyBid[idx1];	//增加的数量
				amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额

				//新增买委托金额
				pOrderEvl->_buyOrderAmt += amt;
				//新增买委托量
				pOrderEvl->_buyOrderVol += volume;
				
				//上个tick的中间价
				int64_t lastMidPrice = GetMidPrice(*sp1);
				
				if (price > lastMidPrice)
				{
					//新增买委托金额，积极委托
					pOrderEvl->_activeBuyOrderAmt += amt;
					//新增买委托量，积极委托
					pOrderEvl->_activeBuyOrderVol += volume;
				}
				else if (price < lastMidPrice)
				{
					//新增买委托金额，消极委托
					pOrderEvl->_passiveBuyOrderAmt += amt;
					//新增买委托量，消极委托
					pOrderEvl->_passiveBuyOrderVol += volume;
				}
			}
			else//两个价格相同数量也相等，修改flag
			{
				flag = 0;
			}

			//两个指针都向后移，统计下一个档位
			idx1++;
			idx2++;
		}
		else if (sp1->_pxBid[idx1] < sp2->_pxBid[idx2])
		{
			//sp2里的这个档位的价格，是新增的买委托
			price = sp2->_pxBid[idx2];	//价格
			volume = sp2->_qtyBid[idx2];	//增加的数量
			amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额

			//新增买委托金额
			pOrderEvl->_buyOrderAmt += amt;
			//新增买委托量
			pOrderEvl->_buyOrderVol += volume;

			//上个tick的中间价
			int64_t lastMidPrice = GetMidPrice(*sp1);

			if (price > lastMidPrice)
			{
				//新增买委托金额，积极委托
				pOrderEvl->_activeBuyOrderAmt += amt;
				//新增买委托量，积极委托
				pOrderEvl->_activeBuyOrderVol += volume;
			}
			else if (price < lastMidPrice)
			{
				//新增买委托金额，消极委托
				pOrderEvl->_passiveBuyOrderAmt += amt;
				//新增买委托量，消极委托
				pOrderEvl->_passiveBuyOrderVol += volume;
			}

			//idx2往后移，看下一个是否和sp1里的相等
			idx2++;
		}
		else if (sp1->_pxBid[idx1] > sp2->_pxBid[idx2])
		{
			//原本在sp1里有的一个申买价格档位，在sp2里没有了，是有人主动卖给他了，还是自己撤单了
			price = sp1->_pxBid[idx1];	//价格
			volume = sp1->_qtyBid[idx1];	//减少的数量
			amt = price * volume * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;	//金额

			//是因为有主动卖所以减少
			if (flag == 1)
			{
				//更新卖的金额
				pOrderEvl->_accAmountSell += amt;
			}
			else if (flag == 0)//因为自己撤了买委托，所以减少
			{
				//上个tick的中间价
				int64_t lastMidPrice = GetMidPrice(*sp1);

				//委托价格是否低于上个tick的中间价
				if (price < lastMidPrice)
				{
					//更新买委托撤单的金额
					pOrderEvl->_buyOrderCanceledAmt += amt;
					//更新买委托撤单的数量
					pOrderEvl->_buyOrderCanceledVol += volume;
				}
			}

			//idx1后移，统计下一个档位
			idx1++;
		}
	}

	//把这次数据存起来，下次使用
	_lastSnapshots[code] = snapInfo;

	return;
}

//更新成交数据
void EnhanceMinuteKLine::UpdateExecutionData(const OrdAndExeInfo& tickExecution)
{
	//如果不是成交数据，就不处理
	if (!(tickExecution._MLFlag == 'A' || tickExecution._MLFlag == 'C'))
	{
		return;
	}

	uint32_t code = tickExecution._sc % 1000000;
	//如果是这一分钟第一次收到该证券的数据，设置开始价
	if (_securityAndExecutionData.count(code) == 0)
	{
		_securityAndExecutionData[code]._startPrice = tickExecution._px;
		//设置最高价和最低价，后面每次都比较更新
		_securityAndExecutionData[code]._highPrice = tickExecution._px;
		_securityAndExecutionData[code]._lowPrice = tickExecution._px;
	}

	//这里是不是可以 if else，是第一次，不是第一次，
	//暂时别改

	ExecutionData* pExeData = &(_securityAndExecutionData[code]);
	//更新最高价
	if (tickExecution._px > pExeData->_highPrice)
	{
		pExeData->_highPrice = tickExecution._px;
	}
	//更新最低价
	if (tickExecution._px < pExeData->_lowPrice)
	{
		pExeData->_lowPrice = tickExecution._px;
	}
	//保留最新一次的价格，跨分钟时直接作为结束价记录
	pExeData->_endPrice = tickExecution._px;

	//交易笔数加一
	pExeData->_tradeNum++;

	//int32_t flag = -1;
	//判断是主动买还是主动卖
	//判断交易市场
	//if (tickExecution._MLFlag == 'C')
	//{
	//	//深交所
	//	//主买成交
	//	if (tickExecution._BSFlag == 'B')
	//	{
	//		flag = 1;
	//	}
	//	else
	//	{
	//		flag = 0;
	//	}
	//}
	//else if (tickExecution._MLFlag == 'A')//上交所
	//{
	//	//主买成交
	//	if (tickExecution._BSFlag == 'B')
	//	{
	//		flag = 1;
	//	}
	//	else if (tickExecution._BSFlag == 'S')//主卖成交
	//	{
	//		flag = 0;
	//	}
	//	else
	//	{
	//		//printf("tickExecution.size == 'N', not buy or sell\n");
	//		return;
	//	}
	//}

	//主买成交
	if (tickExecution._BSFlag == 'B')
	{
		//主买成交交易笔数
		pExeData->_buyTradeNum++;
		//主买成交金额
		pExeData->_buyTradeAmt += tickExecution._px * tickExecution._qty * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;
		//主买成交交易量
		pExeData->_buyTradeVol += tickExecution._qty;
	}
	else if (tickExecution._BSFlag == 'S')	//主卖成交
	{
		//统计主卖成交笔数
		pExeData->_sellTradeNum++;
		//主卖成交金额
		pExeData->_sellTradeAmt += tickExecution._px * tickExecution._qty * AMT_MULTIPLE / PRICE_MULTIPLE / QTY_MULTIPLE;
		//主卖成交量
		pExeData->_sellTradeVol += tickExecution._qty;
	}
	else
	{
		//printf("tickExecution BSflag is not B or S, it is %c\n", tickExecution._BSFlag);
	}
	
	return;
}

//根据snapshot数据计算中间价
uint32_t EnhanceMinuteKLine::GetMidPrice(const SnapshotInfo& snapshot)
{
	uint32_t midPrice;
	uint32_t firstBidPrice = snapshot._pxBid[0];
	uint32_t firstOfferPrice = snapshot._pxAsk[0];
	if (firstBidPrice == 0 && firstOfferPrice == 0)
	{
		midPrice = snapshot._pxLast;
	}
	else if (firstBidPrice == 0 && firstOfferPrice != 0)
	{
		midPrice = firstOfferPrice;
	}
	else if (firstBidPrice != 0 && firstOfferPrice == 0)
	{
		midPrice = firstBidPrice;
	}
	else
	{
		midPrice = (firstBidPrice + firstOfferPrice) / 2;
	}

	return midPrice;
}

//创造实例的函数
MDApplication* CreateObjKLine()
{
	return new EnhanceMinuteKLine;
}

//注册到反射里，key为4
RegisterAction g_regActKLine("4", CreateObjKLine);