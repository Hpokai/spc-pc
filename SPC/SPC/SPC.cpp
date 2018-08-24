#include "SPC.h"
#include <atlstr.h>
using namespace System;
using namespace System::Threading;

#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyACM0" //Arduino SA Mega 2560 R3 (CDC ACM)
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;
int fd;
HANDLE serial_handle;		// Open serial port
DCB old_serial_params = {0}, new_serial_params = {0}, temp_serial_params = {0};	// Do some basic settings

byte* r_data, *s_command;	//receiced "Data", sent "command".
int loop = 0;
bool isCorrectData = false, isInfoPrepared = false, isKeyError = false;

enum ACTION {NONE, CKD_STATE, WAIT_STATE, SEND_INFO, WAIT_KEY, RUN_TIME_CHECK, WAIT_RANDOM_CHECK, RANDOM_TIMER};
ACTION ACT = NONE;

static byte* KEY = NULL;
int random_times = 0;
int msleep_in_mainloop = 1000;
int check_times = 0;


int get_random_value ( int min, int max, bool isON = true )
{
    if ( true == isON )	srand ( (unsigned)time ( NULL ) );

    return ( rand() % ( max - min + 1 ) ) + min;
}

int parse_dongle_key ( byte* r_key )
{
    int ret = -1;
    //printf("key - %s\n", r_key);
    byte* one = ( r_key + 32 );

    if ( 0 != strcmp ( ( char* ) one, "1" ) )
    {
        puts ( "dongle_key 32 is not 1" );
    }
    else
    {
        for ( int i = 0; i < 16; i++ )
        {
            byte temp_str[2] = {'\0'};
            temp_str[0] = * ( r_key + i * 2 );
            temp_str[1] = * ( r_key + i * 2 + 1 );
            * ( KEY + i ) = ( byte ) strtol ( ( char* ) temp_str, NULL, 16 );
        }

        * ( KEY + 16 ) = ( byte ) 0x01;
        //printf("KEY - %s\n", KEY);
        ret = 0;
    }

    return ret;
}

byte* parse_received_data ( byte data[] )
{
    int start_index = -1, end_index = -1;

    char *pch_s = strstr ( ( char* ) data, "7D" );

    if ( pch_s != NULL )	start_index = ( int ) ( pch_s - ( char* ) data ) + 2;

    char *pch_e = strstr ( ( char* ) data, "7E" );

    if ( pch_e != NULL )	end_index = ( int ) ( pch_e - ( char* ) data );

    byte* temp_data = NULL;

    if ( ( pch_s != NULL ) && ( pch_e != NULL )	)
    {
        int length = end_index - start_index + 1;
        temp_data = ( byte * ) malloc ( length * sizeof ( byte ) );

        for ( int i = 0; i < length; i++ )	* ( temp_data + i ) = 0;

        for ( int i = 0; i < ( length - 1 ); i++ )	* ( temp_data + i ) = data[start_index + i];

        //printf("data=%s\n",data);
        //printf("temp_data=%s\n",temp_data);

        isCorrectData = true;
        return temp_data;
    }
    else
    {
        puts ( "NOT correct data!" );
        return NULL;
    }
}

