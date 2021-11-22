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
	long ioCount = 0; // io count �Լ�
	long sendFlag = false;
};

#pragma pack(push, 1)   
struct HeaderInfo
{
	WORD length = 0;
};
#pragma pack(pop)

/*
sendpost wsasend ����
recvPos wsarecv ����
sendpacket sessionid ã�Ƽ� sendpost ����
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
	int AcceptThread_Working(); // �������� ������ ���� ��
	int WorkerThread_Working(); // �������� ������ ���� ��
	SessionInfo* AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr);

private:
	void SendProcess(SessionInfo* sessionInfo);
	bool SendPost(SessionInfo* sessionInfo);	// WSA Send ���� �Լ�
	void RecvPost(SessionInfo* sessionInfo);	// WSA Recv ���� �Լ�
	void RecvProcess(SessionInfo* sessionInfo); // Recv �����͸� �̿��� ������ ������ ó���ϴ� �Լ�
	void Release(SessionInfo* sessionInfo);

protected: // ���� ������ �ο��� ���� �� �����Լ�
	virtual bool OnConnectionRequest(const DWORD ip, const WORD port) PURE; // accept ���� return false; �� Ŭ���̾�Ʈ �ź�. return true; �� ���� ���
	//virtual void OnClientJoin(Client ���� /SessionID / ��Ÿ���) = 0; // Accept �� ����ó�� �Ϸ� �� ȣ��.
	virtual void OnClientLeave(const DWORD64 sessionID) PURE; // Release �� ȣ��
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) PURE; // ��Ŷ ���� �Ϸ� ��

private: // ���� ���� ����
	SOCKET mListenSock;
	HANDLE mHcp;

private: // ������ ���� ����
	HANDLE* mThread;
	unsigned int mThreadID;
	DWORD mMaxThreadNum = 0;
	bool mShutDown = false;
	HANDLE mExitEvent = nullptr;

private: // ���ǰ��� ����
	unordered_map<DWORD64, SessionInfo*> mSessionData;
	DWORD64 mSessionID_Num = 0;
	

private:
	DWORD mMaxUserNum = 0;
};

