#include <signal.h>
#include <execinfo.h>
#include <string.h>
#include <string>
#include <iostream>
#include "SPLog.h"

using namespace std;

extern SPLog g_log; //记录日志

//执行shell命令的结果
std::string ExecuteShellCommand(const std::string& shellCmd)
{
	char buf[1024] = { 0 };
	FILE* fp = popen(shellCmd.c_str(), "r");
	if (fp == NULL)
	{
		perror("executeShellCommand: popen");
		exit(1);
	}

	fread(buf, 1, 1024, fp);

	pclose(fp);

	std::string ret = buf;
	return ret;
}

/*把函数栈地址转化为函数所在文件和行号
输入：/stackInfo(_Z8test_errv+0x10) [0x401412]
执行：addr2line -e stackInfo 0x401412 -f
输出：_Z8test_errv
/home/huangjp/job/test/err_sig/stackInfo.cpp:83
sscanf("%*s%s")获得函数栈地址
*/
std::string GetStackPosition(const char* exeName, const char* str)
{
	//把函数栈地址提取出来
	char addr[64] = { 0 };
	sscanf(str, "%*s%s", addr);
	size_t len = strlen(addr);
	if (!(addr[0] == '[' && addr[len - 1] == ']'))
	{
		printf("get stackposition failed\n");
		return string();
	}
	addr[len - 1] = '\0';

	//拼凑addr2line命令
	char cmd[128] = { 0 };
	sprintf(cmd, "addr2line -e %s %s -f", exeName, addr + 1);
	//printf("%s\n", cmd);

	std::string ret = ExecuteShellCommand(cmd);
	//cout << ret << endl;

	return ret;
}

//获得信号触发时最近的函数栈的信息
std::string GetLastStacks()
{
	const int32_t backtraceLevel = 50;

	void* array[backtraceLevel];
	size_t sz;
	sz = backtrace(array, backtraceLevel);

	char** strs;
	strs = backtrace_symbols(array, sz);
	std::string ret;
	std::string position;
	for (unsigned int i = 0; i < sz; ++i)
	{
		position = GetStackPosition("StrategyPlatform", strs[i]);
		ret = ret + strs[i] + '\n' + position + '\n';
	}

	free(strs);
	return ret;
}


//信号捕捉,回调函数
void SigHandler(int signum)
{
	switch (signum)
	{
	case SIGINT:
		g_log.WriteLog("get signal SIGINT, exit");
		break;
	case SIGQUIT:
		g_log.WriteLog("get signal SIGQUIT, exit");
		break;
	case SIGKILL:
		g_log.WriteLog("get signal SIGKILL, exit");
		break;
	default:
		break;
	}

	g_log.CloseLogFile();
	_Exit(1);
	return;
}

//信号捕捉,回调函数，需要回溯函数栈
void SigHandlerWithBacktrace(int signum)
{
	switch (signum)
	{
	case SIGABRT:
		cout << "get signal SIGABRT" << endl;
		g_log.WriteLog("get signal SIGABRT, exit");
		break;
	case SIGSEGV:
		cout << "get signal SIGSEGV" << endl;
		g_log.WriteLog("get signal SIGSEGV, exit");
		break;
	case SIGFPE:
		cout << "get signal SIGFPE" << endl;
		g_log.WriteLog("get signal SIGFPE, exit");
		break;
	default:
		break;
	}

	string stackInfo = GetLastStacks();
	cout << stackInfo << endl;
	g_log.WriteLog(stackInfo);

	g_log.CloseLogFile();
	_Exit(1);
	return;
}

//注册信号回调函数
void SignalRegister()
{
	struct sigaction act1, act2;

	act1.sa_handler = SigHandler;
	sigemptyset(&(act1.sa_mask));            // 清空 sa_mask屏蔽字
	sigaddset(&act1.sa_mask, SIGQUIT);       // 信号SIGINT捕捉函数执行期间, SIGQUIT信号会被屏蔽
	act1.sa_flags = 0;               // 本信号自动屏蔽.

	// 注册信号捕捉函数.
	int ret = sigaction(SIGINT, &act1, NULL);
	if (ret == -1)
	{
		perror("sigaction");
		exit(1);
	}
	sigaction(SIGQUIT, &act1, NULL);
	sigaction(SIGKILL, &act1, NULL);

	//需要回溯栈的信号
	act2.sa_handler = SigHandlerWithBacktrace;
	sigemptyset(&(act2.sa_mask));            // 清空 sa_mask屏蔽字
	sigaddset(&act2.sa_mask, SIGQUIT);       // 信号SIGINT捕捉函数执行期间, SIGQUIT信号会被屏蔽
	act2.sa_flags = 0;               // 本信号自动屏蔽.
	sigaction(SIGSEGV, &act2, NULL);
	sigaction(SIGABRT, &act2, NULL);
	sigaction(SIGFPE, &act2, NULL);
}


