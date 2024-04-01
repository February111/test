#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

//函数声明，获得时间戳
int GetLocalTm();

//在已有均值的情况下，求标准差，函数模板
template<typename T>
double CalculateStandardDeviation(const std::vector<T>& data, double avrg)
{
	if (data.empty())
	{
		return 0;
	}

	//计算平方和
	double differ;
	double squareSum = 0;
	for (auto it = data.begin(); it != data.end(); ++it)
	{
		//与均值的差
		if (*it > avrg)
		{
			differ = *it - avrg;
		}
		else
		{
			differ = avrg - *it;
		}
		//平方和
		squareSum += differ * differ;
	}

	//开方
	double standardDeviation = std::sqrt(squareSum / data.size());

	return standardDeviation;
}

//函数模板，根据均值，标准差，求一组数据的偏度
template<typename T>
double CalculateSkewness(std::vector<T> data, double avrg, double stdDevi)
{
	if (data.empty())
	{
		return 0;
	}

	//求中位数
	double median;
	//先排序
	std::sort(data.begin(), data.end());
	//元素个数，奇数个还是偶数个
	if (data.size() % 2 == 1)
	{
		//奇数个元素
		size_t idx = data.size() / 2 + 1 - 1;
		median = data[idx];
	}
	else
	{
		//偶数个元素
		size_t idx = data.size() / 2 - 1;
		double sum = data[idx] + data[idx + 1];
		median = sum / 2;
	}

	double skewness;
	if (stdDevi != 0)
	{
		skewness = 3 * (avrg - median) / stdDevi;
	}
	else
	{
		skewness = 0;
	}
	
	return skewness;
}