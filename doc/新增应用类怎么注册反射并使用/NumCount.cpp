#include "NumCount.h"
#include "reflect.h"

void NumCount::OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt)
{
	if (snapshot == NULL)
	{
		printf("NumCount::OnSnapshot(): info == NULL\n");
		return;
	}

	_snapshotCount += cnt;

	uint32_t i;
	uint32_t flag;
	for (i = 0; i < cnt; i++)
	{
		flag = snapshot[i]._sc / 100000000;
		if (flag == 1)
		{
			_sseCount++;
		}
		else if (flag == 2)
		{
			_szseCount++;
		}
		else
		{
			printf("error: security code do not begin with 1 or 2\n");
		}
	}

	return;
}

void NumCount::OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt)
{
	_ordAndExeCount += cnt;
}

void NumCount::OnQuickSnap(QuickSnapInfo* info, uint32_t cnt)
{
	_quickSnapCount += cnt;
}

//注册到反射的工厂对象里
REGISTER(NumCount)

//void* CreateNumCount()
//{
//	return new NumCount;
//}
//
//RegisterAction g_registerNumCount("NumCount", CreateNumCount);
