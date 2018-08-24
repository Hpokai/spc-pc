// cSPC.h

 #pragma once
  
 #ifndef CSPC_H
 #define CSPC_H 

#include "SPC.h"


//#ifdef CSPC_EXPORTS
#define CSPC_API __declspec(dllexport) 
//#else
//#define CSPC_API __declspec(dllimport) 
//#endif

extern "C"
{	
	CSPC_API LibSPC* CreateLibSPC();

	CSPC_API void Close(LibSPC* );
    CSPC_API bool IsKeyError(LibSPC* );

	CSPC_API int test(LibSPC* ctor, int , int, char [], bool, bool );
}




 #endif