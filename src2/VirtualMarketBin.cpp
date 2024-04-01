#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <set>
#include "VirtualMarketBin.h"
#include "ConfigFile.h"
#include "lz4.h"

using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::set;
using std::cout;

//委托簿数据的档位个数
#define ORDERBOOK_LEVEL_LENGTH 2

extern ConfigFile g_cfgFile;//其他文件的全局变量

//函数声明，实现在其他文件
extern int32_t GetLocalTm();

VirtualMarketBin::VirtualMarketBin()
{

	//回调函数初始化，给它赋一个什么都不做的匿名函数值
	_onMDSnapshot = [](SnapshotInfo* snapshot, uint32_t cnt) {};
	//_onMDTickOrder = [](amd::ama::MDTickOrder* ticks, uint32_t cnt) {};
	//_onMDTickExecution = [](amd::ama::MDTickExecution* tick, uint32_t cnt) {};
	_onOrdAndExecu = [](OrdAndExeInfo* tick, uint32_t cnt) {};
	//_onMDOrderBook = [](std::vector<amd::ama::MDOrderBook>& order_book) {};
	_onQuickSnap = [](QuickSnapInfo* info, uint32_t cnt) {};

	//初始化一下flag，默认不播放任何数据
	_playSnapshot = false;
	//_playTickOrder = false;
	//_playTickExecution = false;
	_playOrdAndExecu = false;
	//_playOrderBook = false;
	_playQuickSnap = false;

	//play时会不断判断最小回调次数，为0的不参与判断
	_callbackCntSnapshot = 0;
	//_callbackCntTickOrder = 0;
	//_callbackCntTickExecution = 0;
	_callbackCntOrdAndExe = 0;
	_callbackCntsOrderBook.clear();

	_pArraySnapshot = NULL;
	//_pArrayTickOrder = NULL;
	//_pArrayTickExecution = NULL;
	_pArrayOrdAndExecu = NULL;
	_arrsOrderBook.clear();
}

//初始化
void VirtualMarketBin::Init()
{
	//行情数据文件所在目录
	_filePath = g_cfgFile.Get("VirtualMarketBinFilePath");
	if (_filePath == string())
	{
		cerr << " VirtualMarketBinFilePath not found in config file" << endl;
		exit(1);
	}

	//要播放的文件的日期
	_date = g_cfgFile.Get("playFileDate");
	if (_date == string())
	{
		cerr << " playFileDate not found in config file" << endl;
		exit(1);
	}

	//压缩等级
	string level = g_cfgFile.Get("compressLevel");
	if (level == string())
	{
		cerr << "VirtualMarketBin::Init() compressLevel not found in config file" << endl;
	}
	else
	{
		_compressLevel = std::stoi(level);
		printf("VirtualMarketBin::Init(): compressLevel = %d\n", _compressLevel);
	}

	string needPlay("1");
	//根据配置文件的信息，判断某类数据要不要播放
	if (g_cfgFile.Get("playSnapshot") == needPlay)
	{
		_playSnapshot = true;
		InitSnapshot();
	}
	else
	{
		//析构的时候遇到NULL指针不再执行fclose和delete
		_snapshotFile = NULL;
		_pArraySnapshot = NULL;
	}

	/*if (g_cfgFile.Get("playTickOrder") == needPlay)
	{
		_playTickOrder = true;
		InitTickOrder();
	}
	else
	{
		_tickOrderFile = NULL;
		_pArrayTickOrder = NULL;
	}*/

	/*if (g_cfgFile.Get("playTickExecution") == needPlay)
	{
		_playTickExecution = true;
		InitTickExecution();
	}
	else
	{
		_tickExecutionFile = NULL;
		_pArrayTickExecution = NULL;
	}*/

	if (g_cfgFile.Get("playOrdAndExecu") == needPlay)
	{
		_playOrdAndExecu = true;
		InitOrdAndExecu();
	}
	else
	{
		_ordAndExeFile = NULL;
		_pArrayOrdAndExecu = NULL;
	}

	if (g_cfgFile.Get("playQuickSnap") == needPlay)
	{
		_playQuickSnap = true;
		InitOrderBook();
	}
	else
	{
		_orderBookFiles.clear();
		_arrsOrderBook.clear();
		_threadNumOrderBook = 0;//也是为了保证不创建播放orderBook的线程
	}

	return;
}

VirtualMarketBin::~VirtualMarketBin()
{
	if (_snapshotFile != NULL)
	{
		fclose(_snapshotFile);
		_snapshotFile == NULL;
	}

	/*if (_tickOrderFile != NULL)
	{
		fclose(_tickOrderFile);
		_tickOrderFile == NULL;
	}*/

	/*if (_tickExecutionFile != NULL)
	{
		fclose(_tickExecutionFile);
		_tickExecutionFile == NULL;
	}*/

	if (_ordAndExeFile != NULL)
	{
		fclose(_ordAndExeFile);
		_ordAndExeFile == NULL;
	}


	for (FILE* fp : _orderBookFiles)
	{
		if (fp != NULL)
		{
			fclose(fp);
		}
	}

	if (_pArraySnapshot != NULL)
	{
		delete _pArraySnapshot;
	}

	//if (_pArrayTickOrder != NULL)
	//{
	//	delete _pArrayTickOrder;
	//	//free(_pArrayTickOrder);
	//}

	/*if (_pArrayTickExecution != NULL)
	{
		delete _pArrayTickExecution;
	}*/

	if (_pArrayOrdAndExecu != NULL)
	{
		delete _pArrayOrdAndExecu;
	}

	//如果数组里没有元素，自然不会进入循环
	for (size_t i = 0; i < _arrsOrderBook.size(); i++)
	{
		delete _arrsOrderBook[i];
	}

}

void VirtualMarketBin::InitSnapshot()
{
	string snapshotFile = _filePath + "/" + SNAPSHOT_FILE_NAME + _date + ".bin";

	cout << "playFileName: " << snapshotFile << endl;

	_snapshotFile = fopen(snapshotFile.c_str(), "r");
	if (_snapshotFile == NULL)//文件不存在
	{
		cerr << "VirtualMarketBin::VirutalMarket():" << snapshotFile << " not found" << endl;
		exit(1);
	}

	//获得文件大小
	//long fileSize = GetFileSize(_snapshotFile);
	//printf("fileSize = %ld\n", fileSize);

	size_t length;
	size_t ret;

	//读出数组长度
	ret = fread(&length, sizeof(length), 1, _snapshotFile);
	if (ret == 0)//空文件
	{
		cerr << "snapshot file is empty" << endl;
		exit(1);
	}

	printf("snapshot data length = %lu\n", length);
	_lengthSnapshotArray = length;

	//给数组申请空间
	_pArraySnapshot = new vector<SnapshotInfo>;
	_pArraySnapshot->resize(length);

	//如果不需要解压缩
	if (_compressLevel == 0)
	{
		//读入数据
		ret = fread(&(_pArraySnapshot->at(0)), sizeof(SnapshotInfo), length, _snapshotFile);
		if (ret == 0)
		{
			perror("fread _snapshotFile");
			exit(1);
		}
		else
		{
			printf("fread snapshot data, ret = %lu\n", ret);
			if (ret < length)
			{
				printf("fread snapshot data not enough\n");
				printf("read data bytes = %lu\n", ret * sizeof(SnapshotInfo));
				_lengthSnapshotArray = ret;
			}

			//看看读完的数据对不对
			/*printf("fisrt data: mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
				_pArraySnapshot->at(0)._snapshot.market_type,
				_pArraySnapshot->at(0)._snapshot.security_code,
				_pArraySnapshot->at(0)._callbackCnt,
				_pArraySnapshot->at(0)._sequence);*/
		}
	}
	else//需要解压缩
	{
		//要解压缩的数据长度
		/*size_t srcLen = fileSize - sizeof(length);
		char* srcBuf = (char*)malloc(srcLen);
		if (srcBuf == NULL)
		{
			perror("malloc srcBuf");
			exit(1);
		}*/
		//把文件中需要解压缩的数据读出来
		/*ret = fread(srcBuf, 1, srcLen, _snapshotFile);
		if (ret < srcLen)
		{
			printf("fread snapshot file data not enough, ret = %lu\n", ret);
			exit(1);
		}*/

		//数据解压缩到数组
		//UncompressData(srcBuf, (char*)&(_pArraySnapshot->at(0)), srcLen, sizeof(SnapshotInfo) * length);

		DecompressFileData(_snapshotFile, (char*)&(_pArraySnapshot->at(0)), sizeof(SnapshotInfo) * length);
	}

	return;
}

