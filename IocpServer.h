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
	// main 함수에서 발동하는 서버 시작 함수
	bool Start(const WCHAR* outServerIP, const WORD port,													
		const DWORD workThreadNum, const bool isNodelay, const DWORD maxUserNum);	
	// main 함수에서 발동하는 서버 중지 함수
	bool Stop();
	// Send 데이터 처리 함수
	bool SendPacket(const DWORD64 sessionID, PacketBuffer* packetBuffer);									

public:
	vector<thread>& GetIocpServer_Threads() { return mThreadData; }

private:
	static unsigned __stdcall AcceptThread(void* arguments);
	static unsigned __stdcall IOThread(void* arguments);

private:
	// Completion Port 오브젝트 생성 함수
	HANDLE CreateNewCompletionPort(const DWORD numberOfConcurrentThreads);				
	// Accept 후 클라이언트 접속 처리 함수
	SessionInfo* AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr);			
	// Accept 처리 실질적인 쓰레드 실행 부
	int AcceptThread_Working();	
	// IO 처리 및 진행 실질적인 쓰레드 실행 부
	int IOThread_Working();																					

private:
	// 완료통지가 오면 보내는 1:1 구조로 보내기 위해 sendFlag로 체크 함수
	void SendProcess(SessionInfo* sessionInfo);		
	// WSA Send 실행 함수
	bool SendPost(SessionInfo* sessionInfo);		
	// WSA Recv 실행 함수
	void RecvPost(SessionInfo* sessionInfo);		
	// Recv 데이터 처리 함수
	void RecvProcess(const SessionInfo* sessionInfo);
protected:
	// 클라이언트 접속 종료 함수
	void Disconnect(SessionInfo* sessionInfo);

protected: // 실제 컨테츠 부에서 실행 할 가상함수
	// accept 직후 return false; 시 클라이언트 거부. return true; 시 접속 허용 함수(컨텐츠부)
	virtual bool OnConnectionRequest(const WCHAR* ip, const WORD port) PURE;		
	// Accept 접속처리 완료 후 호출 함수(컨텐츠부)
	virtual void OnConnectionSuccess(const DWORD64 sessionID) PURE;			
	// Release 호출 후 처리 함수(컨텐츠 부)
	virtual void OnClientLeave(const DWORD64 sessionID) PURE;												
#ifdef NORMAL_VERSION
	// 패킷 수신 완료 후 처리 함수(컨텐츠 부)
	virtual void OnRecv(const DWORD64 sessionID, 
		const HeaderInfo& header, PacketBuffer& packetBuffer) PURE;
#elif DUMMY_TEST_VERSION
	// 패킷 수신 완료 후 처리 함수(컨텐츠 부)
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) PURE;							
#endif
	// 컨텐츠 부에서 유저를 접속 종료 시키는 함수(컨텐츠 부)
	virtual void OnDisconnect(const DWORD64 sessionID) PURE;												

protected: // 디버깅용 함수
	void SessionMemoryLog(const char funcName, const unsigned int threadID, void* sessionInfo);

private: // 소켓 관련 변수
	SOCKET mListenSock = INVALID_SOCKET;
	HANDLE mHcp = nullptr;

private: // 쓰레드 관련 변수
	unsigned int mThreadID = 0;
	vector<thread> mThreadData;
	static constexpr float ADD_WORKER_THREAD_NUM = 1.3f;

private: // 세션관련 변수
	DWORD64 mSessionID_Num = 0;
	SessionIndexInfo* mSessionIndexInfo[MAX_SESSION_DATA_NUM] = { 0 };
	MemoryPool<SessionIndexInfo> mSessionIndex;

protected:
	SessionInfo* mSessionArray[MAX_SESSION_DATA_NUM] = { 0 };
	bool mShutDown = false;

private: // 유저체크 변수
	long mUserCount = 0;
	long mMaxUserNum = 0;

protected:
	MemoryPool<PacketBuffer> mPacketBuffer;
	SessionMemoryLogInfo mMemoryLog[MEMORY_LOG_MAX_NUM];
	long mMemoryLogIndex = -1;

// 디버깅용 변수
protected:
	__declspec(align(64)) long mSendTPS = 0;
	__declspec(align(64)) long mRecvTPS = 0;
	__declspec(align(64)) long mRecvCompleteTPS = 0;
	__declspec(align(64)) long mSendCompleteTPS = 0;
	__declspec(align(64)) unsigned long mJobQueueTPS = 0;
	__declspec(align(64)) unsigned long mPacketProcessTPS = 0;
};

