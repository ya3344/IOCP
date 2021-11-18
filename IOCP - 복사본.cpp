// IOCP.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//
#include "pch.h"
#include "../Common\RingBuffer/RingBuffer.h"
#include "../Common\PacketBuffer/PacketBuffer.h"
#include "EchoServer.h"

struct SessionInfo
{
	SOCKET clientSock = INVALID_SOCKET;
	WCHAR ip[IP_BUFFER_SIZE] = { 0, };
	WORD port = 0;
	DWORD sessionID = 0;
	class RingBuffer* sendRingBuffer = nullptr;
	class RingBuffer* recvRingBuffer = nullptr;
	OVERLAPPED recvOverlapped;
	OVERLAPPED sendOverlapped;
	SRWLOCK srwLock;
};

#pragma pack(push, 1)   
struct HeaderInfo
{
	WORD length = 0;
};
#pragma pack(pop)

SOCKET gListenSock = INVALID_SOCKET;
HANDLE gHcp = NULL;
int gSessionID_Num = 0;
unordered_map<DWORD, SessionInfo*> gSessionData;
HANDLE gThread[THREAD_MAX];
long gIOCount = 0;

bool Initialize();
SessionInfo* AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr);
unsigned __stdcall AcceptThread(void* arguments);
unsigned __stdcall WorkerThread(void* arguments);
void RecvProcess(SessionInfo* sessionInfo);
bool SendProcess(SessionInfo* sessionInfo);
void Release(SessionInfo* sessionInfo);

bool Initialize()
{
//	WSADATA wsaData;
//	SOCKADDR_IN serveraddr;
//	WCHAR serverIP[IP_BUFFER_SIZE] = { 0, };
//	LINGER optval;
//	int sendBufferOption;
//	int retVal;
//	unsigned int threadID[THREAD_MAX];
//
//	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
//	{
//		CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSAStartup() errcode[%d]", WSAGetLastError());
//		return false;
//	}
//
//	gListenSock = socket(AF_INET, SOCK_STREAM, 0);
//	if (INVALID_SOCKET == gListenSock)
//	{
//		CONSOLE_LOG(LOG_LEVEL_ERROR, L"listen_sock error:%d", WSAGetLastError());
//		return false;
//	}
//
//	// timewait 남기지 않도록 설정
//	optval.l_onoff = 1;
//	optval.l_linger = 0;
//	retVal = setsockopt(gListenSock, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
//	if (retVal == SOCKET_ERROR)
//	{
//		CONSOLE_LOG(LOG_LEVEL_ERROR, L"timewait option error:%d", WSAGetLastError());
//		return false;
//	}
//
//	// 송신버퍼 0 으로 초기화 -> Overlapped I/O 유도
//	sendBufferOption = 0;
//	retVal = setsockopt(gListenSock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufferOption, sizeof(sendBufferOption));
//	if (retVal == SOCKET_ERROR)
//	{
//		CONSOLE_LOG(LOG_LEVEL_ERROR, L"SO_SNDBUF option error:%d", WSAGetLastError());
//		return false;
//	}
//
//	// 논블록킹 소켓으로 전환
//	//u_long on = 1;
//	//if (SOCKET_ERROR == ioctlsocket(gListenSock, FIONBIO, &on))
//	//{
//	//	CONSOLE_LOG(LOG_LEVEL_ERROR, L"ioctlsocket() errcode[%d]", WSAGetLastError());
//	//	return false;
//	//}
//
//	// bind
//	ZeroMemory(&serveraddr, sizeof(serveraddr));
//	serveraddr.sin_family = AF_INET;
//	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
//	serveraddr.sin_port = htons(SERVER_PORT);
//	InetNtop(AF_INET, &serveraddr.sin_addr, serverIP, _countof(serverIP));
//
//	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"[CHAT SERVER] SERVER IP: %s SERVER Port:%d", serverIP, ntohs(serveraddr.sin_port));
//
//	retVal = bind(gListenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
//	if (retVal == SOCKET_ERROR)
//	{
//		CONSOLE_LOG(LOG_LEVEL_ERROR, L"bind error:%d", WSAGetLastError());
//		return false;
//	}
//
//	//listen
//	retVal = listen(gListenSock, SOMAXCONN);
//	if (retVal == SOCKET_ERROR)
//	{
//		CONSOLE_LOG(LOG_LEVEL_ERROR, L"listen error:%d ", WSAGetLastError());
//		return false;
//	}
//	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"server open\n");
//
//	// IOCP handle 초기화
//	gHcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, THREAD_MAX);
//	if (gHcp == NULL)
//	{
//		CONSOLE_LOG(LOG_LEVEL_ERROR, L"gHcp NULL:%d ", WSAGetLastError());
//		return false;
//	}
//
//	// Thread 초기화
//#ifdef SINGLE_THREAD
//	gThread[WORKER_THREAD_1] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, NULL, 0, &threadID[WORKER_THREAD_1]);
//#else
//	gThread[WORKER_THREAD_1] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, NULL, 0, &threadID[WORKER_THREAD_1]);
//	gThread[WORKER_THREAD_2] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, NULL, 0, &threadID[WORKER_THREAD_2]);
//	gThread[WORKER_THREAD_3] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, NULL, 0, &threadID[WORKER_THREAD_3]);
//	gThread[WORKER_THREAD_4] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, NULL, 0, &threadID[WORKER_THREAD_4]);
//	gThread[WORKER_THREAD_5] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, NULL, 0, &threadID[WORKER_THREAD_5]);
//#endif
//	gThread[ACCEPT_THREAD] = (HANDLE)_beginthreadex(NULL, 0, &AcceptThread, NULL, 0, &threadID[ACCEPT_THREAD]);
//	
//	for (int i = 0; i < THREAD_MAX; i++)
//	{
//		if (gThread[i] == NULL)
//		{
//			CONSOLE_LOG(LOG_LEVEL_ERROR, L"thread[WORKER_THREAD_%d] NULL:%d ",i, WSAGetLastError());
//			return false;
//		}
//	}

	return true;
}

