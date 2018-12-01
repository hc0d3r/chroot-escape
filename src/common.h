#ifndef __COMMON_H__
#define __COMMON_H__

#define say(x, y, z...) do { printf(x y, ##z);} while(0)

#define info(x, y...) say("[\033[1;34m*\033[0m] ", x, ##y)
#define good(x, y...) say("[\033[1;32m+\033[0m] ", x, ##y)
#define bad(x, y...) say("[\033[0;31m-\033[0m] ", x, ##y)

#endif
