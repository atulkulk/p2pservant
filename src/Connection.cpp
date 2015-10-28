/************************************************************************************************/
/*					Project : CSCI551 Final Part 1 Servant Project		 						*/
/*					Filename: Connection.cpp													*/
/*					Credits : Abhishek Deshmukh, Atul Kulkarni									*/
/************************************************************************************************/
//New header files
#include "CommonHeaderFile.h"
#include "ParseFile.h"
#include "MainPrg.h"
#include "DoHello.h"
#include "ProcessWrite.h"
#include "HandleRead.h"
#include "Store.h"
#include "StatusFile.h"
#include "Search.h"
#include "Get.h"
#include "Delete.h"

//Declaration of variables
pthread_t accReadThread[NUM_THREADS];
pthread_t accWriteThread[NUM_THREADS];
pthread_t handleAccept[NUM_THREADS];

pthread_cond_t commonDataCondWait;

int acceptThreadCount = 0;
int myMainSocket;

typedef struct CommonData		//Common Queue Structure
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
struct CommonData *CData_Head=NULL;

typedef struct UOIDStructure	//Common UOID Structure
{
	char *UOID;
	uint16_t recvPort;
	int msgLifeTime;
}UStruct;

vector<UStruct> UOIDVector;
pthread_mutex_t vectormutex;

pthread_mutex_t commonQmutex;

//Description: Prints the message into log file
//Start of printMessage()
void printMessage(string prntStr) 
{
	fLog.open(tmplogfiles.c_str(), fstream::in | fstream::out | fstream::app);
	fLog << "**" << prntStr << endl;
	fLog.flush();
	fLog.close();
}
//End of printMessage()

//Description: Gets the data from the queue structure
//Start of CommonData_Get()
struct CommonData* CommonData_Get()      // Function to be called only when there is data in the queue
{
	pthread_mutex_lock(&commonQmutex);

	while(CData_Head == NULL) {
		pthread_cond_wait(&commonDataCondWait, &commonQmutex);
		if (shutdownFlag == true)
		{
			pthread_mutex_unlock(&commonQmutex);
			pthread_exit(0);
		}
	}

    struct CommonData *temp1,*temp2;
	temp1=CData_Head;
	temp2=temp1;

	while (temp1->next !=NULL)
	{
		temp2=temp1;
		temp1=temp1->next;
	}

	if (temp1==temp2)
	{
		CData_Head=NULL;
	    
		pthread_mutex_unlock(&commonQmutex);
		return temp1;
	}
	else
	{
		temp2->next=NULL;

		pthread_mutex_unlock(&commonQmutex);
		return temp1;	
	}
}
//End of CommonData_Get()

//Description: Gets the data from the individual node queue structure
//Start of WriteQueue_Get()
struct CommonData* WriteQueue_Get(int pos)    // Function to be called only when there is data in the queue
{
	struct CommonData *temp1,*temp2;
	temp1=ctb[pos].writeHead;
	temp2=temp1;

	while (temp1->next !=NULL)
	{
		temp2=temp1;
		temp1=temp1->next;
	}

	if (temp1==temp2)
	{
		ctb[pos].writeHead=NULL;
		return temp1;
	}
	else
	{
		temp2->next=NULL;
		return temp1;	
	}
}
//End of WriteQueue_Get()

//Description: Writes the data into the individual node's queue structure
//Start of WriteQueue_put()
void WriteQueue_put(struct CommonData *tmp_data, int i) 
{
	if (ctb[i].writeHead==NULL)
	{
		tmp_data->next=NULL;
		ctb[i].writeHead=tmp_data;
	}
	else
	{
		tmp_data->next=ctb[i].writeHead;
		ctb[i].writeHead=tmp_data;	
	}

	pthread_mutex_lock(&(ctb[i].writeMutex));

	pthread_cond_signal(&(ctb[i].writeDataCondWait));

	pthread_mutex_unlock(&(ctb[i].writeMutex));
}
//End of WriteQueue_put()

