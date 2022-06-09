#pragma once

enum HEADER_INDEX
{
	HEADER_NONE = 0,
	HEADER_CS_ECHO_DATA = 1,
	HEADER_SC_ECHO_DATA = 1,
};

#pragma pack(push, 1)   
struct HeaderInfo
{
#ifdef NORMAL_VERSION
	WORD msgType = HEADER_NONE;								// Dummy Test 에는 msyType 제거
#endif
	WORD length = 0;
};
#pragma pack(pop)

