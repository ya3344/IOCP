#include "pch.h"
#include "CrashDump.h"

long CrashDump::gDumpCount = 0;

CrashDump::CrashDump()
{
	gDumpCount = 0;

	_invalid_parameter_handler oldHandler, newHandler;
	newHandler = MyInvalidParameterHandler;
	oldHandler = _set_invalid_parameter_handler(newHandler);
	
	_CrtSetReportMode(_CRT_WARN, 0);
	_CrtSetReportMode(_CRT_ASSERT, 0);
	_CrtSetReportMode(_CRT_ERROR, 0);

	_CrtSetReportHook(CustomReportHook);

	_set_purecall_handler(MyPureCallHandler);

	SetHandlerDump();
}

CrashDump::~CrashDump()
{
}

void CrashDump::Crash()
{
	int* p = nullptr;
	*p = 0;
}

LONG WINAPI CrashDump::MyExceptionFilter(PEXCEPTION_POINTERS exceptionPointer)
{
	int workingMemory = 0;
	SYSTEMTIME nowTime;

	long dumpCount = InterlockedIncrement(&gDumpCount);

	//현재 프로세스의 메모리 사용량을 얻어온다
	HANDLE process = 0;
	PROCESS_MEMORY_COUNTERS pmc;

	process = GetCurrentProcess();
	if (process == NULL)
		return 0;

	if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc)))
	{
		workingMemory = (int)(pmc.WorkingSetSize / 1024 / 1024);
	}
	CloseHandle(process);

	// 현재 날짜와 시간을 알아온다
	WCHAR fileName[MAX_PATH];

	GetLocalTime(&nowTime);
	wsprintf(fileName, L"Dump_%d%02d%02d_%02d.%02d.%02d_%d_%dMB.dmp",
		nowTime.wYear, nowTime.wMonth, nowTime.wDay, nowTime.wHour, nowTime.wMinute, nowTime.wSecond, gDumpCount,
		workingMemory);

	wprintf(L"\n\n\n!!!Crash Error!!! %d.%d.%d / %d:%d:%d \n",
		nowTime.wYear, nowTime.wMonth, nowTime.wDay, nowTime.wHour, nowTime.wMinute, nowTime.wSecond);
	wprintf(L"Now Save dump file... \n");

	HANDLE dumpFile = ::CreateFile(fileName,
		GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (dumpFile != INVALID_HANDLE_VALUE)
	{
		_MINIDUMP_EXCEPTION_INFORMATION miniDumpExceptionInformation;

		miniDumpExceptionInformation.ThreadId = ::GetCurrentThreadId();
		miniDumpExceptionInformation.ExceptionPointers = exceptionPointer;
		miniDumpExceptionInformation.ClientPointers = TRUE;

		MiniDumpWriteDump(GetCurrentProcess(),
			GetCurrentProcessId(),
			dumpFile,
			MiniDumpWithFullMemory,
			&miniDumpExceptionInformation,
			NULL,
			NULL);
	}

	CloseHandle(dumpFile);
	wprintf(L"CrashDump Save Finish!");

	return EXCEPTION_EXECUTE_HANDLER;
}

void CrashDump::SetHandlerDump()
{
	SetUnhandledExceptionFilter(MyExceptionFilter);
}

void CrashDump::MyInvalidParameterHandler(const TCHAR* expression, const TCHAR* function, const TCHAR* file, unsigned int line, uintptr_t reserved)
{
	Crash();
}

int CrashDump::CustomReportHook(int repostType, char* message, int* returnValue)
{
	Crash();
	return true;
}

void CrashDump::MyPureCallHandler(void)
{
	Crash();
}