//Description: Writes the data into common queue structure
//Start of CommonData_put()
void CommonData_put(uint8_t msg_type, char *UOID, uint8_t TTL, uint32_t nw_datalen, uint16_t ReceivingPort, char *Request, uint16_t DestPort)
{
	pthread_mutex_lock(&commonQmutex);

	//Copies data into the structure
	struct CommonData *ptr_data= (struct CommonData*)malloc(sizeof(struct CommonData));
	ptr_data->msg_type=msg_type;
	ptr_data->UOID=UOID;
	ptr_data->TTL=TTL;
	ptr_data->nw_datalen=nw_datalen;
	ptr_data->ReceivingPort=ReceivingPort;
	ptr_data->Request=Request;
	ptr_data->DestPort=DestPort;

	if (CData_Head==NULL)
	{
		ptr_data->next=NULL;
		CData_Head=ptr_data;
	}
	else
	{
		ptr_data->next=CData_Head;
		CData_Head=ptr_data;	
	}

	pthread_cond_signal(&commonDataCondWait);

	pthread_mutex_unlock(&commonQmutex);
}
//End of CommonData_put()

//Description: Establishes a connection with the other socket
//Start of SocketConnect()
//Reference: Used this function earlier in class CSCI 499 - Distributed Applications
int SocketConnect(u_long address, u_short port)		//Connect to other socket
{
	int mySocket;

	socklen_t *addressLength = new socklen_t;
    *addressLength = 100;
						
	//Create a stream socket
	mySocket = socket(AF_INET, SOCK_STREAM, 0);

	//Parameters for sockaddr_in structure
	sockaddr_in sinRemote;

	sinRemote.sin_family = AF_INET;
	sinRemote.sin_addr.s_addr = address;
	sinRemote.sin_port = htons(port);

	//Connect to remote socket
	int connect_val = connect( mySocket, (sockaddr*)&sinRemote, sizeof(sockaddr_in));
	if (connect_val == SOCKETERR) 
	{
	//	cout << "Socket Connection Failed...!!!" << endl;
		return SOCKETERR;
	} 

	if(getsockname(mySocket, (sockaddr *)&sinRemote, (socklen_t *)&addressLength) == SOCKETERR ) {
//		cout << "GetSockName Failed" << endl;
		printMessage("GetSockName Failed");
		return -2;
	}
	return mySocket;
}
//End of SocketConnect()

//Description: Reads the incoming data from the socket
//Start of ReadThreadFunc()
//################### READ THREAD FUNCTION#########################
void *ReadThreadFunc(void *acceptReturn) {

	int socDesc = (int)acceptReturn;
	int myCTBPos=0;

	pthread_mutex_lock(&tiebreakermutex);
	for (int i=0; i< connTieBreakerCount; i++)
	{
	 if(socDesc==ctb[i].ctbSocDesc && ctb[i].nodeActiveFlag == true)
	 {
		 myCTBPos=i;	
		 break;
	 } 	    
	}
	pthread_mutex_unlock(&tiebreakermutex);

    while (true)
    {
		bool part2Flag=false;

		Head ARH1;
		char *readBuffer;
		ARH1= ReceiveHeader(socDesc,true,myCTBPos);					//Reading received header


		pthread_mutex_lock(&tiebreakermutex);
		  ctb[myCTBPos].ctbKeepAliveTimeout=init.KeepAliveTimeout;
		pthread_mutex_unlock(&tiebreakermutex);


        if (ARH1.nw_msg_type == STOR)
        {
			part2Flag=true;
			HandleStore_REQ(socDesc,myCTBPos,ARH1);
        }
		else if (ARH1.nw_msg_type == GTRS)
		{
			part2Flag=true;
			HandleGet_RSP(socDesc,myCTBPos,ARH1);
		}


	  if (part2Flag==false)											// part1 message processing
	  {

		if(ARH1.nw_msg_type != CKRQ && ARH1.nw_msg_type != KPAV) 	//Check for check and keepalive message: No data for these messages
		{
			readBuffer = (char*)malloc(ARH1.nw_datalen);
			if(read(socDesc, readBuffer, ARH1.nw_datalen) != (int)ARH1.nw_datalen) 	//Check with read length
			{
				ctb[myCTBPos].nodeActiveFlag=false;
		   		if (MeBeacon == false)
			    {					
					if (shutdownFlag == false)
					{
					  //If read fails, send Check message
					  SendCheckMessage();
					}					
				}
		  	  close(socDesc);   
			  pthread_exit(0);		
				
			}
		}

	
		//Switch case to handle all requests and responses
		switch (ARH1.nw_msg_type)
		{
			case JNRQ:		//Join Request
			{
				HandleJoin_REQ(socDesc,myCTBPos,ARH1,readBuffer);
				break;
			}
			case JNRS:		//Join Response
			{
				HandleJoin_RSP(socDesc,myCTBPos,ARH1,readBuffer);
				break;
			}
			case STRQ:		//Status Request
			{
				uint8_t statreq;

				memcpy(&statreq, &readBuffer[0], 1);
				if(statreq == 0x01)
				{
					HandleStatus_REQ(socDesc,myCTBPos,ARH1,readBuffer);
				}
				else if(statreq == 0x02)
				{
					HandleStatusFiles_REQ(socDesc,myCTBPos,ARH1,readBuffer);
				}

				break;			 
			}
			case STRS:		//Status Response
			{
				HandleStatus_RSP(socDesc,myCTBPos,ARH1,readBuffer);
				break;			 
			}
			case NTFY:		//Notify Request
			{
				HandleNotify_REQ(socDesc,myCTBPos,ARH1,readBuffer);
				break;			 
			}
			case CKRQ:		//Check Request
			{
				HandleCheck_REQ(socDesc,myCTBPos,ARH1);
				break;			 
			}
			case CKRS:		//Check  Response
			{
				HandleCheck_RSP(socDesc,myCTBPos,ARH1,readBuffer);
				break;			 
			}
			case KPAV:		//Keepalive Request
			{
				HandleKalive_REQ(socDesc,myCTBPos,ARH1);
				break;			 
			}
			case SHRQ:		//Search Request
			{
				HandleSearch_REQ(socDesc,myCTBPos,ARH1,readBuffer);
				break;
			}
			case SHRS:		//Search Response
			{
				HandleSearch_RSP(socDesc,myCTBPos,ARH1,readBuffer);
				break;
			}
			case GTRQ:		//Get Request
			{
				HandleGet_REQ(socDesc,myCTBPos,ARH1,readBuffer);
				break;
			}
			case DELT:		//Delete Request
			{
				HandleDel_REQ(socDesc,myCTBPos,ARH1,readBuffer);
				break;
			}
			default: break;
		}
	  }
	}
	pthread_exit(NULL);
}
//End of ReadThreadFunc()