//void VirtualMarketBin::InitTickOrder()
//{
//	string tickOrderFile = _filePath + "/" + TICKORDER_FILE_NAME + _date + ".bin";
//
//	cout << "playFileName: " << tickOrderFile << endl;
//
//	_tickOrderFile = fopen(tickOrderFile.c_str(), "rb");
//	if (_tickOrderFile == NULL)//文件不存在
//	{
//		cerr << tickOrderFile << " not found" << endl;
//		exit(1);
//	}
//
//	//获得文件大小
//	/*long fileSize;
//	fileSize = GetFileSize(_tickOrderFile);*/
//	
//	size_t ret;
//	//读出数组长度
//	ret = fread(&_lengthTickOrderArray, sizeof(_lengthTickOrderArray), 1, _tickOrderFile);
//	if (ret == 0)
//	{
//		perror("fread tickOrderFile");
//		exit(1);
//	}
//	else
//	{
//		printf("tickOrder data length = %lu\n", _lengthTickOrderArray);
//	}
//
//	//申请空间创建数组
//	_pArrayTickOrder = new std::vector<TickOrderInfo>;
//	_pArrayTickOrder->resize(_lengthTickOrderArray);
//
//
//	//记录开始时间
//	//ftime(&_startTim);
//	//如果不需要解压缩
//	if (_compressLevel == 0)
//	{
//		ret = fread(&(_pArrayTickOrder->at(0)), sizeof(TickOrderInfo), _lengthTickOrderArray, _tickOrderFile);
//		if (ret == 0)
//		{
//			perror("fread pArrayTickOrder");
//			exit(1);
//		}
//		else
//		{
//
//			printf("fread TickOrderInfo data, ret = %u\n", ret);
//			if (ret < _lengthTickOrderArray)
//			{
//				printf("fread tickOrder data not enough\n");
//				_lengthTickOrderArray = ret;
//			}
//
//			//看看读完的数据对不对
//			/*printf("fisrt data: mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
//				_pArrayTickOrder->at(0)._tickOrder.market_type,
//				_pArrayTickOrder->at(0)._tickOrder.security_code,
//				_pArrayTickOrder->at(0)._callbackCnt,
//				_pArrayTickOrder->at(0)._sequence);*/
//		}
//	}
//	else//需要解压缩
//	{
//		//要解压缩的数据长度
//		/*size_t srcLen = fileSize - sizeof(_lengthTickOrderArray);
//		char* srcBuf = (char*)malloc(srcLen);
//		if (srcBuf == NULL)
//		{
//			perror("malloc srcBuf");
//			exit(1);
//		}*/
//		//把文件中需要解压缩的数据读出来
//		/*ret = fread(srcBuf, 1, srcLen, _tickOrderFile);
//		if (ret < srcLen)
//		{
//			printf("fread tickOrder file data not enough, ret = %lu\n", ret);
//			exit(1);
//		}*/
//
//		//数据解压缩到数组
//		//UncompressData(srcBuf, (char*)&(_pArrayTickOrder->at(0)), srcLen, sizeof(TickOrderInfo) * _lengthTickOrderArray);
//
//		//文件的数据解压缩到内存空间
//		DecompressFileData(_tickOrderFile, (char*)&(_pArrayTickOrder->at(0)), sizeof(TickOrderInfo) * _lengthTickOrderArray);
//	}
//
//	
//
//	//计算时间间隔
//	//ftime(&_endTim);
//	//_timInterval = GetTimeInterval(_startTim, _endTim);
//	//printf("read file time = %ld\n", _timInterval);
//	
//
//	//_writeIndexTickOrder = 0;
//
//	//_curIdxTickOrder = 0;
//
//	return;
//}

//分片解压缩文件，验证CHUNK_MARK，读出数据长度，解压缩
//int VirtualMarketBin::UncompressData(Bytef* dest, uLongf* destLen, const Bytef* source, uLong sourceLen)
//{
//	ulong chunkSrcLen;//一段待解压的数据长度
//
//	Bytef* chunkDestPos = dest;//解压缩后的数据放入的位置
//	ulong chunkDestLen;//解压缩后要存的数据长度
//	ulong uncompreTotalLen = 0;//记录解压缩的数据总长度
//	int ret;
//
//	for (ulong i = 0; i < sourceLen;)
//	{
//		//printf("i = %lu\n", i);
//
//		if (strcmp(CHUNK_MARK, (const char*)(source + i)) != 0)
//		{
//			printf("VirtualMarketBin::UncompressData：CHUNK_MARK not right\n");
//			break;
//		}
//
//		//i后移
//		i += sizeof(CHUNK_MARK);
//
//		//取得被压缩数据长度
//		memcpy(&chunkSrcLen, source + i, sizeof(chunkSrcLen));
//		printf("chunkSrcLen = %lu\n", chunkSrcLen);
//
//		//i后移
//		i += sizeof(chunkSrcLen);
//
//		//总的解压缩完数据长度，减去已经解出来的数据，剩下还有多少？大于等于chunk吗？
//		if (*destLen - uncompreTotalLen >= COMPRESS_CHUNK_SIZE)
//		{
//			chunkDestLen = COMPRESS_CHUNK_SIZE;
//		}
//		else
//		{
//			chunkDestLen = *destLen - uncompreTotalLen;
//		}
//		//printf("chunkDestLen = %lu\n", chunkDestLen);
//
//		//开始解压缩
//		ret = uncompress(chunkDestPos, &chunkDestLen, source + i, chunkSrcLen);
//		if (ret != Z_OK)
//		{
//			return ret;
//		}
//		//printf("after uncompress, chunkDestLen = %lu\n", chunkDestLen);
//		//记录解压缩的数据长度，更新下次解压缩的数据放置的位置
//		uncompreTotalLen += chunkDestLen;
//		chunkDestPos += chunkDestLen;
//
//		//i后移
//		i += chunkSrcLen;
//
//		//还有多少空间放解压缩后的数据
//		//chunkDestLen = *destLen - destTotalLen;
//	}
//
//	//把解压缩后数据总长度传出去
//	*destLen = uncompreTotalLen;
//
//	return Z_OK;
//}

//void VirtualMarketBin::InitTickExecution()
//{
//	string tickExecutionFile = _filePath + "/" + TICKEXECUTION_FILE_NAME + _date + ".bin";
//
//	cout << "playFileName: " << tickExecutionFile << endl;
//
//	_tickExecutionFile = fopen(tickExecutionFile.c_str(), "rb");
//	if (_tickExecutionFile == NULL)
//	{
//		perror("fopen tickExecutionFile");
//		exit(1);
//	}
//
//	//获得文件大小
//	long fileSize;
//	fileSize = GetFileSize(_tickExecutionFile);
//
//	//读出数组长度
//	size_t ret;
//	ret = fread(&_lengthTickExecutionArray, sizeof(_lengthTickExecutionArray), 1, _tickExecutionFile);
//	if (ret == 0)
//	{
//		perror("fread _lengthTickExecution");
//		exit(1);
//	}
//	else
//	{
//		printf("tickExecution data length = %lu\n", _lengthTickExecutionArray);
//	}
//
//	//开辟空间
//	_pArrayTickExecution = new vector<TickExecutionInfo>;
//	_pArrayTickExecution->resize(_lengthTickExecutionArray);
//
//	//如果不需要解压缩
//	if (_compressLevel == 0)
//	{
//		//读数据
//		ret = fread(&(_pArrayTickExecution->at(0)), sizeof(TickExecutionInfo), _lengthTickExecutionArray, _tickExecutionFile);
//		if (ret == 0)
//		{
//			perror("fread tickExecution data");
//			exit(1);
//		}
//		else
//		{
//			printf("fread tickExecution data, ret = %u\n", ret);
//			if (ret < _lengthTickExecutionArray)
//			{
//				printf("fread tickExecution data not enough\n");
//				_lengthTickExecutionArray = ret;
//			}
//
//			//看看读完的数据对不对
//			/*printf("fisrt data: mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
//				_pArrayTickExecution->at(0)._tickExetution.market_type,
//				_pArrayTickExecution->at(0)._tickExetution.security_code,
//				_pArrayTickExecution->at(0)._callbackCnt,
//				_pArrayTickExecution->at(0)._sequence);*/
//		}
//	}
//	else//需要解压缩
//	{
//		////要解压缩的数据长度
//		//size_t srcLen = fileSize - sizeof(_lengthTickExecutionArray);
//		//char* srcBuf = (char*)malloc(srcLen);
//		//if (srcBuf == NULL)
//		//{
//		//	perror("malloc srcBuf");
//		//	exit(1);
//		//}
//		////把文件中需要解压缩的数据读出来
//		//ret = fread(srcBuf, 1, srcLen, _tickExecutionFile);
//		//if (ret < srcLen)
//		//{
//		//	printf("fread snapshot file data not enough, ret = %lu\n", ret);
//		//	exit(1);
//		//}
//
//		//数据解压缩到数组
//		//UncompressData(srcBuf, (char*)&(_pArrayTickExecution->at(0)), srcLen, sizeof(TickExecutionInfo) * _lengthTickExecutionArray);
//
//		DecompressFileData(_tickExecutionFile, (char*)&(_pArrayTickExecution->at(0)), sizeof(TickExecutionInfo) * _lengthTickExecutionArray);
//	}
//
//	return;
//}

