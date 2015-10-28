/*************************************************************************************************************/
/*												Project : CSCI551 Final Part 1 Project 						*/
/*												Filename: DoHello.cpp (Server Implementation)				*/
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/
#include "CommonHeaderFile.h"
#include "ParseFile.h"
#include "MainPrg.h"
#include "Connection.h"
#include "ProcessWrite.h"
#include "HandleRead.h"
#include "Store.h"
#include <openssl/sha.h> 

using namespace std;

string tmpkwidxfiles, tmpnamefiles, tmpsha1files, tmpfileid, tmponepass, tmpnonce, tmpLRU;

// Structure used to store the addresses for outgoing connections
typedef struct ConPort
{
	u_long ConnectAddress;
	u_short ConnectPort;
};
//struct ConPort ConP[50];

struct ConPort* ConP;

// Common header used for passing between functions
typedef struct Header
{
	uint8_t nw_msg_type;
	char *UOID;
	uint8_t nw_TTL;
	uint8_t nw_zero;
	uint32_t nw_datalen;
}Head;

// Main structure used to store the currently active connections
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


// Common variables and threads declaration
int ConnectCount;
char *node_inst_id, *hName;
string node_id;

struct connectionTieBreaker* ctb;
int connTieBreakerCount = 0;
pthread_mutex_t tiebreakermutex;

pthread_t mainAcceptThread;

pthread_t connectThreads[NUM_THREADS];
pthread_mutex_t connectmutex;

pthread_t connReadThread[NUM_THREADS];
pthread_t connWriteThread[NUM_THREADS];

pthread_t joinBeaconThread;

pthread_t HandleDispatchThread;
pthread_t commandThread;

pthread_mutex_t logmutex;

pthread_t timerCountThread;
pthread_t timerActionThread;

pthread_mutex_t shutdownmutex;
pthread_cond_t shutdownCondWait;
bool shutdownFlag=false;
bool selfRestartFlag=false;

int connectThreadCount = 0;

bool joinQuit=false;

fstream fLog;

// Desciption : Function is used to convert the hex string to character representation in log file
// Start of convertHexToChar()
unsigned char* convertHexToChar(unsigned char *last4UOID) 
{
	unsigned char *tMsgid = (unsigned char *)malloc(9);
	memset(tMsgid, '\0', 9);
	
	int j=0;
	for(int i = 0; i < 4; i++) {
		char hexChars[3];
		memset(hexChars, '\0', 3);	
		
		unsigned int lastUOIDChar = last4UOID[i];
		snprintf(hexChars, 3, "%02x", lastUOIDChar);

		tMsgid[j] = hexChars[0];
		j++;
		tMsgid[j] = hexChars[1];
		j++;
	}
	return tMsgid;
}
// End of function

// Desciption : Function is used to perform the logging operations in all of the other functions
// Start of reportLog()
// if headerflag == true , pass header
// if headerflag == false , pass uoid in structure
void reportLog(string type, string msg_type, int size, int ttl, char *msg_id, char *data, int port, bool headerFlag) 
{
	pthread_mutex_lock(&logmutex);
	struct timeval SimTemp;

	gettimeofday(&SimTemp,NULL);

	//long int tmpmillisecs = ( (( SimTemp.tv_sec * 1000000 )+ SimTemp.tv_usec ) - ((SimStart_Sec*1000000)+SimStart_usec) );

    //long int seconds = tmpmillisecs / 1000000;							// get current time.
	long int seconds=SimTemp.tv_sec;
	//int microsecs= tmpmillisecs % 1000000 ;
	//int millisecs = microsecs / 1000; 
	int millisecs = SimTemp.tv_usec / 1000; 
	

	char time[50];
	snprintf(time, sizeof(time), "%010ld.%.3d", seconds, millisecs);
	
	char *ssize = (char*)malloc(10);
	memset(ssize,'\0',10);
	snprintf(ssize, 10, "%d", size);

	char *ttlstr =  (char*)malloc(sizeof(int));
	snprintf(ttlstr, sizeof(ttlstr), "%d", ttl);

	char logNodeid[50];
	snprintf(logNodeid, sizeof(logNodeid), "%s_%d", hName, port);

	unsigned char *last4UOID = (unsigned char*)malloc(4);					//get last 4 digits of UOID
	memset(last4UOID, '\0', 4);
	if (headerFlag == true)
	{
		memcpy(last4UOID, &msg_id[17], 4);
	}
	else
	{
		memcpy(last4UOID, &msg_id[16], 4);
	}

	unsigned char *tMsgid = convertHexToChar(last4UOID);

	string logEntry = type;
	logEntry = logEntry + " " + time + " " + logNodeid + " " + msg_type + " " + ssize + " " + ttlstr + " " + (char *)tMsgid + " " + data;

	fLog.open(tmplogfiles.c_str(), fstream::in | fstream::out | fstream::app);
		fLog << logEntry << endl;
		fLog.flush();
	fLog.close();
	pthread_mutex_unlock(&logmutex);	
}


// Description : Function used to get the UOID. 
// Reference: Project Specification
// Start of GetUOID()
char *GetUOID (char *node_inst_id, char *obj_type, char *uoid_buf, int uoid_buf_sz)
{
	static unsigned long seq_no=(unsigned long)1;
	char sha1_buf[SHA_DIGEST_LENGTH+1], str_buf[104];

	snprintf(str_buf, sizeof(str_buf), "%s_%s_%1ld", node_inst_id, obj_type, (long)seq_no++);
	SHA1((unsigned char *)str_buf, strlen(str_buf), (unsigned char *)sha1_buf);
	
	memset(uoid_buf, '\0', uoid_buf_sz+1);
	memcpy(uoid_buf, sha1_buf, min((unsigned int)uoid_buf_sz,sizeof(sha1_buf)));

	return uoid_buf;
}


