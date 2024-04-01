#pragma once
#include <string>
#include <map>
#include <vector>
#include "MDApplication.h"

using std::string;

//每次切片时需要暂时保留的数据，放在一个数组里，分钟结束时进行计算
struct SnapTempData
{
    std::vector<double> _askPrcGap;        //卖1和卖10的跨度(10^3)
    std::vector<double> _bidPrcGap;        //买1和买10的跨度(10^3)
    std::vector<double> _askAndBidPrcGap;  //买1和卖1的跨度(10^3)

    std::vector<double> _revenues;             //收益
    std::vector<uint32_t> _timInterv;        //与上个切片的时间间隔
    std::vector<double> _netBidAmt;        //净委买金额(10^3)
};

//利用收到的snapshot，统计的数据，每个证券整个分钟里只有一个，持续更新或分钟结束时统计
struct SnapStatisData
{
    uint32_t _askGapMax;        //委卖价格跨度最大值(10^3)
    uint32_t _askGapMin;        //委卖价格跨度最小值(10^3)
    int32_t _askGapChange;      //委卖价格跨度变化(10^3)
    uint32_t _askGapAvrg;       //委卖价格跨度均值(10^3)
    double _askGapSD;           //委卖价格跨度标准差
    float _buyAmtStrength;      //主买成交金额强度

    uint32_t _bidGapMax;        //委买价格跨度最大值(10^3)
    uint32_t _bidGapMin;        //委买价格跨度最小值(10^3)
    int32_t  _bidGapChange;     //委买价格跨度变化(10^3)
    uint32_t _bidGapAvrg;       //委买价格跨度均值(10^3)
    double   _bidGapSD;         //委买价格跨度标准差
    float _sellAmtStrength;     //主卖成交金额强度

    uint32_t _bidAskGapMax;      //买一卖一跨度最大值(10^3)
    uint32_t _bidAskGapMin;      //买一卖一跨度最小值(10^3)
    int32_t  _bidAskGapChange;   //买一卖一跨度变化(10^3)
    uint32_t _bidAskGapAvrg;     //买一卖一跨度均值(10^3)
    double _bidAskGapSD;         //买一卖一跨度标准差

    double _accAmountBuy;				//买的金额(10^3)
    double _accAmountSell;			//卖的金额
    double _buyOrderAmt;				//新增买委托金额(10^3)
    float _buyOrderAmtStrength;         //新增买委托金额强度
    uint64_t _buyOrderVol;				//新增买委托量(10^0)
    double _activeBuyOrderAmt;		//积极买委托金额
    uint64_t _activeBuyOrderVol;		//积极买委托量
    double _passiveBuyOrderAmt;		//消极买委托金额
    uint64_t _passiveBuyOrderVol;		//消极买委托量
    double _sellOrderAmt;				//新增卖委托金额
    float _sellOrderAmtStrength;        //新增卖委托金额强度
    uint64_t _sellOrderVol;				//新增卖委托量
    double _activeSellOrderAmt;		//新增积极卖委托金额
    double _activeSellOrderVol;		//新增积极卖委托量
    double _passiveSellOrderAmt;		//新增消极卖委托金额
    uint64_t _passiveSellOrderVol;		//新增消极卖委托量
    double _buyOrderCanceledAmt;		//买委托撤单+委托价格低于上个Tick中间价的，金额
    uint64_t _buyOrderCanceledVol;		//买委托撤单+委托价格低于上个Tick中间价的，数量
    double _sellOrderCanceledAmt;		//卖委托撤单+委托价格高于上个Tick中间价的，金额
    uint64_t _sellOrderCanceledVol;		//卖委托撤单+委托价格高于上个Tick中间价的，数量

    double _revenueStandardDeviation;   //一分钟内所有收益的标准差
    double _revenueSkewness;            //一分钟内所有收益的偏度

    uint32_t _prcIncre;                  //成交价格上行次数
    uint32_t _prcDecre;                  //成交价格下行次数
    uint32_t _prcUnchange;               //成交价格相对上次切片，不变的次数

