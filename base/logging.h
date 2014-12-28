#pragma once

#include "base/arch.h"
#include "base/backtrace.h"
#include "base/compat.h"

// Simple wrapper around Android's logging interface that also allows other
// implementations, and also some misc utilities.

#ifdef _MSC_VER
// Disable annoying warnings in VS
#pragma warning (disable:4996)   //strcpy may be dangerous, etc.
#endif

#undef Crash

#include <stdio.h>
// Logging
#ifdef _WIN32

#ifdef _M_X64
inline void Crash() { /*DebugBreak();*/ }
#else
inline void Crash() { __asm { int 3 }; }
#endif

#else

#if defined(_M_IX86) || defined(_M_X64)
inline void Crash() {
	PrintBacktraceToStderr();
	asm("int $0x3");
}
#else
inline void Crash() {
	PrintBacktraceToStderr();
	char *p = (char *)1337;
	*p = 1;
}
#endif

#endif

// Just ILOGs on nonWindows. On Windows it outputs to the VS output console.
void OutputDebugStringUTF8(const char *p);

#if defined(ANDROID)

#include <android/log.h>

// Must only be used for logging
#ifndef APP_NAME
#define APP_NAME "NativeApp"
#endif

#ifdef _DEBUG
#define DLOG(...)    __android_log_print(ANDROID_LOG_INFO, APP_NAME, __VA_ARGS__);
#else
#define DLOG(...)
#endif

#define ILOG(...)    __android_log_print(ANDROID_LOG_INFO, APP_NAME, __VA_ARGS__);
#define WLOG(...)    __android_log_print(ANDROID_LOG_WARN, APP_NAME, __VA_ARGS__);
#define ELOG(...)    __android_log_print(ANDROID_LOG_ERROR, APP_NAME, __VA_ARGS__);
#define FLOG(...)   { __android_log_print(ANDROID_LOG_ERROR, APP_NAME, __VA_ARGS__); Crash(); }

#define MessageBox(a, b, c, d) __android_log_print(ANDROID_LOG_INFO, APP_NAME, "%s %s", (b), (c));

#elif defined(__SYMBIAN32__)
#include <QDebug>
#ifdef _DEBUG
#define DLOG(...) { qDebug(__VA_ARGS__);}
#else
#define DLOG(...)
#endif
#define ILOG(...) { qDebug(__VA_ARGS__);}
#define WLOG(...) { qDebug(__VA_ARGS__);}
#define ELOG(...) { qDebug(__VA_ARGS__);}
#define FLOG(...) { qDebug(__VA_ARGS__); Crash();}

#else

#ifdef _WIN32

#define _LOG(s, ...) { \
	const int BUFSIZE = 512; \
	char buf[BUFSIZE + 1]; \
	int len; \
	len = snprintf(buf, BUFSIZE, "%s%s:%i: ", s, __FILE__, __LINE__); \
	len += snprintf(buf + len, BUFSIZE - len, s __VA_ARGS__); \
	sprintf(buf + len, "\n"); \
	OutputDebugStringUTF8(buf); \
}


#ifdef _DEBUG
#define DLOG(...) _LOG("D: ", __VA_ARGS__)
#else
#define DLOG(...)
#endif

#define ILOG(...) _LOG("I: ", __VA_ARGS__)
#define WLOG(...) _LOG("W: ", __VA_ARGS__)
#define ELOG(...) _LOG("E: ", __VA_ARGS__)
#define FLOG(...) _LOG("F: ", __VA_ARGS__) Crash();

// TODO: Win32 version using OutputDebugString
#else

#include <stdio.h>

#ifdef _DEBUG
#define DLOG(...) {printf("D: %s:%i: ", __FILE__, __LINE__); printf("D: " __VA_ARGS__); printf("\n");}
#else
#define DLOG(...)
#endif
#define ILOG(...) {printf("I: %s:%i: ", __FILE__, __LINE__); printf("I: " __VA_ARGS__); printf("\n");}
#define WLOG(...) {printf("W: %s:%i: ", __FILE__, __LINE__); printf("W: " __VA_ARGS__); printf("\n");}
#define ELOG(...) {printf("E: %s:%i: ", __FILE__, __LINE__); printf("E: " __VA_ARGS__); printf("\n");}
#define FLOG(...) {printf("F: %s:%i: ", __FILE__, __LINE__); printf("F: " __VA_ARGS__); printf("\n"); Crash();}

#endif
#endif

#undef CHECK

#define CHECK(a) {if (!(a)) {FLOG("%i: CHECK failed on this line", __LINE__);}}
#define CHECK_P(a, ...) {if (!(a)) {FLOG("CHECK failed: " __VA_ARGS__);}}
#define CHECK_EQ(a, b) CHECK((a) == (b));
#define CHECK_NE(a, b) CHECK((a) != (b));
#define CHECK_GT(a, b) CHECK((a) > (b));
#define CHECK_GE(a, b) CHECK((a) >= (b));
#define CHECK_LT(a, b) CHECK((a) < (b));
#define CHECK_LE(a, b) CHECK((a) <= (b));
