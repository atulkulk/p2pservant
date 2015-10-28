/************************************************************************************************/
/*					Project : CSCI551 Final Part 1 Servant Project		 						*/
/*					Filename: HandleRead.cpp													*/
/*					Credits : Abhishek Deshmukh, Atul Kulkarni									*/
/************************************************************************************************/

//Inclusion of new header files
#include "CommonHeaderFile.h"
#include "Connection.h"
#include "MainPrg.h"
#include "DoHello.h"
#include "ParseFile.h"
#include "ProcessWrite.h"
#include "StatusFile.h"

//Declaration of Variables
typedef struct linkStructure		//Link structure for nam
{
	uint16_t source;
	uint16_t destination;
}LStruct;

vector<LStruct> LinkVector;
vector<uint16_t> NodeVector;
pthread_mutex_t linkvectormutex;

//Description: Inserts UOID of messages into a vector
//Start of pushUOIDStruture()
bool pushUOIDStruture(int myCTBPos,struct Header ARH1) 
{
	UStruct tempUOIDStruct;
	bool uoidFoundFlag = false;

	struct timeval vecTimeout;
	vecTimeout.tv_sec=0;
    vecTimeout.tv_usec=200;
    select(0, NULL, NULL, NULL, &vecTimeout);

	pthread_mutex_lock(&vectormutex);
	for (int i = 0; i < (int)UOIDVector.size(); i++)
	{
		size_t checkStr=memcmp (ARH1.UOID, UOIDVector[i].UOID, 20 );

		if (checkStr==0)
		{
		 uoidFoundFlag = true;
		}
	}
	
	if(uoidFoundFlag == false)                         // UOID not present in the structure
	{
        tempUOIDStruct.UOID=(char*)malloc(21); 
		memset(tempUOIDStruct.UOID,'\0',21);
		memcpy(tempUOIDStruct.UOID, ARH1.UOID,20);
		tempUOIDStruct.recvPort = ctb[myCTBPos].ctbPort;
		if (ARH1.nw_msg_type != GTRQ)
		{
		 tempUOIDStruct.msgLifeTime = init.MsgLifetime;
		}
		else
		{
		 tempUOIDStruct.msgLifeTime = init.GetMsgLifetime;
		}

		UOIDVector.push_back(tempUOIDStruct);
	}
	pthread_mutex_unlock(&vectormutex);
	return uoidFoundFlag;
}
//End of pushUOIDStruture()

