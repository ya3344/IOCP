#pragma once
#include "LanServer.h"

class EchoServer : public LanServer
{
public:
	EchoServer() = default;
	virtual ~EchoServer() = default;


public: // ���� ������ �ο��� ���� �� �����Լ�
	virtual bool OnConnectionRequest(const DWORD ip, const WORD port) override; // accept ���� return false; �� Ŭ���̾�Ʈ �ź�. return true; �� ���� ���
	//virtual void OnClientJoin(Client ���� /SessionID / ��Ÿ���) = 0; // Accept �� ����ó�� �Ϸ� �� ȣ��.
	virtual void OnClientLeave(const DWORD64 sessionID) override; // Release �� ȣ��
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) override; // ��Ŷ ���� �Ϸ� ��
};