byte select_cpu_type ( char data[] )
{
    char* pch = NULL;
    byte ret = '\0';

    if ( ( pch = strstr ( data, "Intel" ) ) != NULL )
    {
        if ( ( pch = strstr ( data, "Celeron" ) ) != NULL )			ret = ( byte ) ( 0x21 );
        else if ( ( pch = strstr ( data, "Atom" ) ) != NULL )		ret = ( byte ) ( 0x22 );
        else if ( ( pch = strstr ( data, "Pentium" ) ) != NULL )		ret = ( byte ) ( 0x23 );
        else if ( ( pch = strstr ( data, "Core i3" ) ) != NULL )		ret = ( byte ) ( 0x24 );
        else if ( ( pch = strstr ( data, "Core i5" ) ) != NULL )		ret = ( byte ) ( 0x25 );
        else if ( ( pch = strstr ( data, "Core i7" ) ) != NULL )		ret = ( byte ) ( 0x26 );
        else if ( ( pch = strstr ( data, "Xeon E3" ) ) != NULL )		ret = ( byte ) ( 0x27 );
        else if ( ( pch = strstr ( data, "Xeon E5" ) ) != NULL )		ret = ( byte ) ( 0x28 );
        else 														ret = ( byte ) ( 0x29 );
    }
    else if ( ( pch = strstr ( data, "AMD" ) ) != NULL )
    {
        if ( ( pch = strstr ( data, "Pro A" ) ) != NULL )			ret = ( byte ) ( 0x31 );
        else if ( ( pch = strstr ( data, "A" ) ) != NULL )		ret = ( byte ) ( 0x32 );
        else if ( ( pch = strstr ( data, "C" ) ) != NULL )		ret = ( byte ) ( 0x33 );
        else if ( ( pch = strstr ( data, "E" ) ) != NULL )		ret = ( byte ) ( 0x34 );
        else if ( ( pch = strstr ( data, "FX" ) ) != NULL )		ret = ( byte ) ( 0x35 );
        else if ( ( pch = strstr ( data, "GX" ) ) != NULL )		ret = ( byte ) ( 0x36 );
        else if ( ( pch = strstr ( data, "Ryzen" ) ) != NULL )	ret = ( byte ) ( 0x37 );
        else if ( ( pch = strstr ( data, "Z" ) ) != NULL )		ret = ( byte ) ( 0x38 );
        else 													ret = ( byte ) ( 0x39 );
    }
    else
    {
        if ( ( pch = strstr ( data, "BCM" ) ) != NULL )				ret = ( byte ) ( 0x31 );
        else if ( ( pch = strstr ( data, "Qualcomm" ) ) != NULL )	ret = ( byte ) ( 0x41 );
        else if ( ( pch = strstr ( data, "Atmel" ) ) != NULL )		ret = ( byte ) ( 0x51 );
        else	ret = ( byte ) ( 0x61 );
    }

    return ret;
}

byte get_cpu_type_info()
{
	byte return_byte = {'\0'};
    char get_data[64] = {'\0'}, parse_data[64] = {'\0'};
	char* cptr = {'\0'};
	
	SYSTEM_INFO siSysInfo;		// Copy the hardware information to the SYSTEM_INFO structure
	GetSystemInfo(&siSysInfo); 
		
	//printf("OEM ID: %u\n", siSysInfo.dwOemId);	//Display the contents of the SYSTEM_INFO structure. 
	switch (siSysInfo.dwOemId)
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		cptr = ":AMD64\0";
		memmove ( get_data, cptr, strlen ( cptr ) );
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		cptr = ":Intel\0";
		memmove ( get_data, cptr, strlen ( cptr ) );
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		cptr = ":ARM\0";
		memmove ( get_data, cptr, strlen ( cptr ) );
		break;	
	case PROCESSOR_ARCHITECTURE_IA64:
		cptr = ":IA64\0";
		memmove ( get_data, cptr, strlen ( cptr ) );
		break;	
	default:
		cptr = ":Unknown architecture\0";
		memmove ( get_data, cptr, strlen ( cptr ) );
		break;
	}
	//printf("data: %s\n", get_data);

    char* pch = strstr ( get_data, ":" );

    if ( pch != NULL )
    {
        memmove ( parse_data, ( pch + 1 ), strlen ( pch ) - 1 );
        return_byte = select_cpu_type ( parse_data );
    }
    else
    {
        puts ( "ptr is null" );
        return_byte = ( byte ) ( 0xFF );
    }

    //printf("CPU return_byte=%x\n",return_byte);
    return return_byte;
}

byte select_memory_size ( int ram_kb )
{
	double memory_size = 0.0;
	byte ret = '\0';

	memory_size = ram_kb / 1024;
	memory_size  = memory_size / 1024;
	//printf("mem= %lf\n",memory_size);

	if ( 0.85 > memory_size )									ret = ( byte ) ( 0x22 );		// less than 1G
	else if ( 1.85 > memory_size && 0.85 <= memory_size )		ret = ( byte ) ( 0x33 );		// between 1G ~ 2G
	else if ( 3.85 > memory_size && 1.85 <= memory_size )		ret = ( byte ) ( 0x44 );		// between 2G ~ 4G
	else if ( 7.85 > memory_size && 3.85 <= memory_size )		ret = ( byte ) ( 0x55 );		// between 4G ~ 8G
	else if ( 15.85 > memory_size && 7.85 <= memory_size )		ret = ( byte ) ( 0x66 );		// between 8G ~ 16G
	else 														ret = ( byte ) ( 0x77 );		// more than 16G

	return ret;
}

