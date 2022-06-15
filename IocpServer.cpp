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

	// timewait ������ �ʵ��� ����
	timeWaitOptval.l_onoff = 1;
	timeWaitOptval.l_linger = 0;
	retVal = setsockopt(mListenSock, SOL_SOCKET, SO_LINGER, (char*)&timeWaitOptval, sizeof(timeWaitOptval));
	if (retVal == SOCKET_ERROR)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"timewait option error:%d", WSAGetLastError());
		return false;
	}

	// �۽Ź��� 0 ���� �ʱ�ȭ -> Overlapped I/O ����
	sendBufferOptval = 0;
	retVal = setsockopt(mListenSock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufferOptval, sizeof(sendBufferOptval));
	if (retVal == SOCKET_ERROR)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"SO_SNDBUF option error:%d", WSAGetLastError());
		return false;
	}

	// �ϱ۾˰��� ����
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

	//CPU ���� Ȯ��
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	for (WORD i = 0; i < MAX_SESSION_DATA_NUM; i++)
	{
		mSessionIndexInfo[i] = mSessionIndex.Alloc();
		mSessionIndexInfo[i]->index = i;
	}

	// IOCP handle �ʱ�ȭ Completion Port ����
	mHcp = CreateNewCompletionPort(workThreadNum);
	if (mHcp == NULL)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Hcp NULL:%d ", WSAGetLastError());
		return false;
	}
	// �ִ� ������ ����
	mMaxUserNum = maxUserNum;

	// �۾��� �����带 ���� ������ �������� ���� ������־� ���� �����尡 �۾��� �����庸�� ������ ��츦 ���
	for (WORD i = 0; i < (workThreadNum * ADD_WORKER_THREAD_NUM); i++)
	{
		mThreadData.emplace_back(IOThread, this);
	}
	// Accept ������ ����
	for (WORD i = 0; i < ACCEPT_TRHEAD_NUM; i++)
	{
		mThreadData.emplace_back(AcceptThread, this);
	}

	// ���� ���� �� ���� ���� ����(�����Ҵ�)�Ͽ� �������� �� ������带 ����
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
	
	// ���� �ε����� Accept Thread�� Worker Thread�� ���� ������ ����Ƿ� LockFreeStack MemoryPool ���
	for (int i = MAX_SESSION_DATA_NUM - 1; i >= 0; i--)
	{
		mSessionIndex.Free(mSessionIndexInfo[i]); // ������ ���� �����ε����� MemoryPool �� ����(��ȯ)
	}

	// PacketBuffer LockFreeStack MemoryPool �ʱ�ȭ ����
	mPacketBuffer.Initialize(PACKET_MAX_NUM, true);

	return true;
}

