#include "pch.h"
#include "IocpServer.h"

#pragma once

IocpServer::~IocpServer()
{
	closesocket(mListenSock);
	WSACleanup();

	for (int i = 0; i < MAX_SESSION_DATA_NUM; i++)
	{
		SafeDelete(mSessionArray[i]->sendRingBuffer);
		SafeDelete(mSessionArray[i]->recvRingBuffer);
		SafeDelete(mSessionArray[i]);
	}
}

bool IocpServer::Start(const WCHAR* outServerIP, const WORD port, const DWORD workThreadNum, const bool isNodelay, const DWORD maxUserNum)
{
	WSADATA wsaData;
	SOCKADDR_IN serveraddr;
	LINGER timeWaitOptval;
	WCHAR serverIP[IP_BUFFER_SIZE] = { 0, };
	int sendBufferOptval;
	bool noDelayOptval;
	int retVal;
	SessionInfo* sessionInfo = nullptr;

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

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	for (WORD i = 0; i < MAX_SESSION_DATA_NUM; i++)
	{
		mSessionIndexInfo[i] = mSessionIndex.Alloc();
		mSessionIndexInfo[i]->index = i;
	}

	// IOCP handle 초기화 Completion Port 생성
	mHcp = CreateNewCompletionPort(workThreadNum);
	if (mHcp == NULL)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Hcp NULL:%d ", WSAGetLastError());
		return false;
	}
	// 최대 유저수 저장
	mMaxUserNum = maxUserNum;

	// 작업자 쓰레드를 러닝 쓰레드 개수보다 많이 만들어주어 러닝 쓰레드가 작업자 쓰레드보다 많아질 경우를 대비
	for (WORD i = 0; i < (workThreadNum * ADD_WORKER_THREAD_NUM); i++)
	{
		mThreadData.emplace_back(IOThread, this);
	}
	// Accept 쓰레드 생성
	for (WORD i = 0; i < ACCEPT_TRHEAD_NUM; i++)
	{
		mThreadData.emplace_back(AcceptThread, this);
	}

	// 서버 시작 시 세션 정보 세팅(동적할당)하여 유저접속 시 오버헤드를 낮춤
	for (int i = 0; i < MAX_SESSION_DATA_NUM; i++)
	{
		sessionInfo = new SessionInfo;
		_ASSERT(sessionInfo != nullptr);
		sessionInfo->clientSock = INVALID_SOCKET; 

		sessionInfo->recvRingBuffer = new RingBuffer;
		_ASSERT(sessionInfo->recvRingBuffer != nullptr);

		sessionInfo->sendRingBuffer = new RingBuffer;
		_ASSERT(sessionInfo->recvRingBuffer != nullptr);
		mSessionArray[i] = sessionInfo;
	}
	
	// 세션 인덱스는 Accept Thread와 Worker Thread의 경합 과정이 생기므로 LockFreeStack MemoryPool 사용
	for (int i = MAX_SESSION_DATA_NUM - 1; i >= 0; i--)
	{
		mSessionIndex.Free(mSessionIndexInfo[i]); // 세팅해 놓은 세션인덱스를 MemoryPool 에 세팅(반환)
	}

	// PacketBuffer LockFreeStack MemoryPool 초기화 진행
	mPacketBuffer.Initialize(PACKET_MAX_NUM, true);

	return true;
}

bool IocpServer::Stop()
{
	// 쓰레드 종료하기 전에 모든 세션 데이터 Release 진행
	mShutDown = true;

	return false;
}

