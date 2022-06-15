// IOCP.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//
#include "pch.h"
#include "EchoServer.h"

int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	timeBeginPeriod(1); // 타이머 해상도 높이기
    
	EchoServer echoServer;

	if (echoServer.Start(L"127.0.0.1", 6000, 4, true, 50000) == false)
		return EXIT_FAILURE;

	while (true)
	{
		/*if (GetAsyncKeyState(VK_SPACE))
		{
			echoServer.Stop();
			break;
		}*/
	}

	for (thread& threadData : echoServer.GetIocpServer_Threads())
	{
		if (threadData.joinable())
			threadData.join();
	}

	timeEndPeriod(1);
	return EXIT_SUCCESS;
}
