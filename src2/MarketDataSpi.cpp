#include <stdio.h>
#include <mutex>
#include <iostream>
#include <vector>
#include <string>
#include "MarketDataSpi.h"
#include "SPLog.h"

#ifdef _WIN32
#pragma warning (disable:4996)
#endif

extern SPLog g_log;         //记录日志
static std::mutex g_mutex;  //同步cout输出，回调的线程模型请参看开发指南

//构造函数
MarketDataSpi::MarketDataSpi()
{
    //给它赋一个什么都不做的匿名函数值
    _onMySnapshot = [](SnapshotInfo* snapshotInfo, uint32_t cnt) {};
    //_onMyTickOrder = [](amd::ama::MDTickOrder* ticks, uint32_t cnt) {};
    //_onMyTickExecution = [](amd::ama::MDTickExecution* tick, uint32_t cnt) {};
    _onOrderAndExecu = [](OrdAndExeInfo* tick, uint32_t cnt) {};
    //_onMyOrderBook = [](std::vector<amd::ama::MDOrderBook>& order_book) {};
    _onQuickSnap = [](QuickSnapInfo* order_book, uint32_t cnt) {};
}

// 定义日志回调处理方法
void MarketDataSpi::OnLog(const int32_t& level,
    const char* log,
    uint32_t len)
{
    //std::lock_guard<std::mutex> _(g_mutex);
    //std::cout << "AMA log: " << "    level: " << level << "    log:   " << log << std::endl;
    static char logBuf[1024] = { 0 };
    sprintf(logBuf, "AMA log: level: %u log: %s", level, log);
    //printf("%s\n", logBuf);
    g_log.WriteLog(logBuf);

}

// 定义监控回调处理方法
void MarketDataSpi::OnIndicator(const char* indicator,
    uint32_t len)
{
    std::lock_guard<std::mutex> _(g_mutex);
    std::cout << "AMA indicator: " << indicator << std::endl;
}

// 定义事件回调处理方法  level 对照 EventLevel 数据类型 code 对照 EventCode 数据类型
void MarketDataSpi::OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len)
{
    std::cout << "AMA event: " << event_msg << std::endl;
    
    //写入日志
    std::string eventStr("AMA event: ");
    eventStr += event_msg;
    g_log.WriteLog(eventStr);
    g_log.FlushLogFile();
}

// 定义快照数据回调处理方法
void MarketDataSpi::OnMDSnapshot(amd::ama::MDSnapshot* snapshot,
    uint32_t cnt)
{
    _vecSnapshot.resize(cnt);
    //int chgFlag;

    for (uint32_t i = 0; i < cnt; i++)
    {
        //转化为snapshotInfo类型
        ConvertToSnapshotInfo(snapshot + i, &_vecSnapshot[i]);

        //看情况是否需要更新证券对应的unchangedData,如果有更新，传给应用
        UpdateUnchangedData(snapshot[i]);
    }

    _onMySnapshot(&_vecSnapshot[0], cnt);

    /* 手动释放数据内存, 否则会造成内存泄露 */
    amd::ama::IAMDApi::FreeMemory(snapshot);
}

//更新证券对应的unchangedData，或者检查发现不需要更新
int MarketDataSpi::UpdateUnchangedData(const amd::ama::MDSnapshot& snapshot)
{
    
    uint32_t code;
    sscanf(snapshot.security_code, "%u", &code);

    int chgFlag = 0;
    UnchangedMarketData& data = _unchangedDatas[code];

    //if (data._getAll)
    //{
    //    //已全部更新
    //    return 0;
    //}

    //访问逐个成员，是否已经获得过值
    if (data._openPrice == 0)
    {
        //没有获得过openPrice值，看本次获得的是不是非0值
        if (snapshot.open_price != 0)
        {
            data._openPrice = snapshot.open_price / 1000;
            chgFlag = 1;
        }
    }
    
    if (data._preClosePrice == 0)
    {
        //没有更新过preClosePrice，看看本次的数据是否要更新进去
        if (snapshot.pre_close_price != 0)
        {
            data._preClosePrice = snapshot.pre_close_price /1000;
            chgFlag = 1;
        }
    }

    if (chgFlag == 1)//如果有更新
    {
        //根据市场类型，在证券代码前面加上1或2
        if (snapshot.market_type == amd::ama::MarketType::kSSE)
        {
            code = code + 100000000;
        }
        else if (snapshot.market_type == amd::ama::MarketType::kSZSE)
        {
            code += 200000000;
        }
        else
        {
            g_log.WriteLog("MarketDataSpi::UpdateUnchangedData, get snapshot not sse or szse");
            return 0;
        }

        //调用应用的回调函数
        for (MDApplication* app : _appObjects)
        {
            app->OnUnchangedMarketData(code, data);
        }

        /*printf("got and changed ,code = %u, prePric = %u, openPrc = %u\n",
            code, data._preClosePrice, data._openPrice);*/
    }

    return chgFlag;
}