    uint32_t _snapCount;                //切片个数
    uint32_t _maxTimInterv;             //切片间隔的最大值（毫秒）
    uint32_t _minTimInterv;             //切片间隔的最小值（毫秒）
    uint32_t _timIntervAvrg;            //切片间隔的均值
    double _timIntervSD;                //切片间隔的标准差

    int64_t _netBidAmtAvrg;            //净委买金额的均值(10^3)
    double _netBidAmtSD;                //净委买金额的标准差(10^3)

    double _snapRevenue;                //切片间数据，收益

    double _allDayRevenueByOpenPric;    //全天数据，收益，相对于开盘价的收益
    double _allDayRevenueByPreClosePric;    //全天数据，收益，相对于昨收价的收益

public:
    //默认构造函数
    SnapStatisData()
    {
        _askGapMax = 0;
        _askGapMin = 0;
        _askGapChange = 0;
        _askGapAvrg = 0;
        _askGapSD = 0;

        _bidGapMax = 0;
        _bidGapMin = 0;
        _bidGapChange = 0;
        _bidGapAvrg = 0;
        _bidGapSD = 0;

        _bidAskGapMax = 0;
        _bidAskGapMin = 0;
        _bidAskGapChange = 0;
        _bidAskGapAvrg = 0;
        _bidAskGapSD = 0;

        _buyAmtStrength = 0;
        _sellAmtStrength = 0;
        _accAmountBuy = 0;
        _accAmountSell = 0;
        _buyOrderAmt = 0;
        _buyOrderAmtStrength = 0;
        _buyOrderVol = 0;
        _activeBuyOrderAmt = 0;
        _activeBuyOrderVol = 0;
        _passiveBuyOrderAmt = 0;
        _passiveBuyOrderVol = 0;
        _sellOrderAmt = 0;
        _sellOrderAmtStrength = 0;
        _sellOrderVol = 0;
        _activeSellOrderAmt = 0;
        _activeSellOrderVol = 0;
        _passiveSellOrderAmt = 0;
        _passiveSellOrderVol = 0;
        _buyOrderCanceledAmt = 0;
        _buyOrderCanceledVol = 0;
        _sellOrderCanceledAmt = 0;
        _sellOrderCanceledVol = 0;

        _revenueStandardDeviation = 0;
        _revenueSkewness = 0;

        _prcIncre = 0;
        _prcDecre = 0;
        _prcUnchange = 0;

        _snapCount = 0;
        _maxTimInterv = 0;
        _minTimInterv = 0;
        _timIntervAvrg = 0;
        _timIntervSD = 0;

        _netBidAmtAvrg = 0;
        _netBidAmtSD = 0;
    }
};


struct DataPerExecu
{
    double _prc;      //成交价格
    uint32_t _vol;      //成交量
    double _amt;      //成交金额
};

//每秒钟内保留的临时数据，跨秒钟时用它们统计一些东西，然后清空
struct ExecuTmpDataPerSecond
{
    uint64_t _buyVol;       //主买成交量（累加）
    double _buyAmt;       //主买成交金额（累加）
    uint64_t _sellVol;       //主卖成交量（累加）
    double _sellAmt;      //主卖成交金额（累加）

public:
    //默认构造，初始化
    ExecuTmpDataPerSecond()
    {
        _buyVol = 0;
        _sellVol = 0;
        _buyAmt = 0;
        _sellAmt = 0;
    }
};

//每秒钟结束时统计和记录的数据
struct ExeSecRecordData
{
    double _buyAmt;         //这一秒中的主买成交金额
    double _buyAmtRatio;    //主买成交金额占全市场的比例
    double _sellAmt;        //这一秒中的主卖成交金额
    double _sellAmtRatio;   //主卖成交金额占全市场的比例
    double _pricAvg;        //这一秒的平均成交价格（不分买卖方向，总成交金额/总成交量）
};