SessionInfo* AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr)
{
	WCHAR clientIP[IP_BUFFER_SIZE] = { 0, };
	SessionInfo* sessionInfo = nullptr;
	sessionInfo = new SessionInfo;
	_ASSERT(sessionInfo != nullptr);

	InetNtop(AF_INET, &clientAddr.sin_addr, clientIP, 16);

	sessionInfo->clientSock = clientSock;
	wcscpy_s(sessionInfo->ip, _countof(sessionInfo->ip), clientIP);
	sessionInfo->port = ntohs(clientAddr.sin_port);
	sessionInfo->sessionID = ++gSessionID_Num;
	sessionInfo->recvRingBuffer = new RingBuffer;
	sessionInfo->sendRingBuffer = new RingBuffer;
	// 락 초기화
	InitializeSRWLock(&sessionInfo->srwLock);

	AcquireSRWLockExclusive(&sessionInfo->srwLock);
	gSessionData.emplace(sessionInfo->sessionID, sessionInfo);
	ReleaseSRWLockExclusive(&sessionInfo->srwLock);
	
	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[sessionID:%d] session insert size:%d", sessionInfo->sessionID, (int)gSessionData.size());
	
	return sessionInfo;
}


unsigned __stdcall AcceptThread(void* arguments)
{
	int addrlen;
	SOCKET clientSock = INVALID_SOCKET;
	WCHAR clientIP[IP_BUFFER_SIZE] = { 0, };
	SOCKADDR_IN clientaddr;
	int retVal;
	DWORD flags = 0;
	SessionInfo* sessionInfo = nullptr;
	WSABUF wsaBuffer[2];

	while (true)
	{
		addrlen = sizeof(clientaddr);
		clientSock = accept(gListenSock, (SOCKADDR*)&clientaddr, &addrlen);
		if (clientSock == INVALID_SOCKET)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"accept error:%d", WSAGetLastError());
			return EXIT_FAILURE;
		}
		InetNtop(AF_INET, &clientaddr.sin_addr, clientIP, 16);
		CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"[CHAT SERVER] Client IP: %s Clinet Port:%d", clientIP, ntohs(clientaddr.sin_port));

		// 세션 정보 추가
		sessionInfo = AddSessionInfo(clientSock, clientaddr);
		if (sessionInfo == nullptr)
			return EXIT_FAILURE;

		// 소켓과 입출력 완료 포트 연결
		CreateIoCompletionPort((HANDLE)clientSock, gHcp, (ULONG_PTR)sessionInfo, THREAD_MAX - 1);

		// 비동기 입출력 시작
		// 링버퍼 버퍼 포인터 2개 적용해서 다이렉트로 받아오도록 작업 진행
		wsaBuffer[0].buf = sessionInfo->recvRingBuffer->GetNotBroken_BufferPtr();
		wsaBuffer[0].len = sessionInfo->recvRingBuffer->GetNotBroken_WriteSize();
		wsaBuffer[1].buf = sessionInfo->recvRingBuffer->GetBroken_BufferPtr();
		wsaBuffer[1].len = sessionInfo->recvRingBuffer->GetBroken_WriteSize();

		// IO 카운트 진행
		InterlockedIncrement(&gIOCount);

		ZeroMemory(&sessionInfo->recvOverlapped, sizeof(sessionInfo->recvOverlapped));
		retVal = WSARecv(clientSock, wsaBuffer, 2, NULL,
			&flags, &sessionInfo->recvOverlapped, NULL);
		if (retVal == SOCKET_ERROR)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSARecv error:%d", WSAGetLastError());
				InterlockedDecrement(&gIOCount);
				return EXIT_FAILURE;
			}
			else
			{
				CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WSARecv IO_PENDING");
			}
		}
	}

	return 0;
}