//
void MarketDataSpi::OnMDFutureSnapshot(amd::ama::MDFutureSnapshot* snapshots, uint32_t cnt)
{
    for (uint32_t i = 0; i < cnt; ++i)
    {
        //std::lock_guard<std::mutex> _(g_mutex);
        //std::cout << amd::ama::Tools::Serialize(snapshot[i]) << std::endl;
        //std::cout << snapshots->security_code << "==" << snapshots->last_price << "==" << snapshots->total_volume_trade << std::endl;
    }

    /* 手动释放数据内存, 否则会造成内存泄露 */
    amd::ama::IAMDApi::FreeMemory(snapshots);
}
//定义指数快照数据回调处理方法
void MarketDataSpi::OnMDIndexSnapshot(amd::ama::MDIndexSnapshot* snapshots, uint32_t cnt)
{

    for (uint32_t i = 0; i < cnt; ++i)
    {
        //std::lock_guard<std::mutex> _(g_mutex);
        //std::cout << amd::ama::Tools::Serialize(snapshots[i]) << std::endl;
    }

    /* 手动释放数据内存, 否则会造成内存泄露 */
    amd::ama::IAMDApi::FreeMemory(snapshots);
}

//定义逐笔委托数据回调处理方法
void MarketDataSpi::OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt)
{
    //MDTickOrder转化位OrdAndExeInfo
    _vecOrdAndExe1.resize(cnt);
    for (uint32_t i = 0; i < cnt; i++)
    {
        AnalyseTickOrder(ticks + i, &(_vecOrdAndExe1[i]));
    }

    //传给回调函数
    _onOrderAndExecu(&(_vecOrdAndExe1[0]), cnt);

    /* 手动释放数据内存, 否则会造成内存泄露 */
    amd::ama::IAMDApi::FreeMemory(ticks);
}

// 定义逐笔成交数据回调处理方法
void MarketDataSpi::OnMDTickExecution(amd::ama::MDTickExecution* tick, uint32_t cnt)
{
    _vecOrdAndExe2.resize(cnt);
    //MDTickExecution转化位OrderAndExecution
    for (uint32_t i = 0; i < cnt; i++)
    {
        AnalyseTicExecution(tick + i, &_vecOrdAndExe2[i]);
    }

    //传给回调函数
    _onOrderAndExecu(&(_vecOrdAndExe2[0]), cnt);

    /* 手动释放数据内存, 否则会造成内存泄露 */
    amd::ama::IAMDApi::FreeMemory(tick);
}

//定义委托簿数据回调处理方法(本地构建模式下非单线程递交, cfg.thread_num为递交委托簿数据线程数, 服务端推送模式下为单线程递交)
void MarketDataSpi::OnMDOrderBook(std::vector<amd::ama::MDOrderBook>& order_books)
{
    //printf("MarketDataSpi::OnMDOrderBook\n");

    size_t sz = order_books.size();

    std::vector<QuickSnapInfo> vecQuickSnap;
    vecQuickSnap.resize(sz);
    //printf("size = %u\n", sz);

    //分析数据
    for (uint32_t i = 0; i < sz; i++)
    {
        //printf("i = %u\n",i);
        AnalyseOrderBook(order_books[i], &(vecQuickSnap[i]));
    }

    //printf("call _onQuickSnap in MarketDataSpi::OnMDOrderBook");
    _onQuickSnap(&(vecQuickSnap[0]), sz);

    //不需要手动释放内存
}

