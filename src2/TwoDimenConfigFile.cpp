#include <stdio.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include "TwoDimenConfigFile.h"

using std::string;
using std::vector;

TwoDimenConfigFile::TwoDimenConfigFile(const std::string& filePath)
{
	//printf("TwoDimenConfigFile::TwoDimenConfigFile\n");
	_sep = ',';

	//打开文件
	std::ifstream ifs;
	ifs.open(filePath);
	//如果打开失败
	if (ifs.is_open() == false)
	{
		printf("TwoDimenConfigFile::TwoDimenConfigFile， open file %s failed\n", filePath.c_str());
		return;
	}

	//取出表头
	string line;
	std::getline(ifs, line);

	//每个字段分别取出，放入数组
	std::istringstream iss(line);
	string word;
	unsigned int idx = 0;
	while (iss.eof() == false)
	{
		std::getline(iss, word, _sep);
		//printf("word = %s\n", word.c_str());
		_header[word] = idx;
		idx++;
	}

	//遍历每行数据
	while (std::getline(ifs, line))
	{
		//printf("line = %s\n", line.c_str());
		//_content数组增加一行
		_content.emplace_back(std::vector<double>());
		vector<double>& row = _content.back();
		
		//取出每个字段数据
		//iss.str(line);
		std::istringstream iss(line);
		while (iss.eof() == false)
		{
			std::getline(iss, word, _sep);
			//printf("word = %s\n", word.c_str());
			//放入本行数据数组
			row.emplace_back(std::stod(word));
		}
	}
}

//根据第几行数据，哪个字段，获得数据
double TwoDimenConfigFile::GetData(unsigned int idx, const std::string& field)
{
	//printf("TwoDimenConfigFile::GetData\n");
	if (idx >= _content.size())
	{
		printf("TwoDimenConfigFile::GetData，idx over content size\n");
		return 0;
	}

	//寻找field
	if (_header.find(field) == _header.end())
	{
		printf("TwoDimenConfigFile::GetData，field not found in header\n");
		return 0;
	}

	double ret;
	ret = _content[idx][_header[field]];

	return ret;
}

void TwoDimenConfigFile::Show()
{
	printf("how many rows: %d\n", _content.size());
	printf("how many fields: %d\n", _content[0].size());

	for (auto i = 0; i < _content.size(); ++i)
	{
		for (auto j = 0; j < _content[i].size(); ++j)
		{
			printf("%.3lf ", _content[i][j]);
		}
		printf("\n");
	}
}

//返回行数
unsigned int TwoDimenConfigFile::LineNumber()
{
	return _content.size();
}