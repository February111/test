#pragma once
#include "MDApplication.h"
class NumCount :
    public MDApplication
{
public:
    //构造函数
    NumCount()
    {
        _sseCount = 0;
        _szseCount = 0;
        _snapshotCount = 0;
        _ordAndExeCount = 0;
        _quickSnapCount = 0;
    }

    //重写回调函数，统计不同交易所的数据个数
    virtual void OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt) override;
    virtual void OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt) override;
    virtual void OnQuickSnap(QuickSnapInfo* info, uint32_t cnt) override;

    virtual ~NumCount()
    {
        printf("~NumCount(): sseCount = %lu, szseCount = %lu, snapshotCount = %lu, ordAndExeCount = %lu, quickSnapCount = %lu\n", 
            _sseCount, _szseCount, _snapshotCount, _ordAndExeCount, _quickSnapCount);
    }

private:
    uint64_t _sseCount;
    uint64_t _szseCount;
    uint64_t _snapshotCount;
    uint64_t _ordAndExeCount;
    uint64_t _quickSnapCount;
};

