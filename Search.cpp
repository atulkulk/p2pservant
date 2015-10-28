/*************************************************************************************************************/
/*												Project : CSCI551 Final Part 2 Project 						*/
/*												Filename: Search.cpp (Server Implementation)				*/
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/
#include "CommonHeaderFile.h"
#include "Store.h"
#include "ParseFile.h"
#include "ProcessWrite.h"
#include "HandleRead.h"
#include "DoHello.h"
#include "Connection.h"
#include "MainPrg.h"

using namespace std;

//Declaration of variables
bool searchFlag;
int serachResultCount;
pthread_mutex_t searchmutex;

typedef struct ResultStructure	
{
	int resultNumber;
	unsigned char *fileID;
	unsigned char *SHA1;
}ResultStruct;

vector<ResultStruct> Result_vector;

// Desciption : Function is used to write the search result on screen
// Start of writeSearchResult()
void writeSearchResult(string metaName, unsigned char* fileID)
{
	cout<<"["<<serachResultCount<<"]";
	string space;
	if (serachResultCount < 9)
	{
		space="  ";
	}
	else
	{
		space=" ";
	}

	cout<<space<<"FileID=";
	for (int j=0;j<20;j++)
	{
	  printf("%02x",(unsigned char)fileID[j]);
	}
	printf("\n");


	string line;
	fstream fpSend;
	unsigned char* hexsh;
	fpSend.open(metaName.c_str(), fstream::in);
	
	while (! fpSend.eof() )
	{
	  getline(fpSend,line);
	  size_t found1=line.find("metadata");		
	  size_t found2=line.find("Bit-vector");	

	  if (found1==string::npos && found2==string::npos)
	  {
		  cout<<"   "<<space<<line<<endl;
	  }
	  size_t found3=line.find("SHA1");
	  if (found3!=string::npos)
	  {
		string sh = line.substr(5);
		hexsh = CharToHexadec((unsigned char*)sh.c_str(),40);
	  }

	}

	fpSend.close();

	ResultStruct tmpResultStruct;
	tmpResultStruct.resultNumber = serachResultCount;

	tmpResultStruct.fileID=(unsigned char*)malloc(21);
	memset(tmpResultStruct.fileID,'\0',21);
	memcpy(&(tmpResultStruct.fileID[0]),&fileID[0],20);

	tmpResultStruct.SHA1=(unsigned char*)malloc(21);
	memset(tmpResultStruct.SHA1,'\0',21);
	memcpy(&(tmpResultStruct.SHA1[0]),&hexsh[0],20);

	Result_vector.push_back(tmpResultStruct);

	serachResultCount = serachResultCount +1;
}

// Desciption : This function sends this node's search response based on filename as well as floods the search request
// Start of FileName_Search()
bool FileName_Search(string querydata)
{
   bool waitFlag=false;
   TrimSpaces(querydata);

	char *charQuery=(char*)malloc(strlen(querydata.c_str())+1);
    memset(charQuery,'\0',strlen(querydata.c_str())+1);
	memcpy(charQuery,&querydata[0],strlen(querydata.c_str()));
   		
	pthread_mutex_lock(&storemutex);
	 for (int i=0;i< (int)name_ind_vector.size(); i++ )
	 {
		   string tmpLowerName((char*)name_ind_vector[i].value);
		   convert_lower(&tmpLowerName[0]);
			size_t checkStr=memcmp (&tmpLowerName[0], charQuery, strlen(querydata.c_str()) );
			if (checkStr==0)
			{
					std::ostringstream rawStr;
					rawStr << name_ind_vector[i].fileCount;
					std::string convStr = rawStr.str();

					string storeMetaFilename (convStr);
					storeMetaFilename = storeMetaFilename + ".meta"; 

					string sMetaFile(init.HomeDir);
					sMetaFile = sMetaFile + "/files/" + storeMetaFilename;
					
					int filePos=0;

					for (int j=0;j<(int)FileID_vector.size() ; j++)
					{
						if (FileID_vector[j].fileCount == name_ind_vector[i].fileCount)
						{
							filePos=j;
							break;
						}
					}				
								
					writeSearchResult(sMetaFile,FileID_vector[filePos].value);				
			}
		}

 	 pthread_mutex_unlock(&storemutex);

	 if (init.TTL > 0)
	 {
		// put search message on the queue
		uint8_t searchType = 1;
		
		uint32_t dataLength = 1 + strlen(charQuery);

		char *searchHeader = (char*)malloc(COMMON_HEADER_SIZE);
		searchHeader = GetHeader(node_inst_id, SHRQ, (uint8_t)init.TTL, dataLength);

		char *searchUOID = (char*)malloc(21);
		memset(searchUOID, '\0', 21);
		memcpy(&searchUOID[0],&searchHeader[1],20);	// copy to global UOID to decide originator of search message

		//Pushing UOID into UOID structure
		pthread_mutex_lock(&vectormutex);

			UStruct tempUOIDStruct;
			tempUOIDStruct.UOID=(char*)malloc(21); 
			memset(tempUOIDStruct.UOID,'\0',21);
			memcpy(tempUOIDStruct.UOID, searchUOID,20);
			tempUOIDStruct.recvPort = init.Port;
			tempUOIDStruct.msgLifeTime = init.MsgLifetime;

			UOIDVector.push_back(tempUOIDStruct);

		pthread_mutex_unlock(&vectormutex);

		char *putBuffer=(char *)malloc(dataLength+1);
		memset(putBuffer, '\0', dataLength+1);
		memcpy(&putBuffer[0], &searchType, 1);
		memcpy(&putBuffer[1], &charQuery[0], dataLength-1);

		CommonData_put(SHRQ, &(searchUOID[0]), init.TTL,  dataLength, uint16_t(init.Port), putBuffer, 65000);
        waitFlag=true;
	 }

  return waitFlag;
}