//Description: Generates status response and sends it to the node who requested
//Start of statusResponse()
void statusResponse(int acceptReturn, char *sUOID, int port)
{
	//SEND STATUS MESSAGE
	uint16_t wPort = (uint16_t)init.Port;
	uint16_t hostInfoLength = 15; 

	int dataBufferSize = 37;
	char *dataBuffer = (char*)malloc(dataBufferSize);		//MAIN: DATABUFFER
	//Copying data in to an unsigned character buffer.		//Contains this node's information
	memset(dataBuffer, '\0', dataBufferSize);
	memcpy(&dataBuffer[0], &sUOID[0], 20);
	memcpy(&dataBuffer[20], &hostInfoLength, 2);
	memcpy(&dataBuffer[22], &wPort, 2);
	memcpy(&dataBuffer[24], &hName[0], strlen(hName));

	int tbCount = 0;
	pthread_mutex_lock(&tiebreakermutex);

	uint16_t tbPortArray[connTieBreakerCount];

	for (int i=0; i< connTieBreakerCount; i++)
	{
		if(ctb[i].ctbJoinFlag == false && ctb[i].nodeActiveFlag == true)
		{
			tbPortArray[tbCount] = ctb[i].ctbPort; 
			tbCount++;
		} 	    
	}

	pthread_mutex_unlock(&tiebreakermutex);

	int tbBufferSize = 19 * tbCount;
	char *tbBuffer = (char*)malloc(tbBufferSize);		//MAIN: TBBUFFER
	//Copying data in to an unsigned character buffer.
	memset(tbBuffer, '\0', tbBufferSize);				//Contains information about the neighbors of this node
	uint32_t recLength; 
	int arrayPos = 0;

	for (int i=0; i < tbCount; i++)
	{
		if(i == (tbCount - 1)) 
		{
			recLength = 0;
			memcpy(&tbBuffer[arrayPos], &recLength, 4);
		}
		else 
		{
			recLength = 15;
			memcpy(&tbBuffer[arrayPos], &recLength, 4);		
		}
		memcpy(&tbBuffer[arrayPos + 4], &tbPortArray[i], 2);
		memcpy(&tbBuffer[arrayPos + 6], &hName[0], 13);
		arrayPos = arrayPos + 19;
	}	

	int dataLength = dataBufferSize + tbBufferSize;

	//Creation of status response
	char *statusHeader = (char*)malloc(COMMON_HEADER_SIZE);
	statusHeader = GetHeader(node_inst_id, STRS, (uint8_t)init.TTL, dataLength);

	int writeBufferSize = COMMON_HEADER_SIZE + dataLength;

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &sUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	reportLog("s", "STRS", writeBufferSize, init.TTL, statusHeader, logData, port, true);

	char *writeBuffer=(char*)malloc(writeBufferSize);
	if (writeBuffer == NULL) 
	{ 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &statusHeader[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &dataBuffer[0], dataBufferSize);
	memcpy(&writeBuffer[COMMON_HEADER_SIZE + dataBufferSize], &tbBuffer[0], tbBufferSize);
	
	//Writes the response onto the corresponding socket
	if(write(acceptReturn, writeBuffer, writeBufferSize) != writeBufferSize) 
	{
		cout << "Write error... " << endl;
	}

    free(statusHeader);
	free(dataBuffer);
	free(tbBuffer);
	free(writeBuffer);
}
//End of statusResponse

//Description:	Handles incoming join request. This function sends back the response and puts the request
//				onto queue for flooding.
//Start of HandleJoin_REQ()
void HandleJoin_REQ(int socDesc,int myCTBPos,struct Header ARH1, char *readBuffer)
{
	bool uoidFoundFlag = false;

    uint16_t rPort;
	memcpy(&rPort, &readBuffer[4], 2);

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;

	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%d %s", rPort, &hName[0]);

	//Writes into log file
	reportLog("r", "JNRQ", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort,false );

	//Checks if the request has already been processed
	uoidFoundFlag = pushUOIDStruture(myCTBPos, ARH1);
	
	if(uoidFoundFlag == false)         //UOID not present i.e. it's a new request
	{
		uint8_t r_joinTTL=ARH1.nw_TTL - 1;

		uint32_t r_location=0;
		memcpy(&r_location, &readBuffer[0], 4);								 

		//Sends Join Response
		joinResponse(socDesc,&(ARH1.UOID[0]), r_location, ctb[myCTBPos].ctbPort);

		//Inserts the request into queue for flooding
		if (r_joinTTL > 0)
		{
		   CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_joinTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, 65000  );
		}
	}	//Else Message Already Seen
}
//End of HandleJoin_REQ()

//Description:	Handles incoming join response. This function sends back the response and puts the request
//				onto queue for relaying back to the node who sent join request.
//Start of HandleJoin_RSP()
void HandleJoin_RSP(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer)
{

	bool uoidFoundFlag = false;
	uint16_t relayPort;
	
	char *relayUOID = (char*)malloc(21);
	memset(relayUOID, '\0', 21);
	memcpy(relayUOID, &readBuffer[0], 20);

    uint32_t rDistance;
    uint16_t rPort;
	
	memcpy(&rDistance, &readBuffer[20] , 4);
	memcpy(&rPort, &readBuffer[24], 2);

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	char *logData = (char*)malloc((int)LOGDATALEN);

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &relayUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	snprintf(logData, (int)LOGDATALEN, "%s %d %d %s", (char *)tMsgid, rDistance, rPort, &hName[0]);

	//Writes into log file
	reportLog("r", "JNRS", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort,false);

	pthread_mutex_lock(&vectormutex);

	for (int i = 0; i < (int)UOIDVector.size(); i++)
	{
		size_t checkStr=memcmp (relayUOID, UOIDVector[i].UOID, 20 );

		if (checkStr==0)
		{
		 uoidFoundFlag = true;
		 relayPort = UOIDVector[i].recvPort;
		}
	}
	pthread_mutex_unlock(&vectormutex);

	//Relays back the Join Response to the node who requested
	if(uoidFoundFlag == true)              // UOID not already present
	{
		uint8_t r_joinTTL = ARH1.nw_TTL - 1;

		if (r_joinTTL > 0)
		{
		   CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_joinTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, relayPort);
		}
	}	//Else UOID not found
}
//End of HandleJoin_RSP()

