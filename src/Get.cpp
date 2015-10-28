/*************************************************************************************************************/
/*												Project : CSCI551 Final Part 2 Project 						*/
/*												Filename: Get.cpp (Server Implementation)	 			    */
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/

#include "CommonHeaderFile.h"
#include "ParseFile.h"
#include "Search.h"
#include "Store.h"
#include "DoHello.h"
#include "Connection.h"
#include "HandleRead.h"
#include "ProcessWrite.h"

using namespace std;

// comman global variables
string GetFileName;
string sSourceDataFileName;

bool getCompleteFlag;
bool getTimeoutFlag;

int tempFileCount=1;

// Desciption : Function copies the given file from node's home direcory to current working directory
// Start of copyIntoWrokingDir()

void copyIntoWorkingDir(int localFileCount)
{
	std::ostringstream rawStr;
	rawStr << localFileCount;
	std::string convStr = rawStr.str();

	string storeDataFilename (convStr);
	storeDataFilename = storeDataFilename + ".data"; 

	string tFileName(init.HomeDir);
	sSourceDataFileName = tFileName + "/files/" + storeDataFilename;

	size_t found = GetFileName.find("Not_Specified");
	if (found!=string::npos)
	{
		string storeMetaFilename (convStr);
		storeMetaFilename = storeMetaFilename + ".meta"; 

		string sMetaFile(init.HomeDir);
		sMetaFile = sMetaFile + "/files/" + storeMetaFilename;

		MetaStruct rcdMeta;
		rcdMeta=ParseMetaData((char*)sMetaFile.c_str());
		string gName (rcdMeta.FileName);
        
        GetFileName="";
		GetFileName=GetFileName+gName;
	}
   // move the file from LRU to permanent storage

	for (int j=0;j<(int)LRU_vector.size() ; j++)
	{
		if (LRU_vector[j].localFileCount == localFileCount)
		{
			LRUcacheSize = LRUcacheSize - LRU_vector[j].FileSize;
			LRU_vector.erase(LRU_vector.begin()+j);
			break;
		}
	}

	getCompleteFlag=true;

}

// Desciption : Function parses the commandline arguments and checks if the file exists in current node
// Start of CallGet()