// Desciption : This function sends this node's search response based on SHA1 as well as floods the search request
// Start of SHA1_Search()
bool SHA1_Search(string querydata)
{
	bool waitFlag=false;
	TrimSpaces(querydata);

	char *charQuery=(char*)malloc(strlen(querydata.c_str())+1);
    memset(charQuery,'\0',strlen(querydata.c_str())+1);
	memcpy(charQuery,&querydata[0],strlen(querydata.c_str()));
   		
	unsigned char* query = (unsigned char*)malloc(21);	//Converting to Hex
	memset(query,'\0', 21);	
	query=CharToHexadec((unsigned char*)charQuery, 40);

	pthread_mutex_lock(&storemutex);
	 for (int i=0;i< (int)sha1_ind_vector.size(); i++ )
	 {
			size_t checkStr=memcmp (sha1_ind_vector[i].value, query, strlen((char*)query) );
			if (checkStr==0)
			{
				std::ostringstream rawStr;
				rawStr << sha1_ind_vector[i].fileCount;
				std::string convStr = rawStr.str();

				string storeMetaFilename (convStr);
				storeMetaFilename = storeMetaFilename + ".meta"; 

				string sMetaFile(init.HomeDir);
				sMetaFile = sMetaFile + "/files/" + storeMetaFilename;
				
				int filePos=0;

				for (int j=0;j<(int)FileID_vector.size() ; j++)
				{
					if (FileID_vector[j].fileCount == sha1_ind_vector[i].fileCount)
					{
						filePos=j;
						break;
					}
				}				
							
				writeSearchResult(sMetaFile,FileID_vector[filePos].value);				
			}
		}

 	 pthread_mutex_unlock(&storemutex);

	 if (init.TTL > 0)
	 {
		// put search message on the queue
		uint8_t searchType = 2;
		
		uint32_t dataLength = 1 + strlen(charQuery);

		char *searchHeader = (char*)malloc(COMMON_HEADER_SIZE);
		searchHeader = GetHeader(node_inst_id, SHRQ, (uint8_t)init.TTL, dataLength);

		char *searchUOID = (char*)malloc(21);
		memset(searchUOID, '\0', 21);
		memcpy(&searchUOID[0],&searchHeader[1],20);	// copy to global UOID to decide originator of search message

		//Pushing UOID into UOID structure
		pthread_mutex_lock(&vectormutex);

			UStruct tempUOIDStruct;
			tempUOIDStruct.UOID=(char*)malloc(21); 
			memset(tempUOIDStruct.UOID,'\0',21);
			memcpy(tempUOIDStruct.UOID, searchUOID,20);
			tempUOIDStruct.recvPort = init.Port;
			tempUOIDStruct.msgLifeTime = init.MsgLifetime;

			UOIDVector.push_back(tempUOIDStruct);

		pthread_mutex_unlock(&vectormutex);

		char *putBuffer=(char *)malloc(dataLength+1);
		memset(putBuffer, '\0', dataLength+1);
		memcpy(&putBuffer[0], &searchType, 1);
		memcpy(&putBuffer[1], &charQuery[0], dataLength-1);

		CommonData_put(SHRQ, &(searchUOID[0]), init.TTL,  dataLength, uint16_t(init.Port), putBuffer, 65000);
        waitFlag=true;
	 }

  return waitFlag;
}

