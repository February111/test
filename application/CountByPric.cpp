#include "CountByPric.h"
#include "reflect.h"

CountByPric::CountByPric() 
{
	TwoDimenConfigFile configInfo("../data/pric.csv");
	uint32_t rows = configInfo.LineNumber();
	//遍历所有行
	double code;
	double openPric;
	for (int idx = 0; idx < rows; idx++)
	{
		code = configInfo.GetData(idx, "code");
		//printf("code = %.0lf, ", code);
		openPric = configInfo.GetData(idx, "open_price");
		//printf("price = %.0lf\n", openPric);
		_openPrics[static_cast<unsigned int>(code)] = static_cast<unsigned int>(openPric);
	}
}

void CountByPric::OnOrderAndExecu(OrdAndExeInfo* info, uint32_t cnt)
{
	//去掉最高位100或200
	uint32_t code = info->_sc % 1000000;

	//配置信息里有没有
	if (_openPrics.find(code) == _openPrics.end())
	{
		return;
	}
	else
	{
		//配置文件里有
		if (info->_px > _openPrics[code])
		{
			_exeNum[code]._greater++;
		}
		else
		{
			_exeNum[code]._less++;
		}
	}

	return;
}

CountByPric::~CountByPric()
{
	for (auto& elem : _exeNum)
	{
		printf("code %u, greater %lu, less %lu\n", elem.first, elem.second._greater, elem.second._less);
	}
}

REGISTER(CountByPric)