void callGet(string readBuffer) 
{
	//	int oldState=0;
	string strGetResultNo;
	int GetResultNo=0;
	GetFileName="";   
	sSourceDataFileName="";

	getCompleteFlag=false;
	getTimeoutFlag=false;
	
	TrimSpaces(readBuffer);															// remove spaces

	size_t found=readBuffer.find(" ");
	if (found!=string::npos)
	{
		strGetResultNo=readBuffer.substr(0,found);
		GetFileName=readBuffer.substr(found+1);

		TrimSpaces(GetFileName);														// remove spaces

		GetResultNo=atoi(strGetResultNo.c_str());

		if ((int)Result_vector.size() == 0)
		{
			printf("GET command allowed only directly after Search or Get...\n");
			return;
		}


		//Check for file if already exists, if yes deleting it...
		struct stat stFileInfo;
		int intStat = stat(GetFileName.c_str(), &stFileInfo);
		
		if(intStat == 0)
		{
			printf("File Already Exists.... Replace (Y/N) : ");
			char ip_Buffer[100];
			gets(ip_Buffer);
			string ip (ip_Buffer);
			size_t ipfound1=ip.find("N");
			size_t ipfound2=ip.find("n");
			if (ipfound1 != string::npos || ipfound2 != string::npos  )
			{
			  return;
			}
			else
			{
			  remove(GetFileName.c_str());
			}
		}
	}                                                                               
	else                                                                            // No get or search command 
	{
		if ((int)Result_vector.size() == 0)
		{
			printf("GET command allowed only directly after Search or Get...\n");
			return;
		}
		GetResultNo=atoi(readBuffer.c_str());
		GetFileName="Not_Specified";
	}

    int resultPos=0;
	bool resultFD=false;
	for (int i=0; i<(int)Result_vector.size() ;i++ )
	{
		if (GetResultNo == Result_vector[i].resultNumber )
		{
			resultPos=i;
			resultFD=true;
		}
	}

	if (resultFD == false)
	{
       printf("Please Specify correct Request Number from the Search Results...\n"); 
	   return;
	}

  //get the result number from search request
  bool selfFound=false;
  pthread_mutex_lock(&storemutex);
  for (int i=0;i<(int)FileID_vector.size() ;i++ )
  {
	 size_t checkStr=memcmp(FileID_vector[i].value, Result_vector[resultPos].fileID, 20);
	 if(checkStr==0)
	 {
		 selfFound=true;
		 copyIntoWorkingDir(FileID_vector[i].fileCount);
		 break;
	 }
  }
  pthread_mutex_unlock(&storemutex);
	


 if (selfFound== false)
 {
	 // put request on the network
     if (init.TTL > 0)
	 {
		// put search message on the queue
		uint32_t dataLength = 40;

		char *getHeader = (char*)malloc(COMMON_HEADER_SIZE);
		getHeader = GetHeader(node_inst_id, GTRQ, (uint8_t)init.TTL, dataLength);

		char *getUOID = (char*)malloc(21);
		memset(getUOID, '\0', 21);
		memcpy(&getUOID[0],&getHeader[1],20);	// copy to global UOID to decide originator of search message

		//Pushing UOID into UOID structure
		pthread_mutex_lock(&vectormutex);

			UStruct tempUOIDStruct;
			tempUOIDStruct.UOID=(char*)malloc(21); 
			memset(tempUOIDStruct.UOID,'\0',21);
			memcpy(tempUOIDStruct.UOID, getUOID,20);
			tempUOIDStruct.recvPort = init.Port;
			tempUOIDStruct.msgLifeTime = init.GetMsgLifetime;

			UOIDVector.push_back(tempUOIDStruct);

		pthread_mutex_unlock(&vectormutex);

		char *putBuffer=(char *)malloc(dataLength+1);
		memset(putBuffer, '\0', dataLength+1);
		memcpy(&putBuffer[0], &(Result_vector[resultPos].fileID[0]), 20);
		memcpy(&putBuffer[20], &(Result_vector[resultPos].SHA1[0]), 20);

		CommonData_put(GTRQ, &(getUOID[0]), init.TTL,  dataLength, uint16_t(init.Port), putBuffer, 65000);
		sleep(init.MsgLifetime);
	 }
 }

  getTimeoutFlag=true;				//check if the file is downloaded successfully
  if (getCompleteFlag==false)
  {
	  printf("Error while downloading the file...\n");
  }
  else
  {
	//check if the file is alraedy present and ask to overwrite if exists
	struct stat stFileInfo;
	int intStat = stat(GetFileName.c_str(), &stFileInfo);
	
	if(intStat == 0)
	{
		printf("File Already Exists.... Replace (Y/N) : ");
		char ip_Buffer[100];
		gets(ip_Buffer);
		string ip (ip_Buffer);
		size_t ipfound1=ip.find("N");
		size_t ipfound2=ip.find("n");
		if (ipfound1 != string::npos || ipfound2 != string::npos  )
		{
		  return;
		}
		else
		{
		  remove(GetFileName.c_str());
		}
	}


	string exeCommand ("cp");
    exeCommand = exeCommand + " " + sSourceDataFileName + " " + GetFileName;

	system(exeCommand.c_str());

  
  
  }
  
}

// Desciption : Function sends the file data on the network
// Start of writeFileOnNetwork()

void writeFileOnNetwork(string sDataFile, int acceptReturn, long file_datalen)
{
	fstream fpSend;
	long ReadBytes=0;
	
	fpSend.open(sDataFile.c_str(), ios::in | ios::binary);
	while (ReadBytes < file_datalen)							// read all the received bytes	
	{
		int target = file_datalen - ReadBytes;
		char *readData;    
		if(target > BUFFERSIZE)
		{
			readData = (char *)malloc(BUFFERSIZE+1);				//buffer to read 8192 bytes
			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");}

			memset(readData, '\0', BUFFERSIZE+1);  

			fpSend.read(&readData[0], BUFFERSIZE);

			//Sending Data
			if(write(acceptReturn, readData, BUFFERSIZE) != BUFFERSIZE)
			{
				signal(SIGPIPE, pipeHandler);
			}

			free(readData); 		 
		}
		else    //target
		{
			readData = (char *)malloc(target+1);					// buffer to read less than 512 bytes

			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");  }
			memset(readData, '\0', target+1);

			fpSend.read(&readData[0], target);
			
			//Sending Data
			if(write(acceptReturn, readData, target) != target)
			{
				signal(SIGPIPE, pipeHandler);
			}


			free(readData); 			 
		} //end else target
		ReadBytes=ReadBytes + BUFFERSIZE;
	}  //end while

	fpSend.close();
}


