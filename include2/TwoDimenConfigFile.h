#pragma once
#include <string>
#include <vector>
#include <map>

class TwoDimenConfigFile
{
public:
	//构造函数，给它一个文件路径，打开文件把内容提取出来
	TwoDimenConfigFile(const std::string& filePath);

	//根据第几行数据，哪个字段，获得数据
	double GetData(unsigned int idx, const std::string& field);

	void Show();

	//返回行数
	unsigned int LineNumber();

private:
	//表头，字符串数组
	//std::vector<std::string> _header;

	//表头字段，每个字段的字符串映射该字段在数组里下标
	std::map<std::string, unsigned int> _header;

	//每行的内容，一行有多个double
	std::vector<std::vector<double>> _content;

	//分隔符
	char _sep;
};