//每分钟的临时数据，保留一分钟内的某些数据，跨分钟时用于统计一些指标，然后清空
struct ExecuTmpDataPerMinute
{
    std::vector<DataPerExecu> _buyExecuData;        //每次主买成交的价格，量，金额（数组）
    std::vector<DataPerExecu> _sellExecuData;       //每次主卖成交的价格，量，金额（数组）
    std::vector<ExeSecRecordData> _dataPerSec;      //每秒钟内计算并记录的数据

};

//根据不同的价格范围统计的数据
struct  statisDataByPricRange
{
    uint32_t _tradeNum;      //成交单数
    double _tradeAmt;        //成交金额

public:
    //默认构造
    statisDataByPricRange()
    {
        _tradeNum = 0;
        _tradeAmt = 0;
    }
};

//统计分钟数据的结构体
struct ExecuStatisDataMinute
{
    uint32_t _buyTradeNum;	//主买成交的交易笔数
    double _buyTradeAmt;	//主买成交的金额，（保留三位小数）
    uint64_t _buyTradeVol;	//主买成交的交易量（不保留小数位）

    double _buyPrcAvrg;     //主买成交价平均值
    double _buyPrcSD;       //主买成交价标准差
    double _buyAmtSD;       //主买成交金额标准差

    uint32_t _sellTradeNum;	//主卖成交的交易笔数
    double _sellTradeAmt;	//主卖成交的金额（保留三位小数）
    uint64_t _sellTradeVol;	//主卖成交的交易量（不保留小数位）
    
    double _sellPrcAvrg;   //主卖成交价平均值
    double _sellPrcSD;       //主卖成交价标准差
    double _sellAmtSD;       //主卖成交金额标准差

    double _startPrice;	//开始成交价格（保留三位小数）
    double _highPrice;	//最高成交价格（保留三位小数）
    double _lowPrice;		//最低成交价格（保留三位小数）
    double _endPrice;		//结束价格（保留三位小数）

    double _revenueSD;      //每秒为颗粒度，收益的标准差
    double _revenueSkew;    //每秒为颗粒度，收益的偏度
    uint32_t _pricUp;       //每秒为颗粒度，价格上行次数
    uint32_t _pricDown;     //每秒为颗粒度，价格下行次数
    uint32_t _pricUnchange; //每秒为颗粒度，价格不变次数

    //价格小于均值的一些统计
    statisDataByPricRange _buyLessAvgPrc;
    statisDataByPricRange _sellLessAvgPrc;

    //价格在（均值,均值+1std]
    statisDataByPricRange _buyPrcAvgToStd;
    statisDataByPricRange _sellPrcAvgToStd;

    //成交价格属于(avg + 1std, avg + 2std]
    statisDataByPricRange _buyPrc1stdTo2std;
    statisDataByPricRange _sellPrc1stdTo2std;

    //成交价格属于(avg + 2std, avg + 3std]
    statisDataByPricRange _buyPrc2stdTo3std;
    statisDataByPricRange _sellPrc2stdTo3std;

    //成交价格大于avg+3std
    statisDataByPricRange _buyPrcOver3std;
    statisDataByPricRange _sellPrcOver3std;

    double _buyAmtRatFirst;     //主买成交金额占全市场比例的开始值
    double _buyAmtRatLast;      //主买成交金额占全市场比例的结束值
    double _buyAmtRatMax;       //主买成交金额占全市场比例的最大值
    double _buyAmtRatMin;       //主买成交金额占全市场比例的最小值
    double _buyAmtRatAvg;       //主买成交金额占全市场比例的均值
    double _buyAmtRatSD;        //主买成交金额占全市场比例的标准差

    double _sellAmtRatFirst;     //主卖成交金额占全市场比例的开始值
    double _sellAmtRatLast;      //主卖成交金额占全市场比例的结束值
    double _sellAmtRatMax;       //主卖成交金额占全市场比例的最大值
    double _sellAmtRatMin;       //主卖成交金额占全市场比例的最小值
    double _sellAmtRatAvg;       //主卖成交金额占全市场比例的均值
    double _sellAmtRatSD;        //主卖成交金额占全市场比例的标准差

