#include "pch.h"
#include "EchoServer.h"

EchoServer::EchoServer()
{
	mThread = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, this, 0, &mThreadID);
	_ASSERT(mThread != NULL);
	mMonitorThraed = (HANDLE)_beginthreadex(NULL, 0, &MoinitorThread, this, 0, &mThreadID);
	_ASSERT(mMonitorThraed != NULL);

	mWorkThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_ASSERT(mWorkThreadEvent != NULL);
	mJobQueue = new RingBuffer(JOBQUEUE_MAX_BUFFER_SIZE);
	_ASSERT(mJobQueue != NULL);
}

EchoServer::~EchoServer()
{
	CloseHandle(mThread);
	CloseHandle(mMonitorThraed);
	SafeDelete(mJobQueue);
}

unsigned __stdcall EchoServer::WorkerThread(void* arguments)
{
	return ((EchoServer*)arguments)->WorkerThread_Working();
}

int EchoServer::WorkerThread_Working()
{
	int retVal = 0;
	HeaderInfo header = { 0 };
	char data[PACKET_MAX_SIZE] = { 0 };
	PacketBuffer* sendPacketBuffer = nullptr;
	DWORD64 sessionID = 0;
	bool isUpdateProcess = false;
	bool isSendPacket = false;

	while (mShutDown == false)
	{
		WaitForSingleObject(mWorkThreadEvent, INFINITE);

		while (mJobQueue->GetUseSize() >= sizeof(sessionID) + ECHO_SIZE)
		{
			retVal = mJobQueue->Peek((char*)&sessionID, sizeof(sessionID));
			if (retVal != sizeof(sessionID))
			{
				CONSOLE_LOG(LOG_LEVEL_ERROR, L"sessionID Peek size Error [returnVal:%d]", retVal);
				return 0;
			}
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"sessionID %lu", sessionID);
			mJobQueue->MoveReadPos(retVal);

			sendPacketBuffer = mPacketBuffer.Alloc(); // sendPacketBuffer는 스레드간 경합이 발생하므로 LockFreeStack MemoryPool 사용
#ifdef NORMAL_VERSION
			// 컨텐츠 처리 진행
			isUpdateProcess = UpdateProcess(sessionID, sendPacketBuffer);
			if (isUpdateProcess == true)
			{
				isSendPacket = SendPacket(sessionID, sendPacketBuffer);
			}
#elif DUMMY_TEST_VERSION
			retVal = mJobQueue->Peek(data, ECHO_SIZE);
			if (retVal != ECHO_SIZE)
			{
				CONSOLE_LOG(LOG_LEVEL_ERROR, L"ECHO_SIZE Error [SessionId:%d][returnVal:%d]", sessionID, retVal);
				return 0;
			}
			mJobQueue->MoveReadPos(retVal);
			// 컨텐츠 처리 후 data 직렬화 진행 및 처리
			EchoMakePacket(sendPacketBuffer, data, ECHO_SIZE);
			isSendPacket = SendPacket(sessionID, sendPacketBuffer);
#endif
			// sendPacket의 수행이 정상적으로 이루어지지 않을 경우 sendPacketBuffer 강제 반환
			if (isSendPacket == false) 
			{
				mPacketBuffer.Free(sendPacketBuffer);
			}
			// SendPacket에서 증가시켜준 IO Count 감소 및 IO Count가 0일 경우 종료처리 진행.
			OnDisconnect(sessionID);
			//Send
			CONSOLE_LOG(LOG_LEVEL_DEBUG, L"WriteSize:%d  ReadSize:%d", mJobQueue->GetWriteSize(), mJobQueue->GetReadSize());
		}
	}
	return 0;
}

unsigned __stdcall EchoServer::MoinitorThread(void* arguments)
{
	return ((EchoServer*)arguments)->MonitorThread_Working();
}

int EchoServer::MonitorThread_Working()
{
	while (mShutDown == false)
	{
		if (mFrameTime + 1000 < timeGetTime())
		{
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"JobQueue TPS:%d\n", mJobQueueTPS);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"Recv TPS:%d\n", mRecvTPS);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"RecvComplete TPS:%d\n", mRecvCompleteTPS);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"Send TPS:%d\n", mSendTPS);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"SendComplte TPS:%d\n", mSendCompleteTPS);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"PacketProcess TPS:%d\n", mPacketProcessTPS);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"JobQueue UseSize:%d", mJobQueue->GetUseSize());

			mFrameTime = timeGetTime();
			
			InterlockedExchange(&mJobQueueTPS, 0);
			InterlockedExchange(&mRecvTPS, 0);
			InterlockedExchange(&mRecvCompleteTPS, 0);
			InterlockedExchange(&mSendTPS, 0);
			InterlockedExchange(&mSendCompleteTPS, 0);
			InterlockedExchange(&mPacketProcessTPS, 0);
		}
	}

	return 0;
}

