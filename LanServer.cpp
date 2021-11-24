#include "pch.h"
#include "LanServer.h"


LanServer::~LanServer()
{
	closesocket(mListenSock);
	WSACleanup();

	for (DWORD i = 0; i < mMaxThreadNum; i++)
	{
		CloseHandle(LanServer::mThread[i]);
	}

	delete[] mThread;
}

bool LanServer::Start(const WCHAR* outServerIP, const WORD port, const DWORD workThreadNum, const bool isNodelay, const DWORD maxUserNum)
{
	WSADATA wsaData;
	SOCKADDR_IN serveraddr;
	LINGER timeWaitOptval;
	WCHAR serverIP[IP_BUFFER_SIZE] = { 0, };
	int sendBufferOptval;
	bool noDelayOptval;
	int retVal;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSAStartup() errcode[%d]", WSAGetLastError());
		return false;
	}

	mListenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (mListenSock == INVALID_SOCKET)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"listen_sock error:%d", WSAGetLastError());
		return false;
	}

	// timewait 남기지 않도록 설정
	timeWaitOptval.l_onoff = 1;
	timeWaitOptval.l_linger = 0;
	retVal = setsockopt(mListenSock, SOL_SOCKET, SO_LINGER, (char*)&timeWaitOptval, sizeof(timeWaitOptval));
	if (retVal == SOCKET_ERROR)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"timewait option error:%d", WSAGetLastError());
		return false;
	}

	// 송신버퍼 0 으로 초기화 -> Overlapped I/O 유도
	sendBufferOptval = 0;
	retVal = setsockopt(mListenSock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufferOptval, sizeof(sendBufferOptval));
	if (retVal == SOCKET_ERROR)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"SO_SNDBUF option error:%d", WSAGetLastError());
		return false;
	}

	// 니글알고리즘 중지
	if (isNodelay == false)
	{
		noDelayOptval = true;
		setsockopt(mListenSock, IPPROTO_TCP, TCP_NODELAY, (char*)&noDelayOptval, sizeof(noDelayOptval));
	}

	// bind
	lstrcpyW(serverIP, outServerIP);
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);
	InetNtop(AF_INET, &serveraddr.sin_addr, serverIP, _countof(serverIP));

	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"[CHAT SERVER] SERVER IP: %s SERVER Port:%d", serverIP, ntohs(serveraddr.sin_port));

	retVal = bind(mListenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retVal == SOCKET_ERROR)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"bind error:%d", WSAGetLastError());
		return false;
	}

	//listen
	retVal = listen(mListenSock, SOMAXCONN);
	if (retVal == SOCKET_ERROR)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"listen error:%d ", WSAGetLastError());
		return false;
	}
	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"server open\n");

	// IOCP handle 초기화
	mHcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, THREAD_MAX);
	if (mHcp == NULL)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Hcp NULL:%d ", WSAGetLastError());
		return false;
	}
	
	// 최대 유저수 저장
	mMaxUserNum = maxUserNum;

	// 쓰레드 최대 개수 저장
	mMaxThreadNum = workThreadNum + 1;

	// Thread 생성
	mThread = new HANDLE[mMaxThreadNum];

	for (DWORD i = 0; i < workThreadNum; i++)
	{
		mThread[i] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, this, 0, &mThreadID);
	}
	mThread[workThreadNum] = (HANDLE)_beginthreadex(NULL, 0, &AcceptThread, this, 0, &mThreadID);

	for (DWORD i = 0; i < mMaxThreadNum; i++)
	{
		if (LanServer::mThread[i] == NULL)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"thread[WORKER_THREAD_%d] NULL:%d ", i, WSAGetLastError());
			return false;
		}
	}

	return true;
}

bool LanServer::Stop()
{
	// 쓰레드 종료하기 전에 모든 세션 데이터 Release 진행
	for (auto iterSessionData : mSessionData)
	{
		Release(iterSessionData.second);
	}
	
	mShutDown = true;

	return false;
}

