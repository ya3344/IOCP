#pragma once
#include "Macro.h"

extern int gLogLevel; // 출력 저장 대상의 로그 레벨
extern WCHAR gLogBuffer[1024]; // 로그 저장시 필요한 임시 버퍼
extern SystemLog gLog;
extern CrashDump gCrashDump;
