#pragma once

#include "sp_type.h"

class MDApplication
{
public:
	//盘前处理
	virtual int BeforeMarket()
	{
		return 0;
	}

	//如果子类不重写此函数，函数内无任何操作
	virtual void OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt)
	{

	}

	virtual void OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt)
	{

	}

	virtual void OnQuickSnap(QuickSnapInfo* info, uint32_t cnt)
	{

	}

	//传递一天内不改变的数据
	virtual void OnUnchangedMarketData(uint32_t code,const UnchangedMarketData& data)
	{

	}

	//盘后处理
	virtual int AfterMarket()
	{
		return 0;
	}

	virtual ~MDApplication()
	{

	}
};