// Desciption : This function finds the keyword in BitVector
// Start of findInBitVector()
bool findInBitVector(unsigned char* BitValue,vector<string> KeyWVector)
{
	int shaTotalcount=0;
	int md5Totalcount=0;

    
	uint8_t uint_BitVector[128];
	for (int i=0;i<128 ;i++ )
	{
		memcpy(&uint_BitVector[i], &(BitValue[i]), 1);
	}

	for (int i=0;i< (int)KeyWVector.size(); i++ )
	{				
		char *smallKeyd=(char*)malloc(strlen(KeyWVector[i].c_str())+1);
		memset(smallKeyd, '\0', strlen(KeyWVector[i].c_str())+1);
		memcpy(smallKeyd,KeyWVector[i].c_str(),strlen(KeyWVector[i].c_str()));

		unsigned char keySha[20];
		unsigned char keyMd5[16];

		GetSHA(smallKeyd, (char*)&keySha[0], sizeof(keySha));
		GetMD5(smallKeyd, (char*)&keyMd5[0], sizeof(keyMd5));
		uint8_t sha18;
		uint8_t md514;

		sha18=(keySha[18] & 0x01) ? 0x01 : 0x00;
		md514=(keyMd5[14] & 0x01) ? 0x01 : 0x00;

		unsigned char *keyS1 = (unsigned char*)malloc(3);
		memset(keyS1,'\0',3);
		memcpy(&keyS1[0],&sha18,1);
		memcpy(&keyS1[1],&keySha[19],1);

		unsigned char *keym1 = (unsigned char*)malloc(3);
		memset(keym1,'\0',3);
		memcpy(&keym1[0],&md514,1);
		memcpy(&keym1[1],&keyMd5[15],1);

		uint16_t ShaPos;
		memcpy(&ShaPos,&keyS1[0],2);
		ShaPos=ShaPos+512;

		uint16_t Md5Pos;
		memcpy(&Md5Pos,&keym1[0],2);

		int shaDiv=ShaPos/4;
		int shaRem=ShaPos%4;

		int md5Div=Md5Pos/4;
		int md5Rem=Md5Pos%4;

		int shaBitDiv=shaDiv/2;
		int shaBitRem=shaDiv%2;

		int md5BitDiv=md5Div/2;
		int md5BitRem=md5Div%2;

		uint8_t ShaResult=0;
		uint8_t MD5Result=0;
		if(shaBitRem == 0)
		{
			switch (shaRem)
			{
				case 0: 
				{
					ShaResult = uint_BitVector[127-shaBitDiv] & EVEN_ONE ;
					if (ShaResult == EVEN_ONE)
					{
						shaTotalcount++;
					}
					break;
				}
				case 1: 
				{
					ShaResult = uint_BitVector[127-shaBitDiv] & EVEN_TWO ;
					if (ShaResult == EVEN_TWO)
					{
						shaTotalcount++;
					}
					break;
				}
				case 2: 
				{
					ShaResult = uint_BitVector[127-shaBitDiv] & EVEN_THR ;
					if (ShaResult == EVEN_THR)
					{
						shaTotalcount++;
					}
					break;
				}
				case 3: 
				{
					ShaResult = uint_BitVector[127-shaBitDiv] & EVEN_FOR ;
					if (ShaResult == EVEN_FOR)
					{
						shaTotalcount++;
					}
					break;
				}       
			}
		}
		else
		{
			switch (shaRem)
			{
				case 0: 
				{
					ShaResult = uint_BitVector[127-shaBitDiv] & ODD_ONE ;
					if (ShaResult == ODD_ONE)
					{
						shaTotalcount++;
					}
					break;
				}
				case 1: 
				{
					ShaResult = (uint_BitVector[127-shaBitDiv] & ODD_TWO );
					if (ShaResult == ODD_TWO)
					{
						shaTotalcount++;
					}
					break;
				}
				case 2:
				{
					ShaResult = uint_BitVector[127-shaBitDiv] & ODD_THR ;
					if (ShaResult == ODD_THR)
					{
						shaTotalcount++;
					}
					break;
				}
				case 3: 
				{
					ShaResult = uint_BitVector[127-shaBitDiv] & ODD_FOR ;
					if (ShaResult == ODD_FOR)
					{
						shaTotalcount++;
					}
					break;
				}       
			}
		}

		if(md5BitRem == 0)
		{
			switch (md5Rem)
			{
				case 0: 
				{
					MD5Result = uint_BitVector[127-md5BitDiv] & EVEN_ONE ;
					if (MD5Result == EVEN_ONE)
					{
						md5Totalcount++;
					}
					break;
				}
				case 1: 
				{
					MD5Result = uint_BitVector[127-md5BitDiv] & EVEN_TWO ;
					if (MD5Result == EVEN_TWO)
					{
						md5Totalcount++;
					}
					break;
				}
				case 2: 
				{
					MD5Result = uint_BitVector[127-md5BitDiv] & EVEN_THR ;
					if (MD5Result == EVEN_THR)
					{
						md5Totalcount++;
					}
					break;
				}
				case 3: 
				{
					MD5Result = uint_BitVector[127-md5BitDiv] & EVEN_FOR ;
					if (MD5Result == EVEN_FOR)
					{
						md5Totalcount++;
					}
					break;
				}       
			} 
		}
		else
		{
			switch (md5Rem)
			{
				case 0: 
				{
					MD5Result = uint_BitVector[127-md5BitDiv] & ODD_ONE ;
					if (MD5Result == ODD_ONE)
					{
						md5Totalcount++;
					}
					break;
				}
				case 1: 
				{
					MD5Result = uint_BitVector[127-md5BitDiv] & ODD_TWO ;
					if (MD5Result == ODD_TWO)
					{
						md5Totalcount++;
					}
					break;
				}
				case 2: 
				{
					MD5Result = uint_BitVector[127-md5BitDiv] & ODD_THR ;
					if (MD5Result == ODD_THR)
					{
						md5Totalcount++;
					}
					break;
				}
				case 3: 
				{
					MD5Result = uint_BitVector[127-md5BitDiv] & ODD_FOR ;
					if (MD5Result == ODD_FOR)
					{
						md5Totalcount++;
					}
					break;
				}       
			}
		}

		free(smallKeyd);
	
	
	}

	if (shaTotalcount ==(int)KeyWVector.size() && md5Totalcount == (int)KeyWVector.size())
	{
        return true;
	}
	else
	{
	   return false;
	}

}

