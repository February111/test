#pragma once
#include <map>
#include "MDApplication.h"
#include "TwoDimenConfigFile.h"

struct ExeCount
{
    uint64_t _greater;
    uint64_t _less;

public:
    ExeCount()
    {
        _greater = 0;
        _less = 0;
    }
};

class CountByPric :
    public MDApplication
{
public:
    CountByPric();

    virtual void OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt) override;

    ~CountByPric();

private:
    //映射，某个证券的成交价高于开盘价的个数，低于开盘价的个数
    std::map<uint32_t, ExeCount> _exeNum;

    //映射，某个证券和对应的开盘价
    std::map<uint32_t, uint32_t> _openPrics;
};