unsigned __stdcall WorkerThread(void* arguments)
{
	int retVal;
	DWORD transferredBytes = 0;
	OVERLAPPED* overlapped = nullptr;
	SessionInfo* sessionInfo = nullptr;
	DWORD completionKey = 0;
	WSABUF wsaBuffer[2];
	DWORD flags = 0;

	while (true)
	{
		transferredBytes = 0;
		overlapped = nullptr;
		completionKey = 0;
		sessionInfo = nullptr;

		// 입출력 완료 대기
		retVal = GetQueuedCompletionStatus(gHcp, &transferredBytes, (PULONG_PTR)&sessionInfo,
			&overlapped, INFINITE);
		/*if (sessionInfo == nullptr)
			continue;*/
		//AcquireSRWLockExclusive(&sessionInfo->srwLock);

		CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"\n\nWorkerThread Start\n");

		// 타임아웃이 들어가게 되면 다른 예외처리 고려->PostQueue(iocp, 0, 0, 0...)인 경우에는 의도한 경우
		if (overlapped == nullptr && sessionInfo == nullptr && transferredBytes == 0)
		{
			//워커쓰레드 종료
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WorkerThread EXIT!");
			return 0;
		}

		_ASSERT(sessionInfo != nullptr);
		if (transferredBytes == 0)
		{
			//연결종료
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"User transferredBytes0![SesssionID:%d]", sessionInfo->sessionID);
		}
		else if (overlapped == &(sessionInfo->recvOverlapped))
		{
			sessionInfo->recvRingBuffer->MoveWritePos(transferredBytes);

			// recvProcess 링버퍼로 담은 부분 보내기 처리 진행.
			RecvProcess(sessionInfo);

			// 비동기 입출력 시작
			wsaBuffer[0].buf = sessionInfo->recvRingBuffer->GetNotBroken_BufferPtr();
			wsaBuffer[0].len = sessionInfo->recvRingBuffer->GetNotBroken_WriteSize();
			wsaBuffer[1].buf = sessionInfo->recvRingBuffer->GetBroken_BufferPtr();
			wsaBuffer[1].len = sessionInfo->recvRingBuffer->GetBroken_WriteSize();

			ZeroMemory(&sessionInfo->recvOverlapped, sizeof(sessionInfo->recvOverlapped));
			// IOCount 증가
			InterlockedIncrement(&gIOCount);
			retVal = WSARecv(sessionInfo->clientSock, wsaBuffer, 2,
				NULL, &flags, &sessionInfo->recvOverlapped, NULL);
			if (retVal == SOCKET_ERROR)
			{
				// 강제 연결 접속 종료
				if (WSAGetLastError() == WSAECONNRESET)
				{
					// IOCount 감소
					InterlockedDecrement(&gIOCount);
					CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WSAECONNRESET![SesssionID:%d]", sessionInfo->sessionID);
				}
				else if (WSAGetLastError() != ERROR_IO_PENDING)
				{
					InterlockedDecrement(&gIOCount);
					CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSARecv error:%d", WSAGetLastError());
				}
				else
				{
					CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WSARecv IO_PENDING");
				}
			}
		}
		else if (overlapped == &(sessionInfo->sendOverlapped))
		{	
			sessionInfo->sendRingBuffer->MoveReadPos(transferredBytes);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WSA Send Clear[%d Bytes]", transferredBytes);

			CONSOLE_LOG(LOG_LEVEL_WARNING, L"SendRingBuffer UseSize:%d  WirtePos:%d  ReadPos:%d",
				sessionInfo->sendRingBuffer->GetUseSize(), sessionInfo->sendRingBuffer->GetWriteSize(),
				sessionInfo->sendRingBuffer->GetReadSize());

			if (SendProcess(sessionInfo) == false)
			{
				// 하단부에서 IO Count 를 감소시키기 때문에 Send 처리가 안되도 인터락 증가
				InterlockedIncrement(&gIOCount);
			}
		}

		// 입출력 통보가 왔으므로 IO Count 감소
		if (InterlockedDecrement(&gIOCount) == 0) // IO 가 0이므로 종료되었다고 판단하여 모두 삭제 진행
		{
			Release(sessionInfo);
		}
	}

	return 0;
}