bool LanServer::SendPacket(const DWORD64 sessionID, PacketBuffer& packetBuffer)
{
	SessionInfo* sessionInfo = nullptr;
	HeaderInfo header;
	int retVal;

	AcquireSRWLockExclusive(&mSessionDataLock);
	auto iterSessionData = mSessionData.find(sessionID);
	ReleaseSRWLockExclusive(&mSessionDataLock);

	if (iterSessionData == mSessionData.end())
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"SendPacket mSessionData find error![sessionID:%d]", sessionID);
		return false;
	}
	
	sessionInfo = iterSessionData->second;

	EnterCriticalSection(&sessionInfo->csLock);
	// header 담기
	header.length = PACKET_SIZE;
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

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[SessionID:%d] PacketBuffer Size:%d SendRingBuffer useSize:%d",
		sessionInfo->sessionID,
		packetBuffer.GetDataSize(),
		sessionInfo->sendRingBuffer->GetUseSize());

	// Wsa Send 함수
	SendProcess(sessionInfo);

	LeaveCriticalSection(&sessionInfo->csLock);
	return true;
}

bool LanServer::Disconnect(const DWORD64 sessionID)
{
	SessionInfo* sessionInfo;

	// 세션 데이터 삭제
	auto iterSessionData = mSessionData.find(sessionID);
	if (iterSessionData == mSessionData.end())
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Disconnect session find error![sessionID:%d]", sessionID);
		return false;
	}
	sessionInfo = iterSessionData->second;
	_ASSERT(sessionInfo != nullptr);
	

	closesocket(iterSessionData->second->clientSock);
	SafeDelete(sessionInfo->recvRingBuffer);
	SafeDelete(sessionInfo->sendRingBuffer);

	AcquireSRWLockExclusive(&mSessionDataLock);
	mSessionData.erase(iterSessionData);
	ReleaseSRWLockExclusive(&mSessionDataLock);

	SafeDelete(sessionInfo);

	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"Disconnect [SessionID:%d]", sessionID);

	return true;
}

unsigned __stdcall LanServer::AcceptThread(void* arguments)
{
	return ((LanServer*)arguments)->AcceptThread_Working();
}

unsigned __stdcall LanServer::WorkerThread(void* arguments)
{
	return ((LanServer*)arguments)->WorkerThread_Working();
}

int LanServer::AcceptThread_Working()
{
	int addrlen;
	SOCKET clientSock = INVALID_SOCKET;
	WCHAR clientIP[IP_BUFFER_SIZE] = { 0, };
	SOCKADDR_IN clientaddr;
	SessionInfo* sessionInfo = nullptr;

	while (mShutDown == false)
	{
		addrlen = sizeof(clientaddr);
		clientSock = accept(mListenSock, (SOCKADDR*)&clientaddr, &addrlen);
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
		CreateIoCompletionPort((HANDLE)clientSock, mHcp, (ULONG_PTR)sessionInfo, THREAD_MAX - 1);

		RecvPost(sessionInfo);
	}

	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"AcceptThread EXIT![ThreadID:%d]", GetCurrentThreadId());

	return 0;
}