// Description : Function used to return the header . Used in all other messages. Calls GETUOID()
// Strart of GetHeader()
char* GetHeader(char *node_inst_id, uint8_t Msg_type, uint8_t TTL, uint32_t DataLen)
{
	unsigned char UOID[SHA_DIGEST_LENGTH];
	char obj_type[] = "msg";

    // ############  implement node instance id and then pass to this function#######################
	GetUOID(node_inst_id, obj_type, (char*)&UOID[0], sizeof(UOID));

	uint8_t nw_msg_type=htons(Msg_type);				// convert the header to network byte order
	uint8_t nw_TTL=htons(TTL);
	uint8_t nw_zero=htons(0);
	uint32_t nw_datalen=htonl(DataLen);

	int buf_sz = 27;

	char *msg_header=(char*)malloc(buf_sz);

	if (msg_header == NULL) 
	{ fprintf(stderr, "malloc() failed\n");  }

	memset(msg_header, 0, sizeof(msg_header));						// copy to buffer
	memcpy(msg_header, &(nw_msg_type), 1);
	memcpy(&msg_header[1], UOID, SHA_DIGEST_LENGTH);
	memcpy(&msg_header[21], &(nw_TTL), 1);
	memcpy(&msg_header[22], &(nw_zero), 1);
	memcpy(&msg_header[23], &(nw_datalen), 4);
	
  return msg_header;
}

// Reference: Used this function earlier in class CSCI 499 - Distributed Applications
// Resolve hostname into a usable address 
u_long ResolveAddress(const char *remoteHost) 
{
	// Assume ip address (x.x.x.x) was passed
	u_long address = inet_addr(remoteHost);
	// remoteHost was not a valid ip address, try DNS resolution
	if ((signed)address == ERROR) 
	{
		hostent* hostEntry = gethostbyname(remoteHost);
		if (hostEntry == 0) 
		{
			//GetLastErrorMessage("Error");
//			printf("Error : Invalid server address\n");
			printMessage("Error : Invalid server address");
			exit(1);
		}
		address = *((u_long*)hostEntry->h_addr);
	}
  return address;
}


// Description : Function used to call SocketAccept() which accepts all connections requests for the node
// Start of mainAcceptThreadFunc()

void *mainAcceptThrFunc(void *iPort) 
{
	
	int port = (int)iPort;
	int returnCode = SocketAccept(port);
	//cout << "returnCode: " << returnCode  << endl;
	if (returnCode==SOCKETERR)
	{
	  pthread_exit(NULL);
	}

	pthread_exit(NULL);
}


// Description : Function used to send response to the join request
// Start of joinResponse()
void joinResponse(int acceptReturn, char *sUOID, uint32_t s_location, int port)
{
	int writeBufferSize = 0;
	//SEND JOIN MESSAGE
	int dataLength = 26 + strlen(hName);

	uint16_t wPort = (uint16_t)init.Port;
	uint32_t distance=(uint32_t)abs((int)s_location - (int)init.Location);

	char *joinHeader = (char*)malloc(COMMON_HEADER_SIZE);
	joinHeader = GetHeader(node_inst_id, JNRS, (uint8_t)init.TTL, dataLength);			// get join header

	writeBufferSize = COMMON_HEADER_SIZE + dataLength;

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &sUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s %d %d %s", (char *)tMsgid, distance, init.Port, hName);

	reportLog("s", "JNRS", writeBufferSize, init.TTL, joinHeader, logData, port,true);		//write log file

	char *writeBuffer=(char*)malloc(writeBufferSize);
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &joinHeader[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &sUOID[0], 20);
	memcpy(&writeBuffer[47], &distance, 4);
	memcpy(&writeBuffer[51], &wPort, 2);
	memcpy(&writeBuffer[53], &hName[0], strlen(hName));
	
	if(write(acceptReturn, writeBuffer, writeBufferSize) == SOCKETERR) {			//write data
//		cout << "Write error... " << endl;
		printMessage("Write error... ");
	}

    free(joinHeader);
	free(writeBuffer);
}

// Description : Function used to sned response to HELLO request message
// Start of writeHello()
void writeHello(int cSocket, int port)
{
	int writeBufferSize = 0;
	//SEND HELLO MESSAGE
	int dataLength = 2 + strlen(hName);

	char *helloHeader = (char*)malloc(COMMON_HEADER_SIZE);
	helloHeader = GetHeader(node_inst_id, HLLO, 1, dataLength);						// Get Header

	writeBufferSize = COMMON_HEADER_SIZE + dataLength;

	char *logData = (char*)malloc((int)LOGDATALEN);	
	snprintf(logData, (int)LOGDATALEN, "%d %s", init.Port, hName);

	reportLog("s", "HLLO", writeBufferSize, 1, helloHeader, logData, port,true);		// write report

	char *writeBuffer=(char*)malloc(writeBufferSize);
	
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	uint16_t wPort = (uint16_t)init.Port;

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &helloHeader[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &wPort, 2);
	memcpy(&writeBuffer[29], &hName[0], strlen(hName));
	
	if(write(cSocket, writeBuffer, writeBufferSize) == SOCKETERR) {
//		cout << "Write error... " << endl;
		printMessage("Write error... ");
	}

    free(helloHeader);
	free(writeBuffer);
}


// Description : Function used to receive header from the other node...used in all read threads
// Start of ReceiveHeader()

struct Header ReceiveHeader(int cSocket, bool chkCTBflag, int ctbPos)
{
	unsigned char header[COMMON_HEADER_SIZE];
	
