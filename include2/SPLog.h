#pragma once
#include "ConfigFile.h"


//记录日志的类
class SPLog
{
public:
	SPLog();

	//写入日志，在日志内容前加上时间戳，日志内容后加上换行
	int32_t WriteLog(const std::string& str);

	~SPLog()
	{
		if (_logFile != NULL)
		{
			fclose(_logFile);
			_logFile = NULL;
		}
		//printf("~SPLog()\n");
	}

	void FlushLogFile();

	//关闭日志
	void CloseLogFile();

private:
	FILE* _logFile;
};

