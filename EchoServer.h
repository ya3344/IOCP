#pragma once
#include "IocpServer.h"

class EchoServer final : public IocpServer 
{
public:
	explicit EchoServer();
	virtual ~EchoServer();

private:
	static unsigned __stdcall WorkerThread(void* arguments);
	int WorkerThread_Working();	// 실질적인 쓰레드 실행 부
	static unsigned __stdcall MoinitorThread(void* arguments);
	int MonitorThread_Working(); // 실질적인 쓰레드 실행 부

private:
#ifdef NORMAL_VERSION
	// 컨텐츠 처리 및 송신 패킷 직렬화 진행 함수
	bool UpdateProcess(const DWORD64 sessionID, PacketBuffer* outPacketBuffer);	
#endif
	// echoData 직렬화 처리 함수	
	void EchoMakePacket(PacketBuffer* outPacketBuffer, const char* data, const WORD size);			

public:
	const HANDLE Get_EchoThread() const { return mThread; }

public: // 실제 컨테츠 부에서 실행 할 가상함수
	// accept 직후 유저 거부 및 접속 허용 함수(컨텐츠부)
	virtual bool OnConnectionRequest(const WCHAR* ip, const WORD port) override;	
	// Accept 접속처리 완료 후 호출 함수(컨텐츠부)
	virtual void OnConnectionSuccess(const DWORD64 sessionID) override;	
	// Release 호출 후 처리 함수(컨텐츠 부)
	virtual void OnClientLeave(const DWORD64 sessionID) override;	
#ifdef NORMAL_VERSION
	// 패킷 수신 완료 후 처리 함수(컨텐츠 부)
	virtual void OnRecv(const DWORD64 sessionID, 
		const HeaderInfo& header, PacketBuffer& packetBuffer) override; 
#elif DUMMY_TEST_VERSION
	// 패킷 수신 완료 후 처리 함수(컨텐츠 부)
	virtual void OnRecv(const DWORD64 sessionID, PacketBuffer& packetBuffer) override;	
#endif
	// 컨텐츠 부에서 유저를 접속 종료 시키는 함수(컨텐츠 부)
	virtual void OnDisconnect(const DWORD64 sessionID) override;	

private: // 쓰레드 관련 변수
	HANDLE mThread = NULL;
	HANDLE mMonitorThraed = NULL;
	unsigned int mThreadID = 0;
	HANDLE mWorkThreadEvent = NULL;
	mutex mJobQueueLock;

private:
	RingBuffer* mJobQueue = nullptr;

private: //디버깅용 변수
	DWORD mFrameTime = timeGetTime();

};

