#include "pch.h"
#include "EchoServer.h"

bool EchoServer::OnConnectionRequest(const DWORD ip, const WORD port)
{
	return false;
}

void EchoServer::OnClientLeave(const DWORD64 sessionID)
{
}

void EchoServer::OnRecv(const DWORD64 SessionID, const PacketBuffer& packetBuffer)
{
	//packetBuffer.
}