bool IocpServer::SendPacket(const DWORD64 sessionID, PacketBuffer* packetBuffer)
{
	SessionInfo* sessionInfo = nullptr;
	HeaderInfo header;
	int retVal = 0;
	WORD sessionArrayIndex = 0;

	sessionArrayIndex = sessionID & SESSION_ARRAY_INDEX_MASK;

	if (sessionArrayIndex < 0 || sessionArrayIndex >= MAX_SESSION_DATA_NUM)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Session Array Index Overflow![sessionID:%lu][index:%d]",
			sessionID, sessionArrayIndex);
		return false;
	}
	sessionInfo = mSessionArray[sessionArrayIndex];

	// IOThread에서 세션이 종료되는 경우를 대비하여 ioCount를 증가시키고 ioCount return값이 1이라면 종료되었다고 판단
	//if (InterlockedIncrement(&sessionInfo->ioCount) == 1)	
	//{
	//	return false;
	//}
	// SendPacket에 진입하는 순간 Disconnect 후 Accept 되는 순간이 희박하게 발생하므로 예외처리 진행.
	if (sessionInfo->sessionID != sessionID)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"SessionID Error![sessionID:%lu][index:%d]",
			sessionID, sessionArrayIndex);
		return false;
	}

	// packtBuffer 의 주소를 Enqueue 함으로서 복사비용 절약
	retVal = sessionInfo->sendRingBuffer->Enqueue((char*)&packetBuffer, POINTER_SIZE);
	if (retVal != POINTER_SIZE)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Enqueue packetBuffer Error[SessionID:%lu][retVal:%d][useSize:%d]",
			sessionInfo->sessionID, retVal, sessionInfo->sendRingBuffer->GetUseSize());
		return false;
	};

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[SessionID:%lu] PacketBuffer Size:%d SendRingBuffer useSize:%d",
		sessionInfo->sessionID, packetBuffer->GetDataSize(), sessionInfo->sendRingBuffer->GetUseSize());

	InterlockedIncrement(&mPacketProcessTPS);

	// Wsa Send 함수 진행
	SendProcess(sessionInfo);
	return true;
}

unsigned __stdcall IocpServer::AcceptThread(void* arguments)
{
	return ((IocpServer*)arguments)->AcceptThread_Working();
}

unsigned __stdcall IocpServer::IOThread(void* arguments)
{
	return ((IocpServer*)arguments)->IOThread_Working();
}

HANDLE IocpServer::CreateNewCompletionPort(const DWORD numberOfConcurrentThreads)
{
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, numberOfConcurrentThreads);
}

int IocpServer::AcceptThread_Working()
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
			continue;
		}
		InetNtop(AF_INET, &clientaddr.sin_addr, clientIP, 16);
		
		// 컨텐츠 부(EchoServer)에서 접속 요청 확인(핵유저 IP 등 제한 IP 검사)
		if (OnConnectionRequest(clientIP, ntohs(clientaddr.sin_port)) == false)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Deny access![Client IP:%s][Client Port:%d]", 
				clientIP, ntohs(clientaddr.sin_port));
			closesocket(clientSock);
			continue;
		}
		// 세션 정보 추가
		sessionInfo = AddSessionInfo(clientSock, clientaddr);
		if (sessionInfo == nullptr)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"sessionInfo nullptr![Client IP:%s][Client Port:%d]", 
				clientIP, ntohs(clientaddr.sin_port));
			closesocket(clientSock);
			continue;
		}
		// 소켓과 입출력 완료 포트 연결
		CreateIoCompletionPort((HANDLE)clientSock, mHcp, (ULONG_PTR)sessionInfo, 0);

		// 컨텐츠 부에서 접속 후 유저 기본 세팅 진행
		OnConnectionSuccess(sessionInfo->sessionID);
		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[CHAT SERVER] Client IP: %s Clinet Port:%d", clientIP, ntohs(clientaddr.sin_port));

		// WSA RECV 비동기 시작
		RecvPost(sessionInfo);
	}

	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"AcceptThread EXIT![ThreadID:%d]", GetCurrentThreadId());

	return 0;
}

