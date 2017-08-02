#ifndef REDISTRICTER_LOGGING_H
#define REDISTRICTER_LOGGING_H

// probably only works with gnu-c macros

extern "C" int rlogprintf(int level, const char* fmt, ...);
extern "C" int rlog_level;

#define RLOG_ERROR 0
#define RLOG_WARNING 1
#define RLOG_INFO 2
#define RLOG_DEBUG 3
#define RLOG_V1 4
// VN == RLOG_DEBUG + N

#define errorprintf(fmt, ...) if(rlog_level >= RLOG_ERROR) {rlogprintf(RLOG_ERROR, fmt, __VA_ARGS__);}
#define warnprintf(fmt, ...) if(rlog_level >= RLOG_WARNING) {rlogprintf(RLOG_WARNING, fmt, __VA_ARGS__);}
#define infoprintf(fmt, ...) if(rlog_level >= RLOG_INFO) {rlogprintf(RLOG_INFO, fmt, __VA_ARGS__);}
#define debugprintf(fmt, ...) if(rlog_level >= RLOG_INFO) {rlogprintf(RLOG_INFO, fmt, __VA_ARGS__);}
#define verboseprintf(v,fmt, ...) if(rlog_level >= (RLOG_DEBUG+(v))) {rlogprintf((RLOG_DEBUG+(v)), fmt, __VA_ARGS__);}

#endif /* REDISTRICTER_LOGGING_H */