byte get_memory_size_info()
{
    byte return_byte = {'\0'};
	
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof (statex);
	GlobalMemoryStatusEx (&statex);

	ULONGLONG *ptr_mem = new ULONGLONG;  
	if (GetPhysicallyInstalledSystemMemory(ptr_mem)) 
	{  
		//printf("TotalMem : %d\n", *ptr_mem / 1024L);  
		return_byte = select_memory_size (*ptr_mem );	// total KB of installed physical memory
	} 
    //printf("MEM return_byte=%x\n",return_byte);
    return return_byte;
}

byte* GetMACaddress(int type)	// type 1: ethernet MAC, type 2: wifi MAC
{
    byte* return_byte = ( byte* ) malloc ( 7 * sizeof ( byte ) );
	
	IP_ADAPTER_INFO AdapterInfo[16];			// Allocate information for up to 16 NICs
	DWORD dwBufLen = sizeof(AdapterInfo);		// Save the memory size of buffer

	DWORD dwStatus = GetAdaptersInfo(			// Call GetAdapterInfo
		AdapterInfo,							// [out] buffer to receive data
		&dwBufLen);								// [in] size of receive data buffer
	assert(dwStatus == ERROR_SUCCESS);			// Verify return value is valid, no buffer overflow

	PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;// Contains pointer to current adapter info
	
    char* pch = NULL;
	for (int i = 0; i < 6; i++)		*(return_byte + i) = (byte) 0xFF;

	switch (type)
	{
	case 1:
		for (int i = 0; i < 6; i++)		*(return_byte + i) = pAdapterInfo->Address[i];
		break;
	case 2:
		pAdapterInfo = pAdapterInfo->Next;		// Progress through linked list
		do 
		{
			if (( ( pch = strstr ( pAdapterInfo->Description, "Virtual" ) ) != NULL )	||
			    ( ( pch = strstr ( pAdapterInfo->Description, "Miniport" ) ) != NULL )	||
			    ( ( pch = strstr ( pAdapterInfo->Description, "Bluetooth" ) ) != NULL )	)
			{
				pAdapterInfo = pAdapterInfo->Next;		// Progress through linked list
			}
			else
			{
				for (int i = 0; i < 6; i++)		*(return_byte + i) = pAdapterInfo->Address[i];
				break;
			}
		}
		while(pAdapterInfo);	
		break;
	default:
		break;
	}

    return return_byte;
}

byte* get_ethernet_info()
{
    byte* return_byte = ( byte* ) malloc ( 13 * sizeof ( byte ) );

    for ( int i = 0; i < 13; i++ )	* ( return_byte + i ) = ( byte ) 0xFF;
    * ( return_byte + ( strlen ( ( char* ) return_byte ) - 1 ) ) = '\0';
			
    memmove ( ( return_byte + 0 ), GetMACaddress(1), 6 );	//0-5: Ethernat MAC
    memmove ( ( return_byte + 6 ), GetMACaddress(2), 6 );	//6-11: Wifi MAC
	
    //for (int i = 0; i < 13; i++)	printf("return_byte=%X\n",*(return_byte+i));
    return return_byte;
}