void VirtualMarketBin::InitOrdAndExecu()
{
	string fileName = _filePath + "/" + ORDANDEXECU_FILE_NAME + _date + ".bin";

	cout << "playFileName: " << fileName << endl;

	_ordAndExeFile = fopen(fileName.c_str(), "rb");
	if (_ordAndExeFile == NULL)
	{
		perror("fopen _ordAndExeFile");
		exit(1);
	}

	//获得文件大小
	/*long fileSize;
	fileSize = GetFileSize(_ordAndExeFile);*/

	//读出数组长度
	size_t ret;
	ret = fread(&_lengthOrdAndExecu, sizeof(_lengthOrdAndExecu), 1, _ordAndExeFile);
	if (ret == 0)
	{
		perror("fread _lengthOrdAndExecu");
		exit(1);
	}
	else
	{
		printf("_ordAndExe data length = %lu\n", _lengthOrdAndExecu);
	}

	//开辟空间
	_pArrayOrdAndExecu = new vector<OrdAndExeInfo>;
	_pArrayOrdAndExecu->resize(_lengthOrdAndExecu);

	//如果不需要解压缩
	if (_compressLevel == 0)
	{
		//读数据
		ret = fread(&(_pArrayOrdAndExecu->at(0)), sizeof(OrdAndExeInfo), _lengthOrdAndExecu, _ordAndExeFile);
		if (ret == 0)
		{
			perror("fread ordAndExecu data");
			exit(1);
		}
		else
		{
			printf("fread ordAndExecu data, ret = %lu\n", ret);
			if (ret < _lengthOrdAndExecu)
			{
				printf("fread ordAndExecu data not enough\n");
				_lengthOrdAndExecu = ret;
			}

			//看看读完的数据对不对
			/*printf("fisrt data: mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
				_pArrayTickExecution->at(0)._tickExetution.market_type,
				_pArrayTickExecution->at(0)._tickExetution.security_code,
				_pArrayTickExecution->at(0)._callbackCnt,
				_pArrayTickExecution->at(0)._sequence);*/
		}
	}
	else//需要解压缩
	{
		////要解压缩的数据长度
		//size_t srcLen = fileSize - sizeof(_lengthTickExecutionArray);
		//char* srcBuf = (char*)malloc(srcLen);
		//if (srcBuf == NULL)
		//{
		//	perror("malloc srcBuf");
		//	exit(1);
		//}
		////把文件中需要解压缩的数据读出来
		//ret = fread(srcBuf, 1, srcLen, _tickExecutionFile);
		//if (ret < srcLen)
		//{
		//	printf("fread snapshot file data not enough, ret = %lu\n", ret);
		//	exit(1);
		//}

		//数据解压缩到数组
		//UncompressData(srcBuf, (char*)&(_pArrayTickExecution->at(0)), srcLen, sizeof(TickExecutionInfo) * _lengthTickExecutionArray);

		DecompressFileData(_ordAndExeFile, (char*)&(_pArrayOrdAndExecu->at(0)), sizeof(OrdAndExeInfo) * _lengthOrdAndExecu);
	}

	return;
}

void VirtualMarketBin::InitOrderBook()
{
	//委托簿的线程个数
	string threadNum = g_cfgFile.Get("OrderBookThreadNum");
	if (threadNum == string())
	{
		cerr << "VirtualMarketBin::VirutalMarket(): OrderBookThreadNum not found in config file" << endl;
		exit(1);
	}
	_threadNumOrderBook = stoi(threadNum);

	string orderBookFile;
	int i;
	for (i = 1; i <= _threadNumOrderBook; i++)
	{
		orderBookFile = _filePath + "/" + QUICKSNAP_FILE_NAME + _date + "_" + std::to_string(i) + ".bin";

		cout << "playFileName: " << orderBookFile << endl;

		FILE* fp = fopen(orderBookFile.c_str(), "r");
		if (fp == NULL)
		{
			cerr << orderBookFile << "not found" << endl;
			exit(1);
		}
		//文件指针放入数组
		_orderBookFiles.emplace_back(fp);
	}

	
	size_t ret;
	size_t length;
	_lengthsOrderBookArray.resize(_threadNumOrderBook);
	_arrsOrderBook.resize(_threadNumOrderBook);

	//long fileSize;
	for (int i = 0; i < _threadNumOrderBook; i++)
	{
		//文件大小
		//fileSize = GetFileSize(_orderBookFiles[i]);

		//读出数组的长度
		ret = fread(&length, sizeof(length), 1, _orderBookFiles[i]);
		if (ret == 0)
		{
			perror("fread orderbook length");
			exit(1);
		}

		_lengthsOrderBookArray[i] = length;
		printf("_lengthsOrderBookArray[%d] = %lu\n", i, _lengthsOrderBookArray[i]);

		//开辟数组空间
		auto ptr = new std::vector<QuickSnapInfo>;
		ptr->resize(length);
		_arrsOrderBook[i] = ptr;

		//数组长度为0，没有数据，不参与播放
		if (length == 0)
		{
			//_callbackCntsOrderBook[i] = 0;
			continue;
		}


		//如果不需要解压缩
		if (_compressLevel == 0)
		{
			//读入数据
			ret = fread(&(ptr->at(0)), sizeof(QuickSnapInfo), length, _orderBookFiles[i]);
			if (ret == 0)
			{
				perror("fread orderBook data");
				exit(1);
			}
			else
			{
				printf("fread quickSnap file %d data, ret = %lu\n", i, ret);
				if (ret < length)//如果没有读到预期那么多数据
				{
					printf("fread quickSnap file data not enough");
					_lengthsOrderBookArray[i] = ret;
				}

				//看看读完的数据对不对
				/*printf("fisrt data: mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
					ptr->at(0).market_type,
					ptr->at(0).security_code,
					ptr->at(0)._callbackCnt,
					ptr->at(0)._sequence);*/

					/*printf("last data: mktyp = %d, code = %s, callbackCnt = %u, seq = %u\n",
						ptr->at(ret - 1).market_type,
						ptr->at(ret - 1).security_code,
						ptr->at(ret - 1)._callbackCnt,
						ptr->at(ret - 1)._sequence);*/
			}
		}
		else//需要解压缩
		{
			////要解压缩的数据长度
			//size_t srcLen = fileSize - sizeof(length);
			//char* srcBuf = (char*)malloc(srcLen);
			//if (srcBuf == NULL)
			//{
			//	perror("malloc srcBuf");
			//	exit(1);
			//}
			////把文件中需要解压缩的数据读出来
			//ret = fread(srcBuf, 1, srcLen, _orderBookFiles[i]);
			//if (ret < srcLen)
			//{
			//	printf("fread orderBook file data not enough, ret = %lu\n", ret);
			//	exit(1);
			//}

			//数据解压缩到数组
			//UncompressData(srcBuf, (char*)&(ptr->at(0)), srcLen, sizeof(OrderBookInfo) * length);

			//printf("sizeof(QuickSnapInfo) = %lu, lenght = %lu\n", sizeof(QuickSnapInfo), length);
			DecompressFileData(_orderBookFiles[i], (char*)&(ptr->at(0)), sizeof(QuickSnapInfo) * length);
		}
	}

	
	_curIdxesOrderBook.resize(_threadNumOrderBook);
	//扩展orderBook相关数组的长度
	_dataBeginsOrderBook.resize(_threadNumOrderBook);
	_dataCntsOrderBook.resize(_threadNumOrderBook);
	//回调次数数组设置长度
	_callbackCntsOrderBook.resize(_threadNumOrderBook);
	_tempCallbackCntsOrderBook.resize(_threadNumOrderBook);

	return;
}

