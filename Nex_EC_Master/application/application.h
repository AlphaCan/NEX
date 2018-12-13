#pragma once

#ifndef _APPLICATION_
#define _APPLICATION_

#ifdef __cplusplus
extern "C"
{
#endif


#ifdef APP_DEBUG
#define debug_PRINT(format,...) printf(">>Time: %s, Line: %05d, Function: %s: "format"", __TIME__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#else
#define debug_PRINT(...) do{}while(0)
#endif




#ifdef __cplusplus
}
#endif

#endif // !_APPLICATION_
