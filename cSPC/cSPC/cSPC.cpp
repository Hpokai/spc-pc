// cSPC.cpp : �w�q DLL ���ε{�����ץX�禡�C
//

#include "stdafx.h"
#include "cSPC.h"


LibSPC* CreateLibSPC()
{
	printf("cSPC: CreateLibSPC\n");
    return new LibSPC();
}

 void Close(LibSPC* ctor)
 {
	printf("cSPC: Close\n");
	ctor->Close();
 }
 bool IsKeyError(LibSPC* ctor)
 {
	printf("cSPC: IsKeyError\n");
	return ctor->IsKeyError();
 }

int test(LibSPC* ctor, int a, int b, char port_name[], bool run, bool key_error )
{
	printf("cSPC: test\n");
	printf("cSPC: port_name ~##########~ %s\n",port_name);
	return ctor->test(a,b,port_name,run,key_error);
}

