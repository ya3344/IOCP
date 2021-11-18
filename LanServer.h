#pragma once
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
	long ioCount; // io count �Լ�
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
	bool Start(const WCHAR* outServerIP, const WORD port, 
		const DWORD workThreadNum, const bool isNodelay, const DWORD maxUserNum);

public:
	static unsigned __stdcall AcceptThread(void* arguments);
	static unsigned __stdcall WorkerThread(void* arguments);

public:
	int AcceptThread_Working(); // �������� ������ ���� ��
	int WorkerThread_Working(); // �������� ������ ���� ��
	SessionInfo* AddSessionInfo(const SOCKET clientSock, SOCKADDR_IN& clientAddr);

public:
	void SendPost(SessionInfo* sessionInfo);	// WSA Send ���� �Լ�
	void RecvPost(SessionInfo* sessionInfo);	// WSA Recv ���� �Լ�
	void RecvProcess(SessionInfo* sessionInfo); // Recv �����͸� �̿��� ������ ������ ó���ϴ� �Լ�

public: // ���� ������ �ο��� ���� �� �����Լ�
	virtual bool OnConnectionRequest(const DWORD ip, const WORD port) PURE; // accept ���� return false; �� Ŭ���̾�Ʈ �ź�. return true; �� ���� ���
	//virtual void OnClientJoin(Client ���� /SessionID / ��Ÿ���) = 0; // Accept �� ����ó�� �Ϸ� �� ȣ��.
	virtual void OnClientLeave(const DWORD64 sessionID) PURE; // Release �� ȣ��
	virtual void OnRecv(const DWORD64 SessionID, const class PacketBuffer& packetBuffer) PURE; // ��Ŷ ���� �Ϸ� ��

public: // ���� ���� ����
	SOCKET mListenSock;
	HANDLE mHcp;

private: // ������ ���� ����
	HANDLE mThread[THREAD_MAX];
	unsigned int mThreadID[THREAD_MAX];
	DWORD mMaxThreadNum;

private: // ���ǰ��� ����
	unordered_map<DWORD64, SessionInfo*> mSessionData;
	DWORD64 mSessionID_Num;
	

private:
	DWORD mMaxUserNum = 0;
};

