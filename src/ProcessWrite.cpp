/*************************************************************************************************************/
/*												Project : CSCI551 Final Part 1 Project 						*/
/*												Filename: ProcessWrite.cpp									*/
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/
#include "CommonHeaderFile.h"
#include "Connection.h"
#include "MainPrg.h"
#include "DoHello.h"
#include "ParseFile.h"
#include "HandleRead.h"
#include "Store.h"
#include "StatusFile.h"
#include "Search.h"
#include "Delete.h"
#include "Get.h"

using namespace std;

char *StatusGlobalUOID;
bool StatusFlag;

string statusExtFile;
pthread_mutex_t statusFilemutex;


// Structure used to store the timers of all the timer threads
typedef struct TimerSt
{
	int Tm_CheckJoinTimeout;
	int Tm_KeepAliveTimeout;
	int Tm_AutoShutdown;
};

struct TimerSt Timer;
pthread_mutex_t timermutex;

pthread_mutex_t tpSyncMutex;
pthread_cond_t tpSyncCond;
bool tpSyncFlag;


//Description: This function generates pseudo random number.
//Start of Function InitRandom
void InitRandom()
{
	time_t localtime=(time_t)0;
	time(&localtime);
	srand48((long)localtime);
}
//End of Function InitRandom

//Description: This function writes given buffer to any file in the home directory.
//Start of Function FileWrite

void FileWrite(string filename, char* buffer, int destination)
{
	fstream fptr;
	string tmpfilename;

	switch(destination) 
	{
		case 1:
		{
			tmpfilename = init.HomeDir;
			tmpfilename = tmpfilename + "/" + filename;
			break;
		}
		case 2:
		{
			tmpfilename = init.HomeDir;
			tmpfilename = tmpfilename + "/files/" + filename;
			break;
		}
		case 3:
		{
			tmpfilename = tmpfilename + filename;
			break;
		}
		case 4:
		{
			tmpfilename = init.HomeDir;
			tmpfilename = tmpfilename + "/files/tmp/" + filename;
			break;
		}
	}

		fptr.open(tmpfilename.c_str(), ios::in | ios::out | ios::app | ios::binary );

		fptr << buffer;
   	    fptr.close();
}
//End of Function FileWrite


// Description : Function writes data files to the home directory
// Strart of DataFileWrite()
void DataFileWrite(string filename, char* buffer, int size, int type)
{
	fstream fptr;
	string tmpfilename= init.HomeDir;
	switch(type)
	{
		case 1:
		{
			tmpfilename = tmpfilename + "/files/" + filename;
			break;
		}
		case 2:
		{
			tmpfilename = tmpfilename + "/files/tmp/" + filename;
			break;
		}
		default: break;
	}
    fptr.open(tmpfilename.c_str(), ios::in | ios::out | ios::app | ios::binary );
	fptr.write(buffer,size);
    fptr.close();
}



// Description : Function gets the header to be sent along with outgoing messages
// Strart of GetfwdHeader()

char* GetfwdHeader(struct CommonData *ptr_getQ)
{
	uint8_t nw_msg_type=htons(ptr_getQ->msg_type);				// convert the header to network byte order
	uint8_t nw_TTL=htons(ptr_getQ->TTL);
	uint8_t nw_zero=htons(0);
	uint32_t nw_datalen=htonl(ptr_getQ->nw_datalen);

	int buf_sz = 27;

	char *msg_header=(char*)malloc(buf_sz);

	if (msg_header == NULL) 
	{ fprintf(stderr, "malloc() failed\n");  }

	memset(msg_header, 0, sizeof(msg_header));						// copy to buffer
	memcpy(msg_header, &(nw_msg_type), 1);
	memcpy(&msg_header[1], ptr_getQ->UOID, 20);
	memcpy(&msg_header[21], &(nw_TTL), 1);
	memcpy(&msg_header[22], &(nw_zero), 1);
	memcpy(&msg_header[23], &(nw_datalen), 4);

	return msg_header;							// retuen the header to caller
}


// handle pipe in the write message
void pipeHandler(int nsig)					//signal handler for SIGPIPE in child threads to catch errors while reading or writing 
{
    pthread_exit(0);			
}


// Description : Function forwards requets to outgoing threads and performs logging...
// Strart of Process_REQfwd()