int IocpServer::IOThread_Working()
{
	BOOL retVal = FALSE;
	DWORD transferredBytes = 0;
	OVERLAPPED* overlapped = nullptr;
	SessionInfo* sessionInfo = nullptr;
	PacketBuffer* packetBuffer = nullptr;
	int retSize = 0;

	while (mShutDown == false)
	{
		transferredBytes = 0;
		overlapped = nullptr;
		sessionInfo = nullptr;

		// 입출력 완료 대기
		retVal = GetQueuedCompletionStatus(mHcp, &transferredBytes, (PULONG_PTR)&sessionInfo,
			&overlapped, INFINITE);

		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"\n\nWorkerThread Start[threadID:%d]\n", GetCurrentThreadId());

		// IOCP 에러 쓰레드 종료
		if (retVal == FALSE && (overlapped == nullptr || sessionInfo == nullptr))
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"IOCP HANDLE Error[Error:%d]", WSAGetLastError());
			return 0;
		}

		// PostQueue(iocp, 0, 0, 0...)인 경우에는 의도한 경우 IO쓰레드 종료
		if (overlapped == nullptr && sessionInfo == nullptr && transferredBytes == 0)
		{
			//워커쓰레드 종료
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WorkerThread EXIT![ThreadID:%d]", GetCurrentThreadId());
			return 0;
		}
		_ASSERT(sessionInfo != nullptr);

		if (transferredBytes == 0)
		{
			//연결종료
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"User Exit transferredBytes0![SesssionID:%lu]", sessionInfo->sessionID);
		}
		else if (overlapped == &(sessionInfo->recvOverlapped))
		{
			InterlockedIncrement(&mRecvCompleteTPS);
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"sessionInfo->recvOverlapped[transferredBytes:%d][SesssionID:%lu]",
				transferredBytes
				,sessionInfo->sessionID);

			sessionInfo->recvRingBuffer->MoveWritePos(transferredBytes);

			// 링버퍼로 담은 부분 데이터 추출 및 컨텐츠 부로 전달
			RecvProcess(sessionInfo);

			// 비동기 Recv 시작
			RecvPost(sessionInfo);
		}
		else if (overlapped == &(sessionInfo->sendOverlapped))
		{
			InterlockedIncrement(&mSendCompleteTPS);
			for (int i = 0; i < sessionInfo->packetBufferNum; i++)
			{
				packetBuffer = nullptr;
				retSize = sessionInfo->sendRingBuffer->Dequeue((char*)&packetBuffer, POINTER_SIZE);
				if (retSize != POINTER_SIZE)
				{
					CONSOLE_LOG(LOG_LEVEL_ERROR, L"SendRingBuffer Dequeue Size Error[retSize:%d][sessionID:%lu]"
						,retSize, sessionInfo->sessionID);
					break;
				}
				//mPacketBuffer.Free(packetBuffer); // MemoryPool 반환
			}
			sessionInfo->packetBufferNum = 0;
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"WSA Send Clear[%d Bytes]", transferredBytes);

			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"SendRingBuffer UseSize:%d WirtePos:%d ReadPos:%d PacketBufferUseCount:%d",
				sessionInfo->sendRingBuffer->GetUseSize(), sessionInfo->sendRingBuffer->GetWriteSize(),
				sessionInfo->sendRingBuffer->GetReadSize(),mPacketBuffer.GetUseCount());

			// 완료 통지가 왔기 때문에 sendFlag 값 초기화
			sessionInfo->sendFlag = false;

			// 비동기 Send 시작 OnRecv 함수에서 SendRingBuffer 진행
			SendProcess(sessionInfo);
		}
		// 입출력 통보가 왔으므로 IO Count 감소
		if (InterlockedDecrement(&sessionInfo->ioCount) == 0) // IO 가 0이므로 종료되었다고 판단하여 모두 삭제 진행
		{
			Disconnect(sessionInfo);
		}
	}
	CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WorkerThread EXIT![ThreadID:%d]", GetCurrentThreadId());
	return 0;
}

IocpServer::SessionInfo* IocpServer::AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr)
{
	WCHAR clientIP[IP_BUFFER_SIZE] = { 0 };
	SessionInfo* sessionInfo = nullptr;
	SessionIndexInfo* sessionIndex = nullptr;

	if (mUserCount >= mMaxUserNum)
	{
		CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"MAX USER!!!");
		return nullptr;
	}
	// MemoryPool 에 할당해 놓은 개수를 초과하면 에러처리
	if (mSessionIndex.GetUseCount() >= MAX_SESSION_DATA_NUM)
	{
		CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"empty session Index!!!");
		return nullptr;
	}

	// MemoryPool에 저장해놓은 세션 인덱스의 top부터 선택
	sessionIndex = mSessionIndex.Alloc();
	InetNtop(AF_INET, &clientAddr.sin_addr, clientIP, 16);

	mSessionArray[sessionIndex->index]->clientSock = clientSock;
	wcscpy_s(mSessionArray[sessionIndex->index]->ip, _countof(sessionInfo->ip), clientIP);
	mSessionArray[sessionIndex->index]->port = ntohs(clientAddr.sin_port);
	mSessionArray[sessionIndex->index]->sessionID = (++mSessionID_Num << 16);   // 하위 6바이트 유니크값
	mSessionArray[sessionIndex->index]->sessionID |= sessionIndex->index;		// 상위 2바이트 배열 인덱스
	mSessionArray[sessionIndex->index]->recvRingBuffer->Clear();
	mSessionArray[sessionIndex->index]->sendRingBuffer->Clear();
	mSessionArray[sessionIndex->index]->sendFlag = false;
	mSessionArray[sessionIndex->index]->releaseFlag = false;
	mSessionArray[sessionIndex->index]->ioCount = 0;

	InterlockedIncrement(&mUserCount); // 유저수 증가

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"AddSession [sessionID:%lu][Index:%d][UserNum:%d]",
		mSessionArray[sessionIndex->index]->sessionID, sessionIndex->index, mUserCount);

	return mSessionArray[sessionIndex->index];
}