void VirtualMarketBin::SetSnapshotCallback(const SnapshotFunc& snapshotCallback)
{
	_onMDSnapshot = snapshotCallback;
}

//void VirtualMarketBin::SetTickOrderCallback(const TickOrderFunc& tickOrderCallback)
//{
//	_onMDTickOrder = tickOrderCallback;
//}

//void VirtualMarketBin::SetTickExecutionCallback(const TickExecutionFunc& tickExecutionCallback)
//{
//	_onMDTickExecution = tickExecutionCallback;
//}

void VirtualMarketBin::SetOrdAndExecuCallback(const OrderAndExecuFunc& func)
{
	_onOrdAndExecu = func;
}

//void VirtualMarketBin::SetOrderBookCallback(const OrderBookFunc& orderBookCallback)
//{
//	_onMDOrderBook = orderBookCallback;
//}

void VirtualMarketBin::SetQuickSnapCallback(const QuickSnapFunc& quickSnapCallback)
{
	_onQuickSnap = quickSnapCallback;	
}

void VirtualMarketBin::Play()
{
	_runThreadNum = 0;
	_waitThreadNum = 0;
	vector<std::thread> threadVec;

	//创建提取数据触发回调的线程
	if (_playSnapshot)
	{
		threadVec.emplace_back(std::thread(&VirtualMarketBin::ThreadFuncSnapshot, this));
		_runThreadNum++;
	}

	/*if (_playTickOrder)
	{
		threadVec.emplace_back(std::thread(&VirtualMarketBin::ThreadFuncTickOrder, this));
		_runThreadNum++;
	}*/

	/*if (_playTickExecution)
	{
		threadVec.emplace_back(std::thread(&VirtualMarketBin::ThreadFuncTickExecution, this));
		_runThreadNum++;
	}*/

	if (_playOrdAndExecu)
	{
		threadVec.emplace_back(std::thread(&VirtualMarketBin::ThreadFuncOrdAndExecu, this));
		_runThreadNum++;
	}

	if (_playQuickSnap)
	{
		for (int i = 0; i < _threadNumOrderBook; i++)
		{
			threadVec.emplace_back(std::thread(&VirtualMarketBin::ThreadFuncOrderBook, this, i));
			_runThreadNum++;
		}
	}


	printf("PlayThreadNum = %u\n", _runThreadNum);

	/*一直循环直到所有线程都读完第一次数据，开始等待*/
	while (1)
	{
		if (_runThreadNum != _waitThreadNum)
		{
			continue;
		}

		//如果锁没有被释放，说明子线程里某些原子操作还未完成
		if (_cvMutex.try_lock() == false)
		{
			continue;
		}
		else
		{
			_cvMutex.unlock();
		}

		/*所有线程都在等待条件变量了*/
		//cout << "all waiting" << endl;

		//更新最小回调次序
		_minCallbackCnt = MinCallbackCnt();

		//唤醒所有线程
		_condVal.notify_all();

		//只需要唤醒一次
		break;

		//sleep(1);
	}

	//等待所有线程结束
	for (size_t i = 0; i < threadVec.size(); i++)
	{
		threadVec[i].join();
		printf("thread joined\n");
	}

	printf("all thread over\n");

	return;
}

//提取snapshot数据的线程
void VirtualMarketBin::ThreadFuncSnapshot()
{
	//要传递数据的起始位置和数据个数
	_dataBeginSnapshot = 0;
	_dataCntSnapshot = 0;

	//bool ret;
	uint32_t cbCnt;
	//读取第一批snapshot数据
	//第一行数据
	//ret = ReadOneSnapshot(_pArraySnapshot->at(_writeIndexSnapshot), _callbackCntSnapshot);
	//数组是空，或者读出来的数据个数是空，以后不参与循环
	if (_lengthSnapshotArray == 0)
	{
		cerr << "snapshot array has no data" << endl;
		_callbackCntSnapshot = 0;
	}
	else
	{
		_curIdxSnapshot = 0;
		++_dataCntSnapshot;
		//第一个数据的回调次数
		_callbackCntSnapshot = _pArraySnapshot->at(_curIdxSnapshot)._numCb;

		/*printf("fisrt data: mktyp = %d, code = %s, time = %ld, callbackCnt = %u, seq = %u\n",
			_pArraySnapshot->at(_curIdxSnapshot)._snapshot.market_type,
			_pArraySnapshot->at(_curIdxSnapshot)._snapshot.security_code,
			_pArraySnapshot->at(_curIdxSnapshot)._snapshot.orig_time,
			_pArraySnapshot->at(_curIdxSnapshot)._callbackCnt,
			_pArraySnapshot->at(_curIdxSnapshot)._sequence);*/

		//读文件，一直到callbackCnt不同了，或者没数据了
		while (1)
		{
			++_curIdxSnapshot;//遍历到下一个数据
			//没数据了
			if (_curIdxSnapshot == _lengthSnapshotArray)
			{
				_tempCallbackCntSnapshot = 0;
				break;
			}

			//当前数据回调次数
			cbCnt = _pArraySnapshot->at(_curIdxSnapshot)._numCb;

			//读到的数据属于下次回调的数据
			if (cbCnt != _callbackCntSnapshot)
			{
				//缓存记录下来
				_tempCallbackCntSnapshot = cbCnt;
				break;
			}

			//读到数据且是同一批回调的数据
			++_dataCntSnapshot;
		}
	}

	//等待条件变量，等待被唤醒
	std::unique_lock<std::mutex> lock(_cvMutex);
	_waitThreadNum++;	//增加等待线程个数
	_condVal.wait(lock);
	//被唤醒了
	_waitThreadNum--;	//减少等待线程个数
	lock.unlock();//手动解锁


	//如果_callbackCnt被置为0，代表没有数据了，结束线程
	while (_callbackCntSnapshot != 0)
	{
		//检查自己是不是最小的回调次序，是就触发回调函数
		if (_callbackCntSnapshot == _minCallbackCnt)
		{
			//printf("_callbackCntSnapshot = %u\n", _callbackCntSnapshot);

			//把结构体提取出来
			/*vector<MDSnapshotSt> vec;
			for (size_t idx = _dataBeginSnapshot; idx < _dataBeginSnapshot + _dataCntSnapshot; ++idx)
			{
				vec.emplace_back(_pArraySnapshot->at(idx)._snapshot);
			}*/

			_onMDSnapshot(&(_pArraySnapshot->at(_dataBeginSnapshot)), _dataCntSnapshot);

			//更新下一组数据和回调次数
			UpdateSnapshotData();
			
		}
		//没有轮到此线程触发回调，继续下轮等待
	}

	printf("threadSnapshot end\n");

	_mutexThreadNum.lock();
	_runThreadNum--;
	_mutexThreadNum.unlock();

	return;
}