// Desciption : This function is used to find the keywords in the metafile
// Start of findInMetaFile()
bool findInMetaFile(int fileCount, vector<string> KeyWVector)
{
	std::ostringstream rawStr;
	rawStr << fileCount;
	std::string convStr = rawStr.str();

	string storeMetaFilename (convStr);
	storeMetaFilename = storeMetaFilename + ".meta"; 

	string sMetaFile(init.HomeDir);
	sMetaFile = sMetaFile + "/files/" + storeMetaFilename;

    int fileTotalCount=0; 
	string line;
	fstream fpSend;
	fpSend.open(sMetaFile.c_str(), fstream::in);
	
	while (! fpSend.eof() )
	{
	  getline(fpSend,line);
	  size_t found1=line.find("Keywords");		
	  if (found1 != string::npos )
	  {
		  line=line.substr(9);
		  break;		  		  
	  }
	}
	fpSend.close();

   convert_lower(&line[0]);

	vector<string> FileKewdVector;
    FileKewdVector = pushKeywords((char*)line.c_str());

	for (int i=0;i< (int)KeyWVector.size(); i++ )
	{
	  for (int j=0;j< (int)FileKewdVector.size(); j++ )
	  {
		  size_t found2=FileKewdVector[j].find(KeyWVector[i]);
		  if (found2 != string::npos )
		  {
			  fileTotalCount++;
			  break;
		  }
	  }	
	}

   if (fileTotalCount == (int)KeyWVector.size() )
   {
	   return true;
   }
   else
   {
      return false;
   }
	
}

// Desciption : This function sends this node's search response based on keyword/s as well as floods the search request
// Start of Keyword_Search()
bool Keyword_Search(string querydata)
{
	bool waitFlag=false;
	TrimSpaces(querydata);

	//Vector to store keywords
	vector<string> KeyWVector;

	char *trimmedstr=TrimDoubleQuotes((char*)querydata.c_str());

	char *charQuery=(char*)malloc(strlen(trimmedstr)+1);
    memset(charQuery,'\0',strlen(trimmedstr)+1);
	memcpy(charQuery,&trimmedstr[0],strlen(trimmedstr));

	KeyWVector = pushKeywords(trimmedstr);

	pthread_mutex_lock(&storemutex);
	 for (int i=0;i< (int)kwrd_ind_vector.size(); i++ )
	 {
		 bool foundBit=findInBitVector(kwrd_ind_vector[i].value,KeyWVector);
		 if (foundBit == true)
		 {
		 	bool foundKey=findInMetaFile(kwrd_ind_vector[i].fileCount, KeyWVector);
			if (foundKey == true)
		    {
				std::ostringstream rawStr;
				rawStr << kwrd_ind_vector[i].fileCount;
				std::string convStr = rawStr.str();

				string storeMetaFilename (convStr);
				storeMetaFilename = storeMetaFilename + ".meta"; 

				string sMetaFile(init.HomeDir);
				sMetaFile = sMetaFile + "/files/" + storeMetaFilename;
				
				int filePos=0;

				for (int j=0;j<(int)FileID_vector.size() ; j++)
				{
					if (FileID_vector[j].fileCount == kwrd_ind_vector[i].fileCount)
					{
						filePos=j;
						break;
					}
				}				
							
				writeSearchResult(sMetaFile,FileID_vector[filePos].value);
			}
		 }

	  }

 	 pthread_mutex_unlock(&storemutex);

	 if (init.TTL > 0)
	 {
		// put search message on the queue
		uint8_t searchType = 3;
		
		uint32_t dataLength = 1 + strlen(charQuery);

		char *searchHeader = (char*)malloc(COMMON_HEADER_SIZE);
		searchHeader = GetHeader(node_inst_id, SHRQ, (uint8_t)init.TTL, dataLength);

		char *searchUOID = (char*)malloc(21);
		memset(searchUOID, '\0', 21);
		memcpy(&searchUOID[0],&searchHeader[1],20);	// copy to global UOID to decide originator of search message

		//Pushing UOID into UOID structure
		pthread_mutex_lock(&vectormutex);

			UStruct tempUOIDStruct;
			tempUOIDStruct.UOID=(char*)malloc(21); 
			memset(tempUOIDStruct.UOID,'\0',21);
			memcpy(tempUOIDStruct.UOID, searchUOID,20);
			tempUOIDStruct.recvPort = init.Port;
			tempUOIDStruct.msgLifeTime = init.MsgLifetime;

			UOIDVector.push_back(tempUOIDStruct);

		pthread_mutex_unlock(&vectormutex);

		char *putBuffer=(char *)malloc(dataLength+1);
		memset(putBuffer, '\0', dataLength+1);
		memcpy(&putBuffer[0], &searchType, 1);
		memcpy(&putBuffer[1], &charQuery[0], dataLength-1);

		CommonData_put(SHRQ, &(searchUOID[0]), init.TTL,  dataLength, uint16_t(init.Port), putBuffer, 65000);
        waitFlag=true;
	 }

  return waitFlag;
}