int LanServer::WorkerThread_Working()
{
	int retVal;
	DWORD transferredBytes = 0;
	OVERLAPPED* overlapped = nullptr;
	SessionInfo* sessionInfo = nullptr;

	while (mShutDown == false)
	{
		transferredBytes = 0;
		overlapped = nullptr;
		sessionInfo = nullptr;

		// 입출력 완료 대기
		retVal = GetQueuedCompletionStatus(mHcp, &transferredBytes, (PULONG_PTR)&sessionInfo,
			&overlapped, INFINITE);

		CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"\n\nWorkerThread Start\n");

		// 타임아웃이 들어가게 되면 다른 예외처리 고려->PostQueue(iocp, 0, 0, 0...)인 경우에는 의도한 경우
		if (overlapped == nullptr && sessionInfo == nullptr && transferredBytes == 0)
		{
			//워커쓰레드 종료
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WorkerThread EXIT![ThreadID:%d]", GetCurrentThreadId());
			return 0;
		}
	
		_ASSERT(sessionInfo != nullptr);

		// 세션 락
		EnterCriticalSection(&sessionInfo->csLock);

		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"Lock Start![SesssionID:%d]", sessionInfo->sessionID);

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

			// 비동기 Send 시작 OnRecv 함수에서 SendRingBuffer 진행
			//SendProcess(sessionInfo);

			// 비동기 Recv 시작
			RecvPost(sessionInfo);

		
		}
		else if (overlapped == &(sessionInfo->sendOverlapped))
		{
			sessionInfo->sendRingBuffer->MoveReadPos(transferredBytes);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WSA Send Clear[%d Bytes]", transferredBytes);

			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"SendRingBuffer UseSize:%d  WirtePos:%d  ReadPos:%d",
				sessionInfo->sendRingBuffer->GetUseSize(), sessionInfo->sendRingBuffer->GetWriteSize(),
				sessionInfo->sendRingBuffer->GetReadSize());

			// 완료 통지가 왔기 때문에 sendFlag 값 초기화
			sessionInfo->sendFlag = false;

			// 비동기 Send 시작 OnRecv 함수에서 SendRingBuffer 진행
			SendProcess(sessionInfo);
		}
		// 입출력 통보가 왔으므로 IO Count 감소
		if (InterlockedDecrement(&sessionInfo->ioCount) == 0) // IO 가 0이므로 종료되었다고 판단하여 모두 삭제 진행
		{
			Release(sessionInfo);
		}
		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"Lock End![SesssionID:%d]", sessionInfo->sessionID);
		LeaveCriticalSection(&sessionInfo->csLock);
	}

	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WorkerThread EXIT![ThreadID:%d]", GetCurrentThreadId());

	return 0;
}

SessionInfo* LanServer::AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr)
{
	WCHAR clientIP[IP_BUFFER_SIZE] = { 0, };
	SessionInfo* sessionInfo = nullptr;
	sessionInfo = new SessionInfo;
	_ASSERT(sessionInfo != nullptr);

	InetNtop(AF_INET, &clientAddr.sin_addr, clientIP, 16);

	sessionInfo->clientSock = clientSock;
	wcscpy_s(sessionInfo->ip, _countof(sessionInfo->ip), clientIP);
	sessionInfo->port = ntohs(clientAddr.sin_port);
	sessionInfo->sessionID = ++mSessionID_Num;
	sessionInfo->recvRingBuffer = new RingBuffer;
	sessionInfo->sendRingBuffer = new RingBuffer;
	// 락 초기화
	InitializeCriticalSection(&sessionInfo->csLock);

	AcquireSRWLockExclusive(&mSessionDataLock);
	mSessionData.emplace(sessionInfo->sessionID, sessionInfo);
	ReleaseSRWLockExclusive(&mSessionDataLock);

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[sessionID:%d] session insert size:%d", sessionInfo->sessionID, (int)mSessionData.size());

	return sessionInfo;
}

void LanServer::SendProcess(SessionInfo* sessionInfo)
{
	// 완료통지가 오면 보내는 1:1 구조로 보내기 위해 sendFlag로 체크
	// 비동기 Send 시작 
	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"SendPost Call SendFalg:%d", sessionInfo->sendFlag);
	if (InterlockedCompareExchange(&sessionInfo->sendFlag, TRUE, FALSE) == FALSE)
	{
		if(SendPost(sessionInfo) == FALSE)
			sessionInfo->sendFlag = FALSE;
	}
}

bool LanServer::SendPost(SessionInfo* sessionInfo)
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

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"SendRingBuffer UseSize:%d", sessionInfo->sendRingBuffer->GetUseSize());
	// 비동기 입출력 시작
	wsaBuffer.buf = sessionInfo->sendRingBuffer->GetBufferPtr();
	wsaBuffer.len = sessionInfo->sendRingBuffer->GetUseSize();

	ZeroMemory(&sessionInfo->sendOverlapped, sizeof(sessionInfo->sendOverlapped));
	// IOCount 증가
	InterlockedIncrement(&sessionInfo->ioCount);
	retVal = WSASend(sessionInfo->clientSock, &wsaBuffer, 1,
		NULL, flags, &sessionInfo->sendOverlapped, NULL);
	if (retVal == SOCKET_ERROR)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSASend error:%d", WSAGetLastError());
			// IOCount 감소
			if (InterlockedDecrement(&sessionInfo->ioCount) == 0) // IO 가 0이므로 종료되었다고 판단하여 모두 삭제 진행
			{
				Release(sessionInfo);
			}
		}
		else
		{

			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WSA Send IO_PENDIHNG");
		}
	}

	return true;
}