	Head CRH1;

	//RECEIVE HELLO MESSAGE
	if(read(cSocket, header, COMMON_HEADER_SIZE) != COMMON_HEADER_SIZE) {		//Reads the header. If read fails it catches the broken pipe error.
		//cout << "Read Error 1!!!" << endl;
		// ###################################################
		//Add function for auto shutdown, exiting threds, sending check messages etc...
		// #################################################
		
		if (joinQuit==false)
		{
          //check for ctb position
		  if(chkCTBflag == true)                                         // This is false in case of initial hello messages when ctb entry is not done
		  {
		   ctb[ctbPos].nodeActiveFlag=false;
		   		if (MeBeacon == false)
			    {					
					if (shutdownFlag = false)
					{
					  //Send check message
					  SendCheckMessage();
					}					
				}
		  }
		  close(cSocket);   
		  pthread_exit(0);		
		}
		close(cSocket);
		return CRH1;		
	}

	memcpy(&CRH1.nw_msg_type, &header[0], 1);					//Reading Hello Message

	CRH1.UOID = (char*)malloc(21);
	memset(CRH1.UOID, '\0', 21);
	memcpy(CRH1.UOID, &header[1], 20);

	memcpy(&CRH1.nw_TTL, &header[21], 1);
	memcpy(&CRH1.nw_zero, &header[22], 1);
	memcpy(&CRH1.nw_datalen, &header[23], 4);

	//Converting from network byte order to host byte order.
	CRH1.nw_msg_type = ntohs(CRH1.nw_msg_type);
	CRH1.nw_TTL = ntohs(CRH1.nw_TTL);
	CRH1.nw_zero = ntohs(CRH1.nw_zero);
	CRH1.nw_datalen = ntohl(CRH1.nw_datalen);

	return CRH1;
}


// Description : Function creates outgoing connections and exhanges HELLO messages
// Start of connectThreadFunc()

//Normal Connection to the nodes
void *connectThreadFunc(void *i) 
{
	int arrayPos = (int)i;
	int readBufferSize = 0, returnCode;
	Head CRH1;


    // First check if the entry is already present...
	while(true) {
		pthread_mutex_lock(&tiebreakermutex);
		//Notes: Check whether connection already exists
		for(int i = 0; i < connTieBreakerCount; i++) {
			if(ConP[arrayPos].ConnectPort == ctb[i].ctbPort && ctb[i].nodeActiveFlag == true ) {
				pthread_mutex_unlock(&tiebreakermutex);
				pthread_exit(NULL);
			}
		}
		pthread_mutex_unlock(&tiebreakermutex);

		returnCode = SocketConnect(ConP[arrayPos].ConnectAddress, ConP[arrayPos].ConnectPort);
		if(returnCode == SOCKETERR || returnCode == -2) {
			if(MeBeacon) {
				sleep(init.Retry);			//SLEEP FOR MESSAGE TIMEOUT for Beacon nodes. else exit thread
			}
			else {
				pthread_exit(NULL);
			}
		}
		else 
		{
			pthread_mutex_lock(&connectmutex);
  		    connectThreadCount++;
			pthread_mutex_unlock(&connectmutex);
			break;
		}
	}


	writeHello(returnCode, ConP[arrayPos].ConnectPort);			//Sending HELLO Message
	
	CRH1 = ReceiveHeader(returnCode,false,1);					//Receive HELLO Message header
		
	readBufferSize = CRH1.nw_datalen;
	char *readBuffer = (char*)malloc(readBufferSize);
	//Notes: Reads the data. If read fails it catches the broken pipe error.
	if(read(returnCode, readBuffer, readBufferSize) == SOCKETERR) {
//		cout << "Read Error 2!!!" << endl;
		printMessage("Read Error 2!!!");
	}

	uint16_t rPort;
	char *rHostname;

	memcpy(&rPort, &readBuffer[0], 2);
	rHostname = (char*)malloc(CRH1.nw_datalen-2);
	memset(rHostname, '\0', CRH1.nw_datalen-2);
	memcpy(rHostname, &readBuffer[2], CRH1.nw_datalen-2);

	int dataSize = COMMON_HEADER_SIZE + CRH1.nw_datalen;

	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%d %s",rPort, rHostname);			//Perform logging

	reportLog("r", "HLLO", dataSize, CRH1.nw_TTL, CRH1.UOID, logData, rPort, false);


	// Entry into the tiebreaket structure for active connections...
	pthread_mutex_lock(&tiebreakermutex);
		
	bool tempFlag=false;

	//CONNECTION TIE-BREAKER
	for(int i = 0; i < connTieBreakerCount; i++) {
		if(rPort == ctb[i].ctbPort && ctb[i].nodeActiveFlag == true ) {
			if((uint16_t)init.Port < ctb[i].ctbPort){
				shutdown(returnCode, 2);
				close(returnCode);
				pthread_mutex_unlock(&tiebreakermutex);
				pthread_exit(NULL);
			}
			else {
				shutdown(ctb[i].ctbSocDesc, 2);
				close(ctb[i].ctbSocDesc);
				ctb[i].ctbSocDesc = returnCode;
				ctb[i].ctbJoinFlag = false;
				ctb[i].nodeActiveFlag = true;
				ctb[i].ctbKeepAliveTimeout = init.KeepAliveTimeout;
				ctb[i].writeHead=NULL;

				tempFlag=true;
				break;
			}
		}
	}

	// Entry is not already present . Make a new entry
	if (tempFlag == false)
	{
		ctb[connTieBreakerCount].ctbPort =  ConP[arrayPos].ConnectPort;
		ctb[connTieBreakerCount].ctbSocDesc = returnCode;
		ctb[connTieBreakerCount].ctbJoinFlag = false;
		ctb[connTieBreakerCount].nodeActiveFlag = true;
		ctb[connTieBreakerCount].ctbKeepAliveTimeout = init.KeepAliveTimeout;
		ctb[connTieBreakerCount].writeHead=NULL;
		connTieBreakerCount++;
	}

	pthread_mutex_unlock(&tiebreakermutex);

	pthread_mutex_lock(&connectmutex);
	
	//Creates a Read Thread for each request
	if (pthread_create(&connReadThread[connectThreadCount], NULL, ReadThreadFunc, (void *)returnCode) == SOCKETERR) {
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed!");
	}

	
	//Creates a Write Thread for each request
	if (pthread_create(&connWriteThread[connectThreadCount], NULL, WriteThreadFunc, (void *)returnCode) == SOCKETERR) {
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed!");
	}

	pthread_mutex_unlock(&connectmutex);

	pthread_exit(NULL);
}