// Desciption : This function calls respctive functions to execute search queries based on filename, SHA1 and keyword
// Start of callSearch()
void callSearch(string readBuffer) 
{
	searchFlag=true;
	serachResultCount=1;

//	int oldState=0;
	size_t found;
	string searchType;

	Result_vector.clear();

	TrimSpaces(readBuffer);
	convert_lower(&readBuffer[0]);

	//Parsing store request 
	found=readBuffer.find("=");		//Get SearchType
    if (found!=string::npos)
    {
		searchType = readBuffer.substr(0, found);
		TrimSpaces(searchType);
	}

   bool waitFlag=false;
   if (searchType.compare("filename") == 0)
   {
      waitFlag=FileName_Search(readBuffer.substr(found+1));
   }
   else if (searchType.compare("sha1hash") == 0)
   {
      waitFlag=SHA1_Search(readBuffer.substr(found+1));
   }
   else if (searchType.compare("keywords") == 0)
   {
      waitFlag=Keyword_Search(readBuffer.substr(found+1));
   }


   if (waitFlag==true)
   {
  	  sleep(init.MsgLifetime);
   }

   pthread_mutex_lock(&searchmutex);
   	searchFlag=false;
	serachResultCount=1;   
   pthread_mutex_unlock(&searchmutex);
	
}	

// Desciption : This function generated the search response based on filename search
// Start of searchFileResponse()
void searchFileResponse(int acceptReturn, char *sUOID, uint8_t sType, char *query, int port)
{
	int writeBufferSize = 0;
	string sMetaFile(init.HomeDir);
	int filePos;
	//SEND SEARCH RESPONSE

	int dataBufferSize = 20;
	char *dataBuffer = (char*)malloc(dataBufferSize);		//MAIN: DATABUFFER
	//Copying data in to an unsigned character buffer.		//Contains this node's information
	memset(dataBuffer, '\0', dataBufferSize);
	memcpy(&dataBuffer[0], &sUOID[0], 20);

	char *seaFResp_Char=(char*)malloc(BUFFERSIZE);
	memset(seaFResp_Char,'\0',BUFFERSIZE);

    int seaFresp_size=0;

	pthread_mutex_lock(&storemutex);
		uint32_t metaLen;

		for (int i=0; i < (int)name_ind_vector.size()  ; i++ )
		{
		   string tmpLowerName((char*)name_ind_vector[i].value);
		   convert_lower(&tmpLowerName[0]);			
			size_t checkStr=memcmp(&tmpLowerName[0], query, strlen(query));
			if (checkStr==0)
			{
					std::ostringstream rawStr;
					rawStr << name_ind_vector[i].fileCount;
					std::string convStr = rawStr.str();

					string storeMetaFilename (convStr);
					storeMetaFilename = storeMetaFilename + ".meta"; 
				
					sMetaFile = sMetaFile + "/files/" + storeMetaFilename;

					metaLen = strlen(sMetaFile.c_str());
			
					filePos=0;

					for (int j=0;j<(int)FileID_vector.size() ; j++)
					{
						if (FileID_vector[j].fileCount == name_ind_vector[i].fileCount)
						{
							filePos=j;
							updateLRUOrder(name_ind_vector[i].fileCount);
							break;
						}
					}
					memcpy(&seaFResp_Char[seaFresp_size], &metaLen, 4);
					memcpy(&seaFResp_Char[seaFresp_size+4], &(FileID_vector[filePos].value[0]), 20);
					memcpy(&seaFResp_Char[seaFresp_size+24], &sMetaFile[0], metaLen);
             
					seaFresp_size= seaFresp_size + 24 + metaLen;
			}
		
		}

		if (seaFresp_size > 0)
		{
		uint32_t lastMetaLen = 0;
		memcpy(&seaFResp_Char[seaFresp_size-(metaLen+24)], &lastMetaLen, 4);
		}

	pthread_mutex_unlock(&storemutex);

	int dataLength = dataBufferSize + seaFresp_size;

	char *searchRespHeader = (char*)malloc(COMMON_HEADER_SIZE);
	searchRespHeader = GetHeader(node_inst_id, SHRS, (uint8_t)init.TTL, dataLength);

	writeBufferSize = COMMON_HEADER_SIZE + dataLength;

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &sUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	reportLog("s", "SHRS", writeBufferSize, init.TTL, searchRespHeader, logData, port, true);		//write log file

	char *writeBuffer=(char*)malloc(writeBufferSize);
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &searchRespHeader[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &sUOID[0], 20);
	if (seaFresp_size > 0)
	{
	memcpy(&writeBuffer[47], &seaFResp_Char[0], seaFresp_size);
	}

	if(write(acceptReturn, writeBuffer, writeBufferSize) == SOCKETERR) {			//write data
		printMessage("Write error... ");
	}

    free(searchRespHeader);
	free(writeBuffer);
}

