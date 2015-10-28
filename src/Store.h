//Store.h Header File
using namespace std;

extern pthread_mutex_t storemutex;
extern int globalFileCount;

typedef struct metaStructure		//Link structure for nam
{
	char *FileName;
	long FileSize;
	unsigned char *SHA1;
	unsigned char *Nonce;
	char* Keywords;
	unsigned char* BitVector;
}MetaStruct;

typedef struct fileStructure		
{
	int fileCount;
	unsigned char* value;
}ComStruct;

typedef struct LRUStructure		
{
	int localFileCount;
	uint32_t FileSize;
}LRUStruct;

extern vector<LRUStruct> LRU_vector;
extern vector<ComStruct> FileID_vector;
extern vector<ComStruct> OneTmPwd_vector;
extern vector<ComStruct> kwrd_ind_vector;
extern vector<ComStruct> name_ind_vector;
extern vector<ComStruct> sha1_ind_vector;
extern vector<ComStruct> nonce_vector;

extern uint32_t LRUcacheSize;

void convert_lower(char *name);
MetaStruct ParseMetaData(char* metaName);
int addFileToLRU(MetaStruct rcdMeta);
void copyDataFromMetafile(int localFileCount, MetaStruct rcdMeta);

int loadAndStoreFiles(string filename);
unsigned char *loadBitVector(int fileCount, vector<string> KeyWVector);

void deleteVectorFilesEntry(int localFileCount);
void sendStoreMessage(char *initBuffer, char *Request, int socDesc);
void HandleStore_REQ(int socDesc,int myCTBPos,struct Header ARH1);
void discardData(int socDesc,long readDataLen, int myCTBPos);
void errorWhileReading(int myCTBPos, int socDesc);
void updateLRUOrder(int fileCount);

void callStore_Files(string readBuffer); 
unsigned char* HexadecToChar(unsigned char *data, int size);
unsigned char* CharToHexadec(unsigned char *data, int size);
vector<string> pushKeywords(char *trimmedstr);

char *GetSHA (char *input, char *shabuf, int shabuf_sz);
char *GetMD5 (char *input, char *md5buf, int md5buf_sz);