bool IocpServer::Stop()
{
	// ������ �����ϱ� ���� ��� ���� ������ Release ����
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

	// IOThread���� ������ ����Ǵ� ��츦 ����Ͽ� ioCount�� ������Ű�� ioCount return���� 1�̶�� ����Ǿ��ٰ� �Ǵ�
	//if (InterlockedIncrement(&sessionInfo->ioCount) == 1)	
	//{
	//	return false;
	//}
	// SendPacket�� �����ϴ� ���� Disconnect �� Accept �Ǵ� ������ ����ϰ� �߻��ϹǷ� ����ó�� ����.
	if (sessionInfo->sessionID != sessionID)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"SessionID Error![sessionID:%lu][index:%d]",
			sessionID, sessionArrayIndex);
		return false;
	}

	// packtBuffer �� �ּҸ� Enqueue �����μ� ������ ����
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

	// Wsa Send �Լ� ����
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
		
		// ������ ��(EchoServer)���� ���� ��û Ȯ��(������ IP �� ���� IP �˻�)
		if (OnConnectionRequest(clientIP, ntohs(clientaddr.sin_port)) == false)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Deny access![Client IP:%s][Client Port:%d]", 
				clientIP, ntohs(clientaddr.sin_port));
			closesocket(clientSock);
			continue;
		}
		// ���� ���� �߰�
		sessionInfo = AddSessionInfo(clientSock, clientaddr);
		if (sessionInfo == nullptr)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"sessionInfo nullptr![Client IP:%s][Client Port:%d]", 
				clientIP, ntohs(clientaddr.sin_port));
			closesocket(clientSock);
			continue;
		}
		// ���ϰ� ����� �Ϸ� ��Ʈ ����
		CreateIoCompletionPort((HANDLE)clientSock, mHcp, (ULONG_PTR)sessionInfo, 0);

		// ������ �ο��� ���� �� ���� �⺻ ���� ����
		OnConnectionSuccess(sessionInfo->sessionID);
		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"[CHAT SERVER] Client IP: %s Clinet Port:%d", clientIP, ntohs(clientaddr.sin_port));

		// WSA RECV �񵿱� ����
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

		// ����� �Ϸ� ���
		retVal = GetQueuedCompletionStatus(mHcp, &transferredBytes, (PULONG_PTR)&sessionInfo,
			&overlapped, INFINITE);

		CONSOLE_LOG(LOG_LEVEL_DEBUG, L"\n\nWorkerThread Start[threadID:%d]\n", GetCurrentThreadId());

		// IOCP ���� ������ ����
		if (retVal == FALSE && (overlapped == nullptr || sessionInfo == nullptr))
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"IOCP HANDLE Error[Error:%d]", WSAGetLastError());
			return 0;
		}

		// PostQueue(iocp, 0, 0, 0...)�� ��쿡�� �ǵ��� ��� IO������ ����
		if (overlapped == nullptr && sessionInfo == nullptr && transferredBytes == 0)
		{
			//��Ŀ������ ����
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"WorkerThread EXIT![ThreadID:%d]", GetCurrentThreadId());
			return 0;
		}
		_ASSERT(sessionInfo != nullptr);

		if (transferredBytes == 0)
		{
			//��������
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"User Exit transferredBytes0![SesssionID:%lu]", sessionInfo->sessionID);
		}
		else if (overlapped == &(sessionInfo->recvOverlapped))
		{
			InterlockedIncrement(&mRecvCompleteTPS);
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"sessionInfo->recvOverlapped[transferredBytes:%d][SesssionID:%lu]",
				transferredBytes
				,sessionInfo->sessionID);

			sessionInfo->recvRingBuffer->MoveWritePos(transferredBytes);

			// �����۷� ���� �κ� ������ ���� �� ������ �η� ����
			RecvProcess(sessionInfo);

			// �񵿱� Recv ����
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
				//mPacketBuffer.Free(packetBuffer); // MemoryPool ��ȯ
			}
			sessionInfo->packetBufferNum = 0;
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"WSA Send Clear[%d Bytes]", transferredBytes);

			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"SendRingBuffer UseSize:%d WirtePos:%d ReadPos:%d PacketBufferUseCount:%d",
				sessionInfo->sendRingBuffer->GetUseSize(), sessionInfo->sendRingBuffer->GetWriteSize(),
				sessionInfo->sendRingBuffer->GetReadSize(),mPacketBuffer.GetUseCount());

			// �Ϸ� ������ �Ա� ������ sendFlag �� �ʱ�ȭ
			sessionInfo->sendFlag = false;

			// �񵿱� Send ���� OnRecv �Լ����� SendRingBuffer ����
			SendProcess(sessionInfo);
		}
		// ����� �뺸�� �����Ƿ� IO Count ����
		if (InterlockedDecrement(&sessionInfo->ioCount) == 0) // IO �� 0�̹Ƿ� ����Ǿ��ٰ� �Ǵ��Ͽ� ��� ���� ����
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
	// MemoryPool �� �Ҵ��� ���� ������ �ʰ��ϸ� ����ó��
	if (mSessionIndex.GetUseCount() >= MAX_SESSION_DATA_NUM)
	{
		CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"empty session Index!!!");
		return nullptr;
	}

	// MemoryPool�� �����س��� ���� �ε����� top���� ����
	sessionIndex = mSessionIndex.Alloc();
	InetNtop(AF_INET, &clientAddr.sin_addr, clientIP, 16);

	mSessionArray[sessionIndex->index]->clientSock = clientSock;
	wcscpy_s(mSessionArray[sessionIndex->index]->ip, _countof(sessionInfo->ip), clientIP);
	mSessionArray[sessionIndex->index]->port = ntohs(clientAddr.sin_port);
	mSessionArray[sessionIndex->index]->sessionID = (++mSessionID_Num << 16);   // ���� 6����Ʈ ����ũ��
	mSessionArray[sessionIndex->index]->sessionID |= sessionIndex->index;		// ���� 2����Ʈ �迭 �ε���
	mSessionArray[sessionIndex->index]->recvRingBuffer->Clear();
	mSessionArray[sessionIndex->index]->sendRingBuffer->Clear();
	mSessionArray[sessionIndex->index]->sendFlag = false;
	mSessionArray[sessionIndex->index]->releaseFlag = false;
	mSessionArray[sessionIndex->index]->ioCount = 0;

	InterlockedIncrement(&mUserCount); // ������ ����

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"AddSession [sessionID:%lu][Index:%d][UserNum:%d]",
		mSessionArray[sessionIndex->index]->sessionID, sessionIndex->index, mUserCount);

	return mSessionArray[sessionIndex->index];
}