// Desciption : This function generated the search response based on SHA1 search
// Start of searchSHA1Response()
void searchSHA1Response(int acceptReturn, char *sUOID, uint8_t sType, unsigned char *query, int port)
{
	int writeBufferSize = 0;
	int filePos;
	//SEND SEARCH RESPONSE
	int dataBufferSize = 20;
	char *dataBuffer = (char*)malloc(dataBufferSize);		//MAIN: DATABUFFER
	//Copying data in to an unsigned character buffer.		//Contains this node's information
	memset(dataBuffer, '\0', dataBufferSize);
	memcpy(&dataBuffer[0], &sUOID[0], 20);

	char *seaSResp_Char=(char*)malloc(BUFFERSIZE);
	memset(seaSResp_Char,'\0',BUFFERSIZE);

    int seaSresp_size=0;

	pthread_mutex_lock(&storemutex);
		uint32_t metaLen;

		for (int i=0; i < (int)sha1_ind_vector.size()  ; i++ )
		{
			string sMetaFile(init.HomeDir);
			size_t checkStr=memcmp(sha1_ind_vector[i].value, query, strlen((char*)query));
			if (checkStr==0)
			{
					std::ostringstream rawStr;
					rawStr << sha1_ind_vector[i].fileCount;
					std::string convStr = rawStr.str();

					string storeMetaFilename (convStr);
					storeMetaFilename = storeMetaFilename + ".meta"; 
				
					sMetaFile = sMetaFile + "/files/" + storeMetaFilename;

					metaLen = strlen(sMetaFile.c_str());
					
					filePos=0;

					for (int j=0;j<(int)FileID_vector.size() ; j++)
					{
						if (FileID_vector[j].fileCount == sha1_ind_vector[i].fileCount)
						{
							filePos=j;
							updateLRUOrder(sha1_ind_vector[i].fileCount);
							break;
						}
					}
					memcpy(&seaSResp_Char[seaSresp_size], &metaLen, 4);
					memcpy(&seaSResp_Char[seaSresp_size+4], &(FileID_vector[filePos].value[0]), 20);
					memcpy(&seaSResp_Char[seaSresp_size+24], &sMetaFile[0], metaLen);
             
					seaSresp_size= seaSresp_size + 24 + metaLen;
			}	
		}

		if (seaSresp_size>0)
		{
		 uint32_t lastMetaLen = 0;
		 memcpy(&seaSResp_Char[seaSresp_size-(metaLen+24)], &lastMetaLen, 4);
		}

	pthread_mutex_unlock(&storemutex);

	int dataLength = dataBufferSize + seaSresp_size;

	char *searchRespHeader = (char*)malloc(COMMON_HEADER_SIZE);
	searchRespHeader = GetHeader(node_inst_id, SHRS, (uint8_t)init.TTL, dataLength);

	writeBufferSize = COMMON_HEADER_SIZE + dataLength;

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &sUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	reportLog("s", "SHRS", writeBufferSize, init.TTL, searchRespHeader, logData, port, true);		//write log file

	char *writeBuffer=(char*)malloc(writeBufferSize);
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &searchRespHeader[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &sUOID[0], 20);
	if (seaSresp_size > 0)
	{
	 memcpy(&writeBuffer[47], &seaSResp_Char[0], seaSresp_size);
    }
	
	if(write(acceptReturn, writeBuffer, writeBufferSize) == SOCKETERR) {			//write data
		printMessage("Write error... ");
	}

    free(searchRespHeader);
	free(writeBuffer);
}

