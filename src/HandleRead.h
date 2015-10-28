using namespace std;

typedef struct linkStructure
{
	uint16_t source;
	uint16_t destination;
}LStruct;

extern vector<LStruct> LinkVector;
extern vector<uint16_t> NodeVector;
extern pthread_mutex_t linkvectormutex;

//Declared in HandleRead.cpp
bool pushUOIDStruture(int myCTBPos,struct Header ARH1);
void SendCheckMessage();
void HandleJoin_REQ(int socDesc,int myCTBPos,struct Header ARH1, char *readBuffer);
void HandleJoin_RSP(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer);
void HandleStatus_REQ(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer);
void HandleStatus_RSP(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer);
void HandleNotify_REQ(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer);
void HandleCheck_REQ(int socDesc,int myCTBPos,struct Header ARH1);
void HandleCheck_RSP(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer);
void HandleKalive_REQ(int socDesc,int myCTBPos,struct Header ARH1);
