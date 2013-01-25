#pragma once

// Simple wrapper around Android's logging interface that also allows other
// implementations, and also some misc utilities.


// Disable annoying warnings in VS
#ifdef _MSC_VER
#pragma warning (disable:4996)   //strcpy may be dangerous
#endif

// Logging
#ifdef _WIN32

#undef Crash

#ifdef _M_X64
inline void Crash() { /*DebugBreak();*/ }
#else
inline void Crash() { __asm { int 3 }; }
#endif

#else

#if defined(ARM)
#undef Crash

inline void Crash() {
  char *p = (char *)1337;
  *p = 1;
}
#else
// TODO: 64-bit version
inline void Crash() {
  asm("int $0x3");
}
#endif

#endif

#if defined(ANDROID)

#include <android/log.h>

// Must only be used for logging
#ifndef APP_NAME
#define APP_NAME "NativeApp"
#endif

#define ILOG(...)    __android_log_print(ANDROID_LOG_INFO, APP_NAME, __VA_ARGS__);
#define WLOG(...)    __android_log_print(ANDROID_LOG_WARN, APP_NAME, __VA_ARGS__);
#define ELOG(...)    __android_log_print(ANDROID_LOG_ERROR, APP_NAME, __VA_ARGS__);
#define FLOG(...)   { __android_log_print(ANDROID_LOG_ERROR, APP_NAME, __VA_ARGS__); Crash(); }

#define MessageBox(a, b, c, d) __android_log_print(ANDROID_LOG_INFO, APP_NAME, "%s %s", (b), (c));

#elif defined(__SYMBIAN32__)
#include <QDebug>
#define ILOG(...) { qDebug(__VA_ARGS__);}
#define WLOG(...) { qDebug(__VA_ARGS__);}
#define ELOG(...) { qDebug(__VA_ARGS__);}
#define FLOG(...) { qDebug(__VA_ARGS__); Crash();}

#else

// TODO: Win32 version using OutputDebugString

#include <stdio.h>

#define ILOG(...) {printf("I: %s:%i: ", __FILE__, __LINE__); printf("I: " __VA_ARGS__); printf("\n");}
#define WLOG(...) {printf("W: %s:%i: ", __FILE__, __LINE__); printf("W: " __VA_ARGS__); printf("\n");}
#define ELOG(...) {printf("E: %s:%i: ", __FILE__, __LINE__); printf("E: " __VA_ARGS__); printf("\n");}
#define FLOG(...) {printf("F: %s:%i: ", __FILE__, __LINE__); printf("F: " __VA_ARGS__); printf("\n"); Crash();}

#endif

#undef CHECK

#define CHECK(a) {if (!(a)) {FLOG("CHECK failed");}}
#define CHECK_P(a, ...) {if (!(a)) {FLOG("CHECK failed: " __VA_ARGS__);}}
#define CHECK_EQ(a, b) CHECK((a) == (b));
#define CHECK_NE(a, b) CHECK((a) != (b));
#define CHECK_GT(a, b) CHECK((a) > (b));
#define CHECK_GE(a, b) CHECK((a) >= (b));
#define CHECK_LT(a, b) CHECK((a) < (b));
#define CHECK_LE(a, b) CHECK((a) <= (b));