//Description:	Handles incoming status request. This function sends back the response and puts the request
//				onto queue for flooding.
//Start of HandleStatus_REQ()
void HandleStatus_REQ(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer)
{
	bool uoidFoundFlag = false;

    uint8_t rStatusType;
	
	memcpy(&rStatusType, &readBuffer[0], 1);

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s%02x","0x", rStatusType);

	//Writes into log file
	reportLog("r", "STRQ", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort, false);

	uoidFoundFlag = pushUOIDStruture(myCTBPos,ARH1); 

	if(uoidFoundFlag == false)                      // UOID not present
	{
		uint8_t r_joinTTL=ARH1.nw_TTL - 1;

		uint8_t statusType=0;						//############# CHANGE FOR PART 2 #############
		memcpy(&statusType, &readBuffer[0], 1);		//######CALL DIFF FUNCTION IF STATUS TYPE IS FILES ##########

		//Send Status Response
		statusResponse(socDesc,&(ARH1.UOID[0]), ctb[myCTBPos].ctbPort);

		//Inserts the request into queue for flooding
		if (r_joinTTL > 0)
		{
			CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_joinTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, 65000  );
		}
	}	//Else message already seen
}
//End of HandleStatus_REQ()

//Description:	Handles incoming status response. This function processes the response by putting the node
//				information into the node vector if the this node is the orinator of the request otherwise
//				puts the request onto queue to relay back the response to the node who sent the request.
//Start of HandleStatus_RSP()
void HandleStatus_RSP(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer)
{

	LStruct tempLStruct;
	bool uoidFoundFlag = false;
	uint16_t relayPort;

	char *relayUOID = (char*)malloc(21);
	memset(relayUOID, '\0', 21);
	memcpy(relayUOID, &readBuffer[0], 20);

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	char *logData = (char*)malloc((int)LOGDATALEN);

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &relayUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);

	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	//Writes into log file
	reportLog("r", "STRS", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort, false);

	size_t newcheckStr=memcmp (relayUOID, StatusGlobalUOID, 20 );

	if(newcheckStr == 0)		// ##### MESSAGE originated from me....
	{
		if (StatusFlag == false)				// status neighbors
		{
		// code for putting into global structure.....
		int dataBufferSize = ARH1.nw_datalen;

		uint16_t rPort;
		memcpy(&rPort, &readBuffer[22], 2);

		pthread_mutex_lock(&linkvectormutex);
			NodeVector.push_back(rPort);

		int statDataSize = dataBufferSize  - 37;
		int loopCount = statDataSize / 19;
		
		uint16_t dPort;
		int z=0;
		for (int i = 0; i < loopCount ; i++)
		{
			memcpy(&dPort, &readBuffer[37+z+4], 2);
			tempLStruct.source = rPort;
			tempLStruct.destination = dPort;
			LinkVector.push_back(tempLStruct);			
			z=z+19;
		}	

		pthread_mutex_unlock(&linkvectormutex);
		}
		else                    // status files
		{
			// code for putting into global structure.....
			int dataBufferSize = ARH1.nw_datalen - 37;

			uint16_t rPort;
			memcpy(&rPort, &readBuffer[22], 2);
			
			string outstring;
			std::ostringstream rawStr;
			rawStr << rPort;
			std::string convStr = rawStr.str();

			char *actualStatData = (char*)malloc(dataBufferSize+1);
			memset(actualStatData, '\0', dataBufferSize+1);
			memcpy(&actualStatData[0], &readBuffer[37], dataBufferSize);
		
			uint32_t recLen;

		   if (dataBufferSize != 0)
			{memcpy(&recLen, &actualStatData[0], 4);}


			if (dataBufferSize == 0)
			{
				pthread_mutex_lock(&statusFilemutex);
				 outstring="nunki.usc.edu:" + convStr + " has no file\n\n";
				 FileWrite(statusExtFile, (char*)outstring.c_str(), 3);
				pthread_mutex_unlock(&statusFilemutex);
			}else if ( recLen==0 )
			{  
				pthread_mutex_lock(&statusFilemutex); 
				  outstring="nunki.usc.edu:" + convStr + " has following file\n";
				  FileWrite(statusExtFile, (char*)outstring.c_str(), 3);

 				  char *metaFile = (char*)malloc((dataBufferSize-4)+1);
				  memset(metaFile, '\0', (dataBufferSize-4)+1);
				  memcpy(&metaFile[0], &actualStatData[4], (dataBufferSize-4));
				  string strMetaFile(metaFile);
				  writeStatusMetadata(strMetaFile);
				  
				  outstring="\n";
				  FileWrite(statusExtFile, (char*)outstring.c_str(), 3);			
				pthread_mutex_unlock(&statusFilemutex); 

			}else
			{
				pthread_mutex_lock(&statusFilemutex); 
				outstring="nunki.usc.edu:" + convStr + " has following files\n";
				FileWrite(statusExtFile, (char*)outstring.c_str(), 3);
				int g = 0;
				while (1)
				{
					char *metaFile = (char*)malloc(recLen+1);
					memset(metaFile, '\0', recLen+1);
					memcpy(&metaFile[0], &actualStatData[g+4], recLen);
					string strMetaFile(metaFile);
					writeStatusMetadata(strMetaFile);
					dataBufferSize = dataBufferSize - (recLen+4);
					g = g + 4 + recLen;
					memcpy(&recLen, &actualStatData[g], 4);
					if (recLen == 0)
					{
						metaFile = (char*)malloc((dataBufferSize-4)+1);
						memset(metaFile, '\0', (dataBufferSize-4)+1);
						memcpy(&metaFile[0], &actualStatData[g+4], (dataBufferSize-4));
						string strMetaFile(metaFile);
						writeStatusMetadata(strMetaFile);
						break;
					}
				}
				
				outstring="\n";
				FileWrite(statusExtFile, (char*)outstring.c_str(), 3);			
				pthread_mutex_unlock(&statusFilemutex); 
			}
  	      	
		}
	}
	else
	{
		pthread_mutex_lock(&vectormutex);

		for (int i = 0; i < (int)UOIDVector.size(); i++)
		{
			size_t checkStr=memcmp (relayUOID, UOIDVector[i].UOID, 20 );			
			if (checkStr == 0)
			{
			 uoidFoundFlag = true;
			 relayPort = UOIDVector[i].recvPort;
			}			
		}
		pthread_mutex_unlock(&vectormutex);

		if(uoidFoundFlag == true)                  // UOID not present
		{
			uint8_t r_joinTTL = ARH1.nw_TTL - 1;
			if (r_joinTTL > 0)
			{
			   CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_joinTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, relayPort);
			}
		}	//Else UOID not found
	 }
}
//End of HandleStatus_RSP()