void IocpServer::SendProcess(SessionInfo* sessionInfo)
{
	// 완료통지가 오면 보내는 1:1 구조로 보내기 위해 sendFlag로 체크
	// 비동기 Send 시작 
	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"SendPost Call [SendFalg:%d][sessionID:%d]", sessionInfo->sendFlag, sessionInfo->sessionID);
	if (InterlockedCompareExchange(&sessionInfo->sendFlag, TRUE, FALSE) == FALSE)
	{
		if(SendPost(sessionInfo) == false)
			sessionInfo->sendFlag = false;
	}
}

bool IocpServer::SendPost(SessionInfo* sessionInfo)
{
	int retVal = 0;
	WSABUF wsaBuffer[PACKET_MAX_BUFFER_SIZE] = { 0 };
	PacketBuffer* packeBuffer[PACKET_MAX_BUFFER_SIZE] = { 0 };
	DWORD flags = 0;
	char stringBuffer[PACKET_MAX_SIZE] = { 0 };

	if (sessionInfo->sendRingBuffer->GetUseSize() <= 0)
	{
		return false;
	}

	// packetBuffer 의 포인터 주소를 받아옴
	retVal = sessionInfo->sendRingBuffer->Peek((char*)packeBuffer, sessionInfo->sendRingBuffer->GetUseSize());
	sessionInfo->packetBufferNum = retVal / POINTER_SIZE;

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"packetBufferNum:%d senRingBuffer useSize:%d", 
		sessionInfo->packetBufferNum,
		sessionInfo->sendRingBuffer->GetUseSize());

	if (retVal < POINTER_SIZE)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"retVal error [retVal:%d]", retVal);
		return false;
	}
	// 최대개수 예외처리 진행
	if (sessionInfo->packetBufferNum > PACKET_MAX_BUFFER_SIZE)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"packetBufferNum Exceed! [returnVal:%d][packetBufferNum:%d]"
			, retVal, sessionInfo->packetBufferNum);
		
		sessionInfo->packetBufferNum = PACKET_MAX_BUFFER_SIZE;
	}

	for (int i = 0; i < sessionInfo->packetBufferNum; i++)
	{
		wsaBuffer[i].buf = packeBuffer[i]->GetBufferPtr();
		wsaBuffer[i].len = packeBuffer[i]->GetDataSize();

		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"WSASend Size:%d[sessionID:%d]"
			,wsaBuffer[i].len, sessionInfo->sessionID);
	}

	ZeroMemory(&sessionInfo->sendOverlapped, sizeof(sessionInfo->sendOverlapped));
	// IOCount 증가
	InterlockedIncrement(&sessionInfo->ioCount);
	retVal = WSASend(sessionInfo->clientSock, wsaBuffer, sessionInfo->packetBufferNum,
		NULL, flags, &sessionInfo->sendOverlapped, NULL);


	if (retVal == SOCKET_ERROR)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSASend error:%d", WSAGetLastError());
			// 에러 시 다시 IOCount 감소
			InterlockedDecrement(&sessionInfo->ioCount);
			return true;
		}
		else
		{
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"WSA Send IO_PENDIHNG");
		}
	}
	InterlockedIncrement(&mSendTPS);

	return true;
}

void IocpServer::RecvPost(SessionInfo* sessionInfo)
{
	int retVal = 0;
	DWORD flags = 0;
	WSABUF wsaBuffer[2] = { 0 };

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
			// 에러 시 다시 IOCount 감소
			InterlockedDecrement(&sessionInfo->ioCount);
			return;
		}
		else
		{
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"WSARecv IO_PENDING");
		}
	}
	InterlockedIncrement(&mRecvTPS);
}

