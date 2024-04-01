#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <string>
#include "StatisticalData.h"
#include "ConfigFile.h"
#include "SPLog.h"
#include "reflect.h"
#include "sp_tool.h"

#define	PRICE_MULTIPLE	1000		//Price的扩大倍数
#define QTY_MULTIPLE	1			//Qty的扩大倍数
#define AMT_MULTIPLE	1000		//Amt的扩大倍数

using std::string;
using std::vector;

extern ConfigFile g_cfgFile;		//其他文件的全局变量
extern SPLog g_log;					//其他文件的全局变量，日志文件

//构造函数
StatisticalData::StatisticalData()
{
	//初始化时间戳
	_tmSse = 0;
	_tmExecuSse = 0;
	_tmSzse = 0;
	_tmExecuSzse = 0;

	//分隔符
	_sep = ',';
}

//重写盘前处理函数
int StatisticalData::BeforeMarket()
{
	//从配置文件获得放置统计数据的路径
	string filePath = g_cfgFile.Get("StatisticalDataPath");
	//如果是空字符串
	if (filePath.size() == 0)
	{
		//记录日志
		g_log.WriteLog("not found StatisticalData in config file");
		//返回非0值，代表盘前处理失败
		return -1;
	}

	//在路径下创建上交所和深交所的数据目录，初始化成员变量
	_sseFilePath = filePath + "/" + "sse";
	_szseFilePath = filePath + "/" + "szse";
	int ret;
	ret = system(("mkdir " + _sseFilePath).c_str());
	if (ret != 0)
	{
		g_log.WriteLog("mkdir _sseFilePath failed");
		//return -1;
	}
	else
	{
		g_log.WriteLog("mkdir _sseFilePath success");
	}

	ret = system(("mkdir " + _szseFilePath).c_str());
	if (ret != 0)
	{
		g_log.WriteLog("mkdir _szseFilePath failed");
		//return -1;
	}
	else
	{
		g_log.WriteLog("mkdir _szseFilePath success");
	}

	return 0;
}

void StatisticalData::OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt)
{
	//printf("StatisticalData::OnSnapshot() is called\n");
	//判断是哪个交易所的
	//printf("code = %u\n", snapshot->_sc);
	uint32_t topDigit = snapshot[0]._sc / 100000000;
	int32_t cmpRet;

	if (topDigit == 1)	//上交所的
	{
		cmpRet = CompareTime(_tmSse, snapshot[0]._tm);

		//如果是上一分钟的数据，不使用
		if (cmpRet < 0)
		{
			return;
		}

		//如果跨分钟了
		if (cmpRet > 0)
		{
			//跨分钟时统计
			CalculateInMiniuteEnd(_lastSnapshotSse, _snapTempDataSse, _snapStatisDataSse);
			//原始数据写文件
			WriteOriginalSnapToFile(_sseFilePath, _tmSse, _lastSnapshotSse);
			//snapshot统计数据写文件
			WriteSnapStatisDataToFile(_sseFilePath, _tmSse, _snapStatisDataSse);
			//清除本分钟内一些数据，参数是某些映射map
			ClearSnapData(_snapTempDataSse, _snapStatisDataSse);
		}

		//更新时间戳
		_tmSse = snapshot[0]._tm;

		//根据snapshot数据更新统计信息
		for (uint32_t i = 0; i < cnt; i++)
		{
			UpdateSnapStatisData(snapshot[i], _lastSnapshotSse, _snapTempDataSse, _snapStatisDataSse);
		}
	}
	else if (topDigit == 2)	//深交所
	{
		cmpRet = CompareTime(_tmSzse, snapshot[0]._tm);

		//如果是上一分钟的数据，不使用
		if (cmpRet < 0)
		{
			return;
		}

		//如果跨分钟了
		if (cmpRet > 0)
		{
			//跨分钟时统计
			CalculateInMiniuteEnd(_lastSnapshotSzse, _snapTempDataSzse, _snapStatisDataSzse);
			//写入文件
			WriteOriginalSnapToFile(_szseFilePath, _tmSzse, _lastSnapshotSzse);
			WriteSnapStatisDataToFile(_szseFilePath, _tmSzse, _snapStatisDataSzse);
			//清除本分钟内一些数据
			ClearSnapData(_snapTempDataSzse, _snapStatisDataSzse);
		}

		//更新时间戳
		_tmSzse = snapshot[0]._tm;

		//根据snapshot数据更新统计信息
		for (uint32_t i = 0; i < cnt; i++)
		{
			UpdateSnapStatisData(snapshot[i], _lastSnapshotSzse, _snapTempDataSzse, _snapStatisDataSzse);
		}
	}
	else
	{
		g_log.WriteLog("StatisticalData::OnSnapshot()，code top digit is not 1 or 2");
		return;
	}

	return;
}