    double _buyAmtLessAvgRat;   //主买成交金额，占市场比例小于均值的
    double _sellAmtLessAvgRat;  //主卖成交金额，占市场比例小于均值的
    double _buyAmtAvgRatTo1std;   //主买成交金额，占市场比例在(均值，均值+标准差]
    double _sellAmtAvgRatTo1std;  //主卖成交金额，占市场比例 (均值，均值+标准差]
    double _buyAmtRat1stdTo2std;   //主买成交金额，占市场比例在(均值+std，均值+2std]
    double _sellAmtRat1stdTo2std;  //主卖成交金额，占市场比例 (均值+std，均值+2std]
    double _buyAmtRat2stdTo3std;   //主买成交金额，占市场比例在(均值+2std，均值+3std]
    double _sellAmtRat2stdTo3std;  //主卖成交金额，占市场比例 (均值+2std，均值+3std]
    double _buyAmtRatOver3std;   //主买成交金额，占市场比例大于均值+3std
    double _sellAmtRatOver3std;  //主卖成交金额，占市场比例大于均值+3std

public:
    //默认构造，把所有值初始化
    ExecuStatisDataMinute()
    {
        _buyTradeNum = 0;
        _buyTradeAmt = 0;
        _buyTradeVol = 0;
        _buyPrcAvrg = 0;
        _buyPrcSD = 0;
        _buyAmtSD = 0;

        _sellTradeNum = 0;
        _sellTradeAmt = 0;
        _sellTradeVol = 0;
        _sellPrcAvrg = 0;
        _sellPrcSD = 0;
        _sellAmtSD = 0;
        
        _startPrice = 0;
        _highPrice = 0;
        _lowPrice = 0;
        _endPrice = 0;

        _pricUp = 0;
        _pricDown = 0;
        _pricUnchange = 0;

        _revenueSD = 0;
        _revenueSkew = 0;
    }
};

//统计全天数据的结构体
struct ExecuStatisDataAllDay
{
    uint32_t _buyTradeCount;    //主买成交单数
    uint64_t _buyVol;           //主买成交量
    double _buyAmt;           //主买成交金额
    double _buyPrcAvrg;       //主买成交价格均值
    double _buyPrcSD;           //主买成交价格标准差

    uint32_t _sellTradeCount;    //主卖成交单数
    uint64_t _sellVol;           //主卖成交量
    double _sellAmt;           //主卖成交金额
    double _sellPrcAvrg;       //主卖成交价格均值
    double _sellPrcSD;           //主卖成交价格标准差

    statisDataByPricRange _buyLessAvgPrc;       //价格小于平均值，主买成交单数，主买成交金额
    statisDataByPricRange _sellLessAvgPrc;      //价格小于平均值，主卖成交单数，主卖成交金额

    //价格在（均值,均值+1std]
    statisDataByPricRange _buyPrcAvgToStd;
    statisDataByPricRange _sellPrcAvgToStd;

    //成交价格属于(avg + 1std, avg + 2std]
    statisDataByPricRange _buyPrc1stdTo2std;
    statisDataByPricRange _sellPrc1stdTo2std;

    //成交价格属于(avg + 2std, avg + 3std]
    statisDataByPricRange _buyPrc2stdTo3std;
    statisDataByPricRange _sellPrc2stdTo3std;

    //成交价格大于avg+3std
    statisDataByPricRange _buyPrcOver3std;
    statisDataByPricRange _sellPrcOver3std;

    double _buyAmtRatiAvg;      //主买成交金额占市场比例，均值
    double _buyAmtRatiSD;       //主买成交金额占市场比例，标准差
    double _sellAmtRatiAvg;     //主卖成交金额占市场比例，均值
    double _sellAmtRatiSD;      //主卖成交金额占市场比例，标准差

