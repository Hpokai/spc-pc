#ifndef LIBSPC_H
#define LIBSPC_H

#include <stdio.h>      /* �зǿ�J��X�w�q */
#include <stdlib.h>     /* �зǨ�Ʈw�w�q */
//#include <unistd.h>     /* Unix �зǨ�Ʃw�q */
#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/ioctl.h>
#include <fcntl.h>      /* �ɱ���w�q */
//#include <termios.h>    /* PPSIX �׺ݱ���w�q */
#include <errno.h>      /* ���~���w�q */
#include <string.h>
//#include <pthread.h>
//#include <sys/signal.h>
#include <signal.h>
//#include <bits/siginfo.h>
//#include <dirent.h>
#include <time.h>
#include <windows.h>
#include <Iphlpapi.h>
#include <assert.h>
#pragma comment(lib, "iphlpapi.lib")
#using <System.dll>


class LibSPC
{

public:
    LibSPC();
    void Close();
    bool IsKeyError();
	int test(int, int, char[], bool, bool);

private:
    //pthread_t thread_id;

};

#endif // LIBSPC_H