//Description: Gets the data from the common queue structure
//Start of WriteThreadFunc()
// ################################################### WRITE THREAD FUNCTION######################################################
void *WriteThreadFunc(void *acceptReturn) 
{
	int socDesc = (int)acceptReturn;
	int myCTBPos=0;

	pthread_mutex_lock(&tiebreakermutex);
	for (int i=0; i< connTieBreakerCount; i++)
	{
		if(socDesc==ctb[i].ctbSocDesc && ctb[i].nodeActiveFlag == true)
		{
			myCTBPos=i;	
			break;
		} 	    
	}
	pthread_mutex_unlock(&tiebreakermutex);

	while (true)
	{
		pthread_mutex_lock(&(ctb[myCTBPos].writeMutex));
		while(ctb[myCTBPos].writeHead == NULL)
		{
		 pthread_cond_wait(&(ctb[myCTBPos].writeDataCondWait),&(ctb[myCTBPos].writeMutex) );			 	   
		 if (shutdownFlag == true)
		 {
			 pthread_mutex_unlock(&(ctb[myCTBPos].writeMutex)); 
			 pthread_exit(0);
		 }
		}
        pthread_mutex_unlock(&(ctb[myCTBPos].writeMutex)); 

		struct CommonData *ptr_getQ=(struct CommonData*)malloc(sizeof(struct CommonData));
		ptr_getQ=WriteQueue_Get(myCTBPos);
		Process_REQfwd(socDesc,ptr_getQ,myCTBPos);	
	}
	pthread_exit(NULL);
}
//End of WriteThreadFunc()

