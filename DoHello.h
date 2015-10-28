//DoHello.h Header file
#include "CommonHeaderFile.h"
using namespace std;

//Declared in DoHello.cpp
extern char *node_inst_id;
extern char* hName;
extern fstream fLog, fPtr[5];
extern string tmpkwidxfiles, tmpnamefiles, tmpsha1files, tmpfileid, tmponepass, tmpLRU;

typedef struct connectionTieBreaker
{
	uint16_t ctbPort;
	int ctbSocDesc;
	bool ctbJoinFlag;
	bool nodeActiveFlag;
	int ctbKeepAliveTimeout;
	struct CommonData *writeHead;
	pthread_mutex_t writeMutex;
	pthread_cond_t writeDataCondWait;
};
//}connTB[100];

extern pthread_cond_t writeDataCondWait;

//extern connTB ctb;
extern struct connectionTieBreaker* ctb;

extern int connTieBreakerCount;
extern pthread_mutex_t tiebreakermutex;

extern pthread_mutex_t logmutex;

typedef struct Header
{
	uint8_t nw_msg_type;
	char *UOID;
	uint8_t nw_TTL;
	uint8_t nw_zero;
	uint32_t nw_datalen;
}Head;

struct Header ReceiveHeader(int cSocket, bool chkCTBflag, int ctbPos);
void writeHello(int cSocket, int port);
void joinResponse(int acceptReturn,char *sUOID, uint32_t s_location, int port);

void HelloInit(string filename);
void JoinInit();
void BeaconInit();
void resetNode();
char *getNodeInstanceId();
u_long ResolveAddress(const char *remoteHost);
//int SocketConnect(u_long address, u_short port);
char *GetUOID (char *node_inst_id, char *obj_type, char *uoid_buf, int uoid_buf_sz);
char* GetHeader(char *node_inst_id, uint8_t Msg_type, uint8_t TTL, uint32_t DataLen);
void reportLog(string type, string msg_type, int size, int ttl, char *msg_id, char *data, int port, bool headerFlag);
unsigned char* convertHexToChar(unsigned char *last4UOID);
void loadFiles();

extern pthread_mutex_t connectmutex;

extern int ConnectCount;
extern int connectThreadCount;
extern bool joinQuit;

extern pthread_mutex_t shutdownmutex;
extern pthread_cond_t shutdownCondWait;
extern bool shutdownFlag;
extern bool selfRestartFlag;


extern pthread_t timerCountThread;
extern pthread_t timerActionThread;
extern pthread_t commandThread;

