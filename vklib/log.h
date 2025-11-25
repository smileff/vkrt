#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

inline void LogInfo(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	printf("[Info]:\t");
	vprintf(format, args);
	printf("\n");
	va_end(args);
}

inline void LogWarning(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	printf("[Warning]:\t");
	vprintf(format, args);
	printf("\n");
	va_end(args);
}

inline void LogError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	printf("[Error]:\t");
	vprintf(format, args);
	printf("\n");
	va_end(args);
	exit(-1);
}