//Description: Handles the message coming from the incoming socket connection
//Start of handleAcceptFunc()
void *handleAcceptFunc(void *acceptRet) 
{
	int acceptReturn=(int) acceptRet;
	Head ARH1;
	int readBufferSize = 0 ;
	uint16_t rPort;
	char *rHostname;
	uint32_t r_location;
	uint8_t r_joinTTL;
	char *readBuffer;

	ARH1=  ReceiveHeader(acceptReturn,false,1);				//Receives the header

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	char *logData = (char*)malloc((int)LOGDATALEN);

	if (ARH1.nw_msg_type == HLLO)
	{
		//RECEIVE HELLO MESSAGE
      
		readBufferSize = ARH1.nw_datalen;
		readBuffer = (char*)malloc(readBufferSize);
		if(read(acceptReturn, readBuffer, readBufferSize) == SOCKETERR) {	//Reads the header. If read fails it catches the broken pipe error.
			printMessage("Read Error 2!!!");								//Check with read length
		}

		memcpy(&rPort, &readBuffer[0], 2);								 
		rHostname = (char*)malloc(ARH1.nw_datalen-2);
		memset(rHostname, '\0', ARH1.nw_datalen-2);
		memcpy(rHostname, &readBuffer[2], ARH1.nw_datalen-2);

		snprintf(logData, (int)LOGDATALEN, "%d %s",rPort, &hName[0]);

		reportLog("r", "HLLO", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, rPort,false);
		
		//Sending Hello Message
		writeHello(acceptReturn, rPort);		
	}
	else if (ARH1.nw_msg_type == JNRQ)
	{
		//Receive Join Request Message
		r_joinTTL=ARH1.nw_TTL - 1;

		readBufferSize = ARH1.nw_datalen;
		readBuffer = (char*)malloc(readBufferSize);

		if(read(acceptReturn, readBuffer, readBufferSize) == SOCKETERR) {	//Reads the header. If read fails it catches the broken pipe error.
			//Check with read length
			printMessage("Read Error 2!!!");
		}

		memcpy(&r_location, &readBuffer[0], 4);								 
		memcpy(&rPort, &readBuffer[4], 2);								 
		rHostname = (char*)malloc(ARH1.nw_datalen-6);
		memset(rHostname, '\0', ARH1.nw_datalen-6);
		memcpy(rHostname, &readBuffer[6], ARH1.nw_datalen-6);

		snprintf(logData, (int)LOGDATALEN, "%d %s", rPort,&hName[0] );

		reportLog("r", "JNRQ", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, rPort,false);

		//Sends Join Response
		joinResponse(acceptReturn,&(ARH1.UOID[0]), r_location, rPort);
	}
 
	pthread_mutex_lock(&tiebreakermutex);

	bool tempFlag=false;
			
	//Connection Tie Breaker
	for(int i = 0; i < connTieBreakerCount; i++) 
	{
		if(rPort == ctb[i].ctbPort && ctb[i].nodeActiveFlag == true) 
		{
			if((uint16_t)init.Port > ctb[i].ctbPort)		//PA > PB	
			{
				shutdown(acceptReturn, 2);
				close(acceptReturn);
				pthread_mutex_unlock(&tiebreakermutex);
				pthread_exit(NULL);
			}
			else 
			{
				shutdown(ctb[i].ctbSocDesc, 2);				//PA < PB
				close(ctb[i].ctbSocDesc);
				ctb[i].ctbSocDesc = acceptReturn;
				if (ARH1.nw_msg_type == JNRQ)
				{
					ctb[i].ctbJoinFlag = true;
				}
				else
				{
					ctb[i].ctbJoinFlag = false;
				}
				ctb[i].nodeActiveFlag = true;
				ctb[i].ctbKeepAliveTimeout = init.KeepAliveTimeout;
				ctb[i].writeHead = NULL;
				tempFlag=true;
				break;
			}
		}
	}

	if (tempFlag == false)
	{
		ctb[connTieBreakerCount].ctbPort =  rPort;
		ctb[connTieBreakerCount].ctbSocDesc = acceptReturn;
		if (ARH1.nw_msg_type == JNRQ)
		{
			ctb[connTieBreakerCount].ctbJoinFlag = true;
		}
		else
		{
			ctb[connTieBreakerCount].ctbJoinFlag = false;
		}
		ctb[connTieBreakerCount].nodeActiveFlag = true;
		ctb[connTieBreakerCount].ctbKeepAliveTimeout = init.KeepAliveTimeout;
		ctb[connTieBreakerCount].writeHead = NULL;
		connTieBreakerCount++;
	}

	pthread_mutex_unlock(&tiebreakermutex);

	if (ARH1.nw_msg_type == JNRQ)
    {
		if (r_joinTTL > 0)
		{
			UStruct tempUOIDStruct;

			tempUOIDStruct.UOID=(char*)malloc(21); 
			memset(tempUOIDStruct.UOID,'\0',21);
			memcpy(tempUOIDStruct.UOID, ARH1.UOID,20);
		
			tempUOIDStruct.recvPort = rPort;
			tempUOIDStruct.msgLifeTime = init.MsgLifetime;
			
			pthread_mutex_lock(&vectormutex);
				UOIDVector.push_back(tempUOIDStruct);
			pthread_mutex_unlock(&vectormutex);

			CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_joinTTL,  ARH1.nw_datalen, rPort, readBuffer, 65000  );
		}
	}

	//Creates a Read Thread for each request
	if (pthread_create(&accReadThread[acceptThreadCount], NULL, ReadThreadFunc, (void *)acceptReturn) == SOCKETERR) 
	{
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed.");
	}

	//Creates a Write Thread for each request
	if (pthread_create(&accWriteThread[acceptThreadCount], NULL, WriteThreadFunc, (void *)acceptReturn) == SOCKETERR)
	{
//		cout << "ERROR: pthread_create() failed." << endl;
		printMessage("ERROR: pthread_create() failed.");
	}

	pthread_exit(NULL);
}
//End of handleAcceptFunc()