// Description : Function copies data from main queue to all write threads data queue
// Start of DispatchHandle()

void *DispatchHandle(void *null)
{
	struct CommonData *get_data=(struct CommonData*)malloc(sizeof(struct CommonData));


	while (true)
	{
		get_data=CommonData_Get();				// get data from write common queue
        
		int tempCnt;
		
		pthread_mutex_lock(&tiebreakermutex);
        tempCnt=connTieBreakerCount;
		pthread_mutex_unlock(&tiebreakermutex);			// get the count of active connections

		for (int i=0; i< tempCnt ; i++)
		{			
			//Notes: Write in the queue of the specific port.
			if (get_data->ReceivingPort != ctb[i].ctbPort && get_data->DestPort == ctb[i].ctbPort && ctb[i].nodeActiveFlag == true) 
			{
				if(ctb[i].ctbJoinFlag == true )
				{
					if(get_data->msg_type == JNRS )
					{
						WriteQueue_put(get_data, i);
					}
				}
				else
				{ 
					WriteQueue_put(get_data, i);
				}
			}
			//Notes: If the DestPort is 65000 write in all queues except the receiving port
			else if(get_data->ReceivingPort != ctb[i].ctbPort && get_data->DestPort == 65000 && ctb[i].nodeActiveFlag == true) 
			{
				if(ctb[i].ctbJoinFlag == true )
				{
					if(get_data->msg_type == JNRS)
					{
						WriteQueue_put(get_data, i);
					}
				}
				else
				{ 
					WriteQueue_put(get_data, i);
				}
			}
		}
		
	}
}


// Description : Function called from all 3 types of starts...creates other treads and checks for self restart
// Start of establishNodeForService()

void establishNodeForService(int iPort) 
{
	//Creates a Main Accept Thread
	if (pthread_create(&mainAcceptThread, NULL, mainAcceptThrFunc, (void *)iPort) == SOCKETERR) {
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed!");
	}

	
	//Creates Connect Threads
	for(int i =0; i < ConnectCount; i++) {	
		if (pthread_create(&connectThreads[i], NULL, connectThreadFunc, (void *)i) == SOCKETERR) {
//			cout << "ERROR: pthread_create() failed." << endl;
			printMessage("ERROR: pthread_create() failed!");
		}
		
	}

	if (pthread_create(&HandleDispatchThread, NULL, DispatchHandle, (void *)NULL) == SOCKETERR) {
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed!");
	}

	// check for number of established connections if not a beacon node    
	if (MeBeacon == false)
	{
		for(int i =0; i < ConnectCount; i++) 
		{	
			pthread_join(connectThreads[i], NULL);				
		}

        if ( connectThreadCount < init.MinNeighbors)
        {
			printMessage("Failed to join with MinNeighbors specified in init_neighbour_list file...");
			
			if (init.NoCheck == 0)
			{
			printMessage("Performing Self Restart...");

			// Deleting the init_neighbor_list file

			string iniFilename(init.HomeDir);
   	        iniFilename=iniFilename+"/init_neighbor_list";
			remove(iniFilename.c_str());

//          Code for cleanup and loop in again inside the main infinite while  

			 selfRestartFlag=true;            
			 shutdownFlag=true;
			 CleanupAndExit();
		 	 delete []ConP;
			}
			else
			{
			  selfRestartFlag=false;            
			  shutdownFlag=true;
			  CleanupAndExit();
			  delete []ConP;			
			}

        }
	}
	
	// make the main thraed wait on condition for shutdown....
	if (selfRestartFlag==false)
	{
	 pthread_mutex_lock(&shutdownmutex);

	  if(shutdownFlag == false) 
	 {
		 pthread_cond_wait(&shutdownCondWait, &shutdownmutex);	
	 }

	 pthread_mutex_unlock(&shutdownmutex);

	 CleanupAndExit();
	 delete []ConP;

	}
     
	selfRestartFlag=false;
}


// Description : Function creates other thraeds after reading init_neigbor_list file
// Strart of helloInit()