byte* get_tranform_key ( char* word )
{
    byte* return_byte = ( byte* ) malloc ( ( 61 - 3 + 1 + 1 ) * sizeof ( byte ) );

    for ( int i = 0; i < ( 61 - 3 + 1 + 1 ); i++ )
    {
        * ( return_byte + i ) = 0;
    }

    for ( int i = 0; i < ( 61 - 3 + 1 ); i++ )
    {
        int temp = get_random_value ( ( int ) 0x01, ( int ) 0xFF, false );

        if ( temp > 0x7A && temp < 0x7F )   temp -= 10;

        * ( return_byte + i ) = ( byte ) temp;
    }

    //for (int i = 0; i < 60; i++)	printf("%d, get_random_value= %X\n",i,*(return_byte+i));


    byte* inverse_key = ( byte* ) malloc ( 18 * sizeof ( byte ) );

    for ( int i = 0; i < 18; i++ )
    {
        * ( inverse_key + i ) = 0;
    }

    for ( int i = 0; i < 17; i++ )
    {
        * ( inverse_key + i ) = ~* ( KEY + i );
    }

    //for ( int i = 0; i < 17; i++ )	printf ( "%d, KEY=%X\n", i, * ( KEY + i ) );
    //for ( int i = 0; i < 17; i++ )	printf ( "%d, inverse_key=%X\n", i, * ( inverse_key + i ) );

    int prime[17]	= {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59};
    int even[12]	= {2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24};
    int odd[16]	= {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};

    int n = 0;
    printf ( "word= %X\n", *word );

    switch ( *word )
    {
    case ( 0x5E ) :

        //puts("0x5E");
        for ( int i = 0; i < 59; i++ )
        {
            if ( i == ( prime[n] - 1 ) )
            {
                //printf("n = %d: prime = %d, i = %d\n",n, prime[n],i);
                memmove ( ( return_byte + i ), ( inverse_key + n ), 1 );
                n++;
            }
        }

        break;

    case ( 0x5F ) :

        //puts("0x5F");
        for ( int i = 0; i < 24; i++ )
        {
            if ( i == ( even[n] - 1 ) )
            {
                //printf("even = %d, i = %d\n",even[n],i);
                memmove ( ( return_byte + i ), ( inverse_key + n ), 1 );
                n++;
            }
        }

        break;

    case ( 0x60 ) :

        //puts("0x60");
        for ( int i = 0; i < 31; i++ )
        {
            if ( i == ( odd[n] - 1 ) )
            {
                //printf("odd = %d, i = %d\n",odd[n],i);
                memmove ( ( return_byte + i ), ( inverse_key + n ), 1 );
                n++;
            }
        }

        break;

    default:
        break;
    }

    //for (int i = 0; i < (int)strlen((char*)return_byte); i++)	printf("%d, return_byte= %X\n",i,*(return_byte+i));
    return return_byte;
}

byte* set_RUN_TIME_CHECK_command()
{
    // (Startbyte) + (Cmdbyte) + (Countbytes) + (Keybytes) + (Endbyte) + '\0' = 1+1+14+1+1 = 18
    byte* cmd = ( byte * ) malloc ( 64 * sizeof ( byte ) );

    for ( int i = 0; i < 64; i++ )
    {
        * ( cmd + i ) = 0;
    }

    * ( cmd + 0 ) = ( byte ) ( 0x7B );							//Start byte
    * ( cmd + 1 ) = ( byte ) ( 0x27 + get_random_value ( 1, 6 ) );	//CMD byte
    * ( cmd + 2 ) = ( byte ) ( 0x5D + get_random_value ( 1, 3 ) );	//Count byte
    //* ( cmd + 2 ) = ( byte ) ( 0x5F );
    //printf("cmd+2= %X\n",*(cmd+2));
    memmove ( ( cmd + 3 ), get_tranform_key ( ( char* ) ( cmd + 2 ) ), ( 61 - 3 + 1 ) );		//para3~61: Transform Data bytes
    * ( cmd + 62 ) = ( byte ) ( 0x7C );							//Endbyte


    //for ( int i = 0; i < ( int ) strlen ( ( char* ) cmd ); i++ )	printf ( "%d, cmd= %X\n", i, * ( cmd + i ) );

    return cmd;
}

byte* set_SEND_INFO_command()
{
    // (Startbyte) + (Cmdbyte) + (Parameterbytes) + (Endbyte) + '\0' = 1+1+14+1+1 = 18
    byte* cmd = ( byte * ) malloc ( 18 * sizeof ( byte ) );

    for ( int i = 0; i < 18; i++ )
    {
        * ( cmd + i ) = 0;
    }

    * ( cmd + 0 ) = ( byte ) ( 0x7B );							//Start byte
    * ( cmd + 1 ) = ( byte ) ( 0x22 );							//CMD byte
	printf ("There is 1\n");
    * ( cmd + 2 ) = get_cpu_type_info();				//para1: CPU Type
	printf ("There is 2\n");
    * ( cmd + 3 ) = get_memory_size_info();  			//para2: RAM Size
	printf ("There is 3\n");
    memmove ( ( cmd + 4 ), get_ethernet_info(), 12 );	//para3~14: Ethernat and Wifi MAC
	printf ("There is 4\n");
    * ( cmd + 16 ) = ( byte ) ( 0x7C );						//Endbyte
	printf ("There is 5\n");

    printf("cmd = %s\n",cmd);
    for (int i = 0; i < 18; i++)	printf("return_byte=%X\n",*(cmd+i));
    return cmd;
}