void IocpServer::RecvProcess(const SessionInfo* sessionInfo)
{
	int retVal = 0;
	bool isPacketWritePos = false;
	HeaderInfo header = { 0, };
	PacketBuffer packetBuffer(PacketBuffer::BUFFER_SIZE_DEFAULT);

	while (sessionInfo->recvRingBuffer->GetUseSize() >= sizeof(header))
	{
		retVal = sessionInfo->recvRingBuffer->Peek((char*)&header, sizeof(header));

		if (retVal != sizeof(header))
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Header Peek size Error [SessionID:%lu][retVal:%d]",sessionInfo->sessionID, retVal);
			return;
		}

		//헤더와 페이로드 사이즈가 합친 사이즈보다 적으면 다음 수행을 할 수 없다. 다음에 처리 진행
		if (header.length + sizeof(header) > sessionInfo->recvRingBuffer->GetUseSize())
		{
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"length error![SessionID:%lu][length:%d]",sessionInfo->sessionID, header.length);
			return;
		}
		// Peek 이동이기 때문에 header 읽은 만큼 추가로 더해준다.
		sessionInfo->recvRingBuffer->MoveReadPos(retVal);

		packetBuffer.Clear();
		retVal = sessionInfo->recvRingBuffer->Peek(packetBuffer.GetBufferPtr(), header.length);

		if (retVal != header.length)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Peek length size Error [SessionID:%lu][returnVal:%d][length:%d]",
				sessionInfo->sessionID, retVal, header.length);
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
		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"RECV:%s Size:%d", (WCHAR*)packetBuffer.GetBufferPtr(), retVal);

		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[sessionID:%lu] RecvRingBuf WriteSize:%d ReadSize:%d",
			sessionInfo->sessionID,
			sessionInfo->recvRingBuffer->GetWriteSize(),
			sessionInfo->recvRingBuffer->GetReadSize());

		// Recv 처리 완료 후 컨테츠 부로 Job을 전달하기 위한 함수
#ifdef NORMAL_VERSION	
		OnRecv(sessionInfo->sessionID, header, packetBuffer);
#else	// Dummy Test 에는 msyType 제거
		OnRecv(sessionInfo->sessionID, packetBuffer);
#endif
	}
}

void IocpServer::Disconnect(SessionInfo* sessionInfo)
{
	_ASSERT(sessionInfo != nullptr);
	WORD sessionArrayIndex = 0;
	int retSize = 0;
	PacketBuffer* packetBuffer = nullptr;

	sessionArrayIndex = sessionInfo->sessionID & SESSION_ARRAY_INDEX_MASK;
	if (sessionArrayIndex < 0 || sessionArrayIndex >= SESSION_ARRAY_INDEX_MASK)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Session Array Index OverFlow![sessionID:%d][index:%d]",
			sessionInfo->sessionID, sessionArrayIndex);
		return;
	}
	if (InterlockedCompareExchange(&sessionInfo->releaseFlag, TRUE, FALSE) == TRUE)	// 컨텐츠 부에서 이미 진입한 상태였는지 체크
	{
		return;
	}
	closesocket(sessionInfo->clientSock);

	// SendPacket 진행 중에 종료될경우를 대비하여 Enqueue된 PacketBuffer 반환 진행.
	for (int i = 0; i < sessionInfo->packetBufferNum; i++)
	{
		packetBuffer = nullptr;
		retSize = sessionInfo->sendRingBuffer->Dequeue((char*)&packetBuffer, POINTER_SIZE);
		if (retSize== POINTER_SIZE)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"SendRingBuffer Dequeue Size Error[retSize:%d][sessionID:%lu]"
				,retSize, sessionInfo->sessionID);
			break;
		}
		mPacketBuffer.Free(packetBuffer); // MemoryPool 반환
	}
	sessionInfo->packetBufferNum = 0;

	// 유저카운트 체크
	InterlockedDecrement(&mUserCount);

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"Disconnect [SessionID:%lu][Index:%d][UserNum:%d][IndexUseCount:%d][IOCount:%d]", 
		sessionInfo->sessionID, mSessionIndexInfo[sessionArrayIndex]->index, mUserCount, mSessionIndex.GetUseCount(), sessionInfo->ioCount);

	// 세션이 종료 됐다는 부분 표시 진행
	sessionInfo->sessionID = DISCONNECT_SESSION_INDEX;

	// 세션이 종료 됐기 때문에 사용하던 인덱스 MemoryPool에 반납(반환은 모든값이 수정 된 후 맨마지막에 해야함.)
	mSessionIndex.Free(mSessionIndexInfo[sessionArrayIndex]); 
}

void IocpServer::SessionMemoryLog(const char funcName, const unsigned int threadID, void* sessionInfo)
{
	long memoryLogIndex = 0;
	memoryLogIndex = InterlockedIncrement(&mMemoryLogIndex);
	memoryLogIndex %= MEMORY_LOG_MAX_NUM;

	mMemoryLog[memoryLogIndex].funcName = funcName;
	mMemoryLog[memoryLogIndex].threadID = threadID;
	mMemoryLog[memoryLogIndex].sessionInfo = sessionInfo;
}
