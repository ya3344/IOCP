#pragma once

//#define NORMAL_VERSION			
#ifndef NORMAL_VERSION
#define DUMMY_TEST_VERSION		1
#endif

//#define WORKTHREAD_ENABLE

enum LOG_INDEX
{
	LOG_LEVEL_DEBUG = 0,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_DISPLAY,
};

enum INFO_INDEX
{
	NAME_BUFFER_SIZE = 15,
	IP_BUFFER_SIZE = 16,
	ADD_THREAD_NUM = 2,
	ACCEPT_TRHEAD_NUM = 1,
	DISCONNECT_FUNC = 1,
	MEMORY_LOG_MAX_NUM = 10000,
};

#define MEMORY_LOG_MAX_NUM		10000