// Desciption : This function generated the search response based on Keyword search
// Start of searchKeyWResponse()
void searchKeyWResponse(int acceptReturn, char *sUOID, uint8_t sType, char *query, int port)
{
	int writeBufferSize = 0;
	string sMetaFile(init.HomeDir);
	int filePos;
	//SEND SEARCH RESPONSE

	int dataBufferSize = 20;
	char *dataBuffer = (char*)malloc(dataBufferSize);		//MAIN: DATABUFFER
	//Copying data in to an unsigned character buffer.		//Contains this node's information
	memset(dataBuffer, '\0', dataBufferSize);
	memcpy(&dataBuffer[0], &sUOID[0], 20);

	char *seaFResp_Char=(char*)malloc(BUFFERSIZE);
	memset(seaFResp_Char,'\0',BUFFERSIZE);

    int seaFresp_size=0;

		//Vector to store keywords
	vector<string> KeyWVector;

	KeyWVector = pushKeywords(query);

	pthread_mutex_lock(&storemutex);
	uint32_t metaLen;

	 for (int i=0;i< (int)kwrd_ind_vector.size(); i++ )
	 {
		 bool foundBit=findInBitVector(kwrd_ind_vector[i].value,KeyWVector);
		 if (foundBit == true)
		 {
		 	bool foundKey=findInMetaFile(kwrd_ind_vector[i].fileCount, KeyWVector);
			if (foundKey == true)
		    {
				std::ostringstream rawStr;
				rawStr << kwrd_ind_vector[i].fileCount;
				std::string convStr = rawStr.str();

				string storeMetaFilename (convStr);
				storeMetaFilename = storeMetaFilename + ".meta"; 

				string sMetaFile(init.HomeDir);
				sMetaFile = sMetaFile + "/files/" + storeMetaFilename;
				
				metaLen = strlen(sMetaFile.c_str());
				filePos=0;

				for (int j=0;j<(int)FileID_vector.size() ; j++)
				{
					if (FileID_vector[j].fileCount == kwrd_ind_vector[i].fileCount)
					{
						filePos=j;
						updateLRUOrder(kwrd_ind_vector[i].fileCount);
						break;
					}
				}				
							
				 memcpy(&seaFResp_Char[seaFresp_size], &metaLen, 4);
				 memcpy(&seaFResp_Char[seaFresp_size+4], &(FileID_vector[filePos].value[0]), 20);
				 memcpy(&seaFResp_Char[seaFresp_size+24], &sMetaFile[0], metaLen);
             
				 seaFresp_size= seaFresp_size + 24 + metaLen;

			}
		 }
	  }

	   if (seaFresp_size > 0)
		{
	     uint32_t lastMetaLen = 0;
      	 memcpy(&seaFResp_Char[seaFresp_size-(metaLen+24)], &lastMetaLen, 4);
		}

 	 pthread_mutex_unlock(&storemutex);	

	int dataLength = dataBufferSize + seaFresp_size;

	char *searchRespHeader = (char*)malloc(COMMON_HEADER_SIZE);
	searchRespHeader = GetHeader(node_inst_id, SHRS, (uint8_t)init.TTL, dataLength);

	writeBufferSize = COMMON_HEADER_SIZE + dataLength;

	unsigned char *last4UOID = (unsigned char*)malloc(4);
	memset(last4UOID, '\0', 4);
	memcpy(last4UOID, &sUOID[16], 4);

	unsigned char *tMsgid = convertHexToChar(last4UOID);
	
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s", (char *)tMsgid);

	reportLog("s", "SHRS", writeBufferSize, init.TTL, searchRespHeader, logData, port, true);		//write log file

	char *writeBuffer=(char*)malloc(writeBufferSize);
	if (writeBuffer == NULL) { 
		fprintf(stderr, "malloc() failed\n");
		exit(1);
	}

	//Copying data in to an unsigned character buffer.
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &searchRespHeader[0], COMMON_HEADER_SIZE);
	memcpy(&writeBuffer[27], &sUOID[0], 20);
	if (seaFresp_size > 0)
	{
	memcpy(&writeBuffer[47], &seaFResp_Char[0], seaFresp_size);
	}

	if(write(acceptReturn, writeBuffer, writeBufferSize) == SOCKETERR) {			//write data
		printMessage("Write error... ");
	}

    free(searchRespHeader);
	free(writeBuffer);
}

