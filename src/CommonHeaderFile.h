#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <thread.h>
#include <signal.h>
#include <fstream>
#include <math.h>
#include <cstdlib>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

#define JNRQ 0xFC
#define JNRS 0xFB
#define HLLO 0xFA
#define KPAV 0xF8
#define NTFY 0xF7
#define CKRQ 0xF6
#define CKRS 0xF5
#define SHRQ 0xEC
#define SHRS 0xEB
#define GTRQ 0xDC
#define GTRS 0xDB
#define STOR 0xCC
#define DELT 0xBC
#define STRQ 0xAC
#define STRS 0xAB

#define ERROR -1
#define SOCKETERR -1
#define SUCCESS 1
#define COMMON_HEADER_SIZE 27
#define BACKLOG 5					//# of pending connections queue will hold
#define NUM_THREADS 250
#define RWSUCCESS 1
#define LOGDATALEN 75
#define COMMON_HEADER_SIZE 27
#define BUFFERSIZE 8190

#ifndef min
#define min(A,B) (((A)>(B)) ? (B) : (A))
#endif

#define EVEN_ONE 0x01
#define EVEN_TWO 0x02
#define EVEN_THR 0x04
#define EVEN_FOR 0x08

#define ODD_ONE 0x10
#define ODD_TWO 0x20
#define ODD_THR 0x40
#define ODD_FOR 0x80
