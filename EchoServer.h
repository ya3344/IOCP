#pragma once
#include "LanServer.h"

class EchoServer : public LanServer
{
public:
	EchoServer();
	virtual ~EchoServer();

private:
	static unsigned __stdcall WorkerThread(void* arguments);
	int WorkerThread_Working(); // �������� ������ ���� ��

public:
	const HANDLE Get_EchoThread() const { return mThread; }

public: // ���� ������ �ο��� ���� �� �����Լ�
	virtual bool OnConnectionRequest(const DWORD ip, const WORD port) override; // accept ���� return false; �� Ŭ���̾�Ʈ �ź�. return true; �� ���� ���
	//virtual void OnClientJoin(Client ���� /SessionID / ��Ÿ���) = 0; // Accept �� ����ó�� �Ϸ� �� ȣ��.
	virtual void OnClientLeave(const DWORD64 sessionID) override; // Release �� ȣ��
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) override; // ��Ŷ ���� �Ϸ� ��

private: // ������ ���� ����
	HANDLE mThread;
	unsigned int mThreadID;
	SRWLOCK mSendBufferLock;
	HANDLE mWorkThreadEvent = NULL;

private:
	RingBuffer* mSendBuffer = nullptr;

};

