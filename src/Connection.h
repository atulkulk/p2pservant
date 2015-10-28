#include "CommonHeaderFile.h"

using namespace std;

//Declared in Connection.cpp

typedef struct CommonData
{
	uint8_t msg_type;
	char *UOID;
	uint8_t TTL;
	uint32_t nw_datalen;
	uint16_t ReceivingPort;
	char *Request;
	uint16_t DestPort;
	struct CommonData *next;
};
extern struct CommonData *CData_Head;

typedef struct UOIDStructure
{
	char *UOID;
	uint16_t recvPort;
	int msgLifeTime;
}UStruct;

extern vector<UStruct> UOIDVector;

extern pthread_mutex_t vectormutex;

void CommonData_put(uint8_t msg_type, char *UOID, uint8_t TTL, uint32_t nw_datalen, uint16_t ReceivingPort, char *Request, uint16_t DestPort  );
struct CommonData* CommonData_Get();
void WriteQueue_put(struct CommonData *ptr_data, int i);
struct CommonData* WriteQueue_Get(int pos);

extern pthread_mutex_t commonQmutex;

int SocketConnect(u_long address, u_short port);
int SocketAccept(int port);

void *WriteThreadFunc(void *acceptReturn);
void *ReadThreadFunc(void *acceptReturn);
void sig_shutdownHandler(int nsig);
void CleanupAndExit();
void printMessage(string prntStr);