//void VirtualMarketBin::ThreadFuncTickOrder()
//{
//	//记录开始时间
//	//ftime(&_startTim);
//
//	//要提取的数据在数组中的起始位置和数据个数
//	_dataBeginTickOrder = 0;
//	_dataCntTickOrder = 0;
//
//	bool ret;
//	uint32_t cbCnt;
//	//读取第一批tickOrder数据
//	if (_lengthTickOrderArray == 0)//如果数组里没有数据
//	{
//		//cerr << "tickorder file has head but has no data" << endl;
//		_callbackCntTickOrder = 0;
//	}
//	else
//	{
//		//第一行数据
//		_curIdxTickOrder = 0;
//		/*printf("first data: mkt = %d, code = %s, cbCnt = %u\n",
//			_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.market_type,
//			_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.security_code,
//			_pArrayTickOrder->at(_curIdxTickOrder)._callbackCnt);*/
//
//		//_callbackCntTickOrder = _pArrayTickOrder[_curIdxTickOrder]._callbackCnt;
//		_callbackCntTickOrder = _pArrayTickOrder->at(_curIdxTickOrder)._callbackCnt;
//		++_dataCntTickOrder;
//		//遍历数据，一直到callbackCnt不同了，或者没数据了
//		while (1)
//		{
//			//遍历数组
//			++_curIdxTickOrder;
//
//			//printf("mkt = %d, code = %s, cbCnt = %u\n",
//			//	//_pArrayTickOrder[_curIdxTickOrder]._tickOrder.market_type,
//			//	_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.market_type,
//			//	_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.security_code,
//			//	_pArrayTickOrder->at(_curIdxTickOrder)._callbackCnt);
//
//			//没数据了
//			if (_curIdxTickOrder == _lengthTickOrderArray)
//			{
//				_tempCallbackCntTickOrder = 0;
//				break;
//			}
//			
//			//cbCnt = _pArrayTickOrder[_curIdxTickOrder]._callbackCnt;
//			cbCnt = _pArrayTickOrder->at(_curIdxTickOrder)._callbackCnt;
//			//读到数据属于下次回调的数据
//			if (cbCnt != _callbackCntTickOrder)
//			{
//				//缓存记录下来
//				_tempCallbackCntTickOrder = cbCnt;
//				break;
//			}
//
//			//读到数据且是同一批回调的数据
//			++_dataCntTickOrder;
//		}
//	}
//
//	//等待条件变量，等待被唤醒
//	std::unique_lock<std::mutex> lock(_cvMutex);
//	_waitThreadNum++;	//增加等待线程个数
//	_condVal.wait(lock);
//
//	_waitThreadNum--;	//减少等待线程个数
//	lock.unlock();//手动解锁
//
//	//如果_callbackCnt被置为0，代表没有数据了，结束线程
//	while (_callbackCntTickOrder != 0)
//	{
//		//检查自己是不是最小的回调次序，是就触发回调函数
//		if (_callbackCntTickOrder == _minCallbackCnt)
//		{
//			//printf("_callbackCntTickOrder = %u\n", _callbackCntTickOrder);
//
//			//调用回调函数
//			vector<amd::ama::MDTickOrder> vec;
//			for (size_t i = _dataBeginTickOrder; i < _dataBeginTickOrder + _dataCntTickOrder; i++)
//			{
//				vec.emplace_back(_pArrayTickOrder->at(i)._tickOrder);
//			}
//			_onMDTickOrder(&vec[0], _dataCntTickOrder);
//			//_onMDTickOrder(&_pArrayTickOrder[_dataBeginTickOrder]._tickOrder, _dataCntTickOrder);
//
//			//更新下一组数据和回调次数
//			UpdateTickOrderData();
//			
//		}
//		//如果没有触发，继续下轮等待
//	}
//
//	//结束时间
//	//ftime(&_endTim);
//	//时间间隔
//	//_timInterval = GetTimeInterval(_startTim, _endTim);
//	//printf("play data time = %ld\n", _timInterval);
//
//	printf("threadTickOrder end\n");
//
//	_mutexThreadNum.lock();
//	_runThreadNum--;
//	_mutexThreadNum.unlock();
//}

//提取tickExecution数据的线程
//void VirtualMarketBin::ThreadFuncTickExecution()
//{
//	_dataBeginTickExecution = 0;
//	_dataCntTickExecution = 0;
//
//	//bool ret;
//	uint32_t cbCnt;
//	//读取第一批tickExecution数据
//	//没数据，以后不参与循环
//	if (_lengthTickExecutionArray == 0)
//	{
//		cerr << "tickExecution has no data" << endl;
//		_callbackCntTickExecution = 0;
//	}
//	else
//	{
//		//第一行数据
//		_curIdxTickExecution = 0;
//		++_dataCntTickExecution;
//		//第一个回调次数
//		_callbackCntTickExecution = _pArrayTickExecution->at(_curIdxTickExecution)._callbackCnt;
//
//		//读文件，一直到callbackCnt不同了，或者没数据了
//		while (1)
//		{
//			//遍历下一个数据
//			++_curIdxTickExecution;
//			//没数据了
//			if (_curIdxTickExecution == _lengthTickExecutionArray)
//			{
//				_tempCallbackCntTickExecution = 0;
//				break;
//			}
//
//			//读出回调次数
//			cbCnt = _pArrayTickExecution->at(_curIdxTickExecution)._callbackCnt;
//
//			//读到数据属于下次回调的数据
//			if (cbCnt != _callbackCntTickExecution)
//			{
//				//缓存记录下来
//				_tempCallbackCntTickExecution = cbCnt;
//				break;
//			}
//
//			++_dataCntTickExecution;
//		}
//	}
//
//	//等待条件变量，等待被唤醒
//	std::unique_lock<std::mutex> lock(_cvMutex);
//	_waitThreadNum++;	//增加等待线程个数
//	_condVal.wait(lock);
//
//	_waitThreadNum--;	//减少等待线程个数
//	lock.unlock();//手动解锁
//
//	//如果_callbackCnt被置为0，代表没有数据了，结束线程
//	while (_callbackCntTickExecution != 0)
//	{
//		//检查自己是不是最小的回调次序，是就触发回调函数
//		if (_callbackCntTickExecution == _minCallbackCnt)
//		{
//			//printf("_callbackCntTickExecution = %u\n", _callbackCntTickExecution);
//
//			vector<MDTickExecutionSt> vec;
//			for (size_t i = _dataBeginTickExecution; i < _dataBeginTickExecution + _dataCntTickExecution; i++)
//			{
//				vec.emplace_back(_pArrayTickExecution->at(i)._tickExetution);
//			}
//
//			_onMDTickExecution(&(vec[0]), _dataCntTickExecution);
//
//			UpdateTickExecutionData();
//		}
//		//没有轮到此线程触发回调，继续下轮等待
//	}
//
//	printf("threadTickExecution end\n");
//
//	_mutexThreadNum.lock();
//	_runThreadNum--;
//	_mutexThreadNum.unlock();
//
//	return;
//}

void VirtualMarketBin::ThreadFuncOrdAndExecu()
{
	//要提取的数据在数组中的起始位置和数据个数
	_dataBeginOrdAndExe = 0;
	_dataCntOrdAndExe = 0;

	//bool ret;
	uint32_t cbCnt;
	//读取第一批tickOrder数据
	if (_lengthOrdAndExecu == 0)//如果数组里没有数据
	{
		//cerr << "tickorder file has head but has no data" << endl;
		_callbackCntOrdAndExe = 0;
	}
	else
	{
		//第一行数据
		_curIdxOrdAndExecu = 0;
		/*printf("first data: mkt = %d, code = %s, cbCnt = %u\n",
			_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.market_type,
			_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.security_code,
			_pArrayTickOrder->at(_curIdxTickOrder)._callbackCnt);*/

			//_callbackCntTickOrder = _pArrayTickOrder[_curIdxTickOrder]._callbackCnt;
		_callbackCntOrdAndExe = _pArrayOrdAndExecu->at(_curIdxOrdAndExecu)._numCb;
		++_dataCntOrdAndExe;
		//遍历数据，一直到callbackCnt不同了，或者没数据了
		while (1)
		{
			//遍历数组
			++_curIdxOrdAndExecu;

			//printf("mkt = %d, code = %s, cbCnt = %u\n",
			//	//_pArrayTickOrder[_curIdxTickOrder]._tickOrder.market_type,
			//	_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.market_type,
			//	_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.security_code,
			//	_pArrayTickOrder->at(_curIdxTickOrder)._callbackCnt);

			//没数据了
			if (_curIdxOrdAndExecu == _lengthOrdAndExecu)
			{
				_tempCallbackCntOrdAndExe = 0;
				break;
			}

			//cbCnt = _pArrayTickOrder[_curIdxTickOrder]._callbackCnt;
			cbCnt = _pArrayOrdAndExecu->at(_curIdxOrdAndExecu)._numCb;
			//读到数据属于下次回调的数据
			if (cbCnt != _callbackCntOrdAndExe)
			{
				//缓存记录下来
				_tempCallbackCntOrdAndExe = cbCnt;
				break;
			}

			//读到数据且是同一批回调的数据
			++_dataCntOrdAndExe;
		}
	}

	//等待条件变量，等待被唤醒
	std::unique_lock<std::mutex> lock(_cvMutex);
	_waitThreadNum++;	//增加等待线程个数
	_condVal.wait(lock);

	_waitThreadNum--;	//减少等待线程个数
	lock.unlock();//手动解锁

	//如果_callbackCnt被置为0，代表没有数据了，结束线程
	while (_callbackCntOrdAndExe != 0)
	{
		//检查自己是不是最小的回调次序，是就触发回调函数
		if (_callbackCntOrdAndExe == _minCallbackCnt)
		{
			//printf("_callbackCntTickOrder = %u\n", _callbackCntTickOrder);

			//调用回调函数
			//_onMDTickOrder(&_pArrayTickOrder[_dataBeginTickOrder]._tickOrder, _dataCntTickOrder);
			_onOrdAndExecu(&_pArrayOrdAndExecu->at(_dataBeginOrdAndExe), _dataCntOrdAndExe);

			//更新下一组数据和回调次数
			//UpdateTickOrderData();
			UpDateOrdAndExecu();

		}
		//如果没有触发，继续下轮等待
	}

	//结束时间
	//ftime(&_endTim);
	//时间间隔
	//_timInterval = GetTimeInterval(_startTim, _endTim);
	//printf("play data time = %ld\n", _timInterval);

	printf("ThreadFuncOrdAndExecu end\n");

	_mutexThreadNum.lock();
	_runThreadNum--;
	_mutexThreadNum.unlock();
}

