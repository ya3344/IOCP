#pragma once
#include "IocpServer.h"

class EchoServer final : public IocpServer 
{
public:
	explicit EchoServer();
	virtual ~EchoServer();

private:
	static unsigned __stdcall WorkerThread(void* arguments);
	int WorkerThread_Working();	// �������� ������ ���� ��

private:
#ifdef NORMAL_VERSION
	// ������ ó�� �� �۽� ��Ŷ ����ȭ ���� �Լ�
	bool UpdateProcess(const DWORD64 sessionID, PacketBuffer* outPacketBuffer);	
#endif
	// echoData ����ȭ ó�� �Լ�	
	void EchoMakePacket(PacketBuffer* outPacketBuffer, const char* data, const WORD size);			

public:
	const HANDLE Get_EchoThread() const { return mThread; }

public: // ���� ������ �ο��� ���� �� �����Լ�
	// accept ���� ���� �ź� �� ���� ��� �Լ�(��������)
	virtual bool OnConnectionRequest(const WCHAR* ip, const WORD port) override;	
	// Accept ����ó�� �Ϸ� �� ȣ�� �Լ�(��������)
	virtual void OnConnectionSuccess(const DWORD64 sessionID) override;	
	// Release ȣ�� �� ó�� �Լ�(������ ��)
	virtual void OnClientLeave(const DWORD64 sessionID) override;	
#ifdef NORMAL_VERSION
	// ��Ŷ ���� �Ϸ� �� ó�� �Լ�(������ ��)
	virtual void OnRecv(const DWORD64 sessionID, 
		const HeaderInfo& header, PacketBuffer& packetBuffer) override; 
#elif DUMMY_TEST_VERSION
	// ��Ŷ ���� �Ϸ� �� ó�� �Լ�(������ ��)
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) override;	
#endif
	// ������ �ο��� ������ ���� ���� ��Ű�� �Լ�(������ ��)
	virtual void OnDisconnect(const DWORD64 sessionID) override;	

private: // ������ ���� ����
	HANDLE mThread = nullptr;
	unsigned int mThreadID = 0;
	HANDLE mWorkThreadEvent = nullptr;
	mutex mJobQueueLock;

private:
	RingBuffer* mJobQueue = nullptr;

};

