#pragma once
template <typename T>
inline void SafeDelete(T& obj)
{
	if (obj)
	{
		delete obj;
		obj = nullptr;
	}
}

template <typename T>
inline void SafeArrayDelete(T& obj)
{
	if (obj)
	{
		delete[] obj;
		obj = nullptr;
	}
}

template <typename T>
inline void SafeFree(T& obj)
{
	if (obj)
	{
		free(obj);
		obj = nullptr;
	}
}

inline short CalDistance(const short srcX, const short srcY, const short destX, const short destY)
{
	short x = destX - srcX;
	short y = destY - srcY;
	short distance = (short)(sqrt((x * x) + (y * y)));

	return distance;
}

#define CONSOLE_LOG(logLevel, fmt, ...)							\
do {															\
	if (gLogLevel <= logLevel)									\
	{															\
		wsprintf(gLogBuffer, fmt, ##__VA_ARGS__);				\
		wprintf(L"[%s] %s\n", TEXT(__FUNCTION__), gLogBuffer);	\
	}															\
} while(0)														\