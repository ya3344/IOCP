#pragma once

#include "../Common\RingBuffer/RingBuffer.h"
#include "../Common\PacketBuffer/PacketBuffer.h"

struct SessionInfo
{
	SOCKET clientSock = INVALID_SOCKET;
	WCHAR ip[IP_BUFFER_SIZE] = { 0, };
	WORD port = 0;
	DWORD64 sessionID = 0;
	class RingBuffer* sendRingBuffer = nullptr;
	class RingBuffer* recvRingBuffer = nullptr;
	OVERLAPPED recvOverlapped;
	OVERLAPPED sendOverlapped;
	SRWLOCK srwLock;
	long ioCount = 0; // io count 함수
	long sendFlag = false;
};

#pragma pack(push, 1)   
struct HeaderInfo
{
	WORD length = 0;
};
#pragma pack(pop)

/*
sendpost wsasend 실행
recvPos wsarecv 실행
sendpacket sessionid 찾아서 sendpost 진행
*/
class LanServer abstract
{
public:
	LanServer() = default;
	virtual ~LanServer();

public:
	enum PACKET_INDEX
	{
		PACKET_SIZE = 8,
	};

public:
	bool Start(const WCHAR* outServerIP, const WORD port, 
		const DWORD workThreadNum, const bool isNodelay, const DWORD maxUserNum);
	bool Stop();
	bool SendPacket(const DWORD64 sessionID, PacketBuffer& packetBuffer);
	bool Disconnect(const DWORD64 sessionID);

public:
	DWORD GetMaxThreadNum() const { return mMaxThreadNum;  }
	const HANDLE* GetThread() const { return mThread; }

private:
	static unsigned __stdcall AcceptThread(void* arguments);
	static unsigned __stdcall WorkerThread(void* arguments);

private:
	int AcceptThread_Working(); // 실질적인 쓰레드 실행 부
	int WorkerThread_Working(); // 실질적인 쓰레드 실행 부
	SessionInfo* AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr);

private:
	void SendProcess(SessionInfo* sessionInfo);
	bool SendPost(SessionInfo* sessionInfo);	// WSA Send 실행 함수
	void RecvPost(SessionInfo* sessionInfo);	// WSA Recv 실행 함수
	void RecvProcess(SessionInfo* sessionInfo); // Recv 데이터를 이용해 보낼거 보내고 처리하는 함수
	void Release(SessionInfo* sessionInfo);

protected: // 실제 컨테츠 부에서 실행 할 가상함수
	virtual bool OnConnectionRequest(const DWORD ip, const WORD port) PURE; // accept 직후 return false; 시 클라이언트 거부. return true; 시 접속 허용
	//virtual void OnClientJoin(Client 정보 /SessionID / 기타등등) = 0; // Accept 후 접속처리 완료 후 호출.
	virtual void OnClientLeave(const DWORD64 sessionID) PURE; // Release 후 호출
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) PURE; // 패킷 수신 완료 후

private: // 소켓 관련 변수
	SOCKET mListenSock;
	HANDLE mHcp;

private: // 쓰레드 관련 변수
	HANDLE* mThread;
	unsigned int mThreadID;
	DWORD mMaxThreadNum = 0;
	bool mShutDown = false;
	HANDLE mExitEvent = nullptr;

private: // 세션관련 변수
	unordered_map<DWORD64, SessionInfo*> mSessionData;
	DWORD64 mSessionID_Num = 0;
	

private:
	DWORD mMaxUserNum = 0;
};