// Desciption : Function writes the response to get request
// Start of getFileResponse()

void getFileResponse(int acceptReturn, char *getUOID, char *fileID, int port)
{
  bool selfFound=false;
  int filePos=0;

  pthread_mutex_lock(&storemutex);								//find file id	
  for (int i=0;i<(int)FileID_vector.size() ;i++ )
  {
	 size_t checkStr=memcmp(FileID_vector[i].value, &fileID[0], 20);
	 if(checkStr==0)
	 {
		 selfFound=true;
		 filePos=i;
		 break;
	 }
  }
  pthread_mutex_unlock(&storemutex);

  if (selfFound == true)						// file id present in this node
  {
	int writeBufferSize = 0;

	//SEND GET RESPONSE
	std::ostringstream rawStr;
	rawStr << FileID_vector[filePos].fileCount;
	std::string convStr = rawStr.str();

	string storeMetaFilename (convStr);
	storeMetaFilename = storeMetaFilename + ".meta"; 

	string sMetaFile(init.HomeDir);
	sMetaFile = sMetaFile + "/files/" + storeMetaFilename;

	string storeDataFilename (convStr);
	storeDataFilename = storeDataFilename + ".data"; 

	string sDataFile(init.HomeDir);
	sDataFile = sDataFile + "/files/" + storeDataFilename;

	struct stat tmp_structure, *buf_stat;
	buf_stat=&tmp_structure;
	stat(sDataFile.c_str(), buf_stat);					// Get filesize
	long file_datalen=buf_stat->st_size;

	uint32_t metaLen = strlen(sMetaFile.c_str());
	uint32_t dataLength = 24 + metaLen + file_datalen;			//get header and send data

	char *getRespHeader = (char*)malloc(COMMON_HEADER_SIZE);
	getRespHeader = GetHeader(node_inst_id, GTRS, (uint8_t)init.TTL, dataLength);

	writeBufferSize = COMMON_HEADER_SIZE + dataLength;

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &getUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	reportLog("s", "GTRS", writeBufferSize, init.TTL, getRespHeader, logData, port, true);		//write log file

	char *writeBuffer=(char*)malloc(COMMON_HEADER_SIZE + 24 + metaLen + 1);
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', (COMMON_HEADER_SIZE + 24 + metaLen + 1));
	memcpy(&writeBuffer[0], &getRespHeader[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &getUOID[0], 20);
	memcpy(&writeBuffer[47], &metaLen, 4);
	memcpy(&writeBuffer[51], &sMetaFile[0], metaLen);

	if(write(acceptReturn, writeBuffer, (COMMON_HEADER_SIZE + 24 + (int)metaLen)) != (COMMON_HEADER_SIZE + 24 + (int)metaLen)) 
	{	
		printMessage("**Write error... ");
	}

	writeFileOnNetwork(sDataFile, acceptReturn, file_datalen);	

	free(writeBuffer);


  } 							//file id not present --- Do nothing
}



// Desciption : Function reads the GET request and calls get response, forwards the get to other nodes
// Start of HandleGet_REQ()

void HandleGet_REQ(int socDesc, int myCTBPos,struct Header ARH1, char *readBuffer)
{
	bool uoidFoundFlag = false;

	char *fileID = (char*)malloc(21);
	memset(fileID, '\0', 21);
	memcpy(&fileID[0], &readBuffer[0], 20);

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;

	char *logData = (char*)malloc((int)LOGDATALEN);

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &(fileID[16]), 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	//Writes into log file
	reportLog("r", "GTRQ", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort,false);

	//Checks if the request has already been processed
	uoidFoundFlag = pushUOIDStruture(myCTBPos, ARH1);
	
	if(uoidFoundFlag == false)         //UOID not present i.e. it's a new request
	{
		uint8_t r_getTTL=ARH1.nw_TTL - 1;

		//Inserts the request into queue for flooding
		if (r_getTTL > 0)
		{
		   CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_getTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, 65000);
		}

		//Sends get Response
		getFileResponse(socDesc, &(ARH1.UOID[0]), fileID, ctb[myCTBPos].ctbPort);

	}	//Else Message Already Seen

}


