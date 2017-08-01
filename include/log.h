#include "libce_guard.h"

#ifndef LOG_H
#define LOG_H

#define ERROR_LEVEL	0x00
#define INFO_LEVEL	0x01
#define DEBUG_LEVEL	0x02

#ifndef LOG_LEVEL
#define LOG_LEVEL	ERROR_LEVEL
#endif


#define FLOG(format, ...)   flog(LOG_INFO, format, ##__VA_ARGS__)

#define ERROR_TAG	"ERROR"
#define INFO_TAG	"INFO"
#define DEBUG_TAG	"DEBUG"

#if LOG_LEVEL >= DEBUG_LEVEL
#define LOG_DEBUG(message, args...) FLOG(message, ## args)
#else
#define LOG_DEBUG(message, args...)
#endif

#if LOG_LEVEL >= INFO_LEVEL
#define LOG_INFO(message, args...)  FLOG(message, ## args)
#else
#define LOG_INFO(message, args...)
#endif

#if LOG_LEVEL >= ERROR_LEVEL
#define LOG_ERROR(message, args...) FLOG(message, ## args)
#else
#define LOG_ERROR(message, args...)
#endif

#endif