//Description: Generates the check message and puts it into common queue structure
//Start of SendCheckMessage()
void SendCheckMessage()
{
	//Sends Check Message
	int dataLength = 0;
	char *checkHeader = (char*)malloc(COMMON_HEADER_SIZE);
	checkHeader = GetHeader(node_inst_id, CKRQ, (uint8_t)init.TTL, dataLength);

	char *writeBufferUOID = (char*)malloc(21);
	memset(writeBufferUOID, '\0', 21);
	memcpy(&writeBufferUOID[0], &checkHeader[1], 20);

   	UStruct tempUOIDStruct;
    
	tempUOIDStruct.UOID=(char*)malloc(21); 
	memset(tempUOIDStruct.UOID, '\0', 21);
	memcpy(tempUOIDStruct.UOID, writeBufferUOID,20);

	tempUOIDStruct.recvPort = (uint16_t)init.Port;
	tempUOIDStruct.msgLifeTime = init.MsgLifetime;

	if (init.NoCheck == 0)						//Check for disabling check message
	{
		pthread_mutex_lock(&vectormutex);
		UOIDVector.push_back(tempUOIDStruct);
	    pthread_mutex_unlock(&vectormutex);

		pthread_mutex_lock(&timermutex);
		Timer.Tm_CheckJoinTimeout=init.JoinTimeout;
		pthread_mutex_unlock(&timermutex);

		//init.Port is used because this node should get the check response and 6th argument value is dummy
		//since check message not have data part
		CommonData_put(CKRQ, &(writeBufferUOID[0]), init.TTL,  dataLength, (uint16_t)init.Port, writeBufferUOID, 65000);
	}
}
//End of SendCheckMessage()

//Description: Handles the notify request, if this node is non-beacon then it sends check message
//Start of HandleNotify_REQ()
void HandleNotify_REQ(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer)
{
	uint8_t errCode=0;
	memcpy(&errCode, &readBuffer[0], 1);

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%d", errCode);

	//Writes into log file
	reportLog("r", "NTFY", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort, false);

	if (MeBeacon == false)
    {
	  	//Send check message
		SendCheckMessage();
	}

	//Closing Read thread
	ctb[myCTBPos].nodeActiveFlag=false;
	close(ctb[myCTBPos].ctbSocDesc);
	pthread_exit(NULL);
}
//End of HandleNotify_REQ()