// Desciption : Function creates a temporary intermediate file in tmp directory
// Start of createTempFile()

string createTempFile(int socDesc, int myCTBPos, long file_datalen)
{
	long ReadBytes=0;

	pthread_mutex_lock(&storemutex);
	
	std::ostringstream rawStr;
	rawStr << tempFileCount;
	std::string convStr = rawStr.str();
	tempFileCount++;

	pthread_mutex_unlock(&storemutex);

	string storetempFile (convStr);
	storetempFile = storetempFile + ".data"; 

	while (ReadBytes < file_datalen)							// read all the received bytes	
	{
		int target = file_datalen - ReadBytes;
		char *readData;
		if(target > BUFFERSIZE)
		{
			readData = (char *)malloc(BUFFERSIZE+1);				//buffer to read 8192 bytes
			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");}

			memset(readData, '\0', BUFFERSIZE+1);  

			//Reading Data
			if(read(socDesc, readData, BUFFERSIZE) != BUFFERSIZE)
			{
				errorWhileReading(myCTBPos, socDesc);
			}

			DataFileWrite(storetempFile, readData, BUFFERSIZE, 2);
			free(readData); 		 
		}
		else    //target
		{
			readData = (char *)malloc(target+1);					// buffer to read less than 512 bytes

			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");  }
			memset(readData, '\0', target+1);
			//Reading Data
			if(read(socDesc, readData, target) != target)
			{
				errorWhileReading(myCTBPos, socDesc);
			}
			DataFileWrite(storetempFile, readData, target, 2);
			
			free(readData); 			 
		} //end else target
		ReadBytes=ReadBytes + BUFFERSIZE;
	}  //end while
	
	string storeName (init.HomeDir);
    storeName = storeName + "/files/tmp/" + storetempFile;    
	
	return storeName;
}



// Desciption : Function to handle responses to destination nodes
// Start of ReceiveGetResponse()

void ReceiveGetResponse(int socDesc,int myCTBPos,struct Header ARH1,char *relayUOID, uint16_t relayPort)
{
	char *readBuffer = (char*)malloc(5);
	memset(readBuffer, '\0', 5);
	
	if(read(socDesc, readBuffer, 4) != 4) 	//Check with read length
	{
		errorWhileReading(myCTBPos,socDesc);
	}

	uint32_t metaLen;
	memcpy(&metaLen, &readBuffer[0], 4);

	char *metaFile = (char*)malloc(metaLen+1);
	memset(metaFile, '\0', metaLen+1);
	if(read(socDesc, metaFile, (int)metaLen) != (int)metaLen) 	//Check with read length
	{
		errorWhileReading(myCTBPos,socDesc);
	}

	long file_datalen = ARH1.nw_datalen - (metaLen+4+20);
	string tmpFilename = createTempFile(socDesc, myCTBPos, file_datalen);
    
	MetaStruct rcdMeta;
	rcdMeta=ParseMetaData(metaFile);

	unsigned char* hexNonce = (unsigned char*)malloc(21);
	memset(hexNonce, '\0', 21);
	hexNonce = CharToHexadec((unsigned char *)rcdMeta.Nonce, 40);

	//check if the nonce is presenet in the current. if present dont store a new file
    bool selfFound=false;
	int filepos=0;
	  pthread_mutex_lock(&storemutex);
	  for (int i=0;i<(int)nonce_vector.size() ;i++ )
	  {
		 size_t checkStr=memcmp(nonce_vector[i].value, &hexNonce[0], 20);
		 if(checkStr==0)
		 {
			filepos=i;
			selfFound=true;
			 break;
		 }
	  }
	  pthread_mutex_unlock(&storemutex);

	  if (selfFound == true)
	  {
		if (getTimeoutFlag == false)
		{
			 copyIntoWorkingDir(nonce_vector[filepos].fileCount);
			 pthread_kill(commandThread, SIGINT);
		}
		return;
	  }
	  else							//create a new file in home directory
	  {
		  string strFile(rcdMeta.FileName);
		  int localFileCount=loadAndStoreFiles(strFile);

			copyDataFromMetafile(localFileCount, rcdMeta);

			std::ostringstream rawStr;
				rawStr << localFileCount;
			std::string convStr = rawStr.str();

			string storeDataFilename (convStr);
			storeDataFilename = storeDataFilename + ".data"; 

			string sDataFile(init.HomeDir);
			sDataFile = sDataFile + "/files/" + storeDataFilename;
			 string exeCommand ("mv");
			 exeCommand = exeCommand + " " + tmpFilename + " " + sDataFile;

			 system(exeCommand.c_str());

			string storeMetaFilename (convStr);
			storeMetaFilename = storeMetaFilename + ".meta"; 

			string sMetaFile(init.HomeDir);
			sMetaFile = sMetaFile + "/files/" + storeMetaFilename;
			string ipMname (metaFile);

			 string exeMCommand ("cp");
			 exeMCommand = exeMCommand + " " + ipMname + " " + sMetaFile;

			 system(exeMCommand.c_str());		

		    if (getTimeoutFlag == false)
		    {
			 copyIntoWorkingDir(localFileCount);
			 pthread_kill(commandThread, SIGINT);
		    }
	  }

}

