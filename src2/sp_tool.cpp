#include <chrono>
#include "sp_tool.h"

//获得时间戳，返回值是int32_t，数字越大时间越晚，精确到毫秒, hhmmssxxx
int GetLocalTm()
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

#if defined(_WIN32) || defined(_WIN64)
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	std::tm tm_now;
	localtime_s(&tm_now, &time_t_now);
#elif defined(__linux__) || defined(__unix__)
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	std::tm tm_now;
	localtime_r(&time_t_now, &tm_now);
#else
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	tm_now = *std::localtime(&time_t_now);
#endif

	int32_t tm = tm_now.tm_hour * 10000000
		+ tm_now.tm_min * 100000
		+ tm_now.tm_sec * 1000
		+ millis % 1000;

	return tm;
}