void RecvProcess(SessionInfo* sessionInfo)
{
	int retVal = 0;
	bool isPacketWritePos = false;
	HeaderInfo header;
	PacketBuffer packetBuffer(PacketBuffer::BUFFER_SIZE_DEFAULT);

	while (sessionInfo->recvRingBuffer->GetUseSize() >= sizeof(header))
	{
		retVal = sessionInfo->recvRingBuffer->Peek((char*)&header, sizeof(header));

		if (retVal != sizeof(header))
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Header Peek size Error [returnVal:%d]", retVal);
			return;
		}

		//헤더와 페이로드 사이즈가 합친 사이즈보다 적으면 다음 수행을 할 수 없다. 다음에 처리 진행
		if (header.length + sizeof(header) > sessionInfo->recvRingBuffer->GetUseSize())
		{
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"length error![length:%d]", header.length);
			return;
		}

		// Peek 이동이기 때문에 읽은 만큼 직접 이동
		sessionInfo->recvRingBuffer->MoveReadPos(sizeof(header));

		packetBuffer.Clear();
		retVal = sessionInfo->recvRingBuffer->Peek(packetBuffer.GetBufferPtr(), header.length);

		if (retVal != header.length)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Peek length size Error [returnVal:%d][length:%d]",
				retVal, header.length);
			return;

		}
		// Peek 이동이기 때문에 읽은 만큼 직접 이동
		sessionInfo->recvRingBuffer->MoveReadPos(retVal);

		// 패킷 버퍼도 버퍼에 직접담은 부분이기 때문에 writepos을 직접 이동시켜준다.
		isPacketWritePos = packetBuffer.MoveWritePos(retVal);
		if (isPacketWritePos == false)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Packet MoveWritePos OverFlow[%d]", retVal);
			return;
		}
		CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"RECV:%s Size:%d", (WCHAR*)packetBuffer.GetBufferPtr(), retVal);

		CONSOLE_LOG(LOG_LEVEL_WARNING, L"[sessionID:%d] RecvRingBuf WriteSize:%d ReadSize:%d",
			sessionInfo->sessionID,
			sessionInfo->recvRingBuffer->GetWriteSize(),
			sessionInfo->recvRingBuffer->GetReadSize());

		// sendRingBuffer에 담기
		retVal = sessionInfo->sendRingBuffer->Enqueue((char*)&header, sizeof(header));
		if (retVal < sizeof(header))
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"[SessionID:%d] Enqueue header Error[%d] RingBuf useSize:%d",
				sessionInfo->sessionID,
				retVal,
				sessionInfo->sendRingBuffer->GetUseSize());
		}

		retVal = sessionInfo->sendRingBuffer->Enqueue(packetBuffer.GetBufferPtr(), packetBuffer.GetDataSize());

		if (retVal < packetBuffer.GetDataSize())
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"[SessionID:%d] Enqueue packetBuffer Error[%d] RingBuf useSize:%d",
				sessionInfo->sessionID,
				retVal,
				sessionInfo->sendRingBuffer->GetUseSize());
		}

		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[SessionID:%d] SendRingBuffer Enqueue Size:%d RingBuf useSize:%d",
			sessionInfo->sessionID,
			packetBuffer.GetDataSize(),
			sessionInfo->sendRingBuffer->GetUseSize());

		SendProcess(sessionInfo);
	}
}

