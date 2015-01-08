#ifndef CONFIG_H
#define CONFIG_H

#define OUTSIDE_SPEEX         1
#define OPUSTOOLS             1

#define inline __inline
#ifndef alloca
#define alloca _alloca
#endif 

#ifndef XP_UNIX
#define getpid _getpid
#endif

#define USE_ALLOCA            1
#define FLOATING_POINT        1
#define SPX_RESAMPLE_EXPORT

#ifndef __SSE__
#define __SSE__
#endif 

#define RANDOM_PREFIX foo

#define PACKAGE "AIRAudio"
#include "version.h"

#endif 

