#pragma once

#include <vector>
#include <functional>
#include <array>
#include <thread>
#include <condition_variable>
#include <map>
#include "ama.h"
#include "sp_type.h"

class VirtualMarket
{
public:
	//构造函数，读取配置信息，打开数据文件，初始化回调函数
	VirtualMarket();
	
	//析构函数，释放资源
	~VirtualMarket();

	//初始化
	void Init();

	//设置回调函数
	void SetSnapshotCallback(const SnapshotFunc& snapshotCallback);
	//void SetTickOrderCallback(const TickOrderFunc& tickOrderCallback);
	//void SetTickExecutionCallback(const TickExecutionFunc& tickExecutionCallback);
	void SetOrdAndExecuCallback(const OrderAndExecuFunc& ordAndExeCallback);
	//void SetOrderBookCallback(const OrderBookFunc& orderBookCallback);
	void SetQuickSnapCallback(const QuickSnapFunc& func);

	//播放
	void Play();

private:
	//初始化snapshot文件和数据
	void InitSnapshot();
	//初始化tickOrder文件和数据
	//void InitTickOrder();
	//初始化tickExecution文件和数据
	//void InitTickExecution();
	// 
	void InitOrdAndExecu();
	//初始化orderBook文件和数据
	void InitOrderBook();

	//提取snapshot数据的线程
	void ThreadFuncSnapshot();
	//提取tickOrder数据的线程
	//void ThreadFuncTickOrder();
	//提取tickExecution数据的线程
	//void ThreadFuncTickExecution();
	// 
	void ThreadFuncOrdAndExe();
	//提取orderBook数据的线程，有多个，需要区分,传入的是orderBook文件指针数组(_orderBookFiles)的下标
	void ThreadFuncOrderBook(int32_t idx);

	//第一次读数据
	//void FirstGetData();
	/*更新数据。先清空vector，再把属于同一次回调的一组数据，放入数组，并记录它们的回调次序。
	如果没有数据了，把_callbackCnt置为0*/
	void UpdateSnapshotData();
	//void UpdateTickOrderData();
	//void UpdateTickExecutionData();
	void UpdateOrdAndExeData();
	void UpdateOrderBookData(size_t idx);

	//读出一条数据，通过参数传出，没读到数据返回false
	bool ReadOneSnapshot(SnapshotInfo& snapshot, uint32_t& cnt);
	//bool ReadOneTickOrder(MDTickOrderSt& tickOrder, uint32_t& cnt);
	//bool ReadOneTickExecution(MDTickExecutionSt& tickExecu, uint32_t& cnt);

	bool ReadOneOrdAndExe(OrdAndExeInfo& info, uint32_t& cnt);
	//bool ReadOneOrderBook(FILE* fileOrderBook, MDOrderBookSt& orderBook, uint32_t& cnt);
	bool ReadOneOrderBook(FILE* _orderBookFile, QuickSnapInfo& orderBook, uint32_t& cnt);
	
	//获得非0的最小的回调次数
	uint32_t MinCallbackCnt();

	//从文件中读字符串，读到分隔符sep为止，并且下次再读文件是从分隔符之后开始
	void FGet_str(FILE* fp, char* str, char sep);

private:
	//配置信息
	//ConfigFile _cfg;

	//要播放的文件所在目录
	std::string _filePath;

	//委托簿的线程总个数
	int32_t _threadNumOrderBook;

	//要播放的文件日期，根据这个寻找文件名
	std::string _date;

	//某类数据是否要播放
	bool _playSnapshot;
	//bool _playTickOrder;
	//bool _playTickExecution;
	bool _playOrdAndExecu;
	bool _playOrderBook;

	//文件是数据来源
	FILE* _snapshotFile;
	//FILE* _tickOrderFile;
	//FILE* _tickExecutionFile;
	FILE* _ordAndExecuFile;
	std::vector<FILE*> _orderBookFiles;

	//不同数据类型传给不同的回调函数
	SnapshotFunc _onSnapshot;
	OrderAndExecuFunc _onOrdAndExecu;
	QuickSnapFunc _onQuickSnap;

	//多个线程共用的，存储每个线程对应的，从文件读出的数据的回调次数
	//std::map<std::thread::id, uint32_t> _threadCallbackCntMap;
	//正在等待唤醒的播放数据线程个数
	uint32_t _waitThreadNum;
	//正在运行的播放数据的线程个数
	uint32_t _runThreadNum;
	//锁，同步对_runThreadNum的修改
	std::mutex _mutexThreadNum;

	//条件变量
	std::condition_variable _condVal;
	//条件变量用的锁
	std::mutex _cvMutex;

	/*用array数组存储读到的所有数据，开辟在堆空间，一开始都是默认初始化，
	之后，慢慢赋值*/
	std::array<SnapshotInfo, ARRAY_LENGTH_SNAPSHOT>* _pArraySnapshot;
	//std::array<MDTickOrderSt, ARRAY_LENGTH_TICKORDER>* _pArrayTickOrder;
	//std::array<MDTickExecutionSt, ARRAY_LENGTH_TICKEXECUTION>* _pArrayTickExecution;
	//std::array<OrdAndExeInfo, ARRAY_LENGTH_ORDANDEXECU>* _pArrayOrdAndExecu;
	std::vector<OrdAndExeInfo>* _pArrayOrdAndExecu;
	//std::vector<std::array<MDOrderBookSt, ARRAY_LENGTH_QUICKSNAP>*> _ptrsArrayOrderBook;
	std::vector<std::array<QuickSnapInfo, ARRAY_LENGTH_QUICKSNAP>*> _ptrsArrayOrderBook;

	/*下一个该写数据的数组元素下标*/
	size_t _writeIndexSnapshot;
	//size_t _writeIndexTickOrder;
	//size_t _writeIndexTickExecution;
	size_t _writeIndexOrdAndExecu;
	std::vector<size_t> _writeIndexesOrderBook;

	/*当前更新好的，这一批数据，在数组中从哪里开始，有多少个*/
	size_t		_dataBeginSnapshot;
	uint32_t	_dataCntSnapshot;
	//size_t		_dataBeginTickOrder;
	//uint32_t	_dataCntTickOrder;
	//size_t		_dataBeginTickExecution;
	//uint32_t	_dataCntTickExecution;
	size_t		_dataBeginOrdAndExe;
	uint32_t	_dataCntOrdAndExe;
	std::vector<size_t> _dataBeginsOrderBook;
	std::vector<uint32_t> _dataCntsOrderBook;

	//当前要播放的数据的对应回调次数
	uint32_t _callbackCntSnapshot;
	//uint32_t _callbackCntTickOrder;
	//uint32_t _callbackCntTickExecution;
	uint32_t _callbackCntOrdAndExe;
	std::vector<uint32_t> _callbackCntsOrderBook;
	//最小的回调次数
	uint32_t _minCallbackCnt;

	/*读到下一次回调的数据了，临时存一下回调次序，下一次更新时用
	如果读到文件末尾了，就把它置为0，下次更新时作为参考标志*/
	uint32_t _tempCallbackCntSnapshot;
	//uint32_t _tempCallbackCntTickOrder;
	//uint32_t _tempCallbackCntTickExecution;
	uint32_t _tempCallbackCntOrdAndExe;
	std::vector<uint32_t> _tempCallbackCntsOrderBook;
};

