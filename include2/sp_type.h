#pragma once
#include <functional>
//#include "ama.h"

#define ARRAY_LENGTH_SNAPSHOT		40000000
#define ARRAY_LENGTH_ORDANDEXECU	150000000
#define ARRAY_LENGTH_QUICKSNAP		30000000

#define SNAPSHOT_FILE_NAME "1MD"
#define ORDANDEXECU_FILE_NAME "23MD"
#define QUICKSNAP_FILE_NAME "4MD"

//压缩时设置一个最大压缩单位，数据太大时，分片压缩
#define COMPRESS_CHUNK_SIZE 1900000000 // 19亿
//#define CHUNK_MARK "CHUNK"

struct SnapshotInfo
{
	uint32_t _sc;			//证券代码
	uint32_t _tm;			//交易发生时间 hhmmssmmm
	uint32_t _pxIopv;		//ETF净值估计
	uint32_t _pxHigh;		//最高价
	uint32_t _pxAvgAsk;		//加权平均委卖价格

	uint32_t _pxAsk[10];	//委卖价 申卖价
	uint32_t _pxLast;		//最新成交价
	uint32_t _pxBid[10];	//委买价 申买价

	uint32_t _pxAvgBid;		//加权平均委买价格
	uint32_t _pxLow;		//最低价
	uint64_t _qtyTotAsk;	//委托卖出总量

	uint64_t _qtyAsk[10];	//委卖量 申卖量
	uint64_t _qtyTot;		//成交总量
	uint64_t _qtyBid[10];	//委买量 申买量

	uint64_t _qtyTotBid;	//委托买入总量

	//uint32_t _ctAsk[10];	//委卖笔数
	uint32_t _ctTot;		//成交总笔数
	//uint32_t _ctBid[10];	//委买笔数

	double	 _cnyTot;		//成交总额
	char	 _sta[8];		//当前品种（交易）状态
	uint32_t _dt;			//交易归属日期

	uint32_t _numCb;		//回调次数
	uint32_t _seq;			//本次回调中的第几个（从1开始）
	int32_t _timLoc;		//收到数据时本地系统时间
};

struct OrdAndExeInfo
{
	uint32_t _sc;		//9位，最高位1代表上交所，2代表深交所，低6位代表证券代码
	uint32_t _tm;		//时间，没有日期，只有时分秒毫秒
	uint32_t _px;		//委托/成交价格，保留三位小数，也就是原数据乘以1000
	uint32_t _qty;		//委托/成交数量，不保留小数位
	uint16_t _ch;		//频道号
	uint32_t _id;		//业务序号（上海）/频道索引（深圳）
	uint32_t _BTag;		//主买委托的原始订单号（委托）/买方委托索引（成交）
	uint32_t _STag;		//主卖委托的原始订单号（委托）/卖方委托索引（成交）
	uint8_t _BSFlag;	//买('B')还是卖('S')
	uint8_t _ADFlag;	//增加委托（'A'）还是删除委托('D')
	uint8_t _MLFlag;	//A上海成交,B上海委托，C深圳成交，M/L/U深圳委托
	//uint32_t _dt;

	uint32_t _numCb;
	uint32_t _seq;
	int32_t _timLoc;
};

struct QuickSnapInfo
{
	uint32_t	_sc;                 // 证券代码
	uint32_t	_tm;                 // 最新逐笔生成时间
	
	//买委托簿
	uint32_t _pxBid01;
	uint64_t _volBid01;
	uint32_t _pxBid02;
	uint64_t _volBid02;
	
	//卖委托簿
	uint32_t _pxAsk01;
	uint64_t _volAsk01;
	uint32_t _pxAsk02;
	uint64_t _volAsk02;

	uint32_t _ctTot;                                               // 基于委托簿演算的成交总笔数
	uint64_t _qtyTot;                                             // 基于委托簿演算的成交总量 
	double _cnyTot;                                              // 基于委托簿演算的成交总金额
	int64_t _pxLast;

	uint32_t _cntCb;
	uint32_t _seq;
	int32_t _tmLoc;

public:
	QuickSnapInfo()
	{
		_pxBid01 = 0;
		_volBid01 = 0;
		_pxBid02 = 0;
		_volBid02 = 0;

		_pxAsk01 = 0;
		_volAsk01 = 0;
		_pxAsk02 = 0;
		_volAsk02 = 0;

		_cntCb = 0;
		_seq = 0;
		_tmLoc = 0;
	}
};

//处理数据的回调函数类型
typedef std::function<void(SnapshotInfo* snapshot, uint32_t cnt) > SnapshotFunc;
typedef std::function<void(OrdAndExeInfo* ordAndExecu, uint32_t cnt) > OrderAndExecuFunc;
typedef std::function<void(QuickSnapInfo * quickSnap, uint32_t cnt)> QuickSnapFunc;

//收到一次后就不会再变的数据
struct UnchangedMarketData
{
	//uint32_t _sc;					//证券代码(前面加了100,200)
	uint32_t _preClosePrice;		//昨收价（10^3）
	uint32_t _openPrice;			//开盘价（10^3）

	//bool _getAll;
public:
	//初始化
	UnchangedMarketData()
	{
		_preClosePrice = 0;
		_openPrice = 0;
		//_getAll = false;
	}
};

//回调函数类型
typedef std::function<void(const UnchangedMarketData& unchgData)> UnchangedMarketDataFunc;