//Description: Accepts the incoming socket connection
//Start of SocketAccept()
//Reference: Used this function earlier in class CSCI 499 - Distributed Applications
int SocketAccept(int port)
{
	UOIDVector.reserve(1000);

	//Parameters for sockaddr_in structure
	sockaddr_in sinRemote;

	//Create a stream socket
	myMainSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (myMainSocket != -1 ) 
	{
		sinRemote.sin_family = AF_INET;
	    sinRemote.sin_addr.s_addr = htonl(INADDR_ANY);
	    sinRemote.sin_port = htons((u_short)port);

		//Allow multiple programs to bind to the same port
		int reuseaddress = 1;
		if(setsockopt(myMainSocket, SOL_SOCKET, SO_REUSEADDR, &reuseaddress, sizeof(reuseaddress)) == SOCKETERR) 
		{
//			cout << "setsockopt SO_REUSEADDR failed: " << strerror(errno);
			printMessage("setsockopt SO_REUSEADDR failed!");
			exit(0);
		}

		//Binds the socket
		int c = bind( myMainSocket, (sockaddr*)&sinRemote, sizeof(sockaddr_in));
	    if ( c == -1 ) {
//			cout << "bad port, the server should quit" << endl;
			printMessage("bad port, the server should quit!");
			exit(1);
	    }
	}

	socklen_t *addressLength = new socklen_t;
	*addressLength = 100;

	//Listens the connection requests
	if (listen(myMainSocket, BACKLOG) == SOCKETERR) 
	{ 
//		cout << "ERROR: Listen failed. The server should quit.";
		printMessage("ERROR: Listen failed. The server should quit!");
		return SOCKETERR;
	}

	fflush(stdout);
	while(1)
	{
		int acceptReturn = accept(myMainSocket, (sockaddr*)&sinRemote, addressLength);
		if(acceptReturn == SOCKETERR)		//Accepts the connection
		{	
			return SOCKETERR;
		}
		else 
		{
			//Creates a Read Thread for each request
			if (pthread_create(&handleAccept[acceptThreadCount], NULL, handleAcceptFunc, (void *)acceptReturn) == SOCKETERR) 
			{
//				cout << "ERROR: pthread_create() failed." << endl;
				printMessage("ERROR: pthread_create() failed!");
			}

			acceptThreadCount++;
		}
	}			
	return 0;
}
//End of SocketAccept()

//Description: Writes out index files to the file system before shutdown
//Start of indexFileWrite()
void indexFileWrite(string filename, vector<ComStruct> commonvector, int size)
{
	string tmpPath(init.HomeDir);
	tmpPath = tmpPath + "/" + filename;
	remove(tmpPath.c_str());

	//read the vector and write out all the files
	for (int i = 0; i < (int)commonvector.size(); i++)
	{
		std::ostringstream rawStr;
			rawStr << commonvector[i].fileCount;
		std::string convStr = rawStr.str();

		unsigned char *charValue = HexadecToChar(commonvector[i].value, size);

		string tmpCon(convStr);
		tmpCon = tmpCon + "|" + (char*)charValue + "\n";
		FileWrite(filename, (char*)tmpCon.c_str(), 1);
	}
			
}

//Description: Overloads the less than operator used for sorting strings
//Start of bool operator<()
inline bool operator<(const ComStruct& a, const ComStruct& b)
{
	if (strcmp((char*)a.value,(char*)b.value) > 0 )
	{
       return false;
	}
	else if (strcmp((char*)a.value,(char*)b.value) < 0 )
	{
		return true;
	}
	else
	{
	  return true;
	}
}