//提取orderBook数据的线程，有多个，需要区分,传入的是orderBook文件指针数组(_orderBookFiles)的下标
void VirtualMarketBin::ThreadFuncOrderBook(int32_t idx)
{
	_dataBeginsOrderBook[idx] = 0;
	_dataCntsOrderBook[idx] = 0;
	//size_t& curIdxInArray = _curIdxesOrderBook[idx];
	//auto pArr = _arrsOrderBook[idx];

	uint32_t cbCnt;
	//读取第一批OrderBook数据
	
	//没数据
	if (_lengthsOrderBookArray[idx] == 0)
	{
		printf("orderbook file %d has no data\n", idx);
		_callbackCntsOrderBook[idx] = 0;
	}
	else
	{
		//第一行数据
		_curIdxesOrderBook[idx] = 0;
		++_dataCntsOrderBook[idx];
		//回调次数
		_callbackCntsOrderBook[idx] = _arrsOrderBook[idx]->at(_curIdxesOrderBook[idx])._cntCb;
		//读文件，一直到callbackCnt不同了，或者没数据了
		while (1)
		{
			//向后遍历
			++(_curIdxesOrderBook[idx]);

			//没数据了
			if (_curIdxesOrderBook[idx] == _lengthsOrderBookArray[idx])
			{
				//printf("curIdx == length : %lu, %lu\n", _curIdxesOrderBook[idx], _lengthsOrderBookArray[idx]);
				_tempCallbackCntsOrderBook[idx] = 0;
				break;
			}
			
			//取出回调次数
			cbCnt = _arrsOrderBook[idx]->at(_curIdxesOrderBook[idx])._cntCb;
			//printf("cbCnt = %u\n", cbCnt);
			//读到数据属于下次回调的数据
			if (cbCnt != _callbackCntsOrderBook[idx])
			{
				//缓存记录下来
				_tempCallbackCntsOrderBook[idx] = cbCnt;
				break;
			}

			++_dataCntsOrderBook[idx];
		}
	}

	//printf("_tempCallbackCntsOrderBook[%d] = %u\n", idx, _tempCallbackCntsOrderBook[idx]);

	//等待条件变量，等待被唤醒
	std::unique_lock<std::mutex> lock(_cvMutex);
	_waitThreadNum++;	//增加等待线程个数
	_condVal.wait(lock);

	_waitThreadNum--;	//减少等待线程个数
	lock.unlock();//手动解锁

	//如果_callbackCnt被置为0，代表没有数据了，结束线程
	while (_callbackCntsOrderBook[idx] != 0)
	{
		//检查自己是不是最小的回调次序，是就触发回调函数
		if (_callbackCntsOrderBook[idx] == _minCallbackCnt)
		{
			//printf("_callbackCntsOrderBook[%d] = %u\n", idx, _callbackCntsOrderBook[idx]);

			/*vector<MDOrderBookSt> vec(_ptrsArrayOrderBook[idx]->begin() + _dataBeginsOrderBook[idx],
				_ptrsArrayOrderBook[idx]->begin() + _dataBeginsOrderBook[idx] + _dataCntsOrderBook[idx]);*/

			//传给回调函数
			_onQuickSnap(&(_arrsOrderBook[idx]->at(_dataBeginsOrderBook[idx])), _dataCntsOrderBook[idx]);

			//vector<MDOrderBookSt> vec;
			//for (size_t i = _dataBeginsOrderBook[idx]; i < _dataBeginsOrderBook[idx] + _dataCntsOrderBook[idx]; i++)
			//{
			//	const OrderBookInfo& info = _arrsOrderBook[idx]->at(i);
			//	MDOrderBookSt orderBook;

			//	//把info里的MDOrderBook数据提取出来
			//	orderBook.channel_no = info.channel_no;
			//	orderBook.market_type = info.market_type;
			//	strcpy(orderBook.security_code, info.security_code);
			//	orderBook.last_tick_time = info.last_tick_time;
			//	orderBook.last_snapshot_time = info.last_snapshot_time;
			//	orderBook.last_tick_seq = info.last_tick_seq;

			//	amd::ama::MDOrderBookItem item;

			//	item.price = info._bidPrice01;
			//	item.volume = info._bidVolume01;
			//	if (!(item.price == 0 && item.volume == 0))
			//	{
			//		orderBook.bid_order_book.emplace_back(item);
			//	}

			//	item.price = info._bidPrice02;
			//	item.volume = info._bidVolume02;
			//	if (!(item.price == 0 && item.volume == 0))
			//	{
			//		orderBook.bid_order_book.emplace_back(item);
			//	}

			//	item.price = info._offerPrice01;
			//	item.volume = info._offerVolume01;
			//	if (!(item.price == 0 && item.volume == 0))
			//	{
			//		orderBook.offer_order_book.emplace_back(item);
			//	}

			//	item.price = info._offerPrice02;
			//	item.volume = info._offerVolume02;
			//	if (!(item.price == 0 && item.volume == 0))
			//	{
			//		orderBook.offer_order_book.emplace_back(item);
			//	}

			//	orderBook.total_num_trades = info.total_num_trades;
			//	orderBook.total_volume_trade = info.total_volume_trade;
			//	orderBook.total_value_trade = info.total_value_trade;
			//	orderBook.last_price = info.last_price;

			//	vec.emplace_back(orderBook);
			//}

			//_onMDOrderBook(vec);

			//更新下一组数据和回调次数
			UpdateOrderBookData(idx);
		}
		//没有轮到此线程触发回调，继续下轮等待
	}

	printf("threadOrderBook %d end\n", idx);

	_mutexThreadNum.lock();
	_runThreadNum--;
	_mutexThreadNum.unlock();

	return;
}


