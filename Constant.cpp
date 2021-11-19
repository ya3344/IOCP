#include "pch.h"
int gLogLevel = LOG_LEVEL_ERROR; // 출력 저장 대상의 로그 레벨
WCHAR gLogBuffer[1024] = { 0, }; // 로그 저장시 필요한 임시 버퍼