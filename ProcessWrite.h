using namespace std;

//Declared in ProcessWrite.cpp
extern char *StatusGlobalUOID;
extern pthread_mutex_t statusFilemutex;
extern string statusExtFile;


typedef struct TimerSt
{
	int Tm_CheckJoinTimeout;
	int Tm_KeepAliveTimeout;
	int Tm_AutoShutdown;
};

extern struct TimerSt Timer;
extern pthread_mutex_t timermutex;

extern pthread_mutex_t tpSyncMutex;
extern bool StatusFlag;

void Process_REQfwd(int socDesc,struct CommonData *ptr_getQ,int myCTBPos);
void *CommandHandle(void *null);
void *timerCount(void *null);
void *timerAction(void *null);
void InitRandom();
void FileWrite(string filename, char* buffer, int destination);
void pipeHandler(int nsig);
void DataFileWrite(string filename, char* buffer, int size, int type);