void Process_REQfwd(int socDesc,struct CommonData *ptr_getQ,int myCTBPos)
{
	char *msg_header=GetfwdHeader(ptr_getQ);
	bool part2Flag=false;

	uint32_t writeBufferSize = COMMON_HEADER_SIZE + ptr_getQ->nw_datalen;

	//specially handle STOR and GTRS messages. Dont read all data from the network
	if (ptr_getQ->msg_type == STOR)
	{
			double randomNbrProb=drand48();

			if (randomNbrProb > init.NeighborStoreProb)
			{
				return;
			}

		part2Flag=true;

		string dummyStr = " ";
		char *logData = (char*)malloc((int)LOGDATALEN);
		snprintf(logData, (int)LOGDATALEN, "%s", dummyStr.c_str());

		if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
		{
			reportLog("s", "STOR", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
		}
		else
		{
			reportLog("f", "STOR", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
		}					

		sendStoreMessage(msg_header,ptr_getQ->Request, socDesc);
	}
	else if (ptr_getQ->msg_type == GTRS)
	{
		part2Flag=true;

		char *logData = (char*)malloc((int)LOGDATALEN);
		unsigned char *last4UOID = (unsigned char*)malloc(4);
		memset(last4UOID, '\0', 4);
		memcpy(last4UOID, &(ptr_getQ->Request[16]), 4);

		unsigned char *tMsgid = convertHexToChar(last4UOID);
		snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

		//Writes into log file
		reportLog("f", "GTRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);	

		sendGetResponseMessage(msg_header,ptr_getQ->Request, socDesc, ptr_getQ->nw_datalen);
	}


  if (part2Flag==false)
  {
  	
	char *writeBuffer=(char*)malloc(writeBufferSize);
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &msg_header[0], COMMON_HEADER_SIZE);

  
	if (ptr_getQ->msg_type != CKRQ && ptr_getQ->msg_type != KPAV)
	{
		memcpy(&writeBuffer[27], ptr_getQ->Request, ptr_getQ->nw_datalen);
	}

	//##################################LOGGING################################################
	switch (ptr_getQ->msg_type)
	{
		case JNRQ:											//Logging for JNRQ			
		{
			uint16_t rPort;
			memcpy(&rPort, &(ptr_getQ->Request[4]), 2);

			char *logData = (char*)malloc((int)LOGDATALEN);
			snprintf(logData, (int)LOGDATALEN, "%d %s", rPort, hName);

			if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
			{
				reportLog("s", "JNRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			else
			{
				reportLog("f", "JNRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			break;
		}
		case JNRS:											//Logging for JNRS
		{
			unsigned char *last4UOID = (unsigned char*)malloc(4);
			memset(last4UOID, '\0', 4);
			memcpy(last4UOID, &(ptr_getQ->Request[16]), 4);

			unsigned char *tMsgid = convertHexToChar(last4UOID);
			
			uint32_t distance;
			memcpy(&distance, &(ptr_getQ->Request[20]), 4);

			uint16_t wPort;
			memcpy(&wPort, &(ptr_getQ->Request[24]), 2);

			char *logData = (char*)malloc((int)LOGDATALEN);
			snprintf(logData, (int)LOGDATALEN, "%s %d %d %s", (char *)tMsgid, distance, wPort, hName);

			if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)					// send or fwd log entry
			{
				reportLog("s", "JNRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			else
			{
				reportLog("f", "JNRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			break;
		}
		case STRQ:													//Logging for STRQ
		{
			uint8_t rStatusType;
			memcpy(&rStatusType, &(ptr_getQ->Request[0]), 1);

			char *logData = (char*)malloc((int)LOGDATALEN);
			snprintf(logData, (int)LOGDATALEN, "%s%02x", "0x",rStatusType);
	
			if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
			{
				reportLog("s", "STRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
			}
			else
			{
				reportLog("f", "STRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
			}
			
			break;			 
		}
		case STRS:													//Logging for STRS
		{
				unsigned char *last4UOID = (unsigned char*)malloc(4);
				memset(last4UOID, '\0', 4);
				memcpy(last4UOID, &(ptr_getQ->Request[16]), 4);

				unsigned char *tMsgid = convertHexToChar(last4UOID);

				char *logData = (char*)malloc((int)LOGDATALEN);
				snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

				if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
				{
					reportLog("s", "STRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}
				else
				{
					reportLog("f", "STRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}		
			break;			 
		}
		case NTFY:														//Logging for NTFY
		{
				uint8_t errCode=0;
				memcpy(&errCode, &(ptr_getQ->Request[0]), 1);

				char *logData = (char*)malloc((int)LOGDATALEN);
				snprintf(logData, (int)LOGDATALEN, "%d", errCode);

				if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
				{
					reportLog("s", "NTFY", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}
				else
				{
					reportLog("f", "NTFY", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}			
			break;			 
		}
		case CKRQ:													////Logging for CKRQ
		{
				string dummyStr = " ";
				char *logData = (char*)malloc((int)LOGDATALEN);
				snprintf(logData, (int)LOGDATALEN, "%s", dummyStr.c_str());

				if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
				{
					reportLog("s", "CKRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}
				else
				{
					reportLog("f", "CKRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}					
			break;			 
		}
		case CKRS:													//Logging for CKRS
		{
				unsigned char *last4UOID = (unsigned char*)malloc(4);
				memset(last4UOID, '\0', 4);
				memcpy(last4UOID, &(ptr_getQ->Request[16]), 4);

				unsigned char *tMsgid = convertHexToChar(last4UOID);

				char *logData = (char*)malloc((int)LOGDATALEN);
				snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

				if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
				{
					reportLog("s", "CKRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}
				else
				{
					reportLog("f", "CKRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}								
			break;			 
		}
		case KPAV:													//Logging for KPAV
		{
				string dummyStr = " ";
				char *logData = (char*)malloc((int)LOGDATALEN);
				snprintf(logData, ((int)LOGDATALEN), "%s", dummyStr.c_str());

				if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
				{
					//printf("Port No. IS : %d\n",ctb[myCTBPos].ctbPort);
					reportLog("s", "KPAV", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}
				else
				{
					reportLog("f", "KPAV", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort, false);
				}												
			break;			 
		}
		case SHRQ:											//Logging for SHRQ		
		{
			uint8_t sType;
			memcpy(&sType, &(ptr_getQ->Request[0]), 1);

			char *query = (char*)malloc(ptr_getQ->nw_datalen);
			memset(query, '\0', ptr_getQ->nw_datalen);
			memcpy(query, &(ptr_getQ->Request[1]), ptr_getQ->nw_datalen-1);

			char *logData = (char*)malloc((int)LOGDATALEN);
			snprintf(logData, (int)LOGDATALEN, "%d %s", sType, &query[0]);

			if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
			{
				reportLog("s", "SHRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			else
			{
				reportLog("f", "SHRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			break;
		}
		case SHRS:											//Logging for SHRS
		{
			unsigned char *last4UOID = (unsigned char*)malloc(4);
			memset(last4UOID, '\0', 4);
			memcpy(last4UOID, &(ptr_getQ->Request[16]), 4);

			unsigned char *tMsgid = convertHexToChar(last4UOID);
			
			char *logData = (char*)malloc((int)LOGDATALEN);
			snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

			if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)					// send or fwd log entry
			{
				reportLog("s", "SHRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			else
			{
				reportLog("f", "SHRS", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			break;
		}
		case GTRQ:											//Logging for GTRQ
		{

			char *logData = (char*)malloc((int)LOGDATALEN);

			unsigned char *last4UOID = (unsigned char*)malloc(4);
			memset(last4UOID, '\0', 4);
			memcpy(last4UOID, &(ptr_getQ->Request[16]), 4);

			unsigned char *tMsgid = convertHexToChar(last4UOID);
			snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

			if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
			{
				reportLog("s", "GTRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			else
			{
				reportLog("f", "GTRQ", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			break;
		}
		case DELT:											//Logging for DELT
		{
			string dummyStr = " ";
			char *logData = (char*)malloc((int)LOGDATALEN);
			snprintf(logData, ((int)LOGDATALEN), "%s", dummyStr.c_str());

			if (ptr_getQ->ReceivingPort == (uint16_t)init.Port)
			{
				reportLog("s", "DELT", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			else
			{
				reportLog("f", "DELT", writeBufferSize, ptr_getQ->TTL, ptr_getQ->UOID, logData, ctb[myCTBPos].ctbPort,false);
			}
			break;
		}
	   default: break;	
	}

	if (shutdownFlag == true)									// exit if shudoen flag is set
    {
		pthread_exit(0);
	}

	if(write(socDesc, writeBuffer, writeBufferSize) != (long)writeBufferSize) 
	{
		signal(SIGPIPE, pipeHandler);
	}

    if (ptr_getQ->msg_type == NTFY)								// exit thread is message sent is NTFY
    {
		pthread_exit(0);
    }

  }

}

// Description : Function to used to do status of files
// Strart of callStatus_Files()

void callStatus_Files(string readBuffer)
{
//	int oldState=0;
	StatusFlag=true;

	TrimSpaces(readBuffer);															// remove spaces

	size_t found=readBuffer.find(" ");
	if (found!=string::npos)
	{
		string strTTL=readBuffer.substr(0,found);
		statusExtFile=readBuffer.substr(found+1);

		TrimSpaces(statusExtFile);														// remove spaces

		int TTL=atoi(strTTL.c_str());

		//Check for file if already exists, if yes deleting it...
		struct stat stFileInfo;
		int intStat = stat(statusExtFile.c_str(), &stFileInfo);

		if(intStat == 0)
		{
			remove(statusExtFile.c_str());
		}

		string outstring;
		std::ostringstream rawStr;
		rawStr << init.Port;
		std::string convStr = rawStr.str();

		//first write out files owned by me
		pthread_mutex_lock(&storemutex);
		
		if ((int)name_ind_vector.size() == 0 )
		 {
			outstring="nunki.usc.edu:" + convStr + " has no file\n";
		 }
		else if ((int)name_ind_vector.size() == 1 )
		 {
			outstring="nunki.usc.edu:" + convStr + " has the following file\n";
		 }
		 else if ((int)name_ind_vector.size() > 1)
		 {
			 outstring="nunki.usc.edu:" + convStr + " has the following files\n";
		 }

        FileWrite(statusExtFile, (char*)outstring.c_str(), 3);

		for (int i=0; i< (int)name_ind_vector.size()  ; i++ )
		{
				std::ostringstream fileStr;
			    fileStr << name_ind_vector[i].fileCount;
			    std::string fileconvStr = fileStr.str();

			string storeMetaFilename (fileconvStr);
			storeMetaFilename = storeMetaFilename + ".meta";

			string metaFile(init.HomeDir);
			metaFile = metaFile + "/files/" + storeMetaFilename;
			writeStatusMetadata(metaFile);
		}

		pthread_mutex_unlock(&storemutex);

		outstring="\n";
		FileWrite(statusExtFile, (char*)outstring.c_str(), 3);		

		
		if( TTL > 0)																// send out message only if TTL > 0
		{
			//SEND STATUS MESSAGE
			int dataLength = 1;

			char *statusHeader = (char*)malloc(COMMON_HEADER_SIZE);
			statusHeader = GetHeader(node_inst_id, STRQ, (uint8_t)TTL, dataLength);

			uint8_t wStatus = 0x02;

			char *putBuffer=(char *)malloc(1);
			memset(putBuffer, '\0', 1);
			memcpy(&putBuffer[0], &wStatus, 1);

			memcpy(&StatusGlobalUOID[0],&statusHeader[1],20);									// copy to global UOID to decide originator of status message

			//pthread_testcancel();

			//pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldState);
            CommonData_put(STRQ, &(StatusGlobalUOID[0]), TTL,  dataLength, uint16_t(init.Port), putBuffer, 65000  );
			//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);

			//pthread_testcancel();
			sleep(init.MsgLifetime);

		}																					// send message only if TTL > 0
         StatusFlag=false;
	 	 memset(StatusGlobalUOID,'\0',21);
		//pthread_testcancel();
		//pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldState);


		//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);
	} // correct message end

}

// Description : Function sends status request and waits for responses. Interruped by ctrl +c
// Strart of callStatus_Neighbors()

void callStatus_Neigbors(string readBuffer)
{
	int oldState=0;
	StatusFlag=false;
	fstream statusFile;
	string extFile;

	LinkVector.clear();
	NodeVector.clear();

	vector<uint16_t> CalcTempVector;

	TrimSpaces(readBuffer);															// remove spaces

	size_t found=readBuffer.find(" ");
	if (found!=string::npos)
	{
		string strTTL=readBuffer.substr(0,found);
		extFile=readBuffer.substr(found+1);

		TrimSpaces(extFile);														// remove spaces

		int TTL=atoi(strTTL.c_str());
		
		if( TTL > 0)																// send out message only if TTL > 0
		{
			//SEND STATUS MESSAGE
			int dataLength = 1;

			char *statusHeader = (char*)malloc(COMMON_HEADER_SIZE);
			statusHeader = GetHeader(node_inst_id, STRQ, (uint8_t)TTL, dataLength);

			uint8_t wStatus = 0x01;

			char *putBuffer=(char *)malloc(1);
			memset(putBuffer, '\0', 1);
			memcpy(&putBuffer[0], &wStatus, 1);

			memcpy(&StatusGlobalUOID[0],&statusHeader[1],20);									// copy to global UOID to decide originator of status message

			pthread_testcancel();

			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldState);
            CommonData_put(STRQ, &(StatusGlobalUOID[0]), TTL,  dataLength, uint16_t(init.Port), putBuffer, 65000  );
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);

			pthread_testcancel();
			sleep(init.MsgLifetime);

		}																					// send message only if TTL > 0

		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldState);

		//Check for file if already exists, if yes deleting it...
		struct stat stFileInfo;
		int intStat = stat(extFile.c_str(), &stFileInfo);

		if(intStat == 0)
		{
			remove(extFile.c_str());
		}

		// create a new status file
		statusFile.open(extFile.c_str(), fstream::in | fstream::out | fstream::app);

		statusFile << "V -t * -v 1.0a5" << endl;
		statusFile << "n -t * -s " << init.Port << " -c red -i black" << endl;

		pthread_mutex_lock(&linkvectormutex);


		bool NVfoundFlag = false;

		// write all the responses to the output file...
		// first write nodes and then links...
		for (int i=0;i< (int)LinkVector.size();i++ )
		{
			NVfoundFlag = false;
			for (int j=0;j< (int)NodeVector.size();j++ )
			{
				if (NodeVector[j] == LinkVector[i].destination)
				{
					NVfoundFlag = true;
					break;
				}
			}
				if(NVfoundFlag == false && LinkVector[i].destination != (uint16_t)init.Port ) 
				{
				 CalcTempVector.push_back(LinkVector[i].destination);				 
				}
		}

		 // remove duplicates and write to file
		for (int i=0;i<(int)CalcTempVector.size() ;i++ )
		{
			NodeVector.push_back(CalcTempVector[i]);
		}

	  std::sort(NodeVector.begin(), NodeVector.end());
      NodeVector.erase(std::unique(NodeVector.begin(), NodeVector.end()), NodeVector.end());

		// write file
		for (int i = 0; i < (int)NodeVector.size(); i++)
   	    {
			if(NodeVector[i] != (uint16_t)init.Port) 
			{	
				statusFile << "n -t * -s " << NodeVector[i] << " -c red -i black" << endl;
			}
		}

		for (int i = 0; i < (int)LinkVector.size(); i++)
   	    {
			statusFile << "l -t * -s " << LinkVector[i].source << " -d "<< LinkVector[i].destination <<" -c blue" << endl;
		}
		pthread_mutex_unlock(&linkvectormutex);

		CalcTempVector.clear();
		statusFile.close();  

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);
	} // correct message end
}


// Description : Function handles shutdown request before sending out notify message
// Strart of callShutdown()

void callShutdown()
{
	int oldState=0;
		//SEND NOTIFY MESSAGE
		int dataLength = 1;

		char *notifyHeader = (char*)malloc(COMMON_HEADER_SIZE);
		notifyHeader = GetHeader(node_inst_id, NTFY, 1, dataLength);

		uint8_t errCode = 1;		//1 = User Shutdown
		
		char *notifyUOID=(char *)malloc(20);
		memset(notifyUOID, '\0', 20);
		memcpy(&notifyUOID[0], &notifyHeader[1], 20);

		char *putBuffer=(char *)malloc(1);
		memset(putBuffer, '\0', 1);
		memcpy(&putBuffer[0], &errCode, 1);

		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldState);
		
		// put notify request in the queue and signal main thread for shutdown
		CommonData_put(NTFY, &(notifyUOID[0]), 1,  dataLength, uint16_t(init.Port), putBuffer, 65000);		

    	pthread_mutex_lock(&shutdownmutex);

		selfRestartFlag = false;

		pthread_cond_signal(&shutdownCondWait);

		pthread_mutex_unlock(&shutdownmutex);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);

}

// interupt handler for ctrl + c
void interruptHandle(int nSig)
{
   printf("\n");
}


// Description : Function reads request from user and calls appropriate functions...
// Strart of CommandHandle()

void *CommandHandle(void *null)
{
	char *buffer;
    size_t found;
	StatusGlobalUOID=(char*)malloc(21);
	memset(StatusGlobalUOID,'\0',21);

	act.sa_handler = interruptHandle;
	sigaction(SIGINT, &act, NULL);
	pthread_sigmask(SIG_UNBLOCK, &newSet, NULL);

	// add handler for ctrl +c 

	while(shutdownFlag == false)
	{	
		pthread_testcancel();

		StatusFlag=false;						// for timeout of status ----
		searchFlag=false;
	    serachResultCount=1;


        memset(StatusGlobalUOID,'\0',21);
		buffer=(char*)malloc(1000);
		found=string::npos;
		memset(buffer,'\0',1000);

		pthread_testcancel();
		printf("servant:%d> ",init.Port );
		pthread_testcancel();

		gets(buffer);							// get input from user	

		buffer[strlen(buffer)]='\0';
		      
		string readBuffer (buffer);

		found=readBuffer.find("shutdown");
	    if (found!=string::npos)
	    {
			Result_vector.clear();
			pthread_testcancel();
			callShutdown();
			break;			//Added 10/30/2009
		}
		
		// determine if the input request is status ot shutdown
		found=readBuffer.find("status");
	    if (found!=string::npos)
	    {
			Result_vector.clear();
			found=readBuffer.find("neighbors");
			if (found!=string::npos)
			{
				pthread_testcancel();
				callStatus_Neigbors(readBuffer.substr(found+10));
			}

			found=readBuffer.find("files");
			if (found!=string::npos)
			{
				pthread_testcancel();
				callStatus_Files(readBuffer.substr(found+6));
			}
		}

		// determine if the input request is status ot shutdown
		found=readBuffer.find("store");
	    if (found!=string::npos)
	    {
				Result_vector.clear();
				pthread_testcancel();
				callStore_Files(readBuffer.substr(found+6));
		}

		found=readBuffer.find("search");
	    if (found!=string::npos)
	    {
				pthread_testcancel();
				callSearch(readBuffer.substr(found+7));
		}

		found=readBuffer.find("get");
	    if (found!=string::npos)
	    {
				pthread_testcancel();
				callGet(readBuffer.substr(found+4));
		}

		found=readBuffer.find("delete");
	    if (found!=string::npos)
	    {
				Result_vector.clear();
				pthread_testcancel();
				callDelete(readBuffer.substr(found+7));
		}


		free(buffer);
	}

   pthread_exit(0);
}


// Description : Function sleeps for 1 second and generates signals for timerAction thread
// Strart of timerCount()

void *timerCount(void *null)
{
	int oldState=0;
    // initialize all timeouts to their default value
	Timer.Tm_CheckJoinTimeout=-99;	
	Timer.Tm_AutoShutdown=init.AutoShutdown;
	Timer.Tm_KeepAliveTimeout=init.KeepAliveTimeout;

	while (true)
	{
	    pthread_testcancel();
		sleep(1);  
	    pthread_testcancel();

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldState);

		pthread_mutex_lock(&tpSyncMutex);

		tpSyncFlag=true;
		pthread_cond_signal(&tpSyncCond);

		pthread_mutex_unlock(&tpSyncMutex);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);
	}
}


// Description : Function takes actions based on timing related issues like autoshutdown, check etc.
// Strart of timerAction()

void *timerAction(void *null)
{
	int oldState=0;

	while (true)
	{
		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldState);


		pthread_mutex_lock(&tpSyncMutex);
		if(tpSyncFlag==false)
		{
			pthread_cond_wait(&tpSyncCond, &tpSyncMutex);
		}
		pthread_mutex_unlock(&tpSyncMutex);

        // decrement all shared Timer variables

		pthread_mutex_lock(&timermutex);
			Timer.Tm_KeepAliveTimeout--;
			Timer.Tm_AutoShutdown--;

			if (Timer.Tm_CheckJoinTimeout > 0)									// DEFAULT VALUE IS -99
			{
				Timer.Tm_CheckJoinTimeout--;
			}
		pthread_mutex_unlock(&timermutex);


		// #################AUTO SHUTDOWN##################################
		if (Timer.Tm_AutoShutdown == 0 && shutdownFlag == false)
		{
		    	pthread_mutex_lock(&shutdownmutex);
				
				selfRestartFlag = false;

				pthread_cond_signal(&shutdownCondWait);
				
				pthread_mutex_unlock(&shutdownmutex);
		}

		if(Timer.Tm_KeepAliveTimeout == (init.KeepAliveTimeout/2) && shutdownFlag == false)
		{
			//SEND KEEPALIVE MESSAGE
			int dataLength = 0;

			char *kaliveHeader = (char*)malloc(COMMON_HEADER_SIZE);
			kaliveHeader = GetHeader(node_inst_id, KPAV, 1, dataLength);

			char *kaliveUOID=(char *)malloc(20);
			memset(kaliveUOID, '\0', 20);
			memcpy(&kaliveUOID[0], &kaliveHeader[1], 20);

			//To create dummy argument
			char *writeBuffer = (char*)malloc(20);
			memset(writeBuffer, '\0', 20);
			
			CommonData_put(KPAV, &(kaliveUOID[0]), 1,  dataLength, uint16_t(init.Port), writeBuffer, 65000);

			Timer.Tm_KeepAliveTimeout = init.KeepAliveTimeout;

		}


		// #################CHECK FAILED.....SELF RESTART##################################
		pthread_mutex_lock(&timermutex);

			if (Timer.Tm_CheckJoinTimeout == 0 && shutdownFlag == false)
			{
				// Deleting the init_neighbor_list file

				string iniFilename(init.HomeDir);
   				iniFilename=iniFilename+"/init_neighbor_list";
				remove(iniFilename.c_str());

				//SEND NOTIFY MESSAGE
				int dataLength = 1;

				char *notifyHeader = (char*)malloc(COMMON_HEADER_SIZE);
				notifyHeader = GetHeader(node_inst_id, NTFY, 1, dataLength);

				uint8_t errCode = 3;		//1 = User Shutdown
				
				char *notifyUOID=(char *)malloc(20);
				memset(notifyUOID, '\0', 20);
				memcpy(&notifyUOID[0], &notifyHeader[1], 20);

				char *putBuffer=(char *)malloc(1);
				memset(putBuffer, '\0', 1);
				memcpy(&putBuffer[0], &errCode, 1);

				// send out notify request since check failed

				CommonData_put(NTFY, &(notifyUOID[0]), 1,  dataLength, uint16_t(init.Port), putBuffer, 65000);

				printMessage("Check Failed...Performing self restart...");

					pthread_mutex_lock(&shutdownmutex);
						// CODE FOR SELF RESTART
					selfRestartFlag = true;

					pthread_cond_signal(&shutdownCondWait);

					pthread_mutex_unlock(&shutdownmutex);

			}

		pthread_mutex_unlock(&timermutex);
       
	   if (shutdownFlag == false)
	   {
	   
		// ################### KEEPALIVE AND CHECK FOR CONNECTED NODES###################
		pthread_mutex_lock(&tiebreakermutex);

		bool checkFlag=false;
			
		//CONNECTION TIE-BREAKER
		for(int i = 0; i < connTieBreakerCount; i++) 
		{
			ctb[i].ctbKeepAliveTimeout--; 
			if(ctb[i].ctbKeepAliveTimeout == 0 && ctb[i].nodeActiveFlag == true) 
			{
				close(ctb[i].ctbSocDesc);
				ctb[i].nodeActiveFlag = false; 
				checkFlag=true;
			}
		}
		
		pthread_mutex_unlock(&tiebreakermutex);
	   
	 	// ############################ REMOVE MESSAGES FROM UOID VECTOR FOR EXPIRED LIFETIME######################

		pthread_mutex_lock(&vectormutex);
		
		for (int i = 0; i < (int)UOIDVector.size(); i++)
		{
			UOIDVector[i].msgLifeTime--;	
			if (UOIDVector[i].msgLifeTime == 0)
			{
				UOIDVector.erase(UOIDVector.begin()+i);
			}
			
		}
		pthread_mutex_unlock(&vectormutex);

	   }

		pthread_mutex_lock(&tpSyncMutex);
			tpSyncFlag=false;
		pthread_mutex_unlock(&tpSyncMutex);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);
	}
}

