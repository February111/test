#pragma once
#include <string>
#include "MDApplication.h"

class StatisticalData :
    public MDApplication
{
public:
    //重写盘前处理函数
    virtual int BeforeMarket() override;

    //重写snapshot回调函数
    virtual void OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt) override;

private:
    //上交所数据目录
    std::string _sseFilePath;
    //深交所数据目录
    std::string _szseFilePath;
};

