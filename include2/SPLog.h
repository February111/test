#pragma once
#include "ConfigFile.h"


//��¼��־����
class SPLog
{
public:
	SPLog();

	//д����־������־����ǰ����ʱ�������־���ݺ���ϻ���
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

	//�ر���־
	void CloseLogFile();

private:
	FILE* _logFile;
};