void VirtualMarketBin::UpdateSnapshotData()
{

	//_tempCallbackCnt若为0，代表上次就读到了文件尾，没数据了
	if (_tempCallbackCntSnapshot == 0)
	{
		_callbackCntSnapshot = 0;//代表不再参与play()里下次循环
		
		printf("_callbackCntSnapshot = 0;\n");
		/*更新最小回调次数，让其他线程接管播放，不更新的话,minCnt还是当前线程本次循环时的回调次数
		  其他线程永远无法匹配到最小回调次数*/
		_minCallbackCnt = MinCallbackCnt();

		return;
	}

	//记录回调次序
	_callbackCntSnapshot = _tempCallbackCntSnapshot;

	//更新最小回调次数
	_minCallbackCnt = MinCallbackCnt();

	//本次数据的起始位置，就是上次遍历到的数据
	_dataBeginSnapshot = _curIdxSnapshot;
	//初始化数据个数
	_dataCntSnapshot = 1;

	uint32_t cbCnt;
	//读文件，一直到callbackCnt不同了，或者没数据了
	while (1)
	{
		//遍历下一个数据
		_curIdxSnapshot++;
		
		//到末尾了
		if (_curIdxSnapshot == _lengthSnapshotArray)
		{
			_tempCallbackCntSnapshot = 0;

			printf("last _curIdxSnapshot = %lu\n", _curIdxSnapshot);
			//printf("last one: code = %u, tim = %u, ", _pArraySnapshot->at(_curIdxSnapshot - 1)._sc, _pArraySnapshot->at(_curIdxSnapshot - 1)._tm);
			//printf("_tempCallbackCntSnapshot = 0;\n");
			//printf("_callbackCntSnapshot = %u, _dataCntSnapshot = %u\n", _callbackCntSnapshot, _dataCntSnapshot);

			break;
		}
		
		//读取回调次数
		cbCnt = _pArraySnapshot->at(_curIdxSnapshot)._numCb;

		//读到了，但回调次数变了
		if (cbCnt != _callbackCntSnapshot)
		{
			//缓存记录下来
			_tempCallbackCntSnapshot = cbCnt;
			break;
		}

		//如果读到的是同一批次数据，更新数据个数
		++_dataCntSnapshot;
	}

	return;
}

//void VirtualMarketBin::UpdateTickOrderData()
//{
//	//_tempCallbackCnt若为0，代表上次就读到了文件尾，没数据了
//	if (_tempCallbackCntTickOrder == 0)
//	{
//		_callbackCntTickOrder = 0;
//
//		//更新最小回调次数，让其他线程接管播放
//		_minCallbackCnt = MinCallbackCnt();
//
//		printf("_callbackCntTickOrder = 0;\n");
//
//		return;
//	}
//
//	/*记录回调次序；*/
//	_callbackCntTickOrder = _tempCallbackCntTickOrder;
//
//	//更新最小回调次数
//	_minCallbackCnt = MinCallbackCnt();
//
//	/*本次数据的起始位置，就是正在遍历到的数据
//	  数据个数初始化为1*/
//	_dataBeginTickOrder = _curIdxTickOrder;
//	_dataCntTickOrder = 1;
//
//	uint32_t cbCnt;
//	//bool ret;
//	//读文件里数据
//	while (1)
//	{
//		//printf("mkt = %d, code = %s, cbCnt = %u\n",
//		//	//_pArrayTickOrder[_curIdxTickOrder]._tickOrder.market_type,
//		//	_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.market_type,
//		//	_pArrayTickOrder->at(_curIdxTickOrder)._tickOrder.security_code,
//		//	_pArrayTickOrder->at(_curIdxTickOrder)._callbackCnt);
//		//如果读到数据，放入数组
//		//ret = ReadOneTickOrder(_pArrayTickOrder->at(_writeIndexTickOrder), cbCnt);
//		_curIdxTickOrder++;
//		//没数据了
//		if (_curIdxTickOrder == _lengthTickOrderArray)
//		{
//			_tempCallbackCntTickOrder = 0;
//
//			printf("last _curIdxTickOrder = %lu\n", _curIdxTickOrder);
//			printf("last one: code = %s, tim = %ld, ", _pArrayTickOrder->at(_curIdxTickOrder - 1)._tickOrder.security_code, _pArrayTickOrder->at(_curIdxTickOrder - 1)._tickOrder.order_time);
//			//printf("_tempCallbackCntTickOrder = 0;\n");
//			printf("_callbackCntTickOrder = %u, _dataCntTickOrder = %u\n", _callbackCntTickOrder, _dataCntTickOrder);
//
//			break;
//		}
//
//		//取出回调次数
//		//cbCnt = _pArrayTickOrder[_curIdxTickOrder]._callbackCnt;
//		cbCnt = _pArrayTickOrder->at(_curIdxTickOrder)._callbackCnt;
//		//是下次回调的数据
//		if (cbCnt != _callbackCntTickOrder)
//		{
//			_tempCallbackCntTickOrder = cbCnt;
//			break;
//		}
//
//		//如果读到的是同一批次数据，更新数据个数
//		_dataCntTickOrder++;
//	}
//
//	return;
//}

//void VirtualMarketBin::UpdateTickExecutionData()
//{
//	//_tempCallbackCnt若为0，代表上次就读到了文件尾，没数据了
//	if (_tempCallbackCntTickExecution == 0)
//	{
//		_callbackCntTickExecution = 0;
//
//		//更新最小回调次数，让其他线程接管播放
//		_minCallbackCnt = MinCallbackCnt();
//
//		printf("_callbackCntTickExecution = 0;\n");
//
//		return;
//	}
//
//	/*记录回调次序；*/
//	_callbackCntTickExecution = _tempCallbackCntTickExecution;
//
//	//更新最小回调次数
//	_minCallbackCnt = MinCallbackCnt();
//
//	//本次数据的起始位置，上次遍历到的地方
//	//数据个数初始化为1
//	_dataBeginTickExecution = _curIdxTickExecution;
//	_dataCntTickExecution = 1;
//
//	uint32_t cbCnt;
//	//bool ret;
//	while (1)
//	{
//		//遍历下个数据
//		++_curIdxTickExecution;
//
//		//如果没有数据了
//		if (_curIdxTickExecution == _lengthTickExecutionArray)
//		{
//			_tempCallbackCntTickExecution = 0;
//
//			printf("last _curIdxTickExecution = %lu\n", _curIdxTickExecution);
//			printf("last one: code = %s, tim = %ld, ", 
//				_pArrayTickExecution->at(_curIdxTickExecution - 1)._tickExetution.security_code, 
//				_pArrayTickExecution->at(_curIdxTickExecution - 1)._tickExetution.exec_time);
//			//printf("_tempCallbackCntTickExecution = 0;\n");
//			printf("_callbackCntTickExecution = %u, _dataCntTickExecution = %u\n", _callbackCntTickExecution, _dataCntTickExecution);
//
//			break;
//		}
//		
//		//读出回调次数
//		cbCnt = _pArrayTickExecution->at(_curIdxTickExecution)._callbackCnt;
//
//		if (cbCnt != _callbackCntTickExecution)
//		{
//			_tempCallbackCntTickExecution = cbCnt;
//			break;
//		}
//
//		_dataCntTickExecution++;
//	}
//
//	return;
//}

void VirtualMarketBin::UpDateOrdAndExecu()
{
	//_tempCallbackCnt若为0，代表上次就读到了文件尾，没数据了
	if (_tempCallbackCntOrdAndExe == 0)
	{
		_callbackCntOrdAndExe = 0;

		//更新最小回调次数，让其他线程接管播放
		_minCallbackCnt = MinCallbackCnt();

		printf("_callbackCntOrdAndExe = 0;\n");

		return;
	}

	/*记录回调次序；*/
	_callbackCntOrdAndExe = _tempCallbackCntOrdAndExe;

	//更新最小回调次数
	_minCallbackCnt = MinCallbackCnt();

	//本次数据的起始位置，上次遍历到的地方
	//数据个数初始化为1
	_dataBeginOrdAndExe = _curIdxOrdAndExecu;
	_dataCntOrdAndExe = 1;

	uint32_t cbCnt;
	//bool ret;
	while (1)
	{
		//遍历下个数据
		++_curIdxOrdAndExecu;

		//如果没有数据了
		if (_curIdxOrdAndExecu == _lengthOrdAndExecu)
		{
			_tempCallbackCntOrdAndExe = 0;

			printf("last _curIdxOrdAndExecu = %lu\n", _curIdxOrdAndExecu);
			/*printf("last one: code = %u, tim = %u, ",
				_pArrayOrdAndExecu->at(_curIdxOrdAndExecu - 1)._sc,
				_pArrayOrdAndExecu->at(_curIdxOrdAndExecu - 1)._tm);*/
			//printf("_tempCallbackCntTickExecution = 0;\n");
			//printf("_callbackCntOrdAndExe = %u, _dataCntOrdAndExe = %u\n", _callbackCntOrdAndExe, _dataCntOrdAndExe);

			break;
		}

		//读出回调次数
		cbCnt = _pArrayOrdAndExecu->at(_curIdxOrdAndExecu)._numCb;

		if (cbCnt != _callbackCntOrdAndExe)
		{
			_tempCallbackCntOrdAndExe = cbCnt;
			break;
		}

		_dataCntOrdAndExe++;
	}

	return;
}


