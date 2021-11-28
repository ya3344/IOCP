#pragma once
class SystemLog
{
public:
	SystemLog() = default;
	~SystemLog() = default;

public:
	enum LOG_INDEX
	{
		TIME_MAX_BUFFER_SIZE = 256,
		FILE_MAX_BUFFER_SIZE = 2056,
	};

	enum LOG_LEVEL
	{
		LOG_LEVEL_DEBUG = 0,
		LOG_LEVEL_ERROR,
		LOG_LEVEL_DISPLAY,
	};

public:
	void WriteLog(const TCHAR* tile, const BYTE logLevel, const TCHAR* funcName, const TCHAR* str, ...);

private:
	FILE* mFp = nullptr;
	TCHAR mTimeData[TIME_MAX_BUFFER_SIZE];
	TCHAR mFileData[FILE_MAX_BUFFER_SIZE] = { 0 };
};