void LanServer::RecvPost(SessionInfo* sessionInfo)
{
	int retVal;
	DWORD flags = 0;
	WSABUF wsaBuffer[2];

	// 비동기 입출력 시작
	// 링버퍼 버퍼 포인터 2개 적용해서 다이렉트로 받아오도록 작업 진행
	wsaBuffer[0].buf = sessionInfo->recvRingBuffer->GetNotBroken_BufferPtr();
	wsaBuffer[0].len = sessionInfo->recvRingBuffer->GetNotBroken_WriteSize();
	wsaBuffer[1].buf = sessionInfo->recvRingBuffer->GetBroken_BufferPtr();
	wsaBuffer[1].len = sessionInfo->recvRingBuffer->GetBroken_WriteSize();

	// IO 카운트 진행
	InterlockedIncrement(&sessionInfo->ioCount);

	ZeroMemory(&sessionInfo->recvOverlapped, sizeof(sessionInfo->recvOverlapped));
	retVal = WSARecv(sessionInfo->clientSock, wsaBuffer, 2, NULL,
		&flags, &sessionInfo->recvOverlapped, NULL);
	if (retVal == SOCKET_ERROR)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSARecv error:%d", WSAGetLastError());
			if (InterlockedDecrement(&sessionInfo->ioCount) == 0) // IO 가 0이므로 종료되었다고 판단하여 모두 삭제 진행
			{
				Release(sessionInfo);
			}
			return;
		}
		else
		{
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WSARecv IO_PENDING");
		}
	}
}

void LanServer::RecvProcess(SessionInfo* sessionInfo)
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
		// Peek 이동이기 때문에 header 읽은 만큼 추가로 더해준다.
		sessionInfo->recvRingBuffer->MoveReadPos(retVal);

		packetBuffer.Clear();
		retVal = sessionInfo->recvRingBuffer->Peek(packetBuffer.GetBufferPtr(), header.length);

		if (retVal != header.length)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Peek length size Error [returnVal:%d][length:%d]",
				retVal, header.length);
			return;

		}
	
		// Peek 이동이기 때문에 packet 읽은 만큼 직접 이동
		sessionInfo->recvRingBuffer->MoveReadPos(retVal);

		// 패킷 버퍼도 버퍼에 직접담은 부분이기 때문에 writepos을 직접 이동시켜준다.
		isPacketWritePos = packetBuffer.MoveWritePos(retVal);
		if (isPacketWritePos == false)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Packet MoveWritePos OverFlow[%d]", retVal);
			return;
		}
		CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"RECV:%s Size:%d", (WCHAR*)packetBuffer.GetBufferPtr(), retVal);

		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[sessionID:%d] RecvRingBuf WriteSize:%d ReadSize:%d",
			sessionInfo->sessionID,
			sessionInfo->recvRingBuffer->GetWriteSize(),
			sessionInfo->recvRingBuffer->GetReadSize());

		// Recv 처리 완료 후 처리 함수
		OnRecv(sessionInfo->sessionID, packetBuffer);
	}
}

void LanServer::Release(SessionInfo* sessionInfo)
{
	_ASSERT(sessionInfo != nullptr);
	DWORD64 sessionID = sessionInfo->sessionID;

	// 세션 데이터 삭제
	AcquireSRWLockExclusive(&mSessionDataLock);
	auto iterSessionData = mSessionData.find(sessionID);
	ReleaseSRWLockExclusive(&mSessionDataLock);

	if (iterSessionData == mSessionData.end())
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Release session find error![sessionID:%d]", sessionID);
		return;
	}
	closesocket(iterSessionData->second->clientSock);
	SafeDelete(sessionInfo->recvRingBuffer);
	SafeDelete(sessionInfo->sendRingBuffer);
	DeleteCriticalSection(&sessionInfo->csLock);
	SafeDelete(sessionInfo);

	AcquireSRWLockExclusive(&mSessionDataLock);
	mSessionData.erase(iterSessionData);
	ReleaseSRWLockExclusive(&mSessionDataLock);

	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"Release [SessionID:%d]", sessionID);

	
	

	
}