int32_t StatisticalData::CompareTime(uint32_t tim1, uint32_t tim2)
{
	//如果tim1是0，就不算跨分钟
	if (tim1 == 0)
	{
		return 0;
	}

	uint32_t hour1 = tim1 / 10000000;
	uint32_t min1 = (tim1 / 100000) % 100;
	uint32_t hour2 = tim2 / 10000000;
	uint32_t min2 = (tim2 / 100000) % 100;
	
	if (hour1 == hour2)
	{
		//在同一个小时
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
			//返回-1，代表也跨分钟了，但是tim2在tim1前面
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


//根据收到的一个snapshot，统计相关数据
void StatisticalData::UpdateSnapStatisData(const SnapshotInfo& snap,
	std::map<uint32_t, SnapshotInfo>& lastSnapshots,
	std::map<uint32_t, SnapTempData>& snapTempDatas,
	std::map<uint32_t, SnapStatisData>& snapStatisDatas)
{
	uint32_t code = snap._sc;
	
	//每次切片需要暂存一些数据（数组）
	//委卖价格跨度
	double askPrcGap = GetAskPrcGap(snap);
	if (askPrcGap != 0)//如果是0，说明没有卖委托，或者只有一个档位没有跨度，不记录
	{
		//转成double，恢复原值
		snapTempDatas[code]._askPrcGap.emplace_back(askPrcGap);
	}
	//委买价格跨度
	double bidPrcGap = GetBidPrcGap(snap);
	if (bidPrcGap != 0)
	{
		snapTempDatas[code]._bidPrcGap.emplace_back(bidPrcGap);
	}
	//卖1和买1跨度
	uint32_t askAndBidPrcGap = 0;
	if (snap._pxAsk[0] != 0 && snap._pxBid[0] != 0)//考虑到其中一个可能是0
	{
		askAndBidPrcGap = snap._pxAsk[0] - snap._pxBid[0];
		snapTempDatas[code]._askAndBidPrcGap.emplace_back(static_cast<double>(askAndBidPrcGap) / PRICE_MULTIPLE);
	}
	
	//委买金额
	double totBidAmt = static_cast<double>(snap._pxAvgBid) / PRICE_MULTIPLE * snap._qtyTotBid;
	//委卖金额
	double totAskAmt = static_cast<double>(snap._pxAvgAsk) / PRICE_MULTIPLE * snap._qtyTotAsk;
	//净委买金额
	double netBidAmt;
	netBidAmt = totBidAmt - totAskAmt;
	
	snapTempDatas[code]._netBidAmt.emplace_back(netBidAmt);

	//如果存在上个snapshot
	if (lastSnapshots.find(code) != lastSnapshots.end())
	{
		//收益，考虑到上个_pxLast可能是0
		if (lastSnapshots[code]._pxLast != 0)
		{
			//这里两个价格都不还原除以1000，不影响结果
			double revenue = (static_cast<double>(snap._pxLast) / lastSnapshots[code]._pxLast) - 1;
			snapTempDatas[code]._revenues.emplace_back(revenue);
		}
		
		//切片之间的时间间隔，结果以毫秒为单位
		uint32_t timInterv = ComputeTimeInterval(lastSnapshots[code]._tm, snap._tm);
		snapTempDatas[code]._timInterv.emplace_back(timInterv);
	}
	 
	//更新一些统计数据
	SnapStatisData& statisData = snapStatisDatas[code];

	//如果存在lastSnapshot
	if (lastSnapshots.find(code) != lastSnapshots.end())
	{
		SnapshotInfo& snapLast = lastSnapshots[code];
		//通过先后两个snapshot的委托价格，计算一些数据，更新到统计数据里
		GetOrderEvaluate(snapLast, snap, statisData);

		//统计成交价格上行或下行
		if (snap._pxLast != 0 && snapLast._pxLast != 0)
		{
			if (snap._pxLast > snapLast._pxLast)
			{
				++statisData._prcIncre;
			}
			else if (snap._pxLast < snapLast._pxLast)
			{
				++statisData._prcDecre;
			}
			else
			{
				++statisData._prcUnchange;
			}
		}
	}

	//不管lastSnapshot是否存在，都需要统计的数据
	++statisData._snapCount;

	//更新lastSnapshot
	lastSnapshots[code] = snap;

	return;
}

//分钟结束时，计算一些数据
void StatisticalData::CalculateInMiniuteEnd(std::map<uint32_t, SnapshotInfo>& lastSnapshots,
	std::map<uint32_t, SnapTempData>& snapTempDatas,
	std::map<uint32_t, SnapStatisData>& snapStatisDatas)
{
	uint32_t code;
	//遍历所有证券的临时数组数据
	for (auto it = snapTempDatas.begin(); it != snapTempDatas.end(); ++it)
	{
		code = it->first;
		SnapTempData& tmpData = it->second;		//该证券临时数据结构体的引用
		SnapStatisData& statisData = snapStatisDatas[code];		//该证券统计数据结构体的引用

		
		if (tmpData._askPrcGap.empty() != true)
		{
			//委卖价格跨度的最大最小值
			auto prcGapIt = std::max_element(tmpData._askPrcGap.begin(), tmpData._askPrcGap.end());
			statisData._askGapMax = *prcGapIt;
			prcGapIt = std::min_element(tmpData._askPrcGap.begin(), tmpData._askPrcGap.end());
			statisData._askGapMin = *prcGapIt;
			//统计委卖价格跨度的均值
			statisData._askGapAvrg = std::accumulate(tmpData._askPrcGap.begin(), tmpData._askPrcGap.end(), 0) / tmpData._askPrcGap.size();
			//委卖价格跨度的标准差
			statisData._askGapSD = CalculateStandardDeviation(tmpData._askPrcGap, statisData._askGapAvrg);
			//价格跨度变化
			statisData._askGapChange = *(tmpData._askPrcGap.end() - 1) - *(tmpData._askPrcGap.begin());
		}
		
		if (tmpData._bidPrcGap.empty() != true)
		{
			//委买价格跨度的最值
			auto prcGapIt = std::max_element(tmpData._bidPrcGap.begin(), tmpData._bidPrcGap.end());
			statisData._bidGapMax = *prcGapIt;
			prcGapIt = std::min_element(tmpData._bidPrcGap.begin(), tmpData._bidPrcGap.end());
			statisData._bidGapMin = *prcGapIt;

			//委买价格跨度的均值
			statisData._bidGapAvrg = std::accumulate(tmpData._bidPrcGap.begin(), tmpData._bidPrcGap.end(), 0) / tmpData._bidPrcGap.size();
			//委买价格跨度的标准差
			statisData._bidGapSD = CalculateStandardDeviation(tmpData._bidPrcGap, statisData._bidGapAvrg);
			//价格跨度变化
			statisData._bidGapChange = *(tmpData._bidPrcGap.end() - 1) - *(tmpData._bidPrcGap.begin());
		}
		
		if (tmpData._askAndBidPrcGap.empty() == false)
		{
			//买1卖1价格跨度的最值
			auto prcGapIt = std::max_element(tmpData._askAndBidPrcGap.begin(), tmpData._askAndBidPrcGap.end());
			statisData._bidAskGapMax = *prcGapIt;
			prcGapIt = std::min_element(tmpData._askAndBidPrcGap.begin(), tmpData._askAndBidPrcGap.end());
			statisData._bidAskGapMin = *prcGapIt;

			//买1卖1价格跨度的均值
			statisData._bidAskGapAvrg = std::accumulate(tmpData._askAndBidPrcGap.begin(), tmpData._askAndBidPrcGap.end(), 0) / tmpData._askAndBidPrcGap.size();
			//买1卖1价格跨度的标准差
			statisData._bidAskGapSD = CalculateStandardDeviation(tmpData._askAndBidPrcGap, statisData._bidAskGapAvrg);
			//价格跨度变化
			statisData._bidAskGapChange = *(tmpData._askAndBidPrcGap.end() - 1) - *(tmpData._askAndBidPrcGap.begin());
		}

		if (tmpData._revenues.empty() == false)
		{
			float reveAvg = std::accumulate(tmpData._revenues.begin(), tmpData._revenues.end(), 0.0f) / tmpData._revenues.size();
			//收益标准差
			statisData._revenueStandardDeviation = CalculateStandardDeviation(tmpData._revenues, reveAvg);
			//收益偏度
			statisData._revenueSkewness = CalculateSkewness(tmpData._revenues, reveAvg, statisData._revenueStandardDeviation);
		}
		
		if (tmpData._timInterv.empty() == false)
		{
			//切片间隔的最大最小值
			auto iterTim = std::max_element(tmpData._timInterv.begin(), tmpData._timInterv.end());
			statisData._maxTimInterv = *iterTim;
			iterTim = std::min_element(tmpData._timInterv.begin(), tmpData._timInterv.end());
			statisData._minTimInterv = *iterTim;
			//切片间隔时间的均值
			statisData._timIntervAvrg = std::accumulate(tmpData._timInterv.begin(), tmpData._timInterv.end(), 0) / tmpData._timInterv.size();
			//切片间隔时间的标准差
			statisData._timIntervSD = CalculateStandardDeviation(tmpData._timInterv, statisData._timIntervAvrg);
		}
		
		if (tmpData._netBidAmt.empty() == false)
		{
			//净委买金额的均值，标准差
			double sum = std::accumulate(tmpData._netBidAmt.begin(), tmpData._netBidAmt.end(), 0l);
			//double avrg = sum / tmpData._netBidAmt.size();
			statisData._netBidAmtAvrg = sum / tmpData._netBidAmt.size();
			statisData._netBidAmtSD = CalculateStandardDeviation(tmpData._netBidAmt, statisData._netBidAmtAvrg);
			
			/*if (code == 100501000)
			{
				printf("sum = %lf, size = %lu, avrg = %lf, sd = %lf\n",
					sum, tmpData._netBidAmt.size(), avrg, statisData._netBidAmtSD);
			}*/
		}

		//切片间数据，收益，只需要临时数据里收益数组的最后一个
		if (tmpData._revenues.empty() == false)
		{
			statisData._snapRevenue = *(tmpData._revenues.end() - 1);
		}
		else
		{
			statisData._snapRevenue = 0;
		}
		
		//全天数据，收益，相对于开盘价的收益
		if (_unchangedMarketDatas[code]._openPrice != 0)
		{
			statisData._allDayRevenueByOpenPric = (static_cast<double>(lastSnapshots[code]._pxLast) / _unchangedMarketDatas[code]._openPrice) - 1;
		}
		else
		{
			statisData._allDayRevenueByOpenPric = 0;
		}
		
		//全天数据，收益，相对于昨收价的收益
		if (_unchangedMarketDatas[code]._preClosePrice != 0)
		{
			statisData._allDayRevenueByPreClosePric = (static_cast<double>(lastSnapshots[code]._pxLast) / _unchangedMarketDatas[code]._preClosePrice) - 1;
		}
		else
		{
			statisData._allDayRevenueByPreClosePric = 0;
		}
	}

	return;
}

void StatisticalData::GetOrderEvaluate(const SnapshotInfo& snap1, const SnapshotInfo& snap2, SnapStatisData& statisData)
{
	const SnapshotInfo* sp1 = &snap1;
	const SnapshotInfo* sp2 = &snap2;
	SnapStatisData* pOrderEvl = &statisData;

	uint32_t levelLen = amd::ama::ConstField::kPositionLevelLen;//档位个数
	int flag = 1;//遇到两个相同价格挂单数量一致，修改flag
	uint32_t idx1 = 0;
	uint32_t idx2 = 0;
	double price;		//价格
	uint64_t volume;	//数量
	double amt;		//金额
	double accAmountBuy = 0;			//买的金额(10^3)
	double accAmountSell = 0;			//卖的金额
	double buyOrderAmt = 0;			//新增买委托金额(10^3)
	double sellOrderAmt = 0;			//新增卖委托金额
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
			price = static_cast<double>(sp2->_pxAsk[idx2]) / PRICE_MULTIPLE;	//价格
			volume = sp2->_qtyAsk[idx2];	//数量
			amt = price * volume;	//金额

			//新增卖委托金额
			sellOrderAmt += amt;
			//新增卖委托量
			pOrderEvl->_sellOrderVol += volume;

			//上个tick中间价
			double lastMidPrice = GetMidPrice(*sp1);
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
			price = static_cast<double>(sp1->_pxAsk[idx1]) / PRICE_MULTIPLE;	//价格
			volume = sp1->_qtyAsk[idx1];	//数量
			amt = price * volume;	//金额
			//是被买完了
			if (flag == 1)
			{
				//更新买金额
				accAmountBuy += amt;
			}
			//是自己撤单，撤完了
			else if (flag == 0)
			{
				//上个tick中间价
				double lastMidPrice = GetMidPrice(*sp1);
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
				price = static_cast<double>(sp1->_pxAsk[idx1]) / PRICE_MULTIPLE;
				//数量
				volume = sp1->_qtyAsk[idx1] - sp2->_qtyAsk[idx2];
				//金额
				amt = price * volume;

				if (flag == 1)
				{
					//主买，金额增加	
					accAmountBuy += amt;
				}
				else if (flag == 0)
				{
					//卖委托，撤单了
					//上个tick中间价
					double lastMidPrice = GetMidPrice(*sp1);
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
				price = static_cast<double>(sp1->_pxAsk[idx1]) / PRICE_MULTIPLE;	//价格	
				volume = sp2->_qtyAsk[idx2] - sp1->_qtyAsk[idx1];	//数量
				amt = price * volume;//金额

				//新增卖委托金额
				sellOrderAmt += amt;
				//新增卖委托量
				pOrderEvl->_sellOrderVol += volume;

				//上个tick中间价
				double lastMidPrice = GetMidPrice(*sp1);
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

			price = static_cast<double>(sp2->_pxAsk[idx2]) / PRICE_MULTIPLE;	//价格
			volume = sp2->_qtyAsk[idx2];	//数量
			amt = price * volume;	//金额

			//新增卖委托金额
			sellOrderAmt += amt;
			//新增卖委托量
			pOrderEvl->_sellOrderVol += volume;

			//上个tick中间价
			double lastMidPrice = GetMidPrice(*sp1);
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
			price = static_cast<double>(sp1->_pxAsk[idx1]) / PRICE_MULTIPLE;	//价格
			volume = sp1->_qtyAsk[idx1];	//数量
			amt = price * volume;	//金额
			//是被买完了
			if (flag == 1)
			{
				//更新买金额
				accAmountBuy += amt;
			}
			//是自己撤单，撤完了
			else if (flag == 0)
			{
				//上个tick中间价
				double lastMidPrice = GetMidPrice(*sp1);
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
	double lastMidPrice;
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
			price = static_cast<double>(sp1->_pxBid[idx1]) / PRICE_MULTIPLE;	//价格
			volume = sp1->_qtyBid[idx1];	//数量
			amt = price * volume;	//金额

			if (flag == 1)
			{
				//主卖成交了,更新卖的金额
				accAmountSell += amt;
			}
			else if (flag == 0)
			{
				//买委托撤单了
				//上个tick的中间价
				lastMidPrice = GetMidPrice(*sp1);

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
			price = static_cast<double>(sp2->_pxBid[idx1]) / PRICE_MULTIPLE;	//价格
			volume = sp2->_qtyBid[idx2];	//数量
			amt = price * volume;	//金额

			//新增买委托金额
			buyOrderAmt += amt;
			//新增买委托量
			pOrderEvl->_buyOrderVol += volume;

			//上个tick的中间价
			lastMidPrice = GetMidPrice(*sp1);

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
				price = static_cast<double>(sp1->_pxBid[idx1]) / PRICE_MULTIPLE;	//价格
				volume = sp1->_qtyBid[idx1] - sp2->_qtyBid[idx2];	//减少的数量
				amt = price * volume;	//金额

				//是因为有主动卖所以减少
				if (flag == 1)
				{
					//更新卖的金额
					accAmountSell += amt;
				}
				else if (flag == 0)//因为自己撤了买委托，所以减少
				{
					//上个tick的中间价
					lastMidPrice = GetMidPrice(*sp1);

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
				price = static_cast<double>(sp1->_pxBid[idx1]) / PRICE_MULTIPLE;	//价格
				volume = sp2->_qtyBid[idx2] - sp1->_qtyBid[idx1];	//增加的数量
				amt = price * volume;	//金额

				//新增买委托金额
				buyOrderAmt += amt;
				//新增买委托量
				pOrderEvl->_buyOrderVol += volume;

				//上个tick的中间价
				lastMidPrice = GetMidPrice(*sp1);

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
			price = static_cast<double>(sp2->_pxBid[idx2]) / PRICE_MULTIPLE;	//价格
			volume = sp2->_qtyBid[idx2];	//增加的数量
			amt = price * volume;	//金额

			//新增买委托金额
			buyOrderAmt += amt;
			//新增买委托量
			pOrderEvl->_buyOrderVol += volume;

			//上个tick的中间价
			lastMidPrice = GetMidPrice(*sp1);

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
			price = static_cast<double>(sp1->_pxBid[idx1]) / PRICE_MULTIPLE;	//价格
			volume = sp1->_qtyBid[idx1];	//减少的数量
			amt = price * volume;	//金额

			//是因为有主动卖所以减少
			if (flag == 1)
			{
				//更新卖的金额
				accAmountSell += amt;
			}
			else if (flag == 0)//因为自己撤了买委托，所以减少
			{
				//上个tick的中间价
				lastMidPrice = GetMidPrice(*sp1);

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

	//利用统计完的金额，算一下强度
	//价格跨度
	double askPrcGap = GetAskPrcGap(*sp1);
	double bidPrcGap = GetBidPrcGap(*sp1);
	//强度
	if (askPrcGap != 0)
	{
		statisData._buyAmtStrength += accAmountBuy / askPrcGap;
		statisData._sellOrderAmtStrength += sellOrderAmt / askPrcGap;
	}
	if (bidPrcGap != 0)
	{
		statisData._sellAmtStrength += accAmountSell / bidPrcGap;
		statisData._buyOrderAmtStrength += buyOrderAmt / bidPrcGap;
	}
	
	//记录到统计数据
	statisData._accAmountBuy += accAmountBuy;
	statisData._accAmountSell += accAmountSell;

	statisData._buyOrderAmt += buyOrderAmt;
	statisData._sellOrderAmt += sellOrderAmt;

	return;
}

double StatisticalData::GetMidPrice(const SnapshotInfo& snapshot)
{
	double midPrice;
	double firstBidPrice = static_cast<double>(snapshot._pxBid[0]) / PRICE_MULTIPLE;
	double firstOfferPrice = static_cast<double>(snapshot._pxAsk[0]) / PRICE_MULTIPLE;
	if (firstBidPrice == 0 && firstOfferPrice == 0)
	{
		midPrice = static_cast<double>(snapshot._pxLast) / PRICE_MULTIPLE;
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

//两个时间戳之间的时间间隔，结果以毫秒为单位，时间戳是hhmmssmmm形式
uint32_t StatisticalData::ComputeTimeInterval(uint32_t tim1, uint32_t tim2)
{
	//如果tim2不是tim1之后的时间戳
	if (tim1 >= tim2)
	{
		return 0;
	}

	//要考虑两个时间戳不是同一秒，不是同一分钟的，它们之间差了多少毫秒？
	uint32_t hour1 = tim1 / 10000000;
	uint32_t min1 = (tim1 / 100000) % 100;
	uint32_t sec1 = (tim1 / 1000) % 100;
	uint32_t msec1 = tim1 % 1000;

	uint32_t hour2 = tim2 / 10000000;
	uint32_t min2 = (tim2 / 100000) % 100;
	uint32_t sec2 = (tim2 / 1000) % 100;
	uint32_t msec2 = tim2 % 1000;

	//计算相差几分钟
	uint32_t minDiff;
	if (hour1 == hour2)//在同一个小时
	{
		minDiff = min2 - min1;
	}
	else
	{
		//跨小时了
		minDiff = 60 - min1 + min2;
	}

	//计算相差几秒
	uint32_t secDiff;
	if (minDiff == 0)//在同一分钟
	{
		secDiff = sec2 - sec1;
	}
	else
	{
		//不在同一分钟
		secDiff = 60 - sec1 + 60 * (minDiff - 1) + sec2;
	}

	//计算相差几毫秒
	uint32_t msecDiff;
	//如果在同一秒内
	if (secDiff == 0)
	{
		msecDiff = msec2 - msec1;
	}
	else
	{
		//不在同一秒内
		msecDiff = 1000 - msec1 + 1000 * (secDiff - 1) + msec2;
	}

	return msecDiff;
}

//计算委卖价格跨度
double StatisticalData::GetAskPrcGap(const SnapshotInfo& snap)
{
	//看看是不是全0
	if (snap._pxAsk[0] == 0)
	{
		return 0;
	}

	//找到非0的最大档位
	int idx;
	for (idx = 1; idx < 10; idx++)
	{
		if (snap._pxAsk[idx] == 0)
		{
			break;
		}
	}

	//idx-1是最大非0档位
	uint32_t gap = snap._pxAsk[idx - 1] - snap._pxAsk[0];
	return static_cast<double>(gap) / PRICE_MULTIPLE;
}

//计算委买价格跨度，结果保留三位小数
double StatisticalData::GetBidPrcGap(const SnapshotInfo& snap)
{
	//是不是全0
	if (snap._pxBid[0] == 0)
	{
		return 0;
	}

	//找到非0的最大档
	int idx;
	for (idx = 1; idx < 10; idx++)
	{
		if (snap._pxBid[idx] == 0)
		{
			break;
		}
	}

	//idx-1是最大非0档位
	uint32_t gap = snap._pxBid[0] - snap._pxBid[idx - 1];
	return static_cast<double>(gap) / PRICE_MULTIPLE;
}

//跨分钟时，把lastSnapshot原始数据的某些字段写文件
void StatisticalData::WriteOriginalSnapToFile(const std::string& dir, uint32_t marketTime, const std::map<uint32_t, SnapshotInfo>& snapshots)
{
	//合成文件名
	string fileTime = std::to_string(marketTime / 100000);
	string fileName = fileTime + "SnapshotOriginalData.csv";
	string fullFileName = dir + "/" + fileName;

	//写方式打开文件
	FILE* fp = fopen(fullFileName.c_str(), "w");
	if (fp == NULL)
	{
		g_log.WriteLog("StatisticalData::WriteOriginalSnapToFile, fopen file failed");
		return;
	}

	//写表头
	WriteHeaderOrignalSnapShot(fp);

	for (const auto& elem : snapshots)
	{
		//写入一行数据
		WriteOneSnapshotData(fp, elem.second);
		//换行
		fputc('\n', fp);
	}

	return;
}

//写表头
void StatisticalData::WriteHeaderOrignalSnapShot(FILE* fp)
{
	//写入表头的一行
	int ret = fprintf(fp, "%s%c", "code", _sep);                    // 证券代码
	if (ret == -1)
	{
		g_log.WriteLog("StatisticalData::WriteHeaderOrignalSnapShot, fprintf error");
		return;
	}
	fprintf(fp, "%s%c", "time", _sep);                    // 时间(HHMMSSsss)
	fprintf(fp, "%s%c", "IOPV", _sep);
	fprintf(fp, "%s%c", "high_price", _sep);
	fprintf(fp, "%s%c", "weighted_avg_offer_price", _sep);

	//申卖价格，10档
	for (int i = 10; i >= 1; i--)
	{
		fprintf(fp, "%s%02d%c", "offer_price", i, _sep);
	}

	fprintf(fp, "%s%c", "last_price", _sep);

	//申买价格，10档
	for (int i = 1; i <= 10; ++i)
	{
		fprintf(fp, "%s%02d%c", "bid_price", i, _sep);
	}

	fprintf(fp, "%s%c", "weighted_avg_bid_price", _sep);
	fprintf(fp, "%s%c", "low_price", _sep);
	fprintf(fp, "%s%c", "total_offer_volume", _sep);

	//申卖量，10档
	for (int i = 10; i >= 1; i--)
	{
		fprintf(fp, "%s%02d%c", "offer_volume", i, _sep);
	}

	fprintf(fp, "%s%c", "total_volume_trade", _sep);

	//申买量，10档
	for (int i = 1; i <= 10; ++i)
	{
		fprintf(fp, "%s%02d%c", "bid_volume", i, _sep);
	}

	fprintf(fp, "%s%c", "total_bid_volume", _sep);
	fprintf(fp, "%s%c", "num_trades", _sep);
	fprintf(fp, "%s%c", "total_value_trade", _sep);
	fprintf(fp, "%s%c", "trading_phase_code", _sep);
	//fprintf(fp, "%s%c", "date", _sep);

	//回调次数
	fprintf(fp, "%s%c", "callBack_cnt", _sep);
	//第几个数据
	fprintf(fp, "%s%c", "sequence", _sep);
	//本地系统时间戳
	fprintf(fp, "%s", "local_time");//最后一列去掉逗号

	fputc('\n', fp);

	return;
}

//写入一条snapshot数据
void StatisticalData::WriteOneSnapshotData(FILE* fp, const SnapshotInfo& snapInfo)
{
	int ret = fprintf(fp, "%u%c", snapInfo._sc, _sep);		//股票代码
	if (ret == -1)
	{
		g_log.WriteLog("StatisticalData::WriteOneSnapshotData， fprintf error");
		return;
	}
	fprintf(fp, "%u%c", snapInfo._tm, _sep);		//时间
	fprintf(fp, "%u%c", snapInfo._pxIopv, _sep);	//ETF净值估计
	fprintf(fp, "%u%c", snapInfo._pxHigh, _sep);	//最高价
	fprintf(fp, "%u%c", snapInfo._pxAvgAsk, _sep);	//加权平均委卖价格

	//申卖价
	for (int i = 9; i >= 0; i--)
	{
		fprintf(fp, "%u%c", snapInfo._pxAsk[i], _sep);
	}

	//最新成交价
	fprintf(fp, "%u%c", snapInfo._pxLast, _sep);

	//申买价
	for (int i = 0; i < 10; ++i)
	{
		fprintf(fp, "%u%c", snapInfo._pxBid[i], _sep);
	}

	//加权平均委买价格
	fprintf(fp, "%u%c", snapInfo._pxAvgBid, _sep);
	//最低价
	fprintf(fp, "%u%c", snapInfo._pxLow, _sep);
	//委托卖出总量
	fprintf(fp, "%lu%c", snapInfo._qtyTotAsk, _sep);

	//申卖量
	for (int i = 9; i >= 0; i--)
	{
		fprintf(fp, "%lu%c", snapInfo._qtyAsk[i], _sep);
	}

	//成交总量
	fprintf(fp, "%lu%c", snapInfo._qtyTot, _sep);

	//申买量
	for (int i = 0; i < 10; ++i)
	{
		fprintf(fp, "%lu%c", snapInfo._qtyBid[i], _sep);
	}

	//委托买入总量
	fprintf(fp, "%lu%c", snapInfo._qtyTotBid, _sep);
	//成交总笔数
	fprintf(fp, "%u%c", snapInfo._ctTot, _sep);
	//成交总额
	fprintf(fp, "%lf%c", snapInfo._cnyTot, _sep);
	//当前品种(交易)状态
	fprintf(fp, "%s%c", snapInfo._sta, _sep);
	//交易归属日期
	//fprintf(fp, "%u%c", snapInfo._dt, _sep);

	fprintf(fp, "%u%c", snapInfo._numCb, _sep);	//回调次数
	fprintf(fp, "%u%c", snapInfo._seq, _sep);	//本次回调中的第几个（从1开始）
	//最后一个字段，不要逗号
	fprintf(fp, "%d", snapInfo._timLoc);	//收到数据时本地系统时间
}

//跨分钟时，snapshot统计数据写文件，参数设为写文件的目录，要写的数据映射map
void StatisticalData::WriteSnapStatisDataToFile(const std::string& dir, uint32_t marketTime, const std::map<uint32_t, SnapStatisData>& statisData)
{
	//合成文件名
	string fileTime = std::to_string(marketTime / 100000);
	string fileName = fileTime + "SnapshotStatisticData.csv";
	string fullFileName = dir + "/" + fileName;

	//写方式打开文件
	FILE* fp = fopen(fullFileName.c_str(), "w");
	if (fp == NULL)
	{
		g_log.WriteLog("fopen SnapshotStatisticData file failed");
		return;
	}

	//写表头
	WriteHeaderSnapStatis(fp);

	for (auto& pair : statisData)
	{
		//写证券代码
		fprintf(fp, "%u,", pair.first);
		//写统计数据
		WriteSnapStatisData(fp, pair.second);
		//换行
		fprintf(fp, "\n");
	}
}

//写表头
void StatisticalData::WriteHeaderSnapStatis(FILE* fp)
{
	
	fprintf(fp, "code,");		//证券代码
	fprintf(fp, "askGapMax,");
	fprintf(fp, "askGapMin,");
	fprintf(fp, "askGapChange,");
	fprintf(fp, "askGapAvrg,");
	fprintf(fp, "askGapSD,");
	fprintf(fp, "buyAmtStrength,");
	fprintf(fp, "bidGapMax,");
	fprintf(fp, "bidGapMin,");
	fprintf(fp, "bidGapChange,");
	fprintf(fp, "bidGapAvrg,");
	fprintf(fp, "bidGapSD,");
	fprintf(fp, "sellAmtStrength,");
	fprintf(fp, "bidAskGapMax,");
	fprintf(fp, "bidAskGapMin,");
	fprintf(fp, "bidAskGapChange,");
	fprintf(fp, "bidAskGapAvrg,");
	fprintf(fp, "bidAskGapSD,");

	fprintf(fp, "accAmountBuy,");
	fprintf(fp, "accAmountSell,");
	fprintf(fp, "buyOrderAmt,");
	fprintf(fp, "buyOrderAmtStrength,");
	fprintf(fp, "buyOrderVol,");
	fprintf(fp, "activeBuyOrderAmt,");
	fprintf(fp, "activeBuyOrderVol,");
	fprintf(fp, "passiveBuyOrderAmt,");
	fprintf(fp, "passiveBuyOrderVol,");
	fprintf(fp, "sellOrderAmt,");
	fprintf(fp, "sellOrderAmtStrength,");
	fprintf(fp, "sellOrderVol,");
	fprintf(fp, "activeSellOrderAmt,");
	fprintf(fp, "activeSellOrderVol,");
	fprintf(fp, "passiveSellOrderAmt,");
	fprintf(fp, "passiveSellOrderVol,");
	fprintf(fp, "buyOrderCanceledAmt,");
	fprintf(fp, "buyOrderCanceledVol,");
	fprintf(fp, "sellOrderCanceledAmt,");
	fprintf(fp, "sellOrderCanceledVol,");
	fprintf(fp, "revenueStandardDeviation,");
	fprintf(fp, "revenueSkewness,");		//收益偏度
	fprintf(fp, "prcIncre,");
	fprintf(fp, "prcDecre,");
	fprintf(fp, "prcUnchange,");
	fprintf(fp, "snapCount,");
	fprintf(fp, "maxTimInterv,");
	fprintf(fp, "minTimInterv,");
	fprintf(fp, "timIntervAvrg,");
	fprintf(fp, "timIntervSD,");
	fprintf(fp, "netBidAmtAvrg,");
	fprintf(fp, "netBidAmtSD,");
	fprintf(fp, "snapRevenue,");
	fprintf(fp, "allDayRevenueByOpenPric,");
	fprintf(fp, "allDayRevenueByPreClosePric");//最后一个字段后面不加分隔符
	//换行
	fprintf(fp, "\n");

	return;
}

void StatisticalData::WriteSnapStatisData(FILE* fp, const SnapStatisData& data)
{
	fprintf(fp, "%u,", data._askGapMax);
	fprintf(fp, "%u,", data._askGapMin);
	fprintf(fp, "%d,", data._askGapChange);
	fprintf(fp, "%u,", data._askGapAvrg);
	fprintf(fp, "%.3lf,", data._askGapSD);
	fprintf(fp, "%.3f,", data._buyAmtStrength);
	fprintf(fp, "%u,", data._bidGapMax);
	fprintf(fp, "%u,", data._bidGapMin);
	fprintf(fp, "%d,", data._bidGapChange);
	fprintf(fp, "%u,", data._bidGapAvrg);
	fprintf(fp, "%.3lf,", data._bidGapSD);
	fprintf(fp, "%.3f,", data._sellAmtStrength);
	fprintf(fp, "%u,", data._bidAskGapMax);
	fprintf(fp, "%u,", data._bidAskGapMin);
	fprintf(fp, "%d,", data._bidAskGapChange);
	fprintf(fp, "%u,", data._bidAskGapAvrg);
	fprintf(fp, "%.3lf,", data._bidAskGapSD);

	fprintf(fp, "%lu,", data._accAmountBuy);
	fprintf(fp, "%lu,", data._accAmountSell);
	fprintf(fp, "%lu,", data._buyOrderAmt);
	fprintf(fp, "%.3f,", data._buyOrderAmtStrength);
	fprintf(fp, "%lu,", data._buyOrderVol);
	fprintf(fp, "%lu,", data._activeBuyOrderAmt);
	fprintf(fp, "%lu,", data._activeBuyOrderVol);
	fprintf(fp, "%lu,", data._passiveBuyOrderAmt);
	fprintf(fp, "%lu,", data._passiveBuyOrderVol);
	fprintf(fp, "%lu,", data._sellOrderAmt);
	fprintf(fp, "%.3f,", data._sellOrderAmtStrength);
	fprintf(fp, "%lu,", data._sellOrderVol);
	fprintf(fp, "%lu,", data._activeSellOrderAmt);
	fprintf(fp, "%lu,", data._activeSellOrderVol);
	fprintf(fp, "%lu,", data._passiveSellOrderAmt);
	fprintf(fp, "%lu,", data._passiveSellOrderVol);
	fprintf(fp, "%lu,", data._buyOrderCanceledAmt);
	fprintf(fp, "%lu,", data._buyOrderCanceledVol);
	fprintf(fp, "%lu,", data._sellOrderCanceledAmt);
	fprintf(fp, "%lu,", data._sellOrderCanceledVol);
	fprintf(fp, "%lf,", data._revenueStandardDeviation);
	fprintf(fp, "%lf,", data._revenueSkewness);		//收益偏度
	fprintf(fp, "%u,", data._prcIncre);
	fprintf(fp, "%u,", data._prcDecre);
	fprintf(fp, "%u,", data._prcUnchange);
	fprintf(fp, "%u,", data._snapCount);
	fprintf(fp, "%u,", data._maxTimInterv);
	fprintf(fp, "%u,", data._minTimInterv);
	fprintf(fp, "%u,", data._timIntervAvrg);
	fprintf(fp, "%.3lf,", data._timIntervSD);
	fprintf(fp, "%ld,", data._netBidAmtAvrg);
	fprintf(fp, "%.3lf,", data._netBidAmtSD);
	fprintf(fp, "%f,", data._snapRevenue);
	fprintf(fp, "%f,", data._allDayRevenueByOpenPric);
	fprintf(fp, "%f", data._allDayRevenueByPreClosePric);		//最后一个字段
	
	return;
}

void StatisticalData::ClearSnapData(std::map<uint32_t, SnapTempData>& snapTempDatas, std::map<uint32_t, SnapStatisData>& snapStatisDatas)
{
	//每次切片存储的临时数据，将数组清空。但是容量空间还在，之后不用重新开辟空间
	for (auto& pair : snapTempDatas)
	{
		pair.second._askPrcGap.clear();
		pair.second._bidPrcGap.clear();
		pair.second._askAndBidPrcGap.clear();
		pair.second._revenues.clear();
		pair.second._timInterv.clear();
		pair.second._netBidAmt.clear();
	}

	//本分钟统计的数据，清空，这里能不能也不删除，只是把各个代码映射的结构体重新初始化。
	snapStatisDatas.clear();
	/*SnapStatisData temp;
	for (auto& pair : snapStatisDatas)
	{
		pair.second = temp;
	}*/

	return;
}

//接收成交数据的回调函数，重写
void StatisticalData::OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt)
{
	//是不是成交数据，不是的话就不处理
	if (!(info[0]._MLFlag == 'A' || info[0]._MLFlag == 'C'))
	{
		return;
	}

	//判断是上交所还是深交所
	uint32_t topDigit = info[0]._sc / 100000000;
	int32_t cmpRet;

	if (topDigit == 1)//是上交所的数据
	{
		//比较时间戳，是否跨秒钟了
		cmpRet = CompareTimeBySec(_tmExecuSse, info[0]._tm);
		
		if (cmpRet < 0)//是上个时间戳之前的数据
		{
			return;
		}
		//跨到下一秒钟了
		if (cmpRet > 0)
		{
			//printf("nex second , %u to %u\n", _tmExecuSse, info[0]._tm);
			
			//计算一秒内的数据
			CalcuExeDataInOneSec(_execuTmpDataPerSecSse, _execuTmpDataMinSse);

			//清理一些数据，为了下一秒钟重新统计
			_execuTmpDataPerSecSse.clear();

			//比较时间戳，是否跨分钟了
			cmpRet = CompareTime(_tmExecuSse, info[0]._tm);
			//如果跨分钟了
			if (cmpRet > 0)
			{
				//printf("\nnex minute , %u to %u\n", _tmExecuSse, info[0]._tm);

				/*if (_execuStatisDataMinSse.empty())
				{
					printf("_execuStatisDataMinSse.size() == 0\n");
				}
				else
				{
					for (const auto& elem : _execuStatisDataMinSse)
					{
						printf("code = %u, buyTradeNum = %u, buyAmt = %.3lf, buyVol = %lu\n",
							elem.first, elem.second._buyTradeNum, elem.second._buyTradeAmt, elem.second._buyTradeVol);
					}
				}*/
				
				//统计一分钟内数据
				CalcuExeDataAtMinuteEnd(_execuTmpDataMinSse, _execuStatisDataMinSse);
				//全天统计数据，跨分钟时，做一些计算。这里面会用到分钟统计的数据（成交金额），所以必须先统计分钟数据
				CalDayDataAtMinuteEnd(topDigit);

				//分钟数据，写入文件
				StoreExecuDataMinute(_execuStatisDataMinSse, _tmExecuSse, _sseFilePath);
				//全天统计数据，写入文件
				StoreExecuDataAllDay(_execuStatisDataAllDaySse, _tmExecuSse, _sseFilePath);

				//清理一些临时数据，用于下一分钟重新计算
				ClearExecuTempData(topDigit);
			}
		}
		
		//更新时间戳
		_tmExecuSse = info[0]._tm;

		//遍历数组，更新相关数据
		for (uint32_t idx = 0; idx < cnt; ++idx)
		{
			/*if (info[idx]._sc == 100511020)
			{
				printf("\nupdate code = 100511020， tim = %u\n", info[idx]._tm);
				UpdateExecuStatisData(info[idx], topDigit);
			}*/

			UpdateExecuStatisData(info[idx], topDigit);
		}
	}
	else if (topDigit == 2)//深交所的数据
	{
		//return;
		//比较时间戳，是否跨秒钟了
		cmpRet = CompareTimeBySec(_tmExecuSzse, info[0]._tm);

		if (cmpRet < 0)//是上个时间戳之前的数据
		{
			return;
		}
		//跨到下一秒钟了
		if (cmpRet > 0)
		{
			//计算一秒内的数据
			CalcuExeDataInOneSec(_execuTmpDataPerSecSzse, _execuTmpDataMinSzse);

			//清理一些数据，为了下一秒钟重新统计
			_execuTmpDataPerSecSzse.clear();

			//比较时间戳，是否跨分钟了
			cmpRet = CompareTime(_tmExecuSzse, info[0]._tm);
			//如果跨分钟了
			if (cmpRet > 0)
			{
				//统计一分钟内数据
				CalcuExeDataAtMinuteEnd(_execuTmpDataMinSzse, _execuStatisDataMinSzse);
				//全天统计数据，跨分钟时，做一些计算。这里面会用到分钟统计的数据（成交金额），所以必须先统计分钟数据
				CalDayDataAtMinuteEnd(topDigit);

				//分钟数据，写入文件
				StoreExecuDataMinute(_execuStatisDataMinSzse, _tmExecuSzse, _szseFilePath);
				//全天统计数据，写入文件
				StoreExecuDataAllDay(_execuStatisDataAllDaySzse, _tmExecuSzse, _szseFilePath);

				//清理一些临时数据，用于下一分钟重新计算
				ClearExecuTempData(topDigit);
			}
		}

		//更新时间戳
		_tmExecuSzse = info[0]._tm;

		//遍历数组，更新相关数据
		for (uint32_t idx = 0; idx < cnt; ++idx)
		{
			UpdateExecuStatisData(info[idx], topDigit);
		}
	}

}

//利用收到的一个成交数据，更新统计数据
void StatisticalData::UpdateExecuStatisData(const OrdAndExeInfo& info, uint32_t market)
{
	/*if (info._sc == 100603338)
	{
		printf("sc == 100603338, tim = %u\n", info._tm);
	}*/

	//统计数据（分钟）
	std::map<uint32_t, ExecuStatisDataMinute>* statisDatasMin;
	//每分钟的暂存数据
	std::map<uint32_t, ExecuTmpDataPerMinute>* tmpDatasMin;
	//每秒钟的暂存数据
	std::map<uint32_t, ExecuTmpDataPerSecond>* tmpDatasPerSec;
	//统计数据，全天
	std::map<uint32_t, ExecuStatisDataAllDay>* statisDatasDay;
	std::map<uint32_t, RequiredDataForDayStatis>* reqDatasDay;
	
	//判断数据属于哪个交易所
	if (market == 1)
	{
		statisDatasMin = &_execuStatisDataMinSse;
		tmpDatasMin = &_execuTmpDataMinSse;
		tmpDatasPerSec = &_execuTmpDataPerSecSse;
		statisDatasDay = &_execuStatisDataAllDaySse;
		reqDatasDay = &_reqDatasForDaySse;
	}
	else if (market == 2)
	{
		statisDatasMin = &_execuStatisDataMinSzse;
		tmpDatasMin = &_execuTmpDataMinSzse;
		tmpDatasPerSec = &_execuTmpDataPerSecSzse;
		statisDatasDay = &_execuStatisDataAllDaySzse;
		reqDatasDay = &_reqDatasForDaySzse;
	}
	else
	{
		g_log.WriteLog("market is not 1 or 2, StatisticalData::UpdateExecuStatisData");
		return;
	}

	uint32_t code = info._sc;
	ExecuStatisDataMinute& statisDataMin = (*statisDatasMin)[code];
	//printf("_execuStatisDataMinSse.size() = %d\n", _execuStatisDataMinSse.size());

	ExecuTmpDataPerMinute& tmpDataPerMin = (*tmpDatasMin)[code];
	ExecuTmpDataPerSecond& tmpDataPerSec = (*tmpDatasPerSec)[code];
	ExecuStatisDataAllDay& staDataDay = (*statisDatasDay)[code];
	RequiredDataForDayStatis& reqDataForDay = (*reqDatasDay)[code];
	
	double pric = (double)info._px / PRICE_MULTIPLE;	//价格还原为浮点数
	double amount = pric * info._qty;	//交易金额

	//printf("pric = %.3lf, amount = %.3lf\n", pric, amount);
	
	//统计一秒钟内的一些数据
	if (info._BSFlag == 'B')
	{
		tmpDataPerSec._buyVol += info._qty;
		tmpDataPerSec._buyAmt += amount;
	}
	else if (info._BSFlag == 'S')
	{
		tmpDataPerSec._sellVol += info._qty;
		tmpDataPerSec._sellAmt += amount;
	}
	else
	{
		//g_log.WriteLog("StatisticalData::UpdateExecuStatisData, execu data not buy or sell");
		return;
	}

	//统计分钟内的一些数据
	//如果是本分钟第一次收到该证券的成交信息
	if (statisDataMin._startPrice == 0)
	{
		//起始价
		statisDataMin._startPrice = pric;
		//给最高价最低价设初值
		statisDataMin._highPrice = pric;
		statisDataMin._lowPrice = pric;
		//结束价，不断更新
		statisDataMin._endPrice = pric;
	}
	else
	{
		//如果不是第一次
		//更新最高最低价，结束价
		if (pric > statisDataMin._highPrice)
		{
			statisDataMin._highPrice = pric;
		}
		if (pric < statisDataMin._lowPrice)
		{
			statisDataMin._lowPrice = pric;
		}
		statisDataMin._endPrice = pric;
	}

	//高开低收
	/*printf("statsPri = %.3lf, highPrc = %.3lf, lowPrc = %.3lf, endPrc = %.3lf\n", 
		statisDataMin._startPrice, statisDataMin._highPrice, statisDataMin._lowPrice, statisDataMin._endPrice);*/

	if (info._BSFlag == 'B')
	{
		//更新买/卖单数，成交金额，交易量
		statisDataMin._buyTradeNum++;
		statisDataMin._buyTradeVol += info._qty;
		statisDataMin._buyTradeAmt += amount;

		//存储一些临时数据，放入数组
		DataPerExecu exeData;
		exeData._prc = pric;
		exeData._vol = info._qty;
		exeData._amt = amount;
		tmpDataPerMin._buyExecuData.emplace_back(exeData);

		//printf("buyTradeNum = %u, buyAmt = %.3lf, buyVol = %lu\n",
			//_execuStatisDataMinSse[code]._buyTradeNum, _execuStatisDataMinSse[code]._buyTradeAmt, _execuStatisDataMinSse[code]._buyTradeVol);
	}
	else if (info._BSFlag == 'S')
	{
		//更新买/卖单数，成交金额，交易量
		statisDataMin._sellTradeNum++;
		statisDataMin._sellTradeVol += info._qty;
		statisDataMin._sellTradeAmt += amount;

		//存储一些临时数据，放入数组
		DataPerExecu exeData;
		exeData._prc = pric;
		exeData._vol = info._qty;
		exeData._amt = amount;
		tmpDataPerMin._sellExecuData.emplace_back(exeData);

		//printf("sellTradeNum = %u, sellAmt = %.3lf, sellVol = %lu\n",
			//_execuStatisDataMinSse[code]._sellTradeNum, _execuStatisDataMinSse[code]._sellTradeAmt, _execuStatisDataMinSse[code]._sellTradeVol);
	}
	else
	{
		g_log.WriteLog("StatisticalData::UpdateExecuStatisData, execu data not buy or sell");
		return;
	}

	//全天统计数据
	if (info._BSFlag == 'B')
	{
		staDataDay._buyTradeCount++;
		staDataDay._buyVol += info._qty;
		staDataDay._buyAmt += amount;

		//记录一些统计需要的数据
		reqDataForDay._buyExeDatas.emplace_back(DataPerExecu());
		DataPerExecu& exeData = reqDataForDay._buyExeDatas.back();
		exeData._prc = pric;
		exeData._vol = info._qty;
		exeData._amt = amount;
	}
	else if (info._BSFlag == 'S')
	{
		staDataDay._sellTradeCount++;
		staDataDay._sellVol += info._qty;
		staDataDay._sellAmt += amount;

		//记录一些统计需要的数据
		reqDataForDay._sellExeDatas.emplace_back(DataPerExecu());
		DataPerExecu& exeData = reqDataForDay._sellExeDatas.back();
		exeData._prc = pric;
		exeData._vol = info._qty;
		exeData._amt = amount;
	}

	return;
}

//比较两个时间戳，有没有跨到下一秒钟，跨秒钟返回1，同一秒钟返回0，倒回上一秒的返回-1
int StatisticalData::CompareTimeBySec(uint32_t tim1, uint32_t tim2)
{
	//如果tim1是0，不算跨秒钟
	if (tim1 == 0)
	{
		return 0;
	}

	uint32_t hms1 = tim1 / 1000; //得到hhmmss形式
	uint32_t hms2 = tim2 / 1000;

	if (hms1 == hms2)
	{
		//时分秒都相同
		return 0;
	}
	else if (hms1 < hms2)
	{
		//跨秒钟了，而且tim2在后面
		return 1;
	}
	else
	{
		//跨秒钟了，而且tim2在前面
		return -1;
	}

}

//比较两个时间戳，有没有跨到下一秒钟，跨秒钟返回1，同一秒钟返回0，倒回上一秒的返回-1
//int StatisticalData::CompareTimeBySec(uint32_t tim1, uint32_t tim2)
//{
//	//如果tim1是0，就不算跨分钟
//	if (tim1 == 0)
//	{
//		return 0;
//	}
//
//	uint32_t hour1 = tim1 / 10000000;
//	uint32_t min1 = (tim1 / 100000) % 100;
//	uint32_t sec1 = (tim1 / 1000) % 100;
//	uint32_t hour2 = tim2 / 10000000;
//	uint32_t min2 = (tim2 / 100000) % 100;
//	uint32_t sec2 = (tim2 / 1000) % 100;
//
//	if (hour1 == hour2)
//	{
//		//在同一个小时
//		if (min2 > min1)
//		{
//			return 1;
//		}
//		else if (min2 == min1)
//		{
//			//在同一分钟
//			if (sec2 > sec1)
//			{
//				//跨秒钟了
//				return 1;
//			}
//			else if (sec2 == sec1)
//			{
//				//在同一秒钟
//				return 0;
//			}
//			else
//			{
//				return -1;
//			}
//		}
//		else
//		{
//			//返回-1，代表也跨分钟了，但是tim2在tim1前面
//			return -1;
//		}
//	}
//	else if (hour1 < hour2)
//	{
//		return 1;
//	}
//	else
//	{
//		return -1;
//	}
//
//}

//统计一秒钟内的数据，这一秒的平均价格，主买金额占市场的比例，主卖金额占市场的比例，统计结果放入分钟临时数据数组里
void StatisticalData::CalcuExeDataInOneSec(const std::map<uint32_t, ExecuTmpDataPerSecond>& tmpDatasSec, std::map<uint32_t, ExecuTmpDataPerMinute>& tmpDatasMin)
{
	//全市场的主买成交金额
	double buyAmtMarket = 0;
	//全市场的主卖成交金额
	double sellAmtMarket = 0;

	for (auto& elem : tmpDatasSec)
	{
		buyAmtMarket += elem.second._buyAmt;
		sellAmtMarket += elem.second._sellAmt;
	}

	for (auto& mp : tmpDatasSec)
	{
		uint32_t code = mp.first;
		const ExecuTmpDataPerSecond& tmpDataSec = mp.second;
		ExecuTmpDataPerMinute& tmpDataMin = tmpDatasMin[code];
		//在分钟临时数据里_dataPerSec数组放入新元素
		tmpDataMin._dataPerSec.emplace_back(ExeSecRecordData());
		ExeSecRecordData& secRecord = tmpDataMin._dataPerSec.back();

		//成交金额
		secRecord._buyAmt = tmpDataSec._buyAmt;
		secRecord._sellAmt = tmpDataSec._sellAmt;

		//这一秒的成交价格均值
		secRecord._pricAvg = (tmpDataSec._buyAmt + tmpDataSec._sellAmt) / (tmpDataSec._buyVol + tmpDataSec._sellVol);

		//这一秒内主买成交金额占全市场的比例
		double ratio;
		if (buyAmtMarket != 0)
		{
			ratio = tmpDataSec._buyAmt / buyAmtMarket;
			secRecord._buyAmtRatio = ratio;
		}
		
		//这一秒内主卖成交金额占全市场的比例
		if (sellAmtMarket != 0)
		{
			ratio = tmpDataSec._sellAmt / sellAmtMarket;
			secRecord._sellAmtRatio = ratio;
		}
		
	}

	return;
}

//分钟结束时，统计本分钟的一些数据
void StatisticalData::CalcuExeDataAtMinuteEnd(const std::map<uint32_t, ExecuTmpDataPerMinute>& tmpDatasMin, std::map<uint32_t, ExecuStatisDataMinute>& statisDataMin)
{
	for (auto& pair : tmpDatasMin)
	{
		uint32_t code = pair.first;
		const ExecuTmpDataPerMinute& tmpDataMin = pair.second;
		ExecuStatisDataMinute& statisData = statisDataMin[code];

		//主买成交价格均值
		double buyPrcAvrg;
		if (statisData._buyTradeVol != 0)
		{
			buyPrcAvrg = statisData._buyTradeAmt / statisData._buyTradeVol;
		}
		else
		{
			buyPrcAvrg = 0;
		}
		statisData._buyPrcAvrg = buyPrcAvrg;
		//主买成交价格标准差
		statisData._buyPrcSD = CalcuPricStandardDeviation(tmpDataMin._buyExecuData, buyPrcAvrg);

		//主卖成交价格均值
		double sellPrcAvrg;
		if (statisData._sellTradeVol != 0)
		{
			sellPrcAvrg = statisData._sellTradeAmt / statisData._sellTradeVol;
		}
		else
		{
			sellPrcAvrg = 0;
		}
		statisData._sellPrcAvrg = sellPrcAvrg;
		//主卖成交价格标准差
		statisData._sellPrcSD = CalcuPricStandardDeviation(tmpDataMin._sellExecuData, sellPrcAvrg);

		//主买单均成交金额
		double avrgAmtByExecu = statisData._buyTradeAmt / statisData._buyTradeNum;
		//主买成交金额的数组,先log再放进去
		vector<double> amtVec;
		for (auto& buyData : tmpDataMin._buyExecuData)
		{
			if (buyData._amt != 0)
			{
				amtVec.push_back(std::log(buyData._amt));
			}
			else
			{
				amtVec.push_back(0); //std::log(1)
			}
			
		}
		//主买成交金额标准差
		statisData._buyAmtSD = CalculateStandardDeviation(amtVec, std::log(avrgAmtByExecu));

		//主卖单均成交金额
		avrgAmtByExecu = statisData._sellTradeAmt / statisData._sellTradeNum;
		//主卖成交金额的数组,先log再放进去
		amtVec.clear();
		for (auto& sellData : tmpDataMin._sellExecuData)
		{
			if (sellData._amt != 0)
			{
				amtVec.push_back(std::log(sellData._amt));
			}
			else
			{
				amtVec.push_back(0); //std::log(1)
			}
			
		}
		//主卖成交金额标准差
		statisData._sellAmtSD = CalculateStandardDeviation(amtVec, std::log(avrgAmtByExecu));

		//根据每秒钟记录数据的价格，算出每秒钟收益的数组
		std::vector<double> revenues = GetRevenuesByPrices(tmpDataMin._dataPerSec);

		if (revenues.size() > 0)
		{
			//收益的平均值
			double revnAvg = std::accumulate(revenues.begin(), revenues.end(), 0.0) / revenues.size();
			//收益的标准差
			statisData._revenueSD = CalculateStandardDeviation(revenues, revnAvg);
			//收益的偏度
			statisData._revenueSkew = CalculateSkewness(revenues, revnAvg, statisData._revenueSD);

		}
		
		//价格上行，下行次数
		GetPricVolatility(tmpDataMin._dataPerSec, statisData);

		//小于等于平均价格的主买成交单数，成交金额
		//主买
		CalcuInRangeOfPrice(tmpDataMin._buyExecuData, statisData._buyLessAvgPrc, 0, buyPrcAvrg);
		//主卖
		CalcuInRangeOfPrice(tmpDataMin._sellExecuData, statisData._sellLessAvgPrc, 0, sellPrcAvrg);

		//平均价格和1std之间的
		//主买
		CalcuInRangeOfPrice(tmpDataMin._buyExecuData, statisData._buyPrcAvgToStd, buyPrcAvrg, buyPrcAvrg + statisData._buyPrcSD);
		//主卖
		CalcuInRangeOfPrice(tmpDataMin._sellExecuData, statisData._sellPrcAvgToStd, sellPrcAvrg, sellPrcAvrg + statisData._sellPrcSD);

		//成交价格属于(avg+1std, avg+2std]
		//主买
		CalcuInRangeOfPrice(tmpDataMin._buyExecuData, statisData._buyPrc1stdTo2std, buyPrcAvrg + statisData._buyPrcSD, buyPrcAvrg + statisData._buyPrcSD * 2);
		//主卖
		CalcuInRangeOfPrice(tmpDataMin._sellExecuData, statisData._sellPrc1stdTo2std, sellPrcAvrg + statisData._sellPrcSD, sellPrcAvrg + statisData._sellPrcSD * 2);

		//成交价格属于(avg+2std, avg+3std]
		//主买
		CalcuInRangeOfPrice(tmpDataMin._buyExecuData, statisData._buyPrc2stdTo3std, buyPrcAvrg + statisData._buyPrcSD * 2, buyPrcAvrg + statisData._buyPrcSD * 3);
		//主卖
		CalcuInRangeOfPrice(tmpDataMin._sellExecuData, statisData._sellPrc2stdTo3std, sellPrcAvrg + statisData._sellPrcSD * 2, sellPrcAvrg + statisData._sellPrcSD * 3);

		//成交价格大于avg + 3std
		//主买
		CalcuInRangeOfPrice(tmpDataMin._buyExecuData, statisData._buyPrcOver3std, buyPrcAvrg + statisData._buyPrcSD * 3, 999999.999);
		//主卖
		CalcuInRangeOfPrice(tmpDataMin._sellExecuData, statisData._sellPrcOver3std, sellPrcAvrg + statisData._sellPrcSD * 3, 999999.999);

		//主买成交金额占市场比例，开始值，结束值，最大最小值
		//其实可以不考虑数组长度为0的情况，因为进入跨分钟统计时，最少有两次成交，也跨秒钟了，所以至少有一秒钟数据
		//不对，也可能就是没有成交，时间戳又不是只有一个证券在用
		//进入此循环，说明有tmpDataMin映射，至少有一次成交，有成交就有tmpDataSec数据，跨分钟前也做了跨秒钟统计，就有secRecord
		if (tmpDataMin._dataPerSec.size() > 0)
		{
			statisData._buyAmtRatFirst = tmpDataMin._dataPerSec[0]._buyAmtRatio;
			statisData._buyAmtRatLast = tmpDataMin._dataPerSec.back()._buyAmtRatio;
			statisData._sellAmtRatFirst = tmpDataMin._dataPerSec[0]._sellAmtRatio;
			statisData._sellAmtRatLast = tmpDataMin._dataPerSec.back()._sellAmtRatio;
			
			vector<double> buyRatVec;	//主买金额占比的数组
			vector<double> sellRatVec;	//主卖金额占比的数组
			for (auto& elem : tmpDataMin._dataPerSec)
			{
				buyRatVec.push_back(elem._buyAmtRatio);
				sellRatVec.push_back(elem._sellAmtRatio);
			}

			//最大值最小值
			auto ratIter = std::max_element(buyRatVec.begin(), buyRatVec.end());
			statisData._buyAmtRatMax = *ratIter;
			ratIter = std::min_element(buyRatVec.begin(), buyRatVec.end());
			statisData._buyAmtRatMin = *ratIter;

			ratIter = std::max_element(sellRatVec.begin(), sellRatVec.end());
			statisData._sellAmtRatMax = *ratIter;
			ratIter = std::min_element(sellRatVec.begin(), sellRatVec.end());
			statisData._sellAmtRatMin = *ratIter;

			//主买/卖成交金额占市场比例，均值，带着权重计算
			CalRatAvgWithWeights(tmpDataMin._dataPerSec, statisData._buyAmtRatAvg, statisData._sellAmtRatAvg);
			//标准差
			CalRatStdDeviWithWeights(tmpDataMin._dataPerSec, statisData._buyAmtRatAvg, statisData._buyAmtRatSD, 
				statisData._sellAmtRatAvg, statisData._sellAmtRatSD);
			
			//成交金额占比，在指定范围内，统计主买主卖成交金额
			//比例 小于均值
			CalTradeAmtInRatioRange(tmpDataMin._dataPerSec, statisData._buyAmtLessAvgRat, 0, statisData._buyAmtRatAvg,
				statisData._sellAmtLessAvgRat, 0, statisData._sellAmtRatAvg);
			//均值~1std
			CalTradeAmtInRatioRange(tmpDataMin._dataPerSec, 
				statisData._buyAmtAvgRatTo1std, statisData._buyAmtRatAvg, statisData._buyAmtRatAvg + statisData._buyAmtRatSD,
				statisData._sellAmtAvgRatTo1std, statisData._sellAmtRatAvg, statisData._sellAmtRatAvg + statisData._sellAmtRatSD);
			//1std~2std
			CalTradeAmtInRatioRange(tmpDataMin._dataPerSec,
				statisData._buyAmtRat1stdTo2std, statisData._buyAmtRatAvg + statisData._buyAmtRatSD, statisData._buyAmtRatAvg + statisData._buyAmtRatSD * 2,
				statisData._sellAmtRat1stdTo2std, statisData._sellAmtRatAvg + statisData._sellAmtRatSD, statisData._sellAmtRatAvg + statisData._sellAmtRatSD * 2);
			//2std~3std
			CalTradeAmtInRatioRange(tmpDataMin._dataPerSec,
				statisData._buyAmtRat2stdTo3std, statisData._buyAmtRatAvg + statisData._buyAmtRatSD * 2, statisData._buyAmtRatAvg + statisData._buyAmtRatSD * 3,
				statisData._sellAmtRat2stdTo3std, statisData._sellAmtRatAvg + statisData._sellAmtRatSD * 2, statisData._sellAmtRatAvg + statisData._sellAmtRatSD * 3);
			//3std以上。上限设为10，因为比例不可能超过1，比例的标准差也更小
			CalTradeAmtInRatioRange(tmpDataMin._dataPerSec,
				statisData._buyAmtRatOver3std, statisData._buyAmtRatAvg + statisData._buyAmtRatSD * 3, 10,
				statisData._sellAmtRatOver3std, statisData._sellAmtRatAvg + statisData._sellAmtRatSD * 3, 10);
		}

	}

	return;
}

//根据分钟临时数据的成交信息数组，和价格平均值，计算价格标准差，考虑权重，也就是成交数量
double StatisticalData::CalcuPricStandardDeviation(const std::vector<DataPerExecu>& execuData, double avrg)
{
	if (execuData.empty())
	{
		return 0;
	}

	double sum = 0;
	double divisor = 0;
	double stdDevi;
	for (auto& data : execuData)
	{
		sum += std::pow(data._prc - avrg, 2) * data._vol; //价格与价格均值的差，平方，乘以成交量
		divisor += data._vol;
	}

	if (divisor != 0)
	{
		stdDevi = std::sqrt(sum / divisor);
	}
	else
	{
		stdDevi = 0;
	}

	return stdDevi;
}

//根据每秒的记录数据里的价格，获得每秒的收益
std::vector<double> StatisticalData::GetRevenuesByPrices(const std::vector<ExeSecRecordData>& secRecords)
{
	vector<double> revenues;
	//如果记录的价格只有1个或0个，就没有收益
	if (secRecords.size() == 0 || secRecords.size() == 1)
	{
		return revenues;
	}

	//从第二个价格开始遍历
	size_t idx;
	double reve;
	for (idx = 1; idx < secRecords.size(); ++idx)
	{
		reve = (secRecords[idx]._pricAvg / secRecords[idx - 1]._pricAvg) - 1;
		revenues.push_back(reve);
	}

	return revenues;
}

//根据一分钟内每秒数据，获得价格波动的数据
void StatisticalData::GetPricVolatility(const std::vector<ExeSecRecordData>& secRecords, ExecuStatisDataMinute& statisData)
{
	//如果只有一个元素或没有元素，直接返回。
	if (secRecords.size() <= 1)
	{
		return;
	}

	for (size_t idx = 1; idx < secRecords.size(); ++idx)
	{
		//跟上个秒钟价格对比
		if (secRecords[idx]._pricAvg > secRecords[idx - 1]._pricAvg)
		{
			statisData._pricUp++;
		}
		else if (secRecords[idx]._pricAvg < secRecords[idx - 1]._pricAvg)
		{
			statisData._pricDown++;
		}
		else
		{
			statisData._pricUnchange++;
		}
	}

	return;
}

//在一定价格范围内统计，主买/卖成交单数，金额，大于low，小于等于high
void StatisticalData::CalcuInRangeOfPrice(const std::vector<DataPerExecu>& exeDatas, statisDataByPricRange& statisData, double lowPric, double highPric)
{
	for (auto& exeData : exeDatas)
	{
		//在不在范围内
		if (exeData._prc > lowPric && exeData._prc <= highPric)
		{
			++statisData._tradeNum;
			statisData._tradeAmt += exeData._amt;
		}
	}
	return;
}

//计算成交金额占市场比例的均值，带着权重计算
void StatisticalData::CalRatAvgWithWeights(const std::vector<ExeSecRecordData>& secRecords, double& buyAmtRatAvg, double& sellAmtRatAvg)
{
	if (secRecords.empty())
	{
		return;
	}

	double buySum = 0;		//带权重的和
	double buyDivisor = 0;		//除数

	double sellSum = 0;		//带权重的和
	double sellDivisor = 0;		//除数

	for (auto& data : secRecords)
	{
		buySum += data._buyAmtRatio * data._buyAmt;
		buyDivisor += data._buyAmt;
		sellSum += data._sellAmtRatio * data._sellAmt;
		sellDivisor += data._sellAmt;
	}

	if (buyDivisor != 0)
	{
		buyAmtRatAvg = buySum / buyDivisor;
	}
	else
	{
		buyAmtRatAvg = 0;
	}
	
	if (sellDivisor != 0)
	{
		sellAmtRatAvg = sellSum / sellDivisor;
	}
	else
	{
		sellAmtRatAvg = 0;
	}
	
	
	return;
}

//计算成交金额占市场比例的标准差，带着权重计算
void StatisticalData::CalRatStdDeviWithWeights(const std::vector<ExeSecRecordData>& secRecords, double buyAmtRatAvg, double& buyAmtRatSD, double sellAmtRatAvg, double& sellAmtRatSD)
{
	double buySum = 0;
	double sellSum = 0;
	double buyDivisor = 0;
	double sellDivisor = 0;

	for (auto& data : secRecords)
	{
		buySum += std::pow(data._buyAmtRatio - buyAmtRatAvg, 2) * data._buyAmt;		//比例值与均值的差，平方，乘以权重
		buyDivisor += data._buyAmt;

		sellSum += std::pow(data._sellAmtRatio - sellAmtRatAvg, 2) * data._sellAmt;
		sellDivisor += data._sellAmt;
	}

	if (buyDivisor != 0)
	{
		buyAmtRatSD = std::sqrt(buySum / buyDivisor);
	}
	else
	{
		buyAmtRatSD = 0;
	}
	
	if (sellDivisor != 0)
	{
		sellAmtRatSD = std::sqrt(sellSum / sellDivisor);
	}
	else
	{
		sellAmtRatSD = 0;
	}
	
	return;
}

//根据成交金额比例的范围，统计主买/卖成交金额。在函数里，先把两个统计的金额置为0
void StatisticalData::CalTradeAmtInRatioRange(const std::vector<ExeSecRecordData>& secRecords,
	double& buyAmt, double buyAmtRatLow, double buyAmtRatHigh,
	double& sellAmt, double sellAmtRatLow, double sellAmtRatHigh)
{
	buyAmt = 0;
	sellAmt = 0;
	//遍历分钟内所有秒的数据
	for (auto& data : secRecords)
	{
		//主买金额
		if (data._buyAmtRatio > buyAmtRatLow && data._buyAmtRatio <= buyAmtRatHigh)
		{
			buyAmt += data._buyAmt;
		}
		//主卖金额
		if (data._sellAmtRatio > sellAmtRatLow && data._sellAmtRatio <= sellAmtRatHigh)
		{
			sellAmt += data._sellAmt;
		}
	}

	return;
}

//跨分钟时采样全天统计数据，进行一些计算
void StatisticalData::CalDayDataAtMinuteEnd(uint32_t market)
{
	std::map<uint32_t, ExecuStatisDataAllDay>* statisDataMp;
	std::map<uint32_t, RequiredDataForDayStatis>* reqDataMp;
	std::map<uint32_t, ExecuStatisDataMinute>* minStatisDataMp;

	//判断是哪个市场的
	if (market == 1)
	{
		//上交所的
		statisDataMp = &_execuStatisDataAllDaySse;
		reqDataMp = &_reqDatasForDaySse;
		minStatisDataMp = &_execuStatisDataMinSse;
	}
	else if (market == 2)
	{
		//深交所
		statisDataMp = &_execuStatisDataAllDaySzse;
		reqDataMp = &_reqDatasForDaySzse;
		minStatisDataMp = &_execuStatisDataMinSzse;
	}

	//这一分钟内，全市场的主买/卖成交金额
	double buyAmtWholeMarket = 0;
	double sellAmtWholeMarket = 0;
	for (const auto& minData : (*minStatisDataMp))
	{
		buyAmtWholeMarket += minData.second._buyTradeAmt;
		sellAmtWholeMarket += minData.second._sellTradeAmt;
	}

	//遍历所有需要统计的证券代码
	for (auto& elem : *statisDataMp)
	{
		uint32_t code = elem.first;
		//把统计数据的结构体取出来引用
		ExecuStatisDataAllDay& statisData = elem.second;
		RequiredDataForDayStatis& reqData = (*reqDataMp)[code];
		const ExecuStatisDataMinute& minuteData = (*minStatisDataMp)[code];

		//主买成交价格均值
		double buyPrcAvg = 0;
		if (statisData._buyVol != 0)
		{
			buyPrcAvg = statisData._buyAmt / static_cast<double>(statisData._buyVol);
		}
		 statisData._buyPrcAvrg = buyPrcAvg;
		 /*printf("code = %u, buyAmt = %lf, buyVol = %lu, buyPrcAvg = %lf\n", code,
			 _execuStatisDataAllDaySse[code]._buyAmt, _execuStatisDataAllDaySse[code]._buyVol, _execuStatisDataAllDaySse[code]._buyPrcAvrg);*/

		//价格标准差
		 double buyPrcSD = CalcuPricStandardDeviation(reqData._buyExeDatas, statisData._buyPrcAvrg);
		 statisData._buyPrcSD = buyPrcSD;

		//主卖成交价格均值
		 double sellPrcAvg = 0;
		 if (statisData._sellVol != 0)
		 {
			 sellPrcAvg = statisData._sellAmt / static_cast<double>(statisData._sellVol);
		 }
		 statisData._sellPrcAvrg = sellPrcAvg;
		 /*printf("code = %u, sellAmt = %lf, sellVol = %lu, sellPrcAvg = %lf\n", code, 
			 _execuStatisDataAllDaySse[code]._sellAmt, _execuStatisDataAllDaySse[code]._sellVol, _execuStatisDataAllDaySse[code]._sellPrcAvrg);*/

		//价格标准差
		double sellPrcSD = CalcuPricStandardDeviation(reqData._sellExeDatas, statisData._sellPrcAvrg);
		statisData._sellPrcSD = sellPrcSD;

		//价格小于平均值，主买成交单数，主买成交金额
		CalcuInRangeOfPrice(reqData._buyExeDatas, statisData._buyLessAvgPrc, 0, buyPrcAvg);
		//价格小于平均值，主卖成交单数，主卖成交金额
		CalcuInRangeOfPrice(reqData._sellExeDatas, statisData._sellLessAvgPrc, 0, sellPrcAvg);

		//价格大于平均值，小于均值+1std
		//主买
		CalcuInRangeOfPrice(reqData._buyExeDatas, statisData._buyPrcAvgToStd, buyPrcAvg, buyPrcAvg + buyPrcSD);
		//主卖
		CalcuInRangeOfPrice(reqData._sellExeDatas, statisData._sellPrcAvgToStd, sellPrcAvg, sellPrcAvg + sellPrcSD);

		//价格大于平均值+1std，小于均值+2std
		//主买
		CalcuInRangeOfPrice(reqData._buyExeDatas, statisData._buyPrc1stdTo2std, buyPrcAvg + buyPrcSD, buyPrcAvg + buyPrcSD * 2);
		//主卖
		CalcuInRangeOfPrice(reqData._sellExeDatas, statisData._sellPrc1stdTo2std, sellPrcAvg + sellPrcSD, sellPrcAvg + sellPrcSD * 2);

		//价格大于平均值+2std，小于均值+3std
		//主买
		CalcuInRangeOfPrice(reqData._buyExeDatas, statisData._buyPrc2stdTo3std, buyPrcAvg + buyPrcSD * 2, buyPrcAvg + buyPrcSD * 3);
		//主卖
		CalcuInRangeOfPrice(reqData._sellExeDatas, statisData._sellPrc2stdTo3std, sellPrcAvg + sellPrcSD * 2, sellPrcAvg + sellPrcSD * 3);

		//价格大于平均值+3std
		//主买
		CalcuInRangeOfPrice(reqData._buyExeDatas, statisData._buyPrcOver3std, buyPrcAvg + buyPrcSD * 3, 999999.999);
		//主卖
		CalcuInRangeOfPrice(reqData._sellExeDatas, statisData._sellPrcOver3std, sellPrcAvg + sellPrcSD * 3, 999999.999);

		//记录这一分钟的主买成交金额
		reqData._buyAmtPerMin.push_back(minuteData._buyTradeAmt);
		//这一分钟主买成交金额占全市场的比例
		double tradeAmtRatio;
		if (buyAmtWholeMarket != 0)
		{
			tradeAmtRatio = minuteData._buyTradeAmt / buyAmtWholeMarket;
		}
		else
		{
			tradeAmtRatio = 0;
		}
		reqData._buyAmtRatioPerMin.push_back(tradeAmtRatio);

		//记录这一分钟的主卖成交金额
		reqData._sellAmtPerMin.push_back(minuteData._sellTradeAmt);
		//这一分钟主卖成交金额占全市场的比例
		if (sellAmtWholeMarket != 0)
		{
			tradeAmtRatio = minuteData._sellTradeAmt / sellAmtWholeMarket;
		}
		else
		{
			tradeAmtRatio = 0;
		}
		reqData._sellAmtRatioPerMin.push_back(tradeAmtRatio);

		//计算 主买成交金额占市场比例，均值，带着权重数组去计算
		statisData._buyAmtRatiAvg = CalAvrgWithWeights(reqData._buyAmtRatioPerMin, reqData._buyAmtPerMin);
		//计算，主买成交金额占市场比例，标准差，带权重计算，利用已有的均值去计算
		statisData._buyAmtRatiSD = CalStdDeviWithWeights(reqData._buyAmtRatioPerMin, reqData._buyAmtPerMin, statisData._buyAmtRatiAvg);
		//计算 主卖成交金额占市场比例，均值，带着权重数组去计算
		statisData._sellAmtRatiAvg = CalAvrgWithWeights(reqData._sellAmtRatioPerMin, reqData._sellAmtPerMin);
		//计算，主卖成交金额占市场比例，标准差，带权重计算，利用已有的均值去计算
		statisData._sellAmtRatiSD = CalStdDeviWithWeights(reqData._sellAmtRatioPerMin, reqData._sellAmtPerMin, statisData._sellAmtRatiAvg);

		//哪些分钟的比例值小于均值，统计它们的成交金额
		//主买金额
		statisData._buyAmtLessRatAvg = CalAmtInRangeOfRatio(reqData._buyAmtPerMin, reqData._buyAmtRatioPerMin, 0, statisData._buyAmtRatiAvg);
		//主卖金额
		statisData._sellAmtLessRatAvg = CalAmtInRangeOfRatio(reqData._sellAmtPerMin, reqData._sellAmtRatioPerMin, 0, statisData._sellAmtRatiAvg);

		//均值~1std
		//主买金额
		statisData._buyAmtRatAvgToStd = CalAmtInRangeOfRatio(reqData._buyAmtPerMin, reqData._buyAmtRatioPerMin,
			statisData._buyAmtRatiAvg, statisData._buyAmtRatiAvg + statisData._buyAmtRatiSD);
		//主卖金额
		statisData._sellAmtRatAvgToStd = CalAmtInRangeOfRatio(reqData._sellAmtPerMin, reqData._sellAmtRatioPerMin,
			statisData._sellAmtRatiAvg, statisData._sellAmtRatiAvg + statisData._sellAmtRatiSD);

		//1std~2std
		//主买金额
		statisData._buyAmtRat1stdTo2std = CalAmtInRangeOfRatio(reqData._buyAmtPerMin, reqData._buyAmtRatioPerMin,
			statisData._buyAmtRatiAvg + statisData._buyAmtRatiSD, statisData._buyAmtRatiAvg + statisData._buyAmtRatiSD * 2);
		//主卖金额
		statisData._sellAmtRat1stdTo2std = CalAmtInRangeOfRatio(reqData._sellAmtPerMin, reqData._sellAmtRatioPerMin,
			statisData._sellAmtRatiAvg + statisData._sellAmtRatiSD, statisData._sellAmtRatiAvg + statisData._sellAmtRatiSD * 2);

		//2std~3std
		//主买金额
		statisData._buyAmtRat2stdTo3std = CalAmtInRangeOfRatio(reqData._buyAmtPerMin, reqData._buyAmtRatioPerMin,
			statisData._buyAmtRatiAvg + statisData._buyAmtRatiSD * 2, statisData._buyAmtRatiAvg + statisData._buyAmtRatiSD * 3);
		//主卖金额
		statisData._sellAmtRat2stdTo3std = CalAmtInRangeOfRatio(reqData._sellAmtPerMin, reqData._sellAmtRatioPerMin,
			statisData._sellAmtRatiAvg + statisData._sellAmtRatiSD * 2, statisData._sellAmtRatiAvg + statisData._sellAmtRatiSD * 3);

		//大于3std
		//主买金额
		statisData._buyAmtRatOver3std = CalAmtInRangeOfRatio(reqData._buyAmtPerMin, reqData._buyAmtRatioPerMin,
			statisData._buyAmtRatiAvg + statisData._buyAmtRatiSD * 3, 1);	//成交金额比例不超过1
		//主卖金额
		statisData._sellAmtRatOver3std = CalAmtInRangeOfRatio(reqData._sellAmtPerMin, reqData._sellAmtRatioPerMin,
			statisData._sellAmtRatiAvg + statisData._sellAmtRatiSD * 3, 1);
	}

	return;
}

//成交金额占市场比例，计算均值，第二个数组是每个比例的对应的权重（成交金额）
double StatisticalData::CalAvrgWithWeights(const std::vector<double>& ratioVec, const std::vector<double>& amtVec)
{
	//是不是一一对应的
	if (ratioVec.size() != amtVec.size())
	{
		g_log.WriteLog("StatisticalData::CalAvrgWithWeights, ratioVec and amtVec have different length");
		return 0;
	}

	if (ratioVec.size() == 0)
	{
		g_log.WriteLog("StatisticalData::CalAvrgWithWeights, ratioVec length is 0");
		return 0;
	}

	double sum = 0;
	double divisor = 0;
	double avg = 0;
	size_t idx = 0;
	for (idx = 0; idx < ratioVec.size(); idx++)
	{
		sum += ratioVec[idx] * amtVec[idx];
		divisor += amtVec[idx];
	}

	if (divisor != 0)
	{
		avg = sum / divisor;
	}
	else
	{
		avg = 0;
	}

	return avg;
}

//成交金额占市场比例，计算标准差，第二个数组是每个比例的对应的权重（成交金额），第三个参数是已经算好的均值
double StatisticalData::CalStdDeviWithWeights(const std::vector<double>& ratioVec, const std::vector<double>& amtVec, double ratioAvg)
{
	//是不是一一对应的
	if (ratioVec.size() != amtVec.size())
	{
		g_log.WriteLog("StatisticalData::CalStdDeviWithWeights, ratioVec and amtVec have different length");
		return 0;
	}

	if (ratioVec.size() == 0)
	{
		g_log.WriteLog("StatisticalData::CalStdDeviWithWeights, ratioVec length is 0");
		return 0;
	}

	double sum = 0;
	double divisor = 0;
	size_t idx;
	for (idx = 0; idx < ratioVec.size(); idx++)
	{
		sum += std::pow(ratioVec[idx] - ratioAvg, 2) * amtVec[idx];		//比例值，减去平均值，平方，乘以权重
		divisor += amtVec[idx];
	}

	//除以权重的和，开方
	double stdDevi;
	if (divisor != 0)
	{
		stdDevi = std::sqrt(sum / divisor);
	}
	else
	{
		stdDevi = 0;
	}
	
	return stdDevi;
}

//统计全天成交金额，要求分钟成交金额占比，在一定范围内
double StatisticalData::CalAmtInRangeOfRatio(const std::vector<double>& amounts, const std::vector<double>& ratios, double lowRatio, double highRatio)
{
	//成交金额和成交金额占比，是不是一一对应的
	if (amounts.size() != ratios.size())
	{
		g_log.WriteLog("StatisticalData::CalAmtInRangeOfRatio, amounts size and ratios size are different");
		return 0;
	}
	//数组为空
	if (amounts.size() == 0)
	{
		g_log.WriteLog("StatisticalData::CalAmtInRangeOfRatio, amounts size is 0");
		return 0;
	}

	//遍历
	size_t idx;
	double amt = 0;
	for (idx = 0; idx < amounts.size(); idx++)
	{
		//比例值在不在范围内
		if (ratios[idx] > lowRatio && ratios[idx] <= highRatio)
		{
			amt += amounts[idx];
		}
	}

	return amt;
}

//这一分钟内的所有证券的成交统计数据，写文件
void StatisticalData::StoreExecuDataMinute(const std::map<uint32_t, ExecuStatisDataMinute>& statisDatas, uint32_t tim, const string& dir)
{
	//打开文件
	//时间戳，留下时和分
	tim = tim / 100000;
	//文件名
	string fileName("execuDataMinute.csv");
	fileName = std::to_string(tim) + fileName;
	//带路径的文件名
	fileName = dir + "/" + fileName;
	//打开
	FILE* fp = fopen(fileName.c_str(), "w");
	if (fp == NULL)
	{
		g_log.WriteLog("StatisticalData::StoreExecuDataMinute， fopen execuDataMinute.csv error");
		return;
	}

	//写表头,写完自动换行
	WriteHeaderExeMinuCsv(fp);

	//遍历所有的data，逐个写入文件
	for (auto& elem : statisDatas)
	{
		//写入一个statisDataMinute，带着证券代码
		WriteOneExeStatisDataMin(fp, elem.first, elem.second);
		//换行
		fprintf(fp, "\n");
	}

	return;
}



//跨分钟时统计完数据后，清理一些临时数据，为了下一分钟重新统计
void StatisticalData::ClearExecuTempData(uint32_t topDigit)
{
	if (topDigit == 1) //上交所
	{
		_execuTmpDataMinSse.clear();
		_execuStatisDataMinSse.clear();
	}
	else if (topDigit == 2) //深交所
	{
		_execuTmpDataMinSzse.clear();
		_execuStatisDataMinSzse.clear();
	}
	else
	{
		g_log.WriteLog("StatisticalData::ClearExecuTempData, topDigit is not 1 or 2");
	}

	return;
}

//成交统计数据，分钟数据，写表头，在函数里写完表头就换行
void StatisticalData::WriteHeaderExeMinuCsv(FILE* fp)
{
	if (fp == NULL)
	{
		return;
	}

	fprintf(fp, "code%c", _sep);

	fprintf(fp, "buyTradeNum%c", _sep);
	fprintf(fp, "buyTradeAmt%c", _sep);
	fprintf(fp, "buyTradeVol%c", _sep);
	fprintf(fp, "buyPrcAvrg%c", _sep);
	fprintf(fp, "buyPrcSD%c", _sep);
	fprintf(fp, "buyAmtSD%c", _sep);
	fprintf(fp, "sellTradeNum%c", _sep);
	fprintf(fp, "sellTradeAmt%c", _sep);
	fprintf(fp, "sellTradeVol%c", _sep);
	fprintf(fp, "sellPrcAvrg%c", _sep);
	fprintf(fp, "sellPrcSD%c", _sep);
	fprintf(fp, "sellAmtSD%c", _sep);
	fprintf(fp, "startPrice%c", _sep);
	fprintf(fp, "highPrice%c", _sep);
	fprintf(fp, "lowPrice%c", _sep);
	fprintf(fp, "endPrice%c", _sep);
	fprintf(fp, "revenueSD%c", _sep);
	fprintf(fp, "revenueSkew%c", _sep);
	fprintf(fp, "pricUp%c", _sep);
	fprintf(fp, "pricDown%c", _sep);
	fprintf(fp, "pricUnchange%c", _sep);

	fprintf(fp, "buyCountLessAvgPrc%c", _sep);
	fprintf(fp, "buyAmtLessAvgPrc%c", _sep);

	fprintf(fp, "sellCountLessAvgPrc%c", _sep);
	fprintf(fp, "sellAmtLessAvgPrc%c", _sep);
	
	fprintf(fp, "buyCountPrcAvgToStd%c", _sep);
	fprintf(fp, "buyCountPrcAvgToStd%c", _sep);
	
	fprintf(fp, "sellCountPrcAvgToStd%c", _sep);
	fprintf(fp, "sellAmtPrcAvgToStd%c", _sep);
	
	fprintf(fp, "buyCountPrc1stdTo2std%c", _sep);
	fprintf(fp, "buyAmtPrc1stdTo2std%c", _sep);
	
	fprintf(fp, "sellCountPrc1stdTo2std%c", _sep);
	fprintf(fp, "sellAmtPrc1stdTo2std%c", _sep);
	
	fprintf(fp, "buyCountPrc2stdTo3std%c", _sep);
	fprintf(fp, "buyAmtPrc2stdTo3std%c", _sep);
	
	fprintf(fp, "sellCountPrc2stdTo3std%c", _sep);
	fprintf(fp, "sellAmtPrc2stdTo3std%c", _sep);
	
	fprintf(fp, "buyCountPrcOver3std%c", _sep);
	fprintf(fp, "buyAmtPrcOver3std%c", _sep);
	
	fprintf(fp, "sellCountPrcOver3std%c", _sep);
	fprintf(fp, "sellAmtPrcOver3std%c", _sep);

	fprintf(fp, "buyAmtRatFirst%c", _sep);
	fprintf(fp, "buyAmtRatLast%c", _sep);
	fprintf(fp, "buyAmtRatMax%c", _sep);
	fprintf(fp, "buyAmtRatMin%c", _sep);
	fprintf(fp, "buyAmtRatAvg%c", _sep);
	fprintf(fp, "buyAmtRatSD%c", _sep);
	fprintf(fp, "sellAmtRatFirst%c", _sep);
	fprintf(fp, "sellAmtRatLast%c", _sep);
	fprintf(fp, "sellAmtRatMax%c", _sep);
	fprintf(fp, "sellAmtRatMin%c", _sep);
	fprintf(fp, "sellAmtRatAvg%c", _sep);
	fprintf(fp, "sellAmtRatSD%c", _sep);
	fprintf(fp, "buyAmtLessAvgRat%c", _sep);
	fprintf(fp, "sellAmtLessAvgRat%c", _sep);
	fprintf(fp, "buyAmtAvgRatTo1std%c", _sep);
	fprintf(fp, "sellAmtAvgRatTo1std%c", _sep);
	fprintf(fp, "buyAmtRat1stdTo2std%c", _sep);
	fprintf(fp, "sellAmtRat1stdTo2std%c", _sep);
	fprintf(fp, "buyAmtRat2stdTo3std%c", _sep);
	fprintf(fp, "sellAmtRat2stdTo3std%c", _sep);
	fprintf(fp, "buyAmtRatOver3std%c", _sep);
	fprintf(fp, "sellAmtRatOver3std");		//最后一个字段，不加分隔符

	fputc('\n', fp);	//换行
	return;
}

//成交统计数据，分钟数据，写入一个证券的统计数据
void StatisticalData::WriteOneExeStatisDataMin(FILE* fp, uint32_t code, const ExecuStatisDataMinute& data)
{
	//写入证券代码
	int ret = fprintf(fp, "%u%c", code, _sep);
	if (ret == -1)
	{
		g_log.WriteLog("StatisticalData::WriteOneExeStatisDataMin， fprintf code return -1");
		return;
	}

	fprintf(fp, "%u%c", data._buyTradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyTradeAmt, _sep);
	fprintf(fp, "%lu%c", data._buyTradeVol, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrcAvrg, _sep);
	fprintf(fp, "%lf%c", data._buyPrcSD, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtSD, _sep);
	fprintf(fp, "%u%c", data._sellTradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellTradeAmt, _sep);
	fprintf(fp, "%lu%c", data._sellTradeVol, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrcAvrg, _sep);
	fprintf(fp, "%lf%c", data._sellPrcSD, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtSD, _sep);
	fprintf(fp, "%.3lf%c", data._startPrice, _sep);
	fprintf(fp, "%.3lf%c", data._highPrice, _sep);
	fprintf(fp, "%.3lf%c", data._lowPrice, _sep);
	fprintf(fp, "%.3lf%c", data._endPrice, _sep);
	fprintf(fp, "%lf%c", data._revenueSD, _sep);
	fprintf(fp, "%.3lf%c", data._revenueSkew, _sep);
	fprintf(fp, "%u%c", data._pricUp, _sep);
	fprintf(fp, "%u%c", data._pricDown, _sep);
	fprintf(fp, "%u%c", data._pricUnchange, _sep);

	fprintf(fp, "%u%c", data._buyLessAvgPrc._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyLessAvgPrc._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._sellLessAvgPrc._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellLessAvgPrc._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._buyPrcAvgToStd._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrcAvgToStd._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._sellPrcAvgToStd._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrcAvgToStd._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._buyPrc1stdTo2std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrc1stdTo2std._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._sellPrc1stdTo2std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrc1stdTo2std._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._buyPrc2stdTo3std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrc2stdTo3std._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._sellPrc2stdTo3std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrc2stdTo3std._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._buyPrcOver3std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrcOver3std._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._sellPrcOver3std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrcOver3std._tradeAmt, _sep);

	fprintf(fp, "%lf%c", data._buyAmtRatFirst, _sep);
	fprintf(fp, "%lf%c", data._buyAmtRatLast, _sep);
	fprintf(fp, "%lf%c", data._buyAmtRatMax, _sep);
	fprintf(fp, "%lf%c", data._buyAmtRatMin, _sep);
	fprintf(fp, "%lf%c", data._buyAmtRatAvg, _sep);
	fprintf(fp, "%lf%c", data._buyAmtRatSD, _sep);
	fprintf(fp, "%lf%c", data._sellAmtRatFirst, _sep);
	fprintf(fp, "%lf%c", data._sellAmtRatLast, _sep);
	fprintf(fp, "%lf%c", data._sellAmtRatMax, _sep);
	fprintf(fp, "%lf%c", data._sellAmtRatMin, _sep);
	fprintf(fp, "%lf%c", data._sellAmtRatAvg, _sep);
	fprintf(fp, "%lf%c", data._sellAmtRatSD, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtLessAvgRat, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtLessAvgRat, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtAvgRatTo1std, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtAvgRatTo1std, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtRat1stdTo2std, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtRat1stdTo2std, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtRat2stdTo3std, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtRat2stdTo3std, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtRatOver3std, _sep);
	fprintf(fp, "%.3lf", data._sellAmtRatOver3std);		//最后一个，不加分隔符
}

//截止采样时，全天的成交统计数据，写文件
void StatisticalData::StoreExecuDataAllDay(const std::map<uint32_t, ExecuStatisDataAllDay>& statisDatas, uint32_t tim, const string& dir)
{
	//打开文件
	//时间戳，留下时和分
	tim = tim / 100000;
	//文件名
	string fileName("execuDataAllDay.csv");
	fileName = std::to_string(tim) + fileName;
	//带路径的文件名
	fileName = dir + "/" + fileName;
	//打开
	FILE* fp = fopen(fileName.c_str(), "w");
	if (fp == NULL)
	{
		g_log.WriteLog("StatisticalData::StoreExecuDataAllDay， fopen execuDataAllDay.csv error");
		return;
	}

	//写表头,写完自动换行
	WriteHeaderExeDataAllDay(fp);

	//遍历所有的data，逐个写入文件
	for (auto& elem : statisDatas)
	{
		//写入一个statisDataMinute，带着证券代码
		WriteOneExeStatiDataAllDay(fp, elem.first, elem.second);
		//换行
		fprintf(fp, "\n");
	}

	return;
}

//成交统计数据，全天数据，某个分钟的采样数据，写文件，写表头
void StatisticalData::WriteHeaderExeDataAllDay(FILE* fp)
{
	int ret = fprintf(fp, "code%c", _sep);
	if (ret == -1)
	{
		g_log.WriteLog("StatisticalData::WriteHeaderExeDataAllDay，fprintf code error");
		return;
	}

	fprintf(fp, "buyTradeCount%c", _sep);
	fprintf(fp, "buyVol%c", _sep);
	fprintf(fp, "buyAmt%c", _sep);
	fprintf(fp, "buyPrcAvrg%c", _sep);
	fprintf(fp, "buyPrcSD%c", _sep);
	fprintf(fp, "sellTradeCount%c", _sep);
	fprintf(fp, "sellVol%c", _sep);
	fprintf(fp, "sellAmt%c", _sep);
	fprintf(fp, "sellPrcAvrg%c", _sep);
	fprintf(fp, "sellPrcSD%c", _sep);

	fprintf(fp, "buyCountLessAvgPrc%c", _sep);
	fprintf(fp, "buyAmtLessAvgPrc%c", _sep);
	fprintf(fp, "sellCountLessAvgPrc%c", _sep);
	fprintf(fp, "sellAmtLessAvgPrc%c", _sep);

	fprintf(fp, "buyCountPrcAvgToStd%c", _sep);
	fprintf(fp, "buyCountPrcAvgToStd%c", _sep);
	fprintf(fp, "sellCountPrcAvgToStd%c", _sep);
	fprintf(fp, "sellAmtPrcAvgToStd%c", _sep);

	fprintf(fp, "buyCountPrc1stdTo2std%c", _sep);
	fprintf(fp, "buyAmtPrc1stdTo2std%c", _sep);
	fprintf(fp, "sellCountPrc1stdTo2std%c", _sep);
	fprintf(fp, "sellAmtPrc1stdTo2std%c", _sep);

	fprintf(fp, "buyCountPrc2stdTo3std%c", _sep);
	fprintf(fp, "buyAmtPrc2stdTo3std%c", _sep);
	fprintf(fp, "sellCountPrc2stdTo3std%c", _sep);
	fprintf(fp, "sellAmtPrc2stdTo3std%c", _sep);

	fprintf(fp, "buyCountPrcOver3std%c", _sep);
	fprintf(fp, "buyAmtPrcOver3std%c", _sep);
	fprintf(fp, "sellCountPrcOver3std%c", _sep);
	fprintf(fp, "sellAmtPrcOver3std%c", _sep);

	fprintf(fp, "_buyAmtRatiAvg%c", _sep);
	fprintf(fp, "_buyAmtRatiSD%c", _sep);
	fprintf(fp, "_sellAmtRatiAvg%c", _sep);
	fprintf(fp, "_sellAmtRatiSD%c", _sep);
	fprintf(fp, "_buyAmtLessRatAvg%c", _sep);
	fprintf(fp, "_sellAmtLessRatAvg%c", _sep);
	fprintf(fp, "_buyAmtRatAvgToStd%c", _sep);
	fprintf(fp, "_sellAmtRatAvgToStd%c", _sep);
	fprintf(fp, "_buyAmtRat1stdTo2std%c", _sep);
	fprintf(fp, "_sellAmtRat1stdTo2std%c", _sep);
	fprintf(fp, "_buyAmtRat2stdTo3std%c", _sep);
	fprintf(fp, "_sellAmtRat2stdTo3std%c", _sep);
	fprintf(fp, "_buyAmtRatOver3std%c", _sep);
	fprintf(fp, "_sellAmtRatOver3std");		//最后一个字段，不加分隔符

	//换行
	fputc('\n', fp);
	return;
}

//成交统计数据，全天数据，某个分钟的采样，写文件，写入其中一个证券代码和它的统计数据
void StatisticalData::WriteOneExeStatiDataAllDay(FILE* fp, uint32_t code, const ExecuStatisDataAllDay& data)
{
	int ret = fprintf(fp, "%u%c", code, _sep);
	if (ret == -1)
	{
		g_log.WriteLog("StatisticalData::WriteOneExeStatiDataAllDay，fprintf code error");
		return;
	}

	fprintf(fp, "%u%c", data._buyTradeCount, _sep);
	fprintf(fp, "%lu%c", data._buyVol, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmt, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrcAvrg, _sep);
	fprintf(fp, "%lf%c", data._buyPrcSD, _sep);
	fprintf(fp, "%u%c", data._sellTradeCount, _sep);
	fprintf(fp, "%lu%c", data._sellVol, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmt, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrcAvrg, _sep);
	fprintf(fp, "%lf%c", data._sellPrcSD, _sep);

	fprintf(fp, "%u%c", data._buyLessAvgPrc._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyLessAvgPrc._tradeAmt, _sep);
	fprintf(fp, "%u%c", data._sellLessAvgPrc._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellLessAvgPrc._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._buyPrcAvgToStd._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrcAvgToStd._tradeAmt, _sep);
	fprintf(fp, "%u%c", data._sellPrcAvgToStd._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrcAvgToStd._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._buyPrc1stdTo2std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrc1stdTo2std._tradeAmt, _sep);
	fprintf(fp, "%u%c", data._sellPrc1stdTo2std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrc1stdTo2std._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._buyPrc2stdTo3std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrc2stdTo3std._tradeAmt, _sep);
	fprintf(fp, "%u%c", data._sellPrc2stdTo3std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrc2stdTo3std._tradeAmt, _sep);

	fprintf(fp, "%u%c", data._buyPrcOver3std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._buyPrcOver3std._tradeAmt, _sep);
	fprintf(fp, "%u%c", data._sellPrcOver3std._tradeNum, _sep);
	fprintf(fp, "%.3lf%c", data._sellPrcOver3std._tradeAmt, _sep);

	fprintf(fp, "%lf%c", data._buyAmtRatiAvg, _sep);
	fprintf(fp, "%lf%c", data._buyAmtRatiSD, _sep);
	fprintf(fp, "%lf%c", data._sellAmtRatiAvg, _sep);
	fprintf(fp, "%lf%c", data._sellAmtRatiSD, _sep);

	fprintf(fp, "%.3lf%c", data._buyAmtLessRatAvg, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtLessRatAvg, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtRatAvgToStd, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtRatAvgToStd, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtRat1stdTo2std, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtRat1stdTo2std, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtRat2stdTo3std, _sep);
	fprintf(fp, "%.3lf%c", data._sellAmtRat2stdTo3std, _sep);
	fprintf(fp, "%.3lf%c", data._buyAmtRatOver3std, _sep);
	fprintf(fp, "%.3lf", data._sellAmtRatOver3std);		//最后一个字段，不加分隔符

	return;
}

//接收不变的数据，重写
void StatisticalData::OnUnchangedMarketData(uint32_t code, const UnchangedMarketData& data)
{
	_unchangedMarketDatas[code] = data;
	/*printf("OnUnchangedMarketData is callbacked, code = %u, preClosePric = %u, openPric = %u\n",
		code, data._preClosePrice, data._openPrice);*/
}

//创造实例的函数
MDApplication* CreateObjStatistic()
{
	return new StatisticalData;
}

//注册到反射里，key为5，value是创建实例的函数指针
RegisterAction g_regActStatistic("5", CreateObjStatistic);