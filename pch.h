// pch.h: 미리 컴파일된 헤더 파일입니다.
// 아래 나열된 파일은 한 번만 컴파일되었으며, 향후 빌드에 대한 빌드 성능을 향상합니다.
// 코드 컴파일 및 여러 코드 검색 기능을 포함하여 IntelliSense 성능에도 영향을 미칩니다.
// 그러나 여기에 나열된 파일은 빌드 간 업데이트되는 경우 모두 다시 컴파일됩니다.
// 여기에 자주 업데이트할 파일을 추가하지 마세요. 그러면 성능이 저하됩니다.

#ifndef PCH_H
#define PCH_H

// 여기에 미리 컴파일하려는 헤더 추가
// socket 관련
#include <winsock2.h>
#include <stdio.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")

// STL
#include <memory>
#include <unordered_map>
#include <iostream>
#include <list>
#include <unordered_set>
#include <string>
#include <wchar.h>
#include <stdio.h>
#include <stdarg.h>
#include <random>
#include <string.h>
#include <process.h>
#include <stack>
#include <sstream>
#include <Windows.h>
#include <time.h>
#include <stdlib.h>
#include <strsafe.h>
#include <stdio.h>
#include <Psapi.h>
//#include <minidumpapiset.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
using namespace std;


#ifdef _DEBUG
#define new new( _NORMAL_BLOCK, __FILE__, __LINE__ )                                            
#endif

// Warning Ignore
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "winmm.lib") 

#include "define.h"
#include "SystemLog.h"
#include "CrashDump.h"
#include "Constant.h"
#include "Macro.h"



#endif //PCH_H