//Description: Cleans up all the structures, threads and variables
//Start of CleanupAndExit()
void CleanupAndExit()
{
	struct timeval timeout;
	timeout.tv_sec=0;
    timeout.tv_usec=70000;

	
	//############## CANCEL 3 Main threads##############
	select(0, NULL, NULL, NULL, &timeout);
	
	pthread_cancel(commandThread);			//Command Thread
	
	select(0, NULL, NULL, NULL, &timeout);

	pthread_mutex_lock(&shutdownmutex);
	   shutdownFlag = true; 
	pthread_mutex_unlock(&shutdownmutex);

	pthread_cancel(timerActionThread);		//Timer Action Thread

	select(0, NULL, NULL, NULL, &timeout);
	
	pthread_cancel(timerCountThread);		//Timer Count Thread


	//########## Write index files ################

		std::sort(sha1_ind_vector.begin(),sha1_ind_vector.end());

		indexFileWrite("perm_file_id", FileID_vector, 20);
		indexFileWrite("one_time_pwd", OneTmPwd_vector, 20);
		indexFileWrite("kwrd_index", kwrd_ind_vector, 128);
		indexFileWrite("sha1_index", sha1_ind_vector, 20);
		indexFileWrite("nonce_file", nonce_vector, 20);

		string tmpPath(init.HomeDir);
		tmpPath = tmpPath + "/name_index";
		remove(tmpPath.c_str());
        
		std::sort(name_ind_vector.begin(),name_ind_vector.end());

		for (int i = 0; i < (int)name_ind_vector.size(); i++)
		{
			std::ostringstream rawStr;
				rawStr << name_ind_vector[i].fileCount;
			std::string convStr = rawStr.str();

			string tmpCon(convStr);
			tmpCon = tmpCon + "|" + (char*)name_ind_vector[i].value + "\n";
			FileWrite("name_index", (char*)tmpCon.c_str(), 1);
		}

		string tmplruPath(init.HomeDir);
		tmplruPath = tmplruPath + "/lru_file";
		remove(tmplruPath.c_str());

		for (int i = 0; i < (int)LRU_vector.size(); i++)               //write LRU file
		{
			std::ostringstream rawStr;
				rawStr << LRU_vector[i].localFileCount;
			std::string convStr = rawStr.str();

			std::ostringstream rawsizeStr;
				rawsizeStr << LRU_vector[i].FileSize;
			std::string convsizeStr = rawsizeStr.str();


			string tmpCon(convStr);
			string tmpsize(convsizeStr);
			tmpCon = tmpCon + "|" + tmpsize + "\n";
			FileWrite("lru_file", (char*)tmpCon.c_str(), 1);
		}

	//###########Signal Dispatch Handler####################
	pthread_cond_signal(&commonDataCondWait);


	//###################CLOSE MAIN ACCEPT SOCKET###########
	close(myMainSocket);


	//###########Read & Write threads Exit####################
	for(int i = 0; i < connTieBreakerCount; i++) 
	{
		pthread_cond_signal(&(ctb[i].writeDataCondWait));
	}
  
	for(int i = 0; i < connTieBreakerCount; i++) 
	{
        ctb[i].nodeActiveFlag=false;   
		close(ctb[i].ctbSocDesc);
	}

	connTieBreakerCount = 0;		//Setting all variables to default values
	connectThreadCount = 0;
	acceptThreadCount = 0;
	ConnectCount=0;
	joinQuit=false;
	StatusFlag=false;

		
	select(0, NULL, NULL, NULL, &timeout);

	pthread_mutex_lock(&commonQmutex);		//Removing entries from the common queue structure

	if (CData_Head != NULL)
    {
	  struct CommonData *temp1,*temp2;
	  temp1=CData_Head;
	  temp2=temp1;
  
		while (temp1->next !=NULL)
		{
			temp2=temp1;
			temp1=temp1->next;
			free(temp2);
		}

		free(temp1);

	  }  
	CData_Head=NULL;
	pthread_mutex_unlock(&commonQmutex);

	delete []ctb;			//Deletes the connection tie-breaker structure
	shutdownFlag=false;
	UOIDVector.clear();		//Clearing vectors
	LinkVector.clear();
	NodeVector.clear();

	//Deleting tmp directory and its contents
	DIR *fNode;
	struct dirent *fent;

    string oFilesDir (init.HomeDir);
	
	oFilesDir=oFilesDir+"/files/tmp";

	fNode = opendir(oFilesDir.c_str());			//Deleting files from the tmp directory
	while((fent=readdir(fNode))) 
	{
		string fDel = fent->d_name;
		fDel = '/' + fDel;
		fDel = oFilesDir + fDel;
		remove(fDel.c_str());		
	}
	closedir(fNode);



	printf("\n");
	if (selfRestartFlag==false)
	{		
		exit(0);
	}
}
//End of CleanupAndExit()