// Desciption : Function copies file between intermediate nodes
// Start of Forwardet response()

void ForwardGetResponse(int socDesc,int myCTBPos,struct Header ARH1,char *relayUOID, uint16_t relayPort)
{
	char *readBuffer = (char*)malloc(5);
	memset(readBuffer, '\0', 5);
	
	if(read(socDesc, readBuffer, 4) != 4) 	//Check with read length
	{
		errorWhileReading(myCTBPos,socDesc);
	}

	uint32_t metaLen;
	memcpy(&metaLen, &readBuffer[0], 4);

	char *metaFile = (char*)malloc(metaLen+1);
	memset(metaFile, '\0', metaLen+1);
	if(read(socDesc, metaFile, (int)metaLen) != (int)metaLen) 	//Check with read length
	{
		errorWhileReading(myCTBPos,socDesc);
	}

	long file_datalen = ARH1.nw_datalen - (metaLen+4+20);
	string tmpFilename = createTempFile(socDesc, myCTBPos, file_datalen);
    
	//forward get request to other node
	uint8_t r_getTTL = ARH1.nw_TTL - 1;
	if (r_getTTL > 0)
	{
	   uint32_t outLen=strlen(tmpFilename.c_str());
	   int sendLen= 24 + metaLen + 4+ outLen;

	   char *sendBuffer=(char*)malloc(sendLen+1);
	   memset(sendBuffer,'\0',sendLen+1);
	   memcpy(&sendBuffer[0],&relayUOID[0],20);
	   memcpy(&sendBuffer[20],&readBuffer[0],4);
       memcpy(&sendBuffer[24],&metaFile[0], metaLen);
	   memcpy(&sendBuffer[24+metaLen],&outLen, 4);
	   memcpy(&sendBuffer[24+metaLen+4],&tmpFilename[0], outLen);
	   

	   CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_getTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, sendBuffer, relayPort);
	}


	//decide if to store file based on cache proba and size	
	double randomCacheProb=drand48();
	
	if (randomCacheProb > init.CacheProb)
	{
		return;
	}

	MetaStruct rcdMeta;
	rcdMeta=ParseMetaData(metaFile);
	unsigned char* hexNonce = (unsigned char*)malloc(21);
	memset(hexNonce, '\0', 21);
	hexNonce = CharToHexadec((unsigned char *)rcdMeta.Nonce, 40);

    bool selfFound=false;
	  pthread_mutex_lock(&storemutex);
	  for (int i=0;i<(int)nonce_vector.size() ;i++ )
	  {
		 size_t checkStr=memcmp(nonce_vector[i].value, &hexNonce[0], 20);
		 if(checkStr==0)
		 {
			 selfFound=true;
			 break;
		 }
	  }
	  pthread_mutex_unlock(&storemutex);

	  if (selfFound == true)
	  {
		  return;
	  }
    
		int localFileCount=addFileToLRU(rcdMeta);		//addFileToLRU

		if (localFileCount == -99)
		{
			return;
		}

		copyDataFromMetafile(localFileCount, rcdMeta);

		// copy file to permanent storage in home directory

		std::ostringstream rawStr;
			rawStr << localFileCount;
		std::string convStr = rawStr.str();

		string storeDataFilename (convStr);
	    storeDataFilename = storeDataFilename + ".data"; 

	    string sDataFile(init.HomeDir);
	    sDataFile = sDataFile + "/files/" + storeDataFilename;
		 string exeCommand ("cp");
         exeCommand = exeCommand + " " + tmpFilename + " " + sDataFile;

		 system(exeCommand.c_str());

		string storeMetaFilename (convStr);
		storeMetaFilename = storeMetaFilename + ".meta"; 

		string sMetaFile(init.HomeDir);
		sMetaFile = sMetaFile + "/files/" + storeMetaFilename;
		string ipMname (metaFile);

		 string exeMCommand ("cp");
         exeMCommand = exeMCommand + " " + ipMname + " " + sMetaFile;

		 system(exeMCommand.c_str());


}