    double _buyAmtLessRatAvg;       //哪些分钟的主买金额比例，<=均值，统计这些分钟的金额
    double _sellAmtLessRatAvg;      //哪些分钟的主卖金额比例，<=均值，统计这些分钟的金额
    double _buyAmtRatAvgToStd;      //分钟的主买金额比例，(均值,均值+std]，统计这些分钟的金额
    double _sellAmtRatAvgToStd;     //分钟的主卖金额比例，(均值,均值+std]，统计这些分钟的金额
    double _buyAmtRat1stdTo2std;    //分钟的主买金额比例，(均值+std,均值+2std]，统计这些分钟的金额
    double _sellAmtRat1stdTo2std;   //分钟的主卖金额比例，(均值+std,均值+2std]，统计这些分钟的金额
    double _buyAmtRat2stdTo3std;    //分钟的主买金额比例，(均值+2std,均值+3std]，统计这些分钟的金额
    double _sellAmtRat2stdTo3std;   //分钟的主卖金额比例，(均值+2std,均值+3std]，统计这些分钟的金额
    double _buyAmtRatOver3std;    //分钟的主买金额比例，(均值+3std, 1]，统计这些分钟的金额
    double _sellAmtRatOver3std;   //分钟的主卖金额比例，(均值+3std, 1]，统计这些分钟的金额

public:
    //默认构造，初始化
    ExecuStatisDataAllDay()
    {
        _buyTradeCount = 0;
        _buyVol = 0;
        _buyAmt = 0;

        _sellTradeCount = 0;
        _sellVol = 0;
        _sellAmt = 0;
    }

};

//统计全天数据需要记录的信息
struct RequiredDataForDayStatis
{
    std::vector<DataPerExecu> _buyExeDatas;     //每次主买成交，保存的信息
    std::vector<DataPerExecu> _sellExeDatas;     //每次主卖成交，保存的信息

    std::vector<double> _buyAmtPerMin;          //每分钟的主买成交金额
    std::vector<double> _buyAmtRatioPerMin;     //每分钟的主买成交金额占市场成交金额的比例
    std::vector<double> _sellAmtPerMin;         //每分钟的主卖成交金额
    std::vector<double> _sellAmtRatioPerMin;    //每分钟的主卖成交金额占市场成交金额的比例
};

class StatisticalData :
    public MDApplication
{
public:
    //构造函数
    StatisticalData();

    //盘前处理函数，重写
    virtual int BeforeMarket() override;

    //snapshotInfo函数，重写
    virtual void OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt) override;

    //接收成交数据的回调函数，重写
    virtual void OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt) override;

    //接收不变的数据，重写
    virtual void OnUnchangedMarketData(uint32_t code, const UnchangedMarketData& data) override;