// Desciption : This function handles the incoming search request
// Start of HandleSearch_REQ()
void HandleSearch_REQ(int socDesc, int myCTBPos, struct Header ARH1, char* readBuffer)
{
	bool uoidFoundFlag = false;

    uint8_t sType;
	memcpy(&sType, &readBuffer[0], 1);

	char *charquery = (char*)malloc(ARH1.nw_datalen);
	memset(charquery, '\0', ARH1.nw_datalen);
	memcpy(&charquery[0], &readBuffer[1], ARH1.nw_datalen - 1);

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;

	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%d %s", sType, &charquery[0]);

	//Writes into log file
	reportLog("r", "SHRQ", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort,false);

	//Checks if the request has already been processed
	uoidFoundFlag = pushUOIDStruture(myCTBPos, ARH1);
	
	if(uoidFoundFlag == false)         //UOID not present i.e. it's a new request
	{
		uint8_t r_searchTTL=ARH1.nw_TTL - 1;

		switch (sType)
		{
			case 1:
			{
				//Sends Search Response
				searchFileResponse(socDesc, &(ARH1.UOID[0]), sType, charquery, ctb[myCTBPos].ctbPort);
				break;
			}
			case 2:
			{
				//Sends Search Response
				unsigned char* query = (unsigned char*)malloc(21);	//Converting to Hex
				memset(query,'\0', 21);	
				query=CharToHexadec((unsigned char*)charquery, 40);
				searchSHA1Response(socDesc, &(ARH1.UOID[0]), sType, query, ctb[myCTBPos].ctbPort);		
				break;
			}
			case 3:
			{
				//Sends Search Response
				searchKeyWResponse(socDesc, &(ARH1.UOID[0]), sType, charquery, ctb[myCTBPos].ctbPort);		
				break;
			}
			default: break;		
		}

		//Inserts the request into queue for flooding
		if (r_searchTTL > 0)
		{
		   CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_searchTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, 65000);
		}
	}	//Else Message Already Seen
}

// Desciption : This function handles the incoming search response
// Start of HandleSearch_RSP()
void HandleSearch_RSP(int socDesc, int myCTBPos, struct Header ARH1, char *readBuffer)
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
	reportLog("r", "SHRS", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort,false);

	pthread_mutex_lock(&vectormutex);

	for (int i = 0; i < (int)UOIDVector.size(); i++)
	{
		size_t checkStr=memcmp (relayUOID, UOIDVector[i].UOID, 20);

		if (checkStr==0)
		{
		 uoidFoundFlag = true;
		 relayPort = UOIDVector[i].recvPort;
		}
	}
	pthread_mutex_unlock(&vectormutex);

	//Relays back the Search Response to the node who requested
	if(uoidFoundFlag == true)              // UOID not already present
	{
		if (relayPort == (uint16_t)init.Port)		//Request originated from Me
		{
			uint32_t nextLen;
			int dataBufferSize = ARH1.nw_datalen-20;

			if (dataBufferSize != 0)
			{
				memcpy(&nextLen, &readBuffer[20], 4);

			if (nextLen == 0)
			{
				char *fileID = (char*)malloc(21);
				memset(fileID, '\0', 21);
				memcpy(&fileID[0], &readBuffer[24], 20);

				char *metaFile = (char*)malloc((dataBufferSize-24)+1);
				memset(metaFile, '\0', (dataBufferSize-24)+1);
				memcpy(&metaFile[0], &readBuffer[44], (dataBufferSize-24));
				string strMetaFile(metaFile);

				pthread_mutex_lock(&searchmutex);
					if(searchFlag == true)
					{
						writeSearchResult(strMetaFile, (unsigned char*)fileID);
					}
				pthread_mutex_unlock(&searchmutex);
			}
			else
			{
				int g = 20;		//First 20 bytes is message UOID 
				while (1)
				{				
					unsigned char *fileID = (unsigned char*)malloc(21);
					memset(fileID, '\0', 21);
					memcpy(&fileID[0], &readBuffer[g+4], 20);
		
					char *metaFile = (char*)malloc(nextLen+1);
					memset(metaFile, '\0', nextLen+1);
					memcpy(&metaFile[0], &readBuffer[g+24], nextLen);
		
					string strMetaFile(metaFile);
					
					pthread_mutex_lock(&searchmutex);
						if(searchFlag == true)
						{
							writeSearchResult(strMetaFile, fileID);
						}
					pthread_mutex_unlock(&searchmutex);
		
					dataBufferSize = dataBufferSize - (nextLen+24);
					g = g + 24 + nextLen;
					memcpy(&nextLen, &readBuffer[g], 4);
					
					if (nextLen == 0)
					{
						unsigned char *fileID = (unsigned char*)malloc(21);
						memset(fileID, '\0', 21);
						memcpy(&fileID[0], &readBuffer[g+4], 20);

						metaFile = (char*)malloc((dataBufferSize-24)+1);
						memset(metaFile, '\0', (dataBufferSize-24)+1);
						memcpy(&metaFile[0], &readBuffer[g+24], (dataBufferSize-24));
						string strMetaFile(metaFile);

						pthread_mutex_lock(&searchmutex);
							if(searchFlag == true)
							{
								writeSearchResult(strMetaFile, fileID);
							}
						pthread_mutex_unlock(&searchmutex);
						break;
					}
				}
			 }
		    }
		}
		else
		{
			uint8_t r_searchTTL = ARH1.nw_TTL - 1;

			if (r_searchTTL > 0)
			{
			   CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_searchTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, relayPort);
			}
		}
	}	//Else UOID not found
}

//End of Search.cpp
