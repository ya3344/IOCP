#include "pch.h"
int gLogLevel = LOG_LEVEL_DISPLAY; // ��� ���� ����� �α� ����
WCHAR gLogBuffer[1024] = { 0, }; // �α� ����� �ʿ��� �ӽ� ����
SystemLog gLog;
CrashDump gCrashDump;