byte* set_CKD_STATE_command()
{
    byte* cmd = ( byte * ) malloc ( 8 * sizeof ( byte ) );

    for ( int i = 0; i < 8; i++ )
    {
        * ( cmd + i ) = 0;
    }

    * ( cmd + 0 ) = ( byte ) ( 0x7B );
    * ( cmd + 1 ) = ( byte ) ( 0x21 );
    * ( cmd + 2 ) = ( byte ) ( 0x7C );
    // printf("%s\n",cmd);
    return cmd;
}

/* -------------------------------
 * Main Loop
 * Input -> Process -> Output
 * -------------------------------
 */

void InputData()
{
	byte data[1024] = {'\0'};
	DWORD nread = 0;
	bool fWaitingOnRead = false;
	OVERLAPPED osReader = {0};
	unsigned long retlen=0;

	// Create the overlapped event. Must be closed before exiting to avoid a handle leak.
	//osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//if (osReader.hEvent == NULL)	printf("Error in creating Overlapped event\n");
	//if (!fWaitingOnRead)
	//{
		if (!ReadFile(serial_handle, data, sizeof(data), &nread,  &osReader)) 
		{			
			printf("ReadFile Fail!\n");
		}
		else
		{
			printf("ReadFile Success!\n");
			if(nread > 0)	
			{
				//if ( ( nread = read ( fd, data, sizeof ( data ) ) ) > 0 )
				//{
				printf ( "data: %s\n", data );
				//printf("%d\n", loop);

				byte* temp_data = parse_received_data ( data );

				if ( NULL != temp_data )
				{
					r_data = temp_data;
					printf ( "r_data = %s\n", r_data );
				}
				else
				{
					puts ( "resent cmd!!!!!!!!!!!!!!!!!!!!!!" );

					switch ( ACT )
					{
					case WAIT_STATE:
						ACT = CKD_STATE;
						break;

					case WAIT_KEY:
						ACT = SEND_INFO;
						break;

					case WAIT_RANDOM_CHECK:
						ACT = RUN_TIME_CHECK;
						break;

					default:
						break;
					}
				}    
			}		
		}
	//}	
}
void ProcessData()
{
    switch ( ACT )
    {
    case CKD_STATE:
        s_command = set_CKD_STATE_command();
        printf ( "s_command: %s\n", s_command );

        break;

    case WAIT_STATE:
        if ( true == isCorrectData )
        {
            if ( 0 == strcmp ( "F0", ( char* ) r_data ) )
            {
                printf ( "state = F0\n" );
                isInfoPrepared = false;
                ACT = SEND_INFO;
            }
            else if ( 0 == strcmp ( "A1", ( char* ) r_data ) )
            {
                printf ( "state = A1\n" );
                ACT = SEND_INFO;
            }
            else
            {
                printf ( "state = WTF\n" );
            }
        }

        break;

    case SEND_INFO:
        s_command = set_SEND_INFO_command();
        printf ( "s_command: %s\n", s_command );
        isInfoPrepared = true;
        break;

    case WAIT_KEY:
        if ( true == isCorrectData )
        {
            if ( 0 == strcmp ( "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", ( char* ) r_data ) )
            {
                printf ( "Got Key already\n" );
				
				DWORD ftyp = GetFileAttributes(L".\\config");
				printf("ftyp = %d\n", ftyp);
				if (ftyp !=INVALID_FILE_ATTRIBUTES)	// this is a directory!
				{
                    FILE* rfp = fopen ( ".\\config\\key", "rb" );

                    if ( NULL != rfp )
                    {
                        fread ( KEY, 18, 1, rfp );
                        //printf ( "KEY read: %s\n", KEY );
                        fclose ( rfp );

                        ACT = RUN_TIME_CHECK;
                    }
                    else
                    {
                        isKeyError = true;
                        puts ( "No File, read fail" );

                        ACT = NONE;
                    }
                }
                else
                {
                    isKeyError = true;
                    puts ( "No Dir, read fail" );

                    ACT = NONE;
                }
            }
            else
            {
                printf ( "Needs to parse & save Key\n" );

                if ( 0 <= parse_dongle_key ( r_data ) )
                {
					DWORD ftyp = GetFileAttributes(L".\\config");
					if( ftyp == INVALID_FILE_ATTRIBUTES)		// this is not a directory!
                    {
						if (!CreateDirectory(L".\\config", NULL)) 
						{ 
							printf("CreateDirectory failed (%d)\n", GetLastError()); 						
						} 
                    }

                    FILE* wfp = fopen ( ".\\config\\key", "wb+" );

                    if ( NULL != wfp )
                    {
                        fwrite ( KEY, strlen ( ( char* ) KEY ), 1, wfp );
                        fclose ( wfp );

                        ACT = RUN_TIME_CHECK;
                    }
                    else
                    {
                        puts ( "write fail" );
                    }
                }
                else
                {
                    puts ( "Parse KEY fail" );
                    ACT = NONE;
                }
            }

            //for ( int i = 0; i < 18; i++ )	printf ( "%d, KEY=%X\n", i, * ( KEY + i ) );
        }

        break;

    case RUN_TIME_CHECK:
        s_command = set_RUN_TIME_CHECK_command();
        printf ( "s_command: %s\n", s_command );
        isInfoPrepared = true;
        break;

    case WAIT_RANDOM_CHECK:
        if ( true == isCorrectData )
        {
            if ( 0 == strcmp ( "AA", ( char* ) r_data ) )
            {
                printf ( "WAIT_RANDOM_CHECK = AA\n" );
                isInfoPrepared = false;
                //random_times = get_random_value(15,20 ) * 60 * (int)(1000/msleep_in_mainloop);		//minute = 15 ~ 20
                random_times = get_random_value ( 1, 2 ) * 1 * ( int ) ( 1000 / msleep_in_mainloop );		//test minute = 1-3
                printf ( "random times = %d\n", random_times );

                check_times = 0;
                ACT = RANDOM_TIMER;
            }
            else
            {
                if  ( 2 < check_times )
                {
                    isKeyError = true;
                    printf ( "WAIT_RANDOM_CHECK = GG lar => no more!\n" );
                    ACT = NONE;
                }
                else
                {
                    check_times++;
                    printf ( "WAIT_RANDOM_CHECK = GG lar => %d times\n", check_times );
                    ACT = RUN_TIME_CHECK;   // resend cmd
                }
            }
        }

        break;

    case RANDOM_TIMER:
        if ( 0 == ( random_times-- )	)
        {
            ACT = RUN_TIME_CHECK;
        }

        break;

    default:
        //printf("gggggggggggggggggg");
        break;
    }
}
void OutputData()
{
	DWORD nwrite = 0;

    switch ( ACT )
    {
    case CKD_STATE:
		PurgeComm(serial_handle,PURGE_TXCLEAR|PURGE_RXCLEAR);	//PURGE_TXCLEAR = clear output, PURGE_RXCLEAR = clear input.			
        isCorrectData = false;
				
		if (WriteFile (serial_handle, s_command, strlen(( char* ) s_command), &nwrite, NULL))
		{
			if ( nwrite > 0 )
			{
				//printf("CKD_STATE nwrite = %d\n", nwrite);
				ACT = WAIT_STATE;
			}
		}
        break;

    case SEND_INFO:
        if ( true == isInfoPrepared )
        {
			PurgeComm(serial_handle,PURGE_TXCLEAR|PURGE_RXCLEAR);	//PURGE_TXCLEAR = clear output, PURGE_RXCLEAR = clear input.			
			isCorrectData = false;

			if (WriteFile (serial_handle, s_command, strlen(( char* ) s_command), &nwrite, NULL))
			{
				if ( nwrite > 0 )
				{
					//printf("SEND_INFO nwrite = %d\n", nwrite);
					isInfoPrepared = false;
					ACT = WAIT_KEY;
				}
			}
        }

        break;

    case RUN_TIME_CHECK:
        if ( true == isInfoPrepared )
        {
			PurgeComm(serial_handle,PURGE_TXCLEAR|PURGE_RXCLEAR);	//PURGE_TXCLEAR = clear output, PURGE_RXCLEAR = clear input.			
            isCorrectData = false;
			
			if (WriteFile (serial_handle, s_command, strlen(( char* ) s_command), &nwrite, NULL))
			{
				if ( nwrite > 0 )
				{
					//printf("RUN_TIME_CHECK nwrite = %d\n", nwrite);
					isInfoPrepared = false;
					ACT = WAIT_RANDOM_CHECK;
				}
			}
		}
        break;

    default:
        break;
    }
}

