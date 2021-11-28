#include "pch.h"
#include "EchoServer.h"

EchoServer::EchoServer()
{
	/*mThread = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, this, 0, &mThreadID);
	_ASSERT(mThread != NULL);
	InitializeSRWLock(&mSendBufferLock);

	mWorkThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_ASSERT(mWorkThreadEvent != NULL);

	mSendBuffer = new RingBuffer(100000);
	_ASSERT(mSendBuffer != NULL);*/
	//mSendBuffer->Init
}

EchoServer::~EchoServer()
{
	CloseHandle(mThread);
	SafeDelete(mSendBuffer);
}

unsigned __stdcall EchoServer::WorkerThread(void* arguments)
{
	return ((EchoServer*)arguments)->WorkerThread_Working();
}

int EchoServer::WorkerThread_Working()
{
	int retVal;
	PacketBuffer sendPacketBuffer(PacketBuffer::BUFFER_SIZE_DEFAULT);
	DWORD64 sessionID;
	bool isPacketWritePos;

	while (true)
	{
		WaitForSingleObject(mWorkThreadEvent, INFINITE);

		while (mSendBuffer->GetUseSize() >= PACKET_SIZE + sizeof(sessionID))
		{
			AcquireSRWLockExclusive(&mSendBufferLock);

			retVal = mSendBuffer->Peek((char*)&sessionID, sizeof(sessionID));

			if (retVal != sizeof(DWORD64))
			{
				CONSOLE_LOG(LOG_LEVEL_ERROR, L"sessionID Peek size Error [returnVal:%d]", retVal);
				ReleaseSRWLockExclusive(&mSendBufferLock);
				return 0;
			}
			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"sessionID %d", sessionID);
			mSendBuffer->MoveReadPos(retVal);

			sendPacketBuffer.Clear();
			retVal = mSendBuffer->Peek(sendPacketBuffer.GetBufferPtr(), PACKET_SIZE);

			if (retVal != PACKET_SIZE)
			{
				CONSOLE_LOG(LOG_LEVEL_ERROR, L"PACKET_SIZE Peek size Error [returnVal:%d]",
					retVal);
				//ReleaseSRWLockExclusive(&mSendBufferLock);
				return 0;
			}

			// Peek 이동이기 때문에 읽은 만큼 이동.
			mSendBuffer->MoveReadPos(retVal);
		
			// 패킷 버퍼도 버퍼에 직접담은 부분이기 때문에 writepos을 직접 이동시켜준다.
			isPacketWritePos = sendPacketBuffer.MoveWritePos(retVal);
			if (isPacketWritePos == false)
			{
				CONSOLE_LOG(LOG_LEVEL_ERROR, L"Packet MoveWritePos OverFlow[%d]", retVal);
				ReleaseSRWLockExclusive(&mSendBufferLock);
				return 0;
			}

			CONSOLE_LOG(LOG_LEVEL_DISPLAY, L"sendBuffer:%s  WriteSize:%d  ReadSize:%d",
				sendPacketBuffer.GetBufferPtr(), mSendBuffer->GetWriteSize(), mSendBuffer->GetReadSize());
			ReleaseSRWLockExclusive(&mSendBufferLock);

		//	SendPacket(sessionID, sendPacketBuffer);
		}
	
	}
	return 0;
}

bool EchoServer::OnConnectionRequest(const DWORD ip, const WORD port)
{
	return false;
}

void EchoServer::OnClientLeave(const DWORD64 sessionID)
{
}

void EchoServer::OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer)
{
	HeaderInfo header;
	//AcquireSRWLockExclusive(&mSendBufferLock);	
//mSendBuffer->Enqueue((char*)&sessionID, sizeof(sessionID));
//mSendBuffer->Enqueue(packetBuffer.GetBufferPtr(), PACKET_SIZE);
//SetEvent(mWorkThreadEvent);
//	ReleaseSRWLockExclusive(&mSendBufferLock);

	char echoData[PACKET_SIZE];
	PacketBuffer* sendPacketBuffer = nullptr;
	sendPacketBuffer = new PacketBuffer(PacketBuffer::BUFFER_SIZE_DEFAULT);

	// 지역변수에 받아온 데이터 저장
	header.length = 8;
	sendPacketBuffer->PutData((char*)&header, sizeof(header));
	packetBuffer.GetData(echoData, PACKET_SIZE);
	sendPacketBuffer->PutData(echoData, PACKET_SIZE);

	// SendPacket
	SendPacket(sessionID, sendPacketBuffer);
}
