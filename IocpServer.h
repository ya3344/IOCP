#pragma once

#include "../Common\RingBuffer/RingBuffer.h"
#include "../Common\PacketBuffer/PacketBuffer.h"
#include "../Common\MemoryPool/MemoryPool.h"

class IocpServer abstract
{
public:
	IocpServer() = default;
	virtual ~IocpServer();

public:
	enum IOCP_SERVER_INDEX
	{
		PACKET_MAX_SIZE = 100,
		POINTER_SIZE = 8,
		ECHO_SIZE = 8,
		MAX_SESSION_DATA_NUM = 3000,
		SESSION_ARRAY_INDEX_MASK = 65535,
		PACKET_MAX_BUFFER_SIZE = 500,
		PACKET_MAX_NUM = 10000,
		DISCONNECT_SESSION_INDEX = 0,
		JOBQUEUE_MAX_BUFFER_SIZE = 100000,
	};
public:
	struct SessionInfo
	{
		SOCKET clientSock = INVALID_SOCKET;
		WCHAR ip[IP_BUFFER_SIZE] = { 0 };
		WORD port = 0;
		DWORD64 sessionID = 0;
		class RingBuffer* sendRingBuffer = nullptr;
		class RingBuffer* recvRingBuffer = nullptr;
		OVERLAPPED recvOverlapped;
		OVERLAPPED sendOverlapped;
		mutex sessionLock;
		long ioCount = 0;
		long sendFlag = FALSE;
		long releaseFlag = FALSE;
		WORD packetBufferNum = 0;
	};
	struct SessionIndexInfo
	{
		WORD index = 0;
	};
	struct SessionMemoryLogInfo
	{
		void* sessionInfo = nullptr;
		unsigned int threadID = 0;
		char funcName = -1;
	};

public:
	// main �Լ����� �ߵ��ϴ� ���� ���� �Լ�
	bool Start(const WCHAR* outServerIP, const WORD port,													
		const DWORD workThreadNum, const bool isNodelay, const DWORD maxUserNum);	
	// main �Լ����� �ߵ��ϴ� ���� ���� �Լ�
	bool Stop();
	// Send ������ ó�� �Լ�
	bool SendPacket(const DWORD64 sessionID, PacketBuffer* packetBuffer);									

public:
	vector<thread>& GetIocpServer_Threads() { return mThreadData; }

private:
	static unsigned __stdcall AcceptThread(void* arguments);
	static unsigned __stdcall IOThread(void* arguments);

private:
	// Completion Port ������Ʈ ���� �Լ�
	HANDLE CreateNewCompletionPort(const DWORD numberOfConcurrentThreads);				
	// Accept �� Ŭ���̾�Ʈ ���� ó�� �Լ�
	SessionInfo* AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr);			
	// Accept ó�� �������� ������ ���� ��
	int AcceptThread_Working();	
	// IO ó�� �� ���� �������� ������ ���� ��
	int IOThread_Working();																					

private:
	// �Ϸ������� ���� ������ 1:1 ������ ������ ���� sendFlag�� üũ �Լ�
	void SendProcess(SessionInfo* sessionInfo);		
	// WSA Send ���� �Լ�
	bool SendPost(SessionInfo* sessionInfo);		
	// WSA Recv ���� �Լ�
	void RecvPost(SessionInfo* sessionInfo);		
	// Recv ������ ó�� �Լ�
	void RecvProcess(const SessionInfo* sessionInfo);
protected:
	// Ŭ���̾�Ʈ ���� ���� �Լ�
	void Disconnect(SessionInfo* sessionInfo);

protected: // ���� ������ �ο��� ���� �� �����Լ�
	// accept ���� return false; �� Ŭ���̾�Ʈ �ź�. return true; �� ���� ��� �Լ�(��������)
	virtual bool OnConnectionRequest(const WCHAR* ip, const WORD port) PURE;		
	// Accept ����ó�� �Ϸ� �� ȣ�� �Լ�(��������)
	virtual void OnConnectionSuccess(const DWORD64 sessionID) PURE;			
	// Release ȣ�� �� ó�� �Լ�(������ ��)
	virtual void OnClientLeave(const DWORD64 sessionID) PURE;												
#ifdef NORMAL_VERSION
	// ��Ŷ ���� �Ϸ� �� ó�� �Լ�(������ ��)
	virtual void OnRecv(const DWORD64 sessionID, 
		const HeaderInfo& header, PacketBuffer& packetBuffer) PURE;
#elif DUMMY_TEST_VERSION
	// ��Ŷ ���� �Ϸ� �� ó�� �Լ�(������ ��)
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) PURE;							
#endif
	// ������ �ο��� ������ ���� ���� ��Ű�� �Լ�(������ ��)
	virtual void OnDisconnect(const DWORD64 sessionID) PURE;												

protected: // ������ �Լ�
	void SessionMemoryLog(const char funcName, const unsigned int threadID, void* sessionInfo);

private: // ���� ���� ����
	SOCKET mListenSock = INVALID_SOCKET;
	HANDLE mHcp = nullptr;

private: // ������ ���� ����
	unsigned int mThreadID = 0;
	vector<thread> mThreadData;
	static constexpr float ADD_WORKER_THREAD_NUM = 1.3f;

private: // ���ǰ��� ����
	DWORD64 mSessionID_Num = 0;
	SessionIndexInfo* mSessionIndexInfo[MAX_SESSION_DATA_NUM] = { 0 };
	MemoryPool<SessionIndexInfo> mSessionIndex;

protected:
	SessionInfo* mSessionArray[MAX_SESSION_DATA_NUM] = { 0 };
	bool mShutDown = false;

private: // ����üũ ����
	long mUserCount = 0;
	long mMaxUserNum = 0;

protected:
	MemoryPool<PacketBuffer> mPacketBuffer;
	SessionMemoryLogInfo mMemoryLog[MEMORY_LOG_MAX_NUM];
	long mMemoryLogIndex = -1;

// ������ ����
protected:
	__declspec(align(64)) long mSendTPS = 0;
	__declspec(align(64)) long mRecvTPS = 0;
	__declspec(align(64)) long mRecvCompleteTPS = 0;
	__declspec(align(64)) long mSendCompleteTPS = 0;
	__declspec(align(64)) unsigned long mJobQueueTPS = 0;
	__declspec(align(64)) unsigned long mPacketProcessTPS = 0;
};

