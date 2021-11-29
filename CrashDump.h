#pragma once

class CrashDump
{
public:
	CrashDump();
	~CrashDump();

public:
	static void Crash();
	static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS exceptionPointer);
	static void SetHandlerDump();
	static void MyInvalidParameterHandler(const TCHAR* expression, const TCHAR* function, const TCHAR* file, unsigned int line, uintptr_t reserved);
	static int CustomReportHook(int repostType, char* message, int* returnValue);
	static void MyPureCallHandler(void);

public:
	static long gDumpCount;
};

