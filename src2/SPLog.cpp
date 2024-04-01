#include "SPLog.h"
#include "sp_tool.h"

SPLog::SPLog()
{
	//������Ϣ
	ConfigFile config;
	if (config.Load("../data/config.conf") == false)
	{
		printf("load config file failed!\n");
		exit(1);
	}

	//�ļ���
	std::string fileNameStr;
	//����
	time_t tim1 = time(NULL);
	struct tm* tim2 = localtime(&tim1);
	char data[16] = { 0 };
	sprintf(data, "%d%02d%02d", tim2->tm_year + 1900, tim2->tm_mon + 1, tim2->tm_mday);
	//fileNameStr = config.Get("filePath") + "/StrategyPlatform.log";
	//�ϳ��ļ���
	fileNameStr = config.Get("logFilePath") + "/" + data + ".log";
	const char* fileName = fileNameStr.c_str();
	//׷�ӷ�ʽ���ļ��������ھʹ������ļ�
	_logFile = fopen(fileName, "a");
	if (_logFile == NULL)
	{
		perror("fopen log file");
		exit(1);
	}
	printf("open file %s\n", fileName);

	return;
}

int32_t SPLog::WriteLog(const std::string& str)
{
	int32_t ret = fprintf(_logFile, "%d\n%s\n", GetLocalTm(), str.c_str());
	if (ret == -1)
	{
		perror("fprintf logFile");
		exit(1);
	}
	else
	{
		return 0;
	}
}

void SPLog::CloseLogFile()
{
	if (_logFile != NULL)
	{
		fclose(_logFile);
		_logFile = NULL;
	}
}

void SPLog::FlushLogFile()
{
	if (_logFile != NULL)
	{
		fflush(_logFile);
	}
	
	return;
}