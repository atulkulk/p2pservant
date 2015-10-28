/************************************************************************************************************/
/*												Project : CSCI551 Final Part 2 Project 						*/
/*												Filename: StatusFile.cpp (Server Implementation)			*/
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/
#include "CommonHeaderFile.h"
#include "ProcessWrite.h"
#include "DoHello.h"
#include "Store.h"
#include "ParseFile.h"
#include "HandleRead.h"
#include "Connection.h"


using namespace std;

// Desciption : This function writes the status response into output file.
// Start of writeStatusMetadata()
void writeStatusMetadata(string metaFile)
{
	//NOTE : MUST HAVE LOCK ON STORE MUTEX BEFORE CALING THIS FUNCTION
	fstream fp;
	long ReadBytes = 0;

	struct stat tmp_structure, *buf_stat;
	buf_stat=&tmp_structure;
	
	stat(metaFile.c_str(), buf_stat);					// Get filesize

	long file_datalen=buf_stat->st_size;

	fp.open(metaFile.c_str(), fstream::in);
	  
	while ( ReadBytes < file_datalen )							// read all the received bytes	
	{
		int target = file_datalen - ReadBytes;
		char *readData;    
		if(target > BUFFERSIZE)
		{
			readData = (char *)malloc(BUFFERSIZE+1);				//buffer to read 8192 bytes
			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");}

			memset(readData, '\0', BUFFERSIZE+1);  

			fp.read(&readData[0], BUFFERSIZE);

			FileWrite(statusExtFile, readData, 3);

			free(readData); 		 
		}
		else    //target
		{
			readData = (char *)malloc(target+1);					// buffer to read less than 512 bytes

			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");  }
			memset(readData, '\0', target+1);
			fp.read(&readData[0], target);

			FileWrite(statusExtFile, readData, 3);
			
			free(readData); 			 
		} //end else target
		ReadBytes=ReadBytes + BUFFERSIZE;
	}  //end while

	fp.close();

}

// Desciption : This function generated the status file response and writes response on to socket
// Start of statusFileResponse()
void statusFileResponse(int acceptReturn, char *sUOID, int port)
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

   char *staResp_Char=(char*)malloc(BUFFERSIZE);
   memset(staResp_Char,'\0',BUFFERSIZE);

   int staresp_size=0;

	pthread_mutex_lock(&storemutex);

		for (int i=0; i < (int)name_ind_vector.size()  ; i++ )
		{
			std::ostringstream fileStr;
			fileStr << name_ind_vector[i].fileCount;
			std::string fileconvStr = fileStr.str();

			string storeMetaFilename (fileconvStr);
			storeMetaFilename = storeMetaFilename + ".meta";

			string metaFile(init.HomeDir);
			metaFile = metaFile + "/files/" + storeMetaFilename;

			uint32_t metaLen = strlen(metaFile.c_str());
			
			if(i != ((int)name_ind_vector.size()-1)) 
			{				
			  memcpy(&staResp_Char[staresp_size], &metaLen, 4);
              memcpy(&staResp_Char[staresp_size+4], &metaFile[0],metaLen );
             
			  staresp_size= staresp_size+ 4+ metaLen;

			}
			else
			{
				uint32_t tmpmetaLen = 0;

			  memcpy(&staResp_Char[staresp_size], &tmpmetaLen, 4);
              memcpy(&staResp_Char[staresp_size+4], &metaFile[0],metaLen );

  		      staresp_size= staresp_size+ 4+ metaLen;

			}

		}


	pthread_mutex_unlock(&storemutex);

	int dataLength = dataBufferSize + staresp_size;
	
	//Creation of status response - log
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

    // write actual data - header first...
	if(write(acceptReturn, statusHeader, COMMON_HEADER_SIZE) != COMMON_HEADER_SIZE) 
	{
		printMessage("**Write error... ");
	}

    char *writeBuffer = (char*)malloc(dataLength);
	memcpy(writeBuffer,dataBuffer,dataBufferSize);
	if (staresp_size>0)
	{
	  memcpy(&writeBuffer[dataBufferSize],staResp_Char,staresp_size);
	}

	// write actual data - common data...
	if(write(acceptReturn, writeBuffer, dataLength) != dataLength) 
	{
		printMessage("**Write error... ");
	}

    free(statusHeader);
	free(dataBuffer);
    free(writeBuffer);
}

// Desciption : This function handles status file request by flooding the request onto the network.
// Start of HandleStatusFiles_REQ()
void HandleStatusFiles_REQ(int socDesc, int myCTBPos, struct Header ARH1, char *readBuffer)
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
		statusFileResponse(socDesc,&(ARH1.UOID[0]), ctb[myCTBPos].ctbPort);

		//Inserts the request into queue for flooding
		if (r_joinTTL > 0)
		{
			CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_joinTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, 65000  );
		}
	}	//Else message already seen
}

//End of StatusFile.cpp