void HelloInit(string filename)
{
	ifstream myfile (&filename[0]);

	if (pthread_create(&commandThread, NULL, CommandHandle, (void *)NULL) == SOCKETERR) 
	{
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed!");
	}

	
	//Creates a TimerCount Thread
	if (pthread_create(&timerCountThread, NULL, timerCount, (void *)NULL) == SOCKETERR) 
	{
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed!");
	}
	
	

	//Creates a TimerAction Thread
	if (pthread_create(&timerActionThread, NULL, timerAction, (void *)NULL) == SOCKETERR) 
	{
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed!");
	}

	
	ConnectCount=0;

	string line;
	if (myfile.is_open())                            // read the file and send to user 
	{
		while (! myfile.eof() )
		{
			getline (myfile,line);
			int port_pos = line.find (":");
			if (port_pos != ERROR)
			{
				string str_hostname = line.substr(0, port_pos);
				string str_port = line.substr (port_pos+1); 
				ConP[ConnectCount].ConnectAddress = ResolveAddress(str_hostname.c_str());
				ConP[ConnectCount].ConnectPort = atoi(str_port.c_str());                       // store all the ports to connect in ConnectPort array
				ConnectCount = ConnectCount + 1;
			}
		}
	    myfile.close();
	}
	else
	{
		printMessage("Error opening init_neighbor_list file....");
		exit(1);	
	}
	establishNodeForService(init.Port);
}


// Description : Function handles SIGUSR1 interrupt 
// Strart of joinHandler()

static void sig_JoinHandler(int nsig)
{
	joinQuit=true;
}


// Description : Function connects with beacn node and receives responses...
// Strart of joinBeacon()

void *joinBeacon(void *null) 
{
	int jSocket, bPort;
	bool beaconAvl = false;
	typedef struct joinNbr
	{
		uint32_t distance;
		uint16_t port;
	};
	struct joinNbr joinNbrs[100];
	int joinedCount=0;

	signal(SIGUSR1,sig_JoinHandler);

	for (int i=0;i<BeaconCount ;i++ )                                  // get port numbers of all other beacon nodes
	{
		u_long address = ResolveAddress(beacon[i].HostName);
		jSocket = SocketConnect(address, beacon[i].PortNo);
		if(jSocket != SOCKETERR) 
		{
			bPort = beacon[i].PortNo;
			beaconAvl = true;
			break;
		}		   
	}

	// check if any beacon node is available or not

	if (beaconAvl == false)
	{
	   printMessage("Failed to connect to a beacon node... Exiting....");
       printf("\n");
	   exit(1);
	}

	int writeBufferSize = 0;

	//SEND JOIN MESSAGE
	int dataLength = 6 + strlen(hName);		// 6 = Location + Port

	char *joinHeader = (char*)malloc(COMMON_HEADER_SIZE);
	joinHeader = GetHeader(node_inst_id, JNRQ, (uint8_t)init.TTL, dataLength); // get the header from function

	writeBufferSize = COMMON_HEADER_SIZE + dataLength;

	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%d %s", init.Port, hName);

	reportLog("s", "JNRQ", writeBufferSize, init.TTL, joinHeader, logData, bPort,true);		// perform logging

	char *writeBuffer=(char*)malloc(writeBufferSize);
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	uint16_t wPort = (uint16_t)init.Port;
	uint32_t Slocation=(uint32_t)init.Location;

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &joinHeader[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &Slocation, 4);
	memcpy(&writeBuffer[31], &wPort, 2);
	memcpy(&writeBuffer[33], &hName[0], strlen(hName));
	
	if(write(jSocket, writeBuffer, writeBufferSize) == SOCKETERR)   // send data to beacon node
	{
//		cout << "Write error... " << endl;
		printMessage("Write error... ");
	}

    free(joinHeader);
	free(writeBuffer);

	// RECEIVE JOIN RESPONSES
	Head RcdHeader;
	int readBufferSize;

	while (true)				// waits for SIGUSR1 interrupts 
	{
		if (joinQuit == true)
		{
			close(jSocket);
			break;
		}
		
		RcdHeader= ReceiveHeader(jSocket,false,1);                 //##############remember to modify receiveheader to handle this

		if (joinQuit == true)
		{
			close(jSocket);
			break;
		}

		readBufferSize = RcdHeader.nw_datalen;
		char *readBuffer = (char*)malloc(readBufferSize);
		  
		if(read(jSocket, readBuffer, readBufferSize) == SOCKETERR) //Reads the header. If read fails it catches the broken pipe error.
		{	
//			cout << "Read Error 2!!!" << endl;
			printMessage("Read Error 2!!!");
		}

		char *rUOID=(char*)malloc(20);

		memcpy(rUOID, &readBuffer[0], 20);
		memcpy(&(joinNbrs[joinedCount].distance), &readBuffer[20] , 4);								 
		memcpy(&(joinNbrs[joinedCount].port), &readBuffer[24], 2);

		// code for logging
		unsigned char *last4UOID = (unsigned char*)malloc(4);
		memset(last4UOID, '\0', 4);
		memcpy(last4UOID, &rUOID[16], 4);

		unsigned char *tMsgid = convertHexToChar(last4UOID);

		int dataSize = COMMON_HEADER_SIZE + RcdHeader.nw_datalen;
		char *logData = (char*)malloc((int)LOGDATALEN);
		snprintf(logData, (int)LOGDATALEN, "%s %d %d %s", (char *)tMsgid, joinNbrs[joinedCount].distance, joinNbrs[joinedCount].port, hName);

		reportLog("r", "JNRS", dataSize,RcdHeader.nw_TTL , RcdHeader.UOID, logData, bPort,false);  // perform logging 

		joinedCount++;
	}

	joinQuit=false;														            // in case looping again in while loop

	// less neigbors available
	if (joinedCount < init.InitNeighbors)
	{
		printMessage("Not Enough Neighbors Available... Shutting down");
		exit(0);
	}

	uint32_t t_dist;
	uint16_t t_port;

	//Sort the returned neighbors in ascending order
	for (int x=0;x<joinedCount ;x++ )
	{
 	   for (int y=x+1;y<joinedCount ;y++ )
		{
		   if (joinNbrs[x].distance > joinNbrs[y].distance)
		   {
			   t_dist = joinNbrs[x].distance;
			   t_port = joinNbrs[x].port;

			   joinNbrs[x].distance = joinNbrs[y].distance;
			   joinNbrs[x].port = joinNbrs[y].port;

			   joinNbrs[y].distance = t_dist;
			   joinNbrs[y].port = t_port;
		   }	   
	    }
	}

	string tmpfiles (init.HomeDir);
    tmpfiles=tmpfiles+"/init_neighbor_list";

    // write dada into the INI file
	fstream initFile;
	initFile.open(tmpfiles.c_str(), fstream::in | fstream::out | fstream::app);

	for (int z=0; z < init.InitNeighbors; z++)
	{
		string str_hostname="nunki.usc.edu";
		ConP[z].ConnectAddress = ResolveAddress(str_hostname.c_str());
        ConP[z].ConnectPort = joinNbrs[z].port;                       // store all the ports to connect in ConnectPort array

		char *pstr = (char*)malloc(sizeof(int));
		sprintf(pstr, "%d", (int)ConP[z].ConnectPort);

		string initFileEntry;
		initFileEntry = str_hostname + ":" + pstr;
		initFile << initFileEntry << endl;

		ConnectCount = ConnectCount + 1;
	}

	initFile.close();			// close the ini file and make connections

	pthread_exit(NULL);

}


