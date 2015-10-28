//Search Header file
using namespace std;

typedef struct ResultStructure	
{
	int resultNumber;
	unsigned char *fileID;
	unsigned char *SHA1;
}ResultStruct;

extern vector<ResultStruct> Result_vector;

extern bool searchFlag;
extern int serachResultCount;
extern pthread_mutex_t searchmutex;

void callSearch(string readBuffer);
void HandleSearch_REQ(int socDesc, int myCTBPos, struct Header ARH1, char* readBuffer);
void HandleSearch_RSP(int socDesc, int myCTBPos, struct Header ARH1, char *readBuffer);