void MarketDataSpi::ConvertToSnapshotInfo(const amd::ama::MDSnapshot* snapshot, SnapshotInfo* info)
{
    uint64_t tmp;
    uint32_t high1 = 999999999;
    uint64_t high2 = 10000000000000000;
    uint32_t high3 = 2147483647;
    int i;
   
    sscanf(snapshot->security_code, "%u", &info->_sc); //证券代码

    //如果是上交所，代码加100000000
    if (snapshot->market_type == amd::ama::MarketType::kSSE)
    {
        info->_sc += 100000000;
    }
    else if (snapshot->market_type == amd::ama::MarketType::kSZSE)
    {
        //深交所，以2开头
        info->_sc += 200000000;
    }
   
    info->_tm = snapshot->orig_time % 1000000000; //时间，只留下时分秒毫秒

    tmp = snapshot->IOPV / 10; //保留5位精度
    info->_pxIopv = tmp > high1 ? high1 : tmp; //是否越界，如果越界，取上限值

    tmp = snapshot->high_price / 1000;//保留3位精度
    info->_pxHigh = tmp > high1 ? high1 : tmp;

    //加权平均委卖价格
    tmp = snapshot->weighted_avg_offer_price / 1000;
    info->_pxAvgAsk = tmp > high1 ? high1 : tmp;

    //申卖价10档
    for (i = 0; i < 10; i++)
    {
        tmp = snapshot->offer_price[i] / 1000;
        info->_pxAsk[i] = tmp > high1 ? high1 : tmp;
    }

    //最新成交价
    tmp = snapshot->last_price / 1000;
    info->_pxLast = tmp > high1 ? high1 : tmp;

    //申买价，10档
    for (i = 0; i < 10; i++)
    {
        tmp = snapshot->bid_price[i] / 1000;
        info->_pxBid[i] = tmp > high1 ? high1 : tmp;
    }

    //加权平均委买价格
    tmp = snapshot->weighted_avg_bid_price / 1000;
    info->_pxAvgBid = tmp > high1 ? high1 : tmp;

    //最低价
    tmp = snapshot->low_price / 1000;
    info->_pxLow = tmp > high1 ? high1 : tmp;
   
    //委托卖出总量
    tmp = snapshot->total_offer_volume / 100; //华锐的保留了两位小数，我的不需要保留小数
    info->_qtyTotAsk = tmp > high2 ? high2 : tmp; //上限是 10000000000000000

    //申卖量10档
    for (i = 0; i < 10; i++)
    {
        tmp = snapshot->offer_volume[i] / 100;
        info->_qtyAsk[i] = tmp > high2 ? high2 : tmp;
    }

    //成交总量
    tmp = snapshot->total_volume_trade / 100;
    info->_qtyTot = tmp > high2 ? high2 : tmp;

    //申买量，10档
    for (i = 0; i < 10; i++)
    {
        tmp = snapshot->bid_volume[i] / 100;
        info->_qtyBid[i] = tmp > high2 ? high2 : tmp;
    }

    //委托买入总量
    tmp = snapshot->total_bid_volume / 100;
    info->_qtyTotBid = tmp > high2 ? high2 : tmp;

    //成交总笔数
    tmp = snapshot->num_trades;
    info->_ctTot = tmp > high3 ? high3 : tmp;   //上限int的上限

    //成交总额
    info->_cnyTot = (double)snapshot->total_value_trade / 100000;    //华锐保留了5位小数，我直接算出小数存入double
    if (info->_cnyTot > 100000000000000000)
    {
        info->_cnyTot = 100000000000000000;
    }

    //当前品种(交易)状态
    strcpy(info->_sta, snapshot->trading_phase_code);

    //交易归属日期
    info->_dt = snapshot->orig_time / 1000000000; //除以10^9，只剩下年月日

    return;
}