// Desciption : Function reads the GET response from the network and calls appropriate functions
// Start of HandleGet_RSP()

void HandleGet_RSP(int socDesc,int myCTBPos,struct Header ARH1)
{
	char *relayUOID = (char*)malloc(21);
	memset(relayUOID, '\0', 21);
	
	if(read(socDesc, relayUOID, 20) != 20) 	//Check with read length
	{
		errorWhileReading(myCTBPos,socDesc);
	}

	bool uoidFoundFlag = false;
	uint16_t relayPort;	

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	char *logData = (char*)malloc((int)LOGDATALEN);

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &relayUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	//Writes into log file
	reportLog("r", "GTRS", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort,false);

	pthread_mutex_lock(&vectormutex);

	for (int i = 0; i < (int)UOIDVector.size(); i++)
	{
		size_t checkStr=memcmp (relayUOID, UOIDVector[i].UOID, 20);

		if (checkStr==0)
		{
		 uoidFoundFlag = true;
		 relayPort = UOIDVector[i].recvPort;
		 break;
		}
	}
	pthread_mutex_unlock(&vectormutex);

	//Relays back the Search Response to the node who requested
	if(uoidFoundFlag == true)              // UOID not already present
	{
		if (relayPort == (uint16_t)init.Port)		//Request originated from Me
		{
			ReceiveGetResponse(socDesc,myCTBPos,ARH1,relayUOID,relayPort);		
		}
		else
		{
			ForwardGetResponse(socDesc,myCTBPos,ARH1,relayUOID,relayPort);		
		}
	}  // uoid not found

}


// Desciption : Function forwards the get response to the next neighbor
// Start of sendGetResponseMessage()

void sendGetResponseMessage(char *msg_header, char *Request, int socDesc, uint32_t dataLen)
{
	char *getUOID = (char*)malloc(21);
	memset(getUOID, '\0', 21);
	memcpy(&getUOID[0],&Request[0],20);	// copy to global UOID to decide originator of search message

	uint32_t metaLen;
	memcpy(&metaLen, &Request[20], 4);

	char *metaFile = (char*)malloc(metaLen+1);
	memset(metaFile, '\0', metaLen+1);
	memcpy(&metaFile[0], &Request[24], metaLen);


	uint32_t tmpFileLen;
	memcpy(&tmpFileLen, &Request[24+metaLen], 4);

	char *tempFile = (char*)malloc(tmpFileLen+1);
	memset(tempFile, '\0', tmpFileLen+1);
	memcpy(&tempFile[0], &Request[24+metaLen+4], tmpFileLen);

	//read the temp file and forward to the next neighbor
	
	int ttlLen = COMMON_HEADER_SIZE + 24 + metaLen;

	char *writeBuffer=(char*)malloc(ttlLen + 1);
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', (ttlLen + 1));
	memcpy(&writeBuffer[0], &msg_header[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &getUOID[0], 20);
	memcpy(&writeBuffer[47], &metaLen, 4);
	memcpy(&writeBuffer[51], &metaFile[0], metaLen);

	if(write(socDesc, writeBuffer, ttlLen) != ttlLen) 
	{	
		printMessage("**Write error... ");
	}
	
	long file_datalen = dataLen - (24+metaLen);
	string sDataFile(tempFile);

	//write file onto the network
	writeFileOnNetwork(sDataFile, socDesc, file_datalen);

}