wchar_t *GetWC(const char *c)
{
    const size_t cSize = strlen(c)+1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs (wc, c, cSize);

    return wc;
}

bool isThreadRun = false;
char com_num[] = {'\0'};
static void SpcThreadRun()
{
	while ((isThreadRun == false) && (STOP == FALSE))
	{	
		printf("--==--==--==\n");
		Sleep ( msleep_in_mainloop );
	}

	if(isThreadRun)
	{
		char port[] = "\\\\\.\\";
		strcat(port,com_num);

		wchar_t* portW = GetWC(port);
		serial_handle = CreateFile(portW, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);	//FILE_FLAG_OVERLAPPED
		if (serial_handle == INVALID_HANDLE_VALUE)
		{
			printf(  "SpcThreadRun: open serial port fail!" );
		
		}
		else
		{

			old_serial_params.DCBlength = sizeof(old_serial_params);
			new_serial_params.DCBlength = sizeof(new_serial_params);
	
			if( (GetCommState(serial_handle, &old_serial_params) == 0) || (GetCommState(serial_handle, &new_serial_params) == 0) )
			{
				printf(  "SpcThreadRun: Error getting device state!" );
			}
			else
			{
				new_serial_params.BaudRate = CBR_115200;			//BaudRate: 115200
				new_serial_params.ByteSize = 8;						//8 bit
				new_serial_params.StopBits = ONESTOPBIT;			//1 stop bit
				new_serial_params.Parity = NOPARITY;				//no parity
				if (SetCommState(serial_handle,&new_serial_params) == 0)
				{
					printf(  "SpcThreadRun: Error setting device parameters!" );
				}
				else
				{
					PurgeComm(serial_handle,PURGE_TXCLEAR|PURGE_RXCLEAR);	//PURGE_TXCLEAR = clear output, PURGE_RXCLEAR = clear input.
			
					SetupComm(serial_handle,1024,1024);     //輸入緩衝區和輸出緩衝區的大小都是1024

					COMMTIMEOUTS TimeOuts;
					//設定讀超時
					TimeOuts.ReadIntervalTimeout=MAXDWORD;
					TimeOuts.ReadTotalTimeoutMultiplier=0;
					TimeOuts.ReadTotalTimeoutConstant=0;
					//設定寫超時
					TimeOuts.WriteTotalTimeoutMultiplier=500;
					TimeOuts.WriteTotalTimeoutConstant=2000;
					if (SetCommTimeouts(serial_handle, &TimeOuts) == 0) //設置超時
					{
						printf(  "SpcThreadRun: Error setting timeouts!" );
					}    
					else
					{
						/* loop while waiting for input.
						* normally we would do something useful here.
						*/
						ACT = CKD_STATE;
						KEY = ( byte* ) malloc ( 18 * sizeof ( byte ) );

						for ( int i = 0; i < 18; i++ )	* ( KEY + i ) = 0;
						while ( STOP == FALSE )
						{
							if ( 0 != ( GetCommState(serial_handle, &temp_serial_params )))
							{
								//printf("InputData\n");
								InputData();
								//printf("ProcessData\n");
								ProcessData();
								//printf("OutputData\n");
								OutputData();
							}
							else
							{
								isKeyError = true;
								puts ( "Key Error!" );
							}

							Sleep ( msleep_in_mainloop );

							//if(2147483647 > loop)	{loop++; }
							//else	{loop = 0;}
				
						}
					}
				}
			}
		}
	
		/* restore old port settings */
		SetCommState(serial_handle,&old_serial_params);
		CloseHandle(serial_handle);

		//printf(  "SpcThreadRun: END\n" );

	}
	//system("pause");
	return;
}

/* Class is HERE
 * LibSPC
 * */
LibSPC::LibSPC()
{
	printf("SPC: LibSPC\n");
		
	Thread^ oThread = gcnew Thread( gcnew ThreadStart( &SpcThreadRun ) );
	oThread->Start();
	printf(  "Thread created successfully\n" );
}

void LibSPC::Close()
{
	printf("SPC: Close\n");
    STOP = TRUE;
	isThreadRun = false;
}

bool LibSPC::IsKeyError()
{
	printf("SPC: IsKeyError\n");
    return isKeyError;
}

int LibSPC::test(int a,int b, char port_name[], bool run, bool key_error)
{
	int sum = a+b;
	printf("SPC: test\n");
	printf("sum999 = %d\n", sum);

	printf("cSPC: port_name ~%%~==== %s\n",port_name);
	if (true==run)
	{
		printf("cSPC: run ~%%~==== %s\n","TRUE");
	}
	else
	{
		printf("cSPC: run ~%%~==== %s\n","FALSE");
	}

	strcpy(com_num, port_name);

	isThreadRun = run;

	isKeyError = key_error;

	return sum;
}