// Description : Function calls joinBeacon Thread and sleeps for join timeout
// Strart of joinInit()

void JoinInit()
{
	ConnectCount=0;

	if (pthread_create(&commandThread, NULL, CommandHandle, (void *)NULL) == SOCKETERR) 
	{
		cout << "ERROR: pthread_create() failed." << endl;
	}

	
	//Creates a TimerCount Thread
	if (pthread_create(&timerCountThread, NULL, timerCount, (void *)NULL) == SOCKETERR) 
	{
		cout << "ERROR: pthread_create() failed." << endl;
	}

	
	//Creates a TimerAction Thread
	if (pthread_create(&timerActionThread, NULL, timerAction, (void *)NULL) == SOCKETERR) 
	{
		cout << "ERROR: pthread_create() failed." << endl;
	}

	
	if (pthread_create(&joinBeaconThread, NULL, joinBeacon, (void *)NULL) == SOCKETERR) 
	{
			cout << "ERROR: pthread_create() failed." << endl;
	}

	sleep(init.JoinTimeout);
    pthread_kill(joinBeaconThread, SIGUSR1);				// send sigusr1 to thread

	pthread_join(joinBeaconThread, NULL);  

	establishNodeForService(init.Port);
	
}

// Description : Function establishes connections with other beacon nodes availble
// Strart of BeaconInit()

void BeaconInit()
{
	ConnectCount=0;

	//Creates a CommandLine Thread
	if (pthread_create(&commandThread, NULL, CommandHandle, (void *)NULL) == SOCKETERR) 
	{
		cout << "ERROR: pthread_create() failed." << endl;
	}

	//Creates a TimerCount Thread
	if (pthread_create(&timerCountThread, NULL, timerCount, (void *)NULL) == SOCKETERR) 
	{
		cout << "ERROR: pthread_create() failed." << endl;
	}
	

	//Creates a TimerAction Thread
	if (pthread_create(&timerActionThread, NULL, timerAction, (void *)NULL) == SOCKETERR)
	{
		cout << "ERROR: pthread_create() failed." << endl;
	}

	
    for (int i=0;i<BeaconCount ;i++ )                                  // get port numbers of all other beacon nodes
	{
		if(init.Port!=beacon[i].PortNo)
		{
			ConP[ConnectCount].ConnectAddress= ResolveAddress(beacon[i].HostName);
			ConP[ConnectCount].ConnectPort = beacon[i].PortNo;
			ConnectCount = ConnectCount + 1;
		}
	}
	establishNodeForService(init.Port);

}


// Description : Function gets the instance id FOR the node
// Strart of getNodeInstaceId()

char *getNodeInstanceId() 
{
	ctb=new struct connectionTieBreaker[200];
	ConP=new struct ConPort[200];

	char *pstr = (char*)malloc(6);
	snprintf(pstr, 6, "%d", init.Port);					// port og the node
	hName = (char*)malloc(15);
	memset(hName,'\0',15);
	string temphName="nunki.usc.edu";
	strncpy(hName,temphName.c_str(),strlen(temphName.c_str()));
	hName[strlen(temphName.c_str())]='\0';

	node_id = hName;
	node_id = node_id + '_';
	node_id = node_id + pstr;
	free(pstr);

	time_t seconds;				//Time in seconds. Appended to the node_id.
	seconds = time(NULL);

	char *sstr = (char*)malloc(sizeof(long double));
	snprintf(sstr, sizeof(long double), "%.0Lf", (long double)seconds);
	string tempNodeInstId (node_id);
	tempNodeInstId = tempNodeInstId + '_';
	tempNodeInstId = tempNodeInstId  + sstr;
	free(sstr);		

	int nLen = strlen(tempNodeInstId.c_str());
	node_inst_id = (char*)malloc(nLen);						//Node Instance ID generated
	memset(node_inst_id, '\0', sizeof(node_inst_id));
	memcpy(&node_inst_id[0], tempNodeInstId.c_str(), nLen);
	node_inst_id[nLen] = '\0';

	return node_inst_id;
}


// Description : Function resets the current node when reset option is specified
// Strart of resetNode()

