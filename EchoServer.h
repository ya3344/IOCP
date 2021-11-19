#pragma once
#include "LanServer.h"

class EchoServer : public LanServer
{
public:
	EchoServer() = default;
	virtual ~EchoServer() = default;


public: // 실제 컨테츠 부에서 실행 할 가상함수
	virtual bool OnConnectionRequest(const DWORD ip, const WORD port) override; // accept 직후 return false; 시 클라이언트 거부. return true; 시 접속 허용
	//virtual void OnClientJoin(Client 정보 /SessionID / 기타등등) = 0; // Accept 후 접속처리 완료 후 호출.
	virtual void OnClientLeave(const DWORD64 sessionID) override; // Release 후 호출
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) override; // 패킷 수신 완료 후
};