void VirtualMarketBin::UpdateOrderBookData(size_t idx)
{
	if (_tempCallbackCntsOrderBook[idx] == 0)
	{
		_callbackCntsOrderBook[idx] = 0;

		printf("_callbackCntOrderBook[%lu] = 0;\n", idx);

		//更新最小回调次数，让其他线程接管播放
		_minCallbackCnt = MinCallbackCnt();

		return;
	}

	//记录回调次序；
	_callbackCntsOrderBook[idx] = _tempCallbackCntsOrderBook[idx];

	//更新最小回调次数
	_minCallbackCnt = MinCallbackCnt();


	size_t& curIdxInArray = _curIdxesOrderBook[idx];
	/*本次数据的起始位置，数据个数初始化*/
	_dataBeginsOrderBook[idx] = curIdxInArray;
	_dataCntsOrderBook[idx] = 1;

	uint32_t cbCnt;
	//从文件读数据
	while (1)
	{
		//向后遍历
		++curIdxInArray;
		//没有数据了
		if (curIdxInArray == _lengthsOrderBookArray[idx])
		{
			_tempCallbackCntsOrderBook[idx] = 0;

			printf("last curIdxInArray %lu = %lu\n", idx, curIdxInArray);
			/*printf("last one: code = %06u, tim = %u, ",
				_arrsOrderBook[idx]->at(curIdxInArray - 1)._sc,
				_arrsOrderBook[idx]->at(curIdxInArray - 1)._tm);*/
			//printf("_tempCallbackCntOrderBook = 0;\n");
			//printf("_callbackCntOrderBook = %u, _dataCntorderBook = %u\n", _callbackCntsOrderBook[idx], _dataCntsOrderBook[idx]);

			break;
		}
		
		//读出回调次数
		cbCnt = _arrsOrderBook[idx]->at(curIdxInArray)._cntCb;

		//读到了下次回调的数据
		if (cbCnt != _callbackCntsOrderBook[idx])
		{
			_tempCallbackCntsOrderBook[idx] = cbCnt;;
			break;
		}

		//读到同一批数据
		_dataCntsOrderBook[idx]++;
	}

	return;
}


uint32_t VirtualMarketBin::MinCallbackCnt()
{
	set<uint32_t> cnts;

	if (_callbackCntSnapshot > 0)
	{
		cnts.insert(_callbackCntSnapshot);
	}

	/*if (_callbackCntTickOrder > 0)
	{
		cnts.insert(_callbackCntTickOrder);
	}*/

	/*if (_callbackCntTickExecution > 0)
	{
		cnts.insert(_callbackCntTickExecution);
	}*/

	if (_callbackCntOrdAndExe > 0)
	{
		cnts.insert(_callbackCntOrdAndExe);
	}

	for (size_t i = 0; i < _callbackCntsOrderBook.size(); i++)
	{
		if (_callbackCntsOrderBook[i] > 0)
		{
			cnts.insert(_callbackCntsOrderBook[i]);
		}
	}

	uint32_t ret;

	if (cnts.empty() == false)
	{
		ret = *(cnts.begin());
	}
	else
	{
		cerr << "VirtualMarketBin::MinCallbackCnt(): all callbackCnt equal 0" << endl;
		ret = 0;
	}

	return ret;
}


//计算时间间隔，毫秒为单位
time_t VirtualMarketBin::GetTimeInterval(timeb start, timeb end)
{
	//142528617
	time_t sec;
	sec = end.time - start.time;

	time_t ret = sec * 1000 + end.millitm - start.millitm;

	return ret;
}

//获得文件大小，执行之后文件指针会置于文件开始
long VirtualMarketBin::GetFileSize(FILE* fp)
{
	long fileSize;
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	return fileSize;
}

//分片解压缩文件，验证CHUNK_MARK，读出数据长度，解压缩
int VirtualMarketBin::UncompressData(const char* source, char* dest, size_t sourceLen, size_t destCapacity)
{
	size_t chunkSrcLen;//待解压的分段数据长度，这里必须是size_t，因为从文件里取出来就是size_t
	char* chunkDestPos = dest;//解压缩后数据放入的位置
	int chunkDestCapacity;//解压缩后放数据的空间大小
	size_t uncompressTotalLen = 0;//解压缩出来的数据总长度
	int decompressedSize;//返回值
	size_t sourceIdx;
	for (sourceIdx = 0; sourceIdx < sourceLen;)
	{
		//验证chunkmark
		/*if (strcmp(source + sourceIdx, CHUNK_MARK) != 0)
		{
			printf("CHUNK_MARK not right\n");
			break;
		}
		else
		{
			printf("CHUNK_MARK is right\n");
		}*/

		//srcIdx后移
		//sourceIdx += sizeof(CHUNK_MARK);

		//取得需要解压缩的分段数据的长度
		memcpy(&chunkSrcLen, source + sourceIdx, sizeof(chunkSrcLen));
		printf("chunkSrcLen = %lu\n", chunkSrcLen);

		//srcIdx后移
		sourceIdx += sizeof(chunkSrcLen);

		//解压缩的数据放入的空间容量
		chunkDestCapacity = destCapacity - uncompressTotalLen;
		
		//开始解压缩
		//decompressedSize = LZ4_decompress_fast(source + sourceIdx, chunkDestPos, originalSize);
		decompressedSize = LZ4_decompress_safe(source + sourceIdx, chunkDestPos, chunkSrcLen, chunkDestCapacity);
		printf("decompressedSize = %d\n", decompressedSize);

		//更新totalLen
		uncompressTotalLen += decompressedSize;

		//srcIdx后移
		sourceIdx += chunkSrcLen;
		//destPos后移
		chunkDestPos += decompressedSize;
	}

	//return uncompressTotalLen;
	return 0;
}

//把文件中数据解压缩到内存
size_t VirtualMarketBin::DecompressFileData(FILE* fp, char* dest, size_t destCapacity)
{
	//printf("sizeof(QuickSnapInfo) * length, destCapacity = %lu\n", destCapacity);

	//开辟空间放置待解压缩的数据
	char* srcBuf = (char*)malloc(COMPRESS_CHUNK_SIZE);
	if (srcBuf == NULL)
	{
		perror("malloc srcBuf");
		return 0;
	}

	//放置解压缩后的数据，开辟空间
	/*int destLen = COMPRESS_CHUNK_SIZE;
	char* destBuf = (char*)malloc(destLen);
	if (destBuf == NULL)
	{
		perror("malloc dest buf");
		return 0;
	}*/

	size_t ret;
	int srcLen;
	int decompressedSize;
	size_t uncompressTotalLen = 0;//解压缩出来的数据总长度
	char* chunkDestPos = dest;//解压缩后数据放入的位置
	int chunkDestLen;//剩余可以放数据的空间大小
	while (1)
	{
		//把这段数据的长度读出来
		ret = fread(&srcLen, sizeof(srcLen), 1, fp);
		if (ret == 0)
		{
			printf("read file end\n");
			break;
		}
		printf("srcLen = %d\n", srcLen);

		//把数据取出来
		ret = fread(srcBuf, 1, srcLen, fp);
		if (ret < srcLen)
		{
			printf("fread srcBuf not enough, ret = %lu\n", ret);
			return 0;
		}

		//判断剩余空间大小
		if (destCapacity - uncompressTotalLen > COMPRESS_CHUNK_SIZE)
		{
			chunkDestLen = COMPRESS_CHUNK_SIZE;
		}
		else
		{
			chunkDestLen = destCapacity - uncompressTotalLen;
		}
		printf("chunkDestLen = %d\n", chunkDestLen);

		//解压缩
		decompressedSize = LZ4_decompress_safe(srcBuf, chunkDestPos, srcLen, chunkDestLen);
		//decompressedSize = LZ4_decompress_safe(srcBuf, destBuf, srcLen, destLen);
		printf("decompressedSize = %d\n", decompressedSize);

		//解压缩得到的数据总长度
		uncompressTotalLen += decompressedSize;

		//放置解压缩数据的位置
		chunkDestPos += decompressedSize;
	}

	//释放空间
	free(srcBuf);

	return uncompressTotalLen;
}