void resetNode() 
{
	DIR *hNode, *fNode;
	struct dirent *hent, *fent;

    string oFilesDir (init.HomeDir);
	
	oFilesDir=oFilesDir+"/files";

	fNode = opendir(oFilesDir.c_str());			//Deleting files from the files directory
	while((fent=readdir(fNode))) 
	{
		if((strcmp(fent->d_name, "tmp") != 0))
		{
		string fDel = fent->d_name;
		fDel = '/' + fDel;
		fDel = oFilesDir + fDel;
		remove(fDel.c_str());		
		}
	}
	closedir(fNode);
	
	hNode = opendir(init.HomeDir);				//Deleting files from the home directory
	while((hent=readdir(hNode))) 
	{	
		if((strcmp(hent->d_name, "files") != 0) && (strcmp(hent->d_name, "init_neighbor_list") != 0))
		{
			string fDel = hent->d_name;
			fDel = '/' + fDel;
			fDel = init.HomeDir + fDel;
			remove(fDel.c_str());
		}
	}
	closedir(hNode);
}

// Desciption : Function loads all index data structures by reading the files
// Start of loadFiles()

void loadFiles()
{
	fstream filePtr;
	size_t found;
	
	//Check for kwrd_index, name_index, sha1_index, perm_file_id, one_time_pwd
	//If the files are not present they are created below

	tmpkwidxfiles = tmpnamefiles = tmpsha1files = tmpfileid = tmponepass = tmpnonce = tmpLRU = init.HomeDir;
	tmpkwidxfiles = tmpkwidxfiles + "/" + "kwrd_index";
	tmpnamefiles = tmpnamefiles + "/" + "name_index";
	tmpsha1files = tmpsha1files + "/" + "sha1_index";
	tmpfileid = tmpfileid + "/" + "perm_file_id";
	tmponepass = tmponepass + "/" + "one_time_pwd";
	tmpnonce = tmpnonce + "/" + "nonce_file";
	tmpLRU = tmpLRU + "/" + "lru_file";

	//load kwrd_index file
	struct stat stFileInfo;
	int intStat = stat(tmpkwidxfiles.c_str(), &stFileInfo);
	if(intStat != 0)
	{
		(filePtr).open(tmpkwidxfiles.c_str(), fstream::in | fstream::out | fstream::app);
		(filePtr).close();
	}
	else
	{
		string str;
		ComStruct tmpBitGetStruct;

		(filePtr).open(tmpkwidxfiles.c_str(), fstream::in | fstream::out | fstream::app);

		while(!filePtr.eof())
		{	
			getline(filePtr, str);
			found=str.find("|");		//Get SHA1
			if (found!=string::npos)
			{
				tmpBitGetStruct.fileCount = atoi((str.substr(0, found)).c_str());
				string strtmp_bit=str.substr(found+1);

				unsigned char* hexBitVector= (unsigned char*)malloc(128);
				hexBitVector=CharToHexadec((unsigned char*)strtmp_bit.c_str(), 256);
				
				tmpBitGetStruct.value = (unsigned char*)malloc(129);
				memset(tmpBitGetStruct.value, '\0', 129);
				memcpy(tmpBitGetStruct.value, hexBitVector, 128);

				pthread_mutex_lock(&storemutex);
					kwrd_ind_vector.push_back(tmpBitGetStruct);					
				pthread_mutex_unlock(&storemutex);
			}		
		}
		(filePtr).close();

	}


	//load file_index structure
	intStat = stat(tmpnamefiles.c_str(), &stFileInfo);
	if(intStat != 0)
	{
		(filePtr).open(tmpnamefiles.c_str(), fstream::in | fstream::out | fstream::app);
		(filePtr).close();
		globalFileCount=1;
	}
	else
	{
		string str;
		ComStruct tmpfileNameGetStruct;

		(filePtr).open(tmpnamefiles.c_str(), fstream::in | fstream::out | fstream::app);
		while(!filePtr.eof())
		{
			getline(filePtr, str);
			found=str.find("|");		//Get Filename
			if (found!=string::npos)
			{
				tmpfileNameGetStruct.fileCount = atoi((str.substr(0, found)).c_str());
				string strtmp_name=str.substr(found+1);
				tmpfileNameGetStruct.value = (unsigned char*)malloc(strlen(strtmp_name.c_str())+1);
				memset(tmpfileNameGetStruct.value, '\0', strlen(strtmp_name.c_str())+1);
				memcpy(tmpfileNameGetStruct.value, strtmp_name.c_str(), strlen(strtmp_name.c_str()));
				pthread_mutex_lock(&storemutex);
					name_ind_vector.push_back(tmpfileNameGetStruct);
					if (atoi((str.substr(0, found)).c_str()) > globalFileCount)
					{
						globalFileCount =  atoi((str.substr(0, found)).c_str());
					}
				pthread_mutex_unlock(&storemutex);
			}		
		}
		pthread_mutex_lock(&storemutex);
		globalFileCount++;
		pthread_mutex_unlock(&storemutex);
		(filePtr).close();

	}

	//load sha1_index structures
	intStat = stat(tmpsha1files.c_str(), &stFileInfo);
	if(intStat != 0)
	{
		(filePtr).open(tmpsha1files.c_str(), fstream::in | fstream::out | fstream::app);
		(filePtr).close();
	}
	else
	{
		string str;
		ComStruct tmpSha1GetStruct;

		(filePtr).open(tmpsha1files.c_str(), fstream::in | fstream::out | fstream::app);

		while(!filePtr.eof())
		{	
			getline(filePtr, str);
			found=str.find("|");		//Get SHA1
			if (found!=string::npos)
			{
				tmpSha1GetStruct.fileCount = atoi((str.substr(0, found)).c_str());
				string strtmp_sha1=str.substr(found+1);

				unsigned char* hexShaResult = (unsigned char*)malloc(20);
				hexShaResult=CharToHexadec((unsigned char*)strtmp_sha1.c_str(), 40);

				tmpSha1GetStruct.value = (unsigned char*)malloc(21);
				memset(tmpSha1GetStruct.value, '\0', 21);
				memcpy(tmpSha1GetStruct.value, hexShaResult, 20);

				
				pthread_mutex_lock(&storemutex);
					sha1_ind_vector.push_back(tmpSha1GetStruct);					
				pthread_mutex_unlock(&storemutex);
			}		
		}

		(filePtr).close();

	}

	//load fileid structure
	intStat = stat(tmpfileid.c_str(), &stFileInfo);
	if(intStat != 0)
	{
		(filePtr).open(tmpfileid.c_str(), fstream::in | fstream::out | fstream::app);
		(filePtr).close();
	}
	else
	{
		string str;
		ComStruct tmpfileIDGetStruct;

		(filePtr).open(tmpfileid.c_str(), fstream::in | fstream::out | fstream::app);

		while(!filePtr.eof())
		{	
			getline(filePtr, str);
			found=str.find("|");		//Get Filename
			if (found!=string::npos)
			{
				tmpfileIDGetStruct.fileCount = atoi((str.substr(0, found)).c_str());
				string strtmp_id=str.substr(found+1);

				unsigned char* hexfileID = (unsigned char*)malloc(20);
				hexfileID=CharToHexadec((unsigned char*)strtmp_id.c_str(), 40);
				
				tmpfileIDGetStruct.value = (unsigned char*)malloc(21);
				memset(tmpfileIDGetStruct.value, '\0', 21);
				memcpy(tmpfileIDGetStruct.value, hexfileID, 20);

				pthread_mutex_lock(&storemutex);
					FileID_vector.push_back(tmpfileIDGetStruct);					
				pthread_mutex_unlock(&storemutex);
			}		
		}

		(filePtr).close();

	}

	//load one_time_password strcucture

	intStat = stat(tmponepass.c_str(), &stFileInfo);
	if(intStat != 0)
	{
		(filePtr).open(tmponepass.c_str(), fstream::in | fstream::out | fstream::app);
		(filePtr).close();
	}
	else
	{
		string str;
		ComStruct tmpPassGetStruct;

		(filePtr).open(tmponepass.c_str(), fstream::in | fstream::out | fstream::app);

		while(!filePtr.eof())
		{	
			getline(filePtr, str);
			found=str.find("|");		//Get One-Time-Pasword
			if (found!=string::npos)
			{
				tmpPassGetStruct.fileCount = atoi((str.substr(0, found)).c_str());
				string strtmp_pwd=str.substr(found+1);

				unsigned char* hexPass = (unsigned char*)malloc(20);
				hexPass=CharToHexadec((unsigned char*)strtmp_pwd.c_str(), 40);
				
				tmpPassGetStruct.value = (unsigned char*)malloc(21);
				memset(tmpPassGetStruct.value, '\0', 21);
				memcpy(tmpPassGetStruct.value, hexPass, 20);
				
				pthread_mutex_lock(&storemutex);
					OneTmPwd_vector.push_back(tmpPassGetStruct);					
				pthread_mutex_unlock(&storemutex);
			}		
		}

		(filePtr).close();

	}


	//load nonce structure
	intStat = stat(tmpnonce.c_str(), &stFileInfo);
	if(intStat != 0)
	{
		(filePtr).open(tmpnonce.c_str(), fstream::in | fstream::out | fstream::app);
		(filePtr).close();
	}
	else
	{
		string str;
		ComStruct tmpNonceGetStruct;

		(filePtr).open(tmpnonce.c_str(), fstream::in | fstream::out | fstream::app);

		while(!filePtr.eof())
		{	
			getline(filePtr, str);
			found=str.find("|");		//Get Nonce
			if (found!=string::npos)
			{
				tmpNonceGetStruct.fileCount = atoi((str.substr(0, found)).c_str());
				string strtmp_non=str.substr(found+1);

				unsigned char* hexnonce = (unsigned char*)malloc(20);
				hexnonce=CharToHexadec((unsigned char*)strtmp_non.c_str(), 40);

				tmpNonceGetStruct.value = (unsigned char*)malloc(21);
				memset(tmpNonceGetStruct.value, '\0', 21);
				memcpy(tmpNonceGetStruct.value, hexnonce, 20);

				pthread_mutex_lock(&storemutex);
					nonce_vector.push_back(tmpNonceGetStruct);					
				pthread_mutex_unlock(&storemutex);
			}		
		}

		(filePtr).close();

	}

	//load LRU Structure
	intStat = stat(tmpLRU.c_str(), &stFileInfo);
	if(intStat != 0)
	{
		(filePtr).open(tmpLRU.c_str(), fstream::in | fstream::out | fstream::app);
		(filePtr).close();
	}
	else
	{
		string str;
		LRUStruct tmpLRUGetStruct;

		(filePtr).open(tmpLRU.c_str(), fstream::in | fstream::out | fstream::app);

		while(!filePtr.eof())
		{	
			getline(filePtr, str);
			found=str.find("|");		//Get Nonce
			if (found!=string::npos)
			{
				tmpLRUGetStruct.localFileCount = atoi((str.substr(0, found)).c_str());
				tmpLRUGetStruct.FileSize = (uint32_t)(atoi((str.substr(found+1)).c_str()));

				pthread_mutex_lock(&storemutex);

				LRUcacheSize = LRUcacheSize + tmpLRUGetStruct.FileSize;

					LRU_vector.push_back(tmpLRUGetStruct);					
				pthread_mutex_unlock(&storemutex);
			}		
		}

		(filePtr).close();
	}

}