void MarketDataSpi::AnalyseTickOrder(const amd::ama::MDTickOrder* ord, OrdAndExeInfo* ordAndExe)
{
    int32_t mkSH = amd::ama::MarketType::kSSE;
    int32_t mkSZ = amd::ama::MarketType::kSZSE;

    //order_price的上限
    uint32_t high1 = 999999999;
    int64_t tmp;

    //股票代码
    sscanf(ord->security_code, "%u", &ordAndExe->_sc);
    //上交所，代码前加100
    if (ord->market_type == mkSH)
    {
        ordAndExe->_sc += 100000000;
    }
    else if (ord->market_type == mkSZ)//深交所，代码前加200
    {
        ordAndExe->_sc += 200000000;
    }

    ordAndExe->_tm = ord->order_time % 1000000000;//时间

    tmp = ord->order_price / 1000;  //华锐保留6位小数，我保留3位小数
    ordAndExe->_px = tmp > high1 ? high1 : tmp; 

    ordAndExe->_qty = ord->order_volume / 100;  //华锐保留2位小数，我不保留小数位

    ordAndExe->_ch = ord->channel_no;

    //根据市场类型不同，来决定一些属性
    if (ord->market_type == mkSH)   //上海
    {
        ordAndExe->_id = ord->biz_index;

        //方向，买还是卖
        if (ord->side == 'B')
        {
            ordAndExe->_BTag = ord->orig_order_no;
            ordAndExe->_STag = 0;

            ordAndExe->_BSFlag = 'B';
        }
        else if (ord->side == 'S')
        {
            ordAndExe->_BTag = 0;
            ordAndExe->_STag = ord->orig_order_no;

            ordAndExe->_BSFlag = 'S';
        }
        else
        {
            ordAndExe->_BTag = ord->orig_order_no;
            ordAndExe->_STag = ord->orig_order_no;

            ordAndExe->_BSFlag = 'N';
        }

        //上海的挂单还是撤单
        if (ord->order_type == 'A')
        {
            ordAndExe->_ADFlag = 'A';
        }
        else if (ord->order_type == 'D')
        {
            ordAndExe->_ADFlag = 'D';
        }

        //市价，限价，本方最优
        ordAndExe->_MLFlag = 'B';

    }
    else if (ord->market_type == mkSZ)   //深圳
    {
        ordAndExe->_id = ord->appl_seq_num;

        //方向
        if (ord->side == '1')
        {
            //买
            ordAndExe->_BTag = ord->appl_seq_num;
            ordAndExe->_STag = 0;

            ordAndExe->_BSFlag = 'B';
        }
        else if (ord->side == '2')
        {
            //卖
            ordAndExe->_BTag = 0;
            ordAndExe->_STag = ord->appl_seq_num;

            ordAndExe->_BSFlag = 'S';
        }
        else
        {
            ordAndExe->_BTag = ord->appl_seq_num;
            ordAndExe->_STag = ord->appl_seq_num;

            ordAndExe->_BSFlag = 'N';
        }

        //深圳的tickOrder被认为是挂单增加委托
        ordAndExe->_ADFlag = 'A';

        //市价，限价，本方最优
        if (ord->order_type == '1')
        {
            ordAndExe->_MLFlag = 'M';
        }
        else if (ord->order_type == '2')
        {
            ordAndExe->_MLFlag = 'L';
        }
        else if (ord->order_type == 'U')
        {
            ordAndExe->_MLFlag = 'U';
        }
    }

    //日期
    //ordAndExe->_dt = ord->order_time / 1000000000;  //除以10^9，只剩下年月日

    return;
}

