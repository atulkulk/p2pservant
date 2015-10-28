using namespace std;

// Declared in ParseCmd.cpp
void Usage(int SwitchUse);
void ParseCommandLine(int argc, char *argv[]);

extern bool ResetFlag;
extern char *iniName;


// Declared in ParseFile.cpp

void TrimSpaces( string& str);
char* TrimDoubleQuotes(char* str);
void ParseINIFile(char *IniName);

typedef struct StInit
{
	int Port;
    double Location;
	char *HomeDir;
	char *LogFilename;
    int AutoShutdown;
    int TTL;
    int MsgLifetime;
    int GetMsgLifetime;
    int InitNeighbors;
    int JoinTimeout;
    int KeepAliveTimeout;
    int MinNeighbors;
	int NoCheck;
    double CacheProb;
    double StoreProb;
    double NeighborStoreProb;
    int CacheSize;
	int Retry;
};
extern struct StInit init;

typedef struct StBeacon
{
	char *HostName;
	int PortNo;
};
extern struct StBeacon beacon[50];

extern int BeaconCount;