void IocpServer::SendProcess(SessionInfo* sessionInfo)
{
	// �Ϸ������� ���� ������ 1:1 ������ ������ ���� sendFlag�� üũ
	// �񵿱� Send ���� 
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

	// packetBuffer �� ������ �ּҸ� �޾ƿ�
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
	// �ִ밳�� ����ó�� ����
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
	// IOCount ����
	InterlockedIncrement(&sessionInfo->ioCount);
	retVal = WSASend(sessionInfo->clientSock, wsaBuffer, sessionInfo->packetBufferNum,
		NULL, flags, &sessionInfo->sendOverlapped, NULL);


	if (retVal == SOCKET_ERROR)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSASend error:%d", WSAGetLastError());
			// ���� �� �ٽ� IOCount ����
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

	// �񵿱� ����� ����
	// ������ ���� ������ 2�� �����ؼ� ���̷�Ʈ�� �޾ƿ����� �۾� ����
	wsaBuffer[0].buf = sessionInfo->recvRingBuffer->GetNotBroken_BufferPtr();
	wsaBuffer[0].len = sessionInfo->recvRingBuffer->GetNotBroken_WriteSize();
	wsaBuffer[1].buf = sessionInfo->recvRingBuffer->GetBroken_BufferPtr();
	wsaBuffer[1].len = sessionInfo->recvRingBuffer->GetBroken_WriteSize();

	// IO ī��Ʈ ����
	InterlockedIncrement(&sessionInfo->ioCount);

	ZeroMemory(&sessionInfo->recvOverlapped, sizeof(sessionInfo->recvOverlapped));
	retVal = WSARecv(sessionInfo->clientSock, wsaBuffer, 2, NULL,
		&flags, &sessionInfo->recvOverlapped, NULL);
	if (retVal == SOCKET_ERROR)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"WSARecv error:%d", WSAGetLastError());
			// ���� �� �ٽ� IOCount ����
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

		//����� ���̷ε� ����� ��ģ ������� ������ ���� ������ �� �� ����. ������ ó�� ����
		if (header.length + sizeof(header) > sessionInfo->recvRingBuffer->GetUseSize())
		{
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"length error![SessionID:%lu][length:%d]",sessionInfo->sessionID, header.length);
			return;
		}
		// Peek �̵��̱� ������ header ���� ��ŭ �߰��� �����ش�.
		sessionInfo->recvRingBuffer->MoveReadPos(retVal);

		packetBuffer.Clear();
		retVal = sessionInfo->recvRingBuffer->Peek(packetBuffer.GetBufferPtr(), header.length);

		if (retVal != header.length)
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Peek length size Error [SessionID:%lu][returnVal:%d][length:%d]",
				sessionInfo->sessionID, retVal, header.length);
			return;
		}
	
		// Peek �̵��̱� ������ packet ���� ��ŭ ���� �̵�
		sessionInfo->recvRingBuffer->MoveReadPos(retVal);

		// ��Ŷ ���۵� ���ۿ� �������� �κ��̱� ������ writepos�� ���� �̵������ش�.
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

		// Recv ó�� �Ϸ� �� ������ �η� Job�� �����ϱ� ���� �Լ�
#ifdef NORMAL_VERSION	
		OnRecv(sessionInfo->sessionID, header, packetBuffer);
#else	// Dummy Test ���� msyType ����
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
	if (InterlockedCompareExchange(&sessionInfo->releaseFlag, TRUE, FALSE) == TRUE)	// ������ �ο��� �̹� ������ ���¿����� üũ
	{
		return;
	}
	closesocket(sessionInfo->clientSock);

	// SendPacket ���� �߿� ����ɰ�츦 ����Ͽ� Enqueue�� PacketBuffer ��ȯ ����.
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
		mPacketBuffer.Free(packetBuffer); // MemoryPool ��ȯ
	}
	sessionInfo->packetBufferNum = 0;

	// ����ī��Ʈ üũ
	InterlockedDecrement(&mUserCount);

	CONSOLE_LOG(LOG_LEVEL_DEBUG, L"Disconnect [SessionID:%lu][Index:%d][UserNum:%d][IndexUseCount:%d][IOCount:%d]", 
		sessionInfo->sessionID, mSessionIndexInfo[sessionArrayIndex]->index, mUserCount, mSessionIndex.GetUseCount(), sessionInfo->ioCount);

	// ������ ���� �ƴٴ� �κ� ǥ�� ����
	sessionInfo->sessionID = DISCONNECT_SESSION_INDEX;

	// ������ ���� �Ʊ� ������ ����ϴ� �ε��� MemoryPool�� �ݳ�(��ȯ�� ��簪�� ���� �� �� �Ǹ������� �ؾ���.)
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