//把tickExecution分析转化为OrdAndExeInfo
void MarketDataSpi::AnalyseTicExecution(const amd::ama::MDTickExecution* execu, OrdAndExeInfo* info)
{
    int32_t mkSH = amd::ama::MarketType::kSSE;
    int32_t mkSZ = amd::ama::MarketType::kSZSE;

    //代码
    sscanf(execu->security_code, "%u", &info->_sc);
    //代码前加100/200，分辨交易所
    if (execu->market_type == mkSH)
    {
        info->_sc += 100000000;
    }
    else if (execu->market_type == mkSZ)
    {
        info->_sc += 200000000;
    }

    info->_tm = execu->exec_time % 1000000000;

    info->_px = execu->exec_price / 1000;
    info->_qty = execu->exec_volume / 100;
    info->_ch = execu->channel_no;

    //根据不同的市场类型，决定某些变量的值
    if (execu->market_type == mkSH)
    {
        //上海
        //id
        info->_id = execu->biz_index;

        //Tag
        info->_BTag = execu->bid_appl_seq_num;
        info->_STag = execu->offer_appl_seq_num;

        //方向，买还是卖
        if (execu->side == 'B')
        {
            //买
            info->_BSFlag = 'B';
        }
        else if (execu->side == 'S')
        {
            //卖
            info->_BSFlag = 'S';
        }
        else
        {
            info->_BSFlag = 'N';
        }

        //ADFlag
        info->_ADFlag = 'F';

        //MLFlag
        info->_MLFlag = 'A';
    }
    else if (execu->market_type == mkSZ)
    {
        //深圳
        //id
        info->_id = execu->appl_seq_num;

        //Tag
        info->_BTag = execu->bid_appl_seq_num;
        info->_STag = execu->offer_appl_seq_num;

        //方向，买还是卖
        if (info->_BTag > info->_STag)
        {
            //买
            info->_BSFlag = 'B';
        }
        else
        {
            //卖
            info->_BSFlag = 'S';
        }
   
        //ADFlag
        if (execu->exec_type == 'F')
        {
            info->_ADFlag = 'F';
        }
        else if (execu->exec_type == '4')
        {
            info->_ADFlag = 'D';
        }

        //MLFlag
        info->_MLFlag = 'C';
    }

    //日期
    //info->_dt = execu->exec_time / 1000000000;  //除以10^9，只剩下年月日
    //info->_dt = 99999999;
    return;
}

//把OrderBook转化为QuickSnapInfo
void MarketDataSpi::AnalyseOrderBook(const amd::ama::MDOrderBook& orderBook, QuickSnapInfo* info)
{
    //证券代码
    sscanf(orderBook.security_code, "%u", &info->_sc);
    //上交所，添加100
    if (orderBook.market_type == amd::ama::MarketType::kSSE)
    {
        info->_sc += 100000000;
    }
    else if (orderBook.market_type == amd::ama::MarketType::kSZSE)
    {
        //深交所，加200
        info->_sc += 200000000;
    }

    //时间，处理一下去掉日期
    info->_tm = orderBook.last_tick_time % 1000000000;

    int64_t price3 = 1000;//华锐的保留6位小数，我再除以1000保留3位小数
    int64_t qty2 = 100;

    //买委托
    if (orderBook.bid_order_book.size() >= 1)
    {
        //printf("bid_order_book[0]， ");
        info->_pxBid01 = orderBook.bid_order_book[0].price / price3;
        info->_volBid01 = orderBook.bid_order_book[0].volume / qty2;

        if (orderBook.bid_order_book.size() >= 2)
        {
            //printf("bid_order_book[1]， ");
            info->_pxBid02 = orderBook.bid_order_book[1].price / price3;
            info->_volBid02 = orderBook.bid_order_book[1].volume / qty2;
        }
    }

    //卖委托
    if (orderBook.offer_order_book.size() >= 1)
    {
        //printf("offer_order_book[0], ");
        info->_pxAsk01 = orderBook.offer_order_book[0].price / price3;
        info->_volAsk01 = orderBook.offer_order_book[0].volume / qty2;

        if (orderBook.offer_order_book.size() >= 2)
        {
            //printf("offer_order_book[1], ");
            info->_pxAsk02 = orderBook.offer_order_book[1].price / price3;
            info->_volAsk02 = orderBook.offer_order_book[1].volume / qty2;
        }
    }
    //printf("\n");

    info->_ctTot = orderBook.total_num_trades;        // 基于委托簿演算的成交总笔数
    info->_qtyTot = orderBook.total_volume_trade / 100;     // 基于委托簿演算的成交总量，华锐的保留了两位小数
    info->_cnyTot = (double)orderBook.total_value_trade / 100000;   // 基于委托簿演算的成交总金额，华锐的保留了5位小数
    info->_pxLast = orderBook.last_price / price3;       // 基于委托簿演算的最新价(华锐的原数据类型:Price6)


    return;
}

//添加应用类对象
void MarketDataSpi::AddApplication(MDApplication* app)
{
    _appObjects.push_back(app);
}