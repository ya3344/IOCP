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

inline void Crash()
{
	int* p = nullptr;
	*p = 0;
}

#define DECLARE_SINGLETON(ClassName)			\
public:											\
	static ClassName* GetInstance()				\
	{											\
		if (m_pInstance == nullptr)				\
			m_pInstance = new ClassName;		\
												\
		return m_pInstance;						\
	}											\
												\
	void DestroyInstance()						\
	{											\
		if (m_pInstance)						\
		{										\
			delete m_pInstance;					\
			m_pInstance = nullptr;				\
		}										\
	}											\
												\
private:										\
	static ClassName* m_pInstance;				\

#define IMPLEMENT_SINGLETON(ClassName)			\
ClassName* ClassName::m_pInstance = nullptr;

#define CONSOLE_LOG(logLevel, fmt, ...)							\
do {															\
	if (gLogLevel <= logLevel)									\
	{															\
		wsprintf(gLogBuffer, fmt, ##__VA_ARGS__);				\
		wprintf(L"[%s] %s\n", TEXT(__FUNCTION__), gLogBuffer);	\
	}															\
} while(0)														\