//Description:	Handles the check request by sending the check response(Only beacon node sends the check
//				response, non-beacon node floods the request
//Start of HandleCheck_REQ()
void HandleCheck_REQ(int socDesc,int myCTBPos,struct Header ARH1)
{
	bool uoidFoundFlag = false;

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	string dummyStr = " ";

	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s", dummyStr.c_str());

	//Writes into log file
	reportLog("r", "CKRQ", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort, false);

	uint8_t r_checkTTL=ARH1.nw_TTL - 1;

	//Checks whether the request has already been processes
	uoidFoundFlag = pushUOIDStruture(myCTBPos, ARH1);
	
	if(uoidFoundFlag == false)        // UOID not already present
	{
		if(MeBeacon == true) 
		{	
			//Generating Check Response
			int dataLength = 20;

			char *checkRespHeader = (char*)malloc(COMMON_HEADER_SIZE);
			checkRespHeader = GetHeader(node_inst_id, CKRS, (uint8_t)init.TTL, dataLength);

			int writeBufferSize = COMMON_HEADER_SIZE + dataLength;

			unsigned char *last4UOID = (unsigned char*)malloc(4);
			memset(last4UOID, '\0', 4);
			memcpy(last4UOID, &(ARH1.UOID[16]), 4);

			unsigned char *tMsgid = convertHexToChar(last4UOID);
			
			char *logData = (char*)malloc((int)LOGDATALEN);
			snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

			//Writes into log file
			reportLog("s", "CKRS", writeBufferSize, init.TTL, checkRespHeader, logData, ctb[myCTBPos].ctbPort, true);	

			char *writeBuffer=(char*)malloc(writeBufferSize);
			if (writeBuffer == NULL) 
			{ 
				fprintf(stderr, "malloc() failed\n");
				exit(1);
			}

			//Copying data in to an unsigned character buffer.
			memset(writeBuffer, '\0', writeBufferSize);
			memcpy(&writeBuffer[0], &checkRespHeader[0], COMMON_HEADER_SIZE);
			memcpy(&writeBuffer[27], &(ARH1.UOID[0]), dataLength);
			
			//Send Check Response
			if(write(socDesc, writeBuffer, writeBufferSize) != writeBufferSize) 
			{
				cout << "Write error... " << endl;
			}

		}
		else	//Else flood again
		{		
			//To create dummy argument
			char *writeBuffer = (char*)malloc(20);
			memset(writeBuffer, '\0', 20);

			if (r_checkTTL > 0)
			{
				CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_checkTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, writeBuffer, 65000);
			}
		}
	}		//Else message already seen
}
//End of HandleCheck_REQ()

//Description:	Handles the check response. If the originator of the request is this node then it processes the 
//				response further if not then it relays the response to the requestor
//Start of HandleCheck_RSP()
void HandleCheck_RSP(int socDesc,int myCTBPos,struct Header ARH1,char *readBuffer) 
{
	bool uoidFoundFlag = false;
	uint16_t relayPort;

	char *relayUOID = (char*)malloc(21);
	memset(relayUOID, '\0', 21);
	memcpy(relayUOID, &readBuffer[0], 20);

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	char *logData = (char*)malloc((int)LOGDATALEN);

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &relayUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	//Writes into log file
	reportLog("r", "CKRS", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort, false);
	
	pthread_mutex_lock(&vectormutex);

	for (int i = 0; i < (int)UOIDVector.size(); i++)
	{	
		size_t checkStr=memcmp (relayUOID, UOIDVector[i].UOID, 20 );			
		if(checkStr == 0)
		{
			 uoidFoundFlag = true;
			 relayPort = UOIDVector[i].recvPort;
		}		
	}
	pthread_mutex_unlock(&vectormutex);

	if(uoidFoundFlag == true)                                     // uoid not already present
	{
		if (relayPort == (uint16_t)init.Port)
		{
			pthread_mutex_lock(&timermutex);
				Timer.Tm_CheckJoinTimeout=-99;
			pthread_mutex_unlock(&timermutex);			
		}
		else
		{
			uint8_t r_checkRespTTL = ARH1.nw_TTL - 1;

			if (r_checkRespTTL > 0)
			{
				CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_checkRespTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, relayPort);
			}
		}
	}	//Else UOID not found
}
//End of HandleCheck_RSP()

//Description:	Handles the keepalive request.
//Start of HandleKalive_REQ()
void HandleKalive_REQ(int socDesc,int myCTBPos,struct Header ARH1) 
{
	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;
	string dummyStr = " ";

	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s", dummyStr.c_str());

	//Writes into log file
	reportLog("r", "KPAV", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort, false);
}