bool SendProcess(SessionInfo* sessionInfo)
{
	int retVal = 0;
	WSABUF wsaBuffer;
	DWORD flags = 0;

	if (sessionInfo->sendRingBuffer->GetUseSize() <= 0)
	{
		return false;
	}

	// 링버퍼의 최대사이즈를 보냄
	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"SEND:%s size:%d", (WCHAR*)sessionInfo->sendRingBuffer->GetBufferPtr(), 
		sessionInfo->sendRingBuffer->GetUseSize());
	
	CONSOLE_LOG(LOG_LEVEL_WARNING, L"SendRingBuffer UseSize:%d", sessionInfo->sendRingBuffer->GetUseSize());
	// 비동기 입출력 시작
	wsaBuffer.buf = sessionInfo->sendRingBuffer->GetBufferPtr();
	wsaBuffer.len = sessionInfo->sendRingBuffer->GetUseSize();

	ZeroMemory(&sessionInfo->sendOverlapped, sizeof(sessionInfo->sendOverlapped));
	// IOCount 증가
	InterlockedIncrement(&gIOCount);
	retVal = WSASend(sessionInfo->clientSock, &wsaBuffer, 1,
		NULL, flags, &sessionInfo->sendOverlapped, NULL);
	if (retVal == SOCKET_ERROR)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSASend error:%d", WSAGetLastError());
		}
		else
		{
			// IOCount 감소
			InterlockedDecrement(&gIOCount);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WSA Send IO_PENDIHNG");
		}
	}

	return true;
}

void Release(SessionInfo* sessionInfo)
{
	_ASSERT(sessionInfo != nullptr);
	int sessionID = sessionInfo->sessionID;
	
	// 세션 데이터 삭제
	AcquireSRWLockExclusive(&sessionInfo->srwLock);
	auto iterSessionData = gSessionData.find(sessionID);
	if (iterSessionData == gSessionData.end())
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Release session find error![sessionID:%d]", sessionID);
		return;
	}
	closesocket(iterSessionData->second->clientSock);
	SafeDelete(sessionInfo->recvRingBuffer);
	SafeDelete(sessionInfo->sendRingBuffer);

	gSessionData.erase(iterSessionData);
	ReleaseSRWLockExclusive(&sessionInfo->srwLock);
	SafeDelete(sessionInfo);

	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"Release [SessionID:%d]", sessionID);
}

int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	timeBeginPeriod(1); // 타이머 해상도 높이기
    
	//if (Initialize() == false)
	//	return EXIT_FAILURE;

	WaitForMultipleObjects(THREAD_MAX, gThread, TRUE, INFINITE);

	closesocket(gListenSock);
	WSACleanup();

#ifdef SINGLE_THREAD
	CloseHandle(gThread[WORKER_THREAD_1]);
#else
	CloseHandle(gThread[WORKER_THREAD_1]);
	CloseHandle(gThread[WORKER_THREAD_2]);
	CloseHandle(gThread[WORKER_THREAD_3]);
	CloseHandle(gThread[WORKER_THREAD_4]);
	CloseHandle(gThread[WORKER_THREAD_5]);
#endif
	CloseHandle(gThread[ACCEPT_THREAD]);

	timeEndPeriod(1);
	return EXIT_SUCCESS;
}