private:
    //如果tim2相比tim1跨到下一分钟了，返回1；同一分钟，返回0；是tim1的前一分钟，返回-1
    int32_t CompareTime(uint32_t tim1, uint32_t tim2);

    //根据收到的一个snapshot，统计相关数据
    void UpdateSnapStatisData(const SnapshotInfo& snap,
        std::map<uint32_t, SnapshotInfo>& lastSnapshots,
        std::map<uint32_t, SnapTempData>& snapTempDatas,
        std::map<uint32_t, SnapStatisData>& snapStatisDatas);

    //分钟结束时，计算一些数据
    void CalculateInMiniuteEnd(std::map<uint32_t, SnapshotInfo>& lastSnapshots,
        std::map<uint32_t, SnapTempData>& snapTempDatas,
        std::map<uint32_t, SnapStatisData>& snapStatisDatas);

    //跨分钟时，把lastSnapshot原始数据的某些字段写文件
    void WriteOriginalSnapToFile(const std::string& dir, uint32_t tim, const std::map<uint32_t, SnapshotInfo>& snapshots);
    
    //写表头
    void WriteHeaderOrignalSnapShot(FILE* fp);

    //写入一条snapshot数据
    void WriteOneSnapshotData(FILE* fp, const SnapshotInfo& snap);

    //跨分钟时，snapshot统计数据写文件，参数设为写文件的目录，要写的数据映射map
    void WriteSnapStatisDataToFile(const std::string& dir, uint32_t time, const std::map<uint32_t, SnapStatisData>& statisData);
    
    void ClearSnapData(std::map<uint32_t, SnapTempData>& snapTempDatas, std::map<uint32_t, SnapStatisData>& snapStatisDatas);

    //利用先后两个snapshot的委托价格，计算一些数据
    void GetOrderEvaluate(const SnapshotInfo& snap1, const SnapshotInfo& snap2, SnapStatisData& snapStatisData);

    //根据snapshot数据计算中间价
    double GetMidPrice(const SnapshotInfo& snapshot);

    //两个时间戳之间的时间间隔，结果以毫秒为单位，时间戳是hhmmssmmm形式
    uint32_t ComputeTimeInterval(uint32_t tim1, uint32_t tim2);

    //计算委卖价格跨度，结果保留三位小数
    double GetAskPrcGap(const SnapshotInfo& snap);

    //计算委买价格跨度，结果保留三位小数
    double GetBidPrcGap(const SnapshotInfo& snap);

    //写表头
    void WriteHeaderSnapStatis(FILE* fp);
    //文件已建好，写统计数据
    void WriteSnapStatisData(FILE* fp, const SnapStatisData& data);

    //利用收到的一个成交数据，更新统计数据
    void UpdateExecuStatisData(const OrdAndExeInfo& info, uint32_t market);

    //比较两个时间戳，有没有跨到下一秒钟，跨秒钟返回1，同一秒钟返回0，倒回上一秒的返回-1
    int CompareTimeBySec(uint32_t tm1, uint32_t tm2);

    //统计一秒钟内的数据，这一秒的平均价格，主买金额占市场的比例，主卖金额占市场的比例，统计结果放入分钟临时数据数组里
    void CalcuExeDataInOneSec(const std::map<uint32_t, ExecuTmpDataPerSecond> &tmpDatasSec, std::map<uint32_t, ExecuTmpDataPerMinute> &tmpDatasMin);

    //分钟结束时，统计本分钟的一些数据
    void CalcuExeDataAtMinuteEnd(const std::map<uint32_t, ExecuTmpDataPerMinute>& tmpDatasMin, std::map<uint32_t, ExecuStatisDataMinute>& statisDataMin);

    //根据分钟临时数据的成交信息数组，和价格平均值，计算价格标准差
    double CalcuPricStandardDeviation(const std::vector<DataPerExecu>& vec, double avrg);

    //根据每秒的记录数据里的价格，获得每秒的收益
    std::vector<double> GetRevenuesByPrices(const std::vector<ExeSecRecordData> &sedRecords);

    //根据一分钟内每秒记录的数据，获得价格波动的数据
    void GetPricVolatility(const std::vector<ExeSecRecordData>& secRecords, ExecuStatisDataMinute& statisData);

    //在一定价格范围内统计，主买/卖成交单数，金额，大于low，小于等于high
    void CalcuInRangeOfPrice(const std::vector<DataPerExecu> &exeData, statisDataByPricRange &statisData, double lowPric, double highPric);

    //计算成交金额占市场比例的均值，带着权重计算
    void CalRatAvgWithWeights(const std::vector<ExeSecRecordData>& secRecords, double &buyAmtRatAvg, double& sellAmtRatAvg);

    //计算成交金额占市场比例的标准差，带着权重计算
    void CalRatStdDeviWithWeights(const std::vector<ExeSecRecordData>& secRecords, double buyAmtRatAvg, double& buyAmtRatSD, double sellAmtRatAvg, double& sellAmtRatSD);

    //根据成交金额比例的范围，统计主买/卖成交金额。在函数里，先把两个统计的金额置为0
    void CalTradeAmtInRatioRange(const std::vector<ExeSecRecordData>& secRecords, 
        double& buyAmt, double buyAmtRatLow, double buyAmtRatHigh,
        double& sellAmt, double sellAmtRatLow, double sellAmtRatHigh);

    //跨分钟时采样全天统计数据，进行一些计算
    void CalDayDataAtMinuteEnd(uint32_t market);

    //成交金额占市场比例，计算均值，第二个数组是每个比例的对应的权重（成交金额）
    double CalAvrgWithWeights(const std::vector<double>& ratioVec, const std::vector<double>& amtVec);

    //成交金额占市场比例，计算标准差，第二个数组是每个比例的对应的权重（成交金额），第三个参数是已经算好的均值
    double CalStdDeviWithWeights(const std::vector<double>& ratioVec, const std::vector<double>& amtVec, double ratioAvg);

    //统计全天成交金额，要求分钟成交金额占比，在一定范围内
    double CalAmtInRangeOfRatio(const std::vector<double>& amounts, const std::vector<double>& ratios, double lowRatio, double highRatio);

    //这一分钟内的所有证券的成交统计数据，写文件
    void StoreExecuDataMinute(const std::map<uint32_t, ExecuStatisDataMinute>& data, uint32_t tim, const std::string& dir);

    //截止采样时，全天的成交统计数据，写文件
    void StoreExecuDataAllDay(const std::map<uint32_t, ExecuStatisDataAllDay>& data, uint32_t tim, const string& dir);

    //跨分钟时统计完数据后，清理一些临时数据，为了下一分钟重新统计
    void ClearExecuTempData(uint32_t topDigit);

    //成交统计数据，分钟数据，写表头
    void WriteHeaderExeMinuCsv(FILE* fp);

    //成交统计数据，分钟数据，写入一个证券的统计数据
    void WriteOneExeStatisDataMin(FILE* fp, uint32_t code, const ExecuStatisDataMinute& data);

    //成交统计数据，全天数据，某个分钟的采样数据，写文件，写表头
    void WriteHeaderExeDataAllDay(FILE* fp);

    //成交统计数据，全天数据，某个分钟的采样，写文件，写入其中一个证券代码和它的统计数据
    void WriteOneExeStatiDataAllDay(FILE* fp, uint32_t code, const ExecuStatisDataAllDay& statisData);

