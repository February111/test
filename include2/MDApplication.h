#pragma once

#include "sp_type.h"

class MDApplication
{
public:
	//��ǰ����
	virtual int BeforeMarket()
	{
		return 0;
	}

	//������಻��д�˺��������������κβ���
	virtual void OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt)
	{

	}

	virtual void OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt)
	{

	}

	virtual void OnQuickSnap(QuickSnapInfo* info, uint32_t cnt)
	{

	}

	//����һ���ڲ��ı������
	virtual void OnUnchangedMarketData(uint32_t code,const UnchangedMarketData& data)
	{

	}

	//�̺���
	virtual int AfterMarket()
	{
		return 0;
	}

	virtual ~MDApplication()
	{

	}
};

