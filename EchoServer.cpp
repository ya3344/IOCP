#include "pch.h"
#include "EchoServer.h"

bool EchoServer::OnConnectionRequest(const DWORD ip, const WORD port)
{
	return false;
}

void EchoServer::OnClientLeave(const DWORD64 sessionID)
{
}

void EchoServer::OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer)
{
	PacketBuffer sendPacketBuffer(PacketBuffer::BUFFER_SIZE_DEFAULT);

	char buffer[PACKET_SIZE] = { 0 };
	packetBuffer.GetData(buffer, PACKET_SIZE);
	sendPacketBuffer.PutData(buffer, PACKET_SIZE);
	SendPacket(sessionID, sendPacketBuffer);
}