#ifdef NORMAL_VERSION
bool EchoServer::UpdateProcess(const DWORD64 sessionID, PacketBuffer* outPacketBuffer)
{
	int retVal = 0;
	HeaderInfo header = { 0 };

	retVal = mJobQueue->Peek((char*)&header, sizeof(header));
	if (retVal != sizeof(header))
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"header size Error [SessionId:%d][returnVal:%d]",sessionID, retVal);
		return false;
	}
	// Peek 이동이기 때문에 읽은 만큼 이동.
	mJobQueue->MoveReadPos(retVal);

	switch (header.msgType)
	{
	case HEADER_SC_ECHO_DATA:
		{
			char data[PACKET_MAX_SIZE] = { 0 };
			retVal = mJobQueue->Peek(data, header.length);
			if (retVal != header.length)
			{
				CONSOLE_LOG(LOG_LEVEL_ERROR, L"data Peek size Error [SessionID:%lu][returnVal:%d]",sessionID, retVal);
				return false;
			}
			// Peek 이동이기 때문에 읽은 만큼 이동.
			mJobQueue->MoveReadPos(retVal);
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"echoData :%s sessionID:%d", data, sessionID);

			// 컨텐츠 처리 후 data 직렬화 진행 및 처리
			EchoMakePacket(outPacketBuffer, data, header.length);
		}
		break;
	default:
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"UnKnow msgType[msgType:%d][SessionID:%d]", header.msgType, sessionID);
		break;
	}

	return true;
}
#endif

bool EchoServer::OnConnectionRequest(const WCHAR* ip, const WORD port)
{
	return true;
}

void EchoServer::OnConnectionSuccess(const DWORD64 sessionID)
{
}

void EchoServer::OnClientLeave(const DWORD64 sessionID)
{
}

#ifdef NORMAL_VERSION
void EchoServer::OnRecv(const DWORD64 sessionID, const HeaderInfo& header, PacketBuffer& packetBuffer)
#elif DUMMY_TEST_VERSION
void EchoServer::OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer)	// DUMMY TEST 시 msgType 제거
#endif
{
#ifdef WORKTHREAD_ENABLE
	int retVal = 0;
	{
		lock_guard<mutex> guard(mJobQueueLock);
		retVal = mJobQueue->Enqueue((char*)(&sessionID), sizeof(sessionID));
		if (retVal != sizeof(sessionID))
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Enqueue SendBuffer sessionID Error[SessionID:%lu][retVal:%d][useSize:%d]",
				sessionID,
				retVal,
				mJobQueue->GetUseSize());
			return;
		};
#ifdef NORMAL_VERSION
		retVal = mJobQueue->Enqueue((char*)(&header), sizeof(header));
		if (retVal != sizeof(header))
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Enqueue header size Error[SessionID:%lu][retVal:%d][useSize:%d]",
				sessionID, retVal, mJobQueue->GetUseSize());
			return;
		};
		retVal = mJobQueue->Enqueue(packetBuffer.GetBufferPtr(), packetBuffer.GetDataSize());
		if (retVal != packetBuffer.GetDataSize())
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Enqueue SendBuffer packetBuffer Size Error[SessionID:%lu][retVal:%d][useSize:%d]",
				sessionID, retVal, mJobQueue->GetUseSize());
			return;
		};
		SetEvent(mWorkThreadEvent);
#elif DUMMY_TEST_VERSION
		char data[PACKET_MAX_SIZE] = { 0 };

		// 지역변수에 받아온 데이터 추출
		packetBuffer.GetData(data, packetBuffer.GetDataSize());

		retVal = mJobQueue->Enqueue(data, packetBuffer.GetDataSize());
		if (retVal != packetBuffer.GetDataSize())
		{
			CONSOLE_LOG(LOG_LEVEL_ERROR, L"Enqueue SendBuffer packetBuffer Size Error[SessionID:%lu][retVal:%d][useSize:%d]",
				sessionID, retVal, mJobQueue->GetUseSize());
			return;
		};
		++mJobQueueTPS;
		SetEvent(mWorkThreadEvent);
#endif
	}
#else
	char data[PACKET_MAX_SIZE] = { 0 };
	packetBuffer.GetData(data, packetBuffer.GetDataSize());
	PacketBuffer* sendPacketBuffer = nullptr;
	sendPacketBuffer = mPacketBuffer.Alloc(); // sendPacketBuffer는 스레드간 경합이 발생하므로 LockFreeStack MemoryPool 사용
	EchoMakePacket(sendPacketBuffer, data, ECHO_SIZE);
	SendPacket(sessionID, sendPacketBuffer);
#endif
}

void EchoServer::OnDisconnect(const DWORD64 sessionID)
{
	WORD sessionArrayIndex = 0;
	SessionInfo* sessionInfo = nullptr;

	sessionArrayIndex = sessionID & SESSION_ARRAY_INDEX_MASK;

	if (sessionArrayIndex < 0 || sessionArrayIndex >= MAX_SESSION_DATA_NUM)
	{
		CONSOLE_LOG(LOG_LEVEL_ERROR, L"Session Array Index Overflow![sessionID:%lu][index:%d]",
			sessionID, sessionArrayIndex);
		return;
	}
	sessionInfo = mSessionArray[sessionArrayIndex];
	if (InterlockedDecrement(&sessionInfo->ioCount) == 0)
	{
		Disconnect(sessionInfo);
	}
}

void EchoServer::EchoMakePacket(PacketBuffer* outPacketBuffer, const char* data, const WORD size)
{
	HeaderInfo header = { 0 };
#ifdef NORMAL_VERSION
	header.msgType = HEADER_SC_ECHO_DATA;
	header.length = size;
#elif DUMMY_TEST_VERSION
	header.length = size;
#endif
	outPacketBuffer->PutData((char*)&header, sizeof(header));
	outPacketBuffer->PutData(data, header.length);
}
