#pragma once
#include <string>
#include "MDApplication.h"

class StatisticalData :
    public MDApplication
{
public:
    //��д��ǰ������
    virtual int BeforeMarket() override;

    //��дsnapshot�ص�����
    virtual void OnSnapshot(SnapshotInfo* snapshot, uint32_t cnt) override;

private:
    //�Ͻ�������Ŀ¼
    std::string _sseFilePath;
    //�������Ŀ¼
    std::string _szseFilePath;
};