private:
    //存储上交所数据的文件路径
    std::string _sseFilePath;
    //存储深交所数据的文件路径
    std::string _szseFilePath;

    //上交所最新时间戳
    uint32_t _tmSse;
    uint32_t _tmExecuSse;
    //深交所最新时间戳
    uint32_t _tmSzse;
    uint32_t _tmExecuSzse;

    //security_code映射的上一个snapshot数据
    std::map<uint32_t, SnapshotInfo> _lastSnapshotSse;
    std::map<uint32_t, SnapshotInfo> _lastSnapshotSzse;

    //证券代码映射每次snapshot需要暂存的数据
    std::map<uint32_t, SnapTempData> _snapTempDataSse;
    std::map<uint32_t, SnapTempData> _snapTempDataSzse;

    //证券代码对应的snapshot统计数据
    std::map<uint32_t, SnapStatisData> _snapStatisDataSse;
    std::map<uint32_t, SnapStatisData> _snapStatisDataSzse;

    //证券代码对应的一些不变数据
    std::map<uint32_t, UnchangedMarketData> _unchangedMarketDatas;

    //证券代码映射的execution统计数据，分钟统计数据
    std::map<uint32_t, ExecuStatisDataMinute> _execuStatisDataMinSse;
    std::map<uint32_t, ExecuStatisDataMinute> _execuStatisDataMinSzse;

    //映射，每分钟临时数据（成交）
    std::map<uint32_t, ExecuTmpDataPerMinute> _execuTmpDataMinSse;
    std::map<uint32_t, ExecuTmpDataPerMinute> _execuTmpDataMinSzse;

    //每秒钟临时记录的数据，映射
    std::map<uint32_t, ExecuTmpDataPerSecond> _execuTmpDataPerSecSse;
    std::map<uint32_t, ExecuTmpDataPerSecond> _execuTmpDataPerSecSzse;

    //证券代码映射的execution统计数据，全天统计数据
    std::map<uint32_t, ExecuStatisDataAllDay> _execuStatisDataAllDaySse;
    std::map<uint32_t, ExecuStatisDataAllDay> _execuStatisDataAllDaySzse;

    //统计全天数据时，需要记录的信息
    std::map<uint32_t, RequiredDataForDayStatis> _reqDatasForDaySse;
    std::map<uint32_t, RequiredDataForDayStatis> _reqDatasForDaySzse;

    //分隔符
    char _sep;
};

