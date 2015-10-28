/*************************************************************************************************************/
/*												Project : CSCI551 Final Part 2 Project 						*/
/*												Filename: Store.cpp (Server Implementation)					*/
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/
#include "CommonHeaderFile.h"
#include "ProcessWrite.h"
#include "HandleRead.h"
#include "DoHello.h"
#include "Connection.h"
#include "ParseFile.h"
#include "MainPrg.h"
#include "iniparser.h"
#include <openssl/sha.h> 
#include <openssl/md5.h> 

using namespace std;

//Declaration of Variables and Structures
pthread_mutex_t storemutex;
int globalFileCount=0;

typedef struct fileStructure		//Link structure for nam
{
	int fileCount;
	unsigned char* value;
}ComStruct;


typedef struct metaStructure		//Link structure for nam
{
	char *FileName;
	long FileSize;
	unsigned char *SHA1;
	unsigned char *Nonce;
	char* Keywords;
	unsigned char* BitVector;
}MetaStruct;


typedef struct LRUStructure		//Link structure for nam
{
	int localFileCount;
	uint32_t FileSize;
}LRUStruct;

uint32_t LRUcacheSize;
vector<LRUStruct> LRU_vector;

vector<ComStruct> FileID_vector;
vector<ComStruct> OneTmPwd_vector;
vector<ComStruct> kwrd_ind_vector;
vector<ComStruct> name_ind_vector;
vector<ComStruct> sha1_ind_vector;
vector<ComStruct> nonce_vector;

// Description : Function used to get the MD5 value. 
// Reference: Project Specification
// Start of GetMD5()
char *GetMD5 (char *input, char *md5buf, int md5buf_sz)
{
	char md5_buf[17], str_buf[104];

	snprintf(str_buf, sizeof(str_buf), "%s", input);
	MD5((unsigned char *)str_buf, strlen(str_buf), (unsigned char *)md5_buf);
	
	memset(md5buf, '\0', md5buf_sz+1);
	memcpy(md5buf, md5_buf, min((unsigned int)md5buf_sz,sizeof(md5_buf)));

	return md5buf;
}

// Description : Function used to get the SHA1 value. 
// Reference: Project Specification
// Start of GetSHA()
char *GetSHA (char *input, char *shabuf, int shabuf_sz)
{
	char sha1_buf[SHA_DIGEST_LENGTH+1], str_buf[104];

	snprintf(str_buf, sizeof(str_buf), "%s", input);
	SHA1((unsigned char *)str_buf, strlen(str_buf), (unsigned char *)sha1_buf);
	
	memset(shabuf, '\0', shabuf_sz+1);
	memcpy(shabuf, sha1_buf, min((unsigned int)shabuf_sz,sizeof(sha1_buf)));

	return shabuf;
}

// Desciption : Function is used to convert the hex string to character representation
// Start of convertHexToChar()
unsigned char* HexadecToChar(unsigned char *data, int size) 
{
	unsigned char *tMsgid = (unsigned char *)malloc((size*2)+1);
	memset(tMsgid, '\0', ((size*2)+1));
	
	int j=0;
	for(int i = 0; i < size; i++) {
		char hexChars[3];
		memset(hexChars, '\0', 3);	
		
		unsigned int dataChar = data[i];
		snprintf(hexChars, 3, "%02x", dataChar);

		tMsgid[j] = hexChars[0];
		j++;
		tMsgid[j] = hexChars[1];
		j++;
	}
	return tMsgid;
}
// End of function

// Desciption : Function is used to convert the character to hex string representation
// Start of CharToHexadec()
unsigned char* CharToHexadec(unsigned char *data, int size)
{
	unsigned char *tMsgid = (unsigned char *)malloc((size/2)+1);
	memset(tMsgid, '\0', ((size/2)+1));

	int j=0;
	for(int i = 0; i < size; i=i+2) {
		uint8_t Charshex;
		uint8_t byte2;

		uint8_t rCharshex;
		uint8_t rbyte2;
		
		if(data[i] == 'a' || data[i] == 'b' || data[i] == 'c' || data[i] == 'd' || data[i] == 'e' || data[i] == 'f')
		{
			memcpy(&Charshex, &data[i], 1);
			Charshex = Charshex - 39;
		}
		else 
		{
			memcpy(&Charshex, &data[i], 1);
		}

		if(data[i+1] == 'a' || data[i+1] == 'b' || data[i+1] == 'c' || data[i+1] == 'd' || data[i+1] == 'e' || data[i+1] == 'f')
		{
			memcpy(&byte2, &data[i+1],1);
			byte2 = byte2 - 39;
		}
		else
		{
			memcpy(&byte2, &data[i+1],1);
		}
        
		rCharshex=(Charshex & 0xf);
		rbyte2 = (byte2 & 0xf);
		rCharshex = (rCharshex << 4);
		Charshex = rCharshex | rbyte2;
		memcpy(&(tMsgid[j]) ,&Charshex,1);
		j++;
	}

	return tMsgid;
}

// Desciption : This function is used to send a check message if it fails to read data from the connection
// Start of errorWhileReading()
void errorWhileReading(int myCTBPos, int socDesc)
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



// Desciption : This function is used to discard data present in the socket
// Start of discardData()
void discardCompleteData(int socDesc,long readDataLen, int myCTBPos)
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

    readDataLen= (readDataLen - (4+metaLen)); 
   

	long ReadBytes=0;
		 
	while ( ReadBytes < readDataLen )							// read all the received bytes	
	{
		int target = readDataLen - ReadBytes;
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

			free(readData); 			 
		} //end else target
		ReadBytes=ReadBytes + BUFFERSIZE;
	}  //end while

}

// Desciption : This function is used to discard data present in the socket
// Start of discardData()
void discardData(int socDesc,long readDataLen, int myCTBPos)
{
	long ReadBytes=0;
		 
	while ( ReadBytes < readDataLen )							// read all the received bytes	
	{
		int target = readDataLen - ReadBytes;
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

			free(readData); 			 
		} //end else target
		ReadBytes=ReadBytes + BUFFERSIZE;
	}  //end while

}

// Desciption : This function is used to convert data to lowercase
// Start of convert_lower()
void convert_lower(char *name)
{
     int i =0;
     while (name[i] != '\0')
     {
           name[i] = tolower(name[i]);
           i++;
     }   
}

// Desciption : This function is used to generate bitvector
// Start of loadBitVector()
unsigned char *loadBitVector(int fileCount, vector<string> KeyWVector)
{
	//Generating Bit Vector and storing it in kwrd_index file
	char* BitVector=(char*)malloc(129);
	memset(BitVector,'\0',129);

	uint8_t uint_BitVector[128];
	for (int i=0;i<128 ;i++ )
	{
		uint_BitVector[i]=0;
	}

	for (int i=0;i<(int)KeyWVector.size() ;i++ )
	{
		//printf("KEWD : %s\n",KeyWVector[i]);
		char *smallKeyd=(char*)malloc(strlen(KeyWVector[i].c_str())+1);
		memset(smallKeyd, '\0', strlen(KeyWVector[i].c_str())+1);
		memcpy(smallKeyd,KeyWVector[i].c_str(),strlen(KeyWVector[i].c_str()));

		convert_lower(smallKeyd);

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
		//	  printf("%d\n",ShaPos);

		uint16_t Md5Pos;
		memcpy(&Md5Pos,&keym1[0],2);
		//	  printf("%d\n",Md5Pos);

		int shaDiv=ShaPos/4;
		int shaRem=ShaPos%4;

		int md5Div=Md5Pos/4;
		int md5Rem=Md5Pos%4;

		int shaBitDiv=shaDiv/2;
		int shaBitRem=shaDiv%2;

		int md5BitDiv=md5Div/2;
		int md5BitRem=md5Div%2;

		if(shaBitRem == 0)
		{
			switch (shaRem)
			{
				case 0: 
				{
					uint_BitVector[127-shaBitDiv] = uint_BitVector[127-shaBitDiv] | EVEN_ONE ;
					break;
				}
				case 1: 
				{
					uint_BitVector[127-shaBitDiv] = uint_BitVector[127-shaBitDiv] | EVEN_TWO ;
					break;
				}
				case 2: 
				{
					uint_BitVector[127-shaBitDiv] = uint_BitVector[127-shaBitDiv] | EVEN_THR ;
					break;
				}
				case 3: 
				{
					uint_BitVector[127-shaBitDiv] = uint_BitVector[127-shaBitDiv] | EVEN_FOR ;
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
					uint_BitVector[127-shaBitDiv] = uint_BitVector[127-shaBitDiv] | ODD_ONE ;
					break;
				}
				case 1: 
				{
					uint_BitVector[127-shaBitDiv] = (uint_BitVector[127-shaBitDiv] | ODD_TWO );
					//printf("%d\n",uint_BitVector[127-shaBitDiv]);
					break;
				}
				case 2:
				{
					uint_BitVector[127-shaBitDiv] = uint_BitVector[127-shaBitDiv] | ODD_THR ;
					break;
				}
				case 3: 
				{
					uint_BitVector[127-shaBitDiv] = uint_BitVector[127-shaBitDiv] | ODD_FOR ;
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
					uint_BitVector[127-md5BitDiv] = uint_BitVector[127-md5BitDiv] | EVEN_ONE ;
					break;
				}
				case 1: 
				{
					uint_BitVector[127-md5BitDiv] = uint_BitVector[127-md5BitDiv] | EVEN_TWO ;
					break;
				}
				case 2: 
				{
					uint_BitVector[127-md5BitDiv] = uint_BitVector[127-md5BitDiv] | EVEN_THR ;
					break;
				}
				case 3: 
				{
					uint_BitVector[127-md5BitDiv] = uint_BitVector[127-md5BitDiv] | EVEN_FOR ;
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
					uint_BitVector[127-md5BitDiv] = uint_BitVector[127-md5BitDiv] | ODD_ONE ;
					break;
				}
				case 1: 
				{
					uint_BitVector[127-md5BitDiv] = uint_BitVector[127-md5BitDiv] | ODD_TWO ;
					break;
				}
				case 2: 
				{
					uint_BitVector[127-md5BitDiv] = uint_BitVector[127-md5BitDiv] | ODD_THR ;
					break;
				}
				case 3: 
				{
					uint_BitVector[127-md5BitDiv] = uint_BitVector[127-md5BitDiv] | ODD_FOR ;
					break;
				}       
			}
		}

		for (int s=0;s<128;s++ )
		{
			memcpy(&BitVector[s], &(uint_BitVector[s]), 1);
		}
		free(smallKeyd);
	}

	std::ostringstream rawStr;
		rawStr << fileCount;
	std::string convStr = rawStr.str();

	ComStruct tmpBitStruct;
	tmpBitStruct.fileCount = fileCount;
	
	tmpBitStruct.value=(unsigned char*)malloc(129);
	memset(tmpBitStruct.value,'\0',129);
	memcpy(tmpBitStruct.value, BitVector,128);

	pthread_mutex_lock(&storemutex);
		kwrd_ind_vector.push_back(tmpBitStruct);

		unsigned char *charBitVector = HexadecToChar((unsigned char *)BitVector, 128);
	
	pthread_mutex_unlock(&storemutex);
	
	return charBitVector;
}

// Desciption : This function is used to load index files vector
// Start of loadAndStoreFiles()
int loadAndStoreFiles(string filename) 
{
	pthread_mutex_lock(&storemutex);
		int localFileCount=globalFileCount;
		globalFileCount++;

		std::ostringstream rawStr;
			rawStr << localFileCount;
		std::string convStr = rawStr.str();

	pthread_mutex_unlock(&storemutex);

	//Generating File Identifier
	unsigned char fileID[SHA_DIGEST_LENGTH];
	char obj_type[] = "file";
	GetUOID(node_inst_id, obj_type, (char*)&fileID[0], sizeof(fileID));

	ComStruct tmpFileStruct;
	tmpFileStruct.fileCount = localFileCount;
	
	tmpFileStruct.value=(unsigned char*)malloc(21); 
	memset(tmpFileStruct.value,'\0',21);
	memcpy(tmpFileStruct.value, fileID,20);
		
	pthread_mutex_lock(&storemutex);
		FileID_vector.push_back(tmpFileStruct);

	pthread_mutex_unlock(&storemutex);


	//Storing filename into file index
	ComStruct tmpFilenameStruct;
	tmpFilenameStruct.fileCount = localFileCount;
	
	tmpFilenameStruct.value=(unsigned char*)malloc(strlen(filename.c_str())+1);
	memset(tmpFilenameStruct.value, '\0', strlen(filename.c_str())+1);
	memcpy(tmpFilenameStruct.value, filename.c_str(), strlen(filename.c_str()));
		
	pthread_mutex_lock(&storemutex);
		name_ind_vector.push_back(tmpFilenameStruct);

	pthread_mutex_unlock(&storemutex);

	return localFileCount;
}

// Desciption : This function is used to send store message over the network
// Start of sendStoreMessage()
void sendStoreMessage(char *initBuffer, char *Request, int socDesc)
{

	string storeMetaFilename (Request);
	storeMetaFilename = storeMetaFilename + ".meta"; 

	string sMetaFile(init.HomeDir);

	sMetaFile = sMetaFile + "/files/" + storeMetaFilename;

	uint32_t writeBufferSize = COMMON_HEADER_SIZE + 4 + strlen(sMetaFile.c_str());
	
	char *writeBuffer = (char*)malloc(writeBufferSize);
	memset(writeBuffer, '\0', writeBufferSize);
	memcpy(&writeBuffer[0], &initBuffer[0], COMMON_HEADER_SIZE);

	uint32_t ptrReqLen = strlen(sMetaFile.c_str());

	memcpy(&writeBuffer[27], &ptrReqLen, 4);
	memcpy(&writeBuffer[31], &sMetaFile[0], strlen(sMetaFile.c_str()));

	//Sending Header + Metadata first
	if(write(socDesc, writeBuffer, writeBufferSize) != (long)writeBufferSize)
	{
		signal(SIGPIPE, pipeHandler);
	}

	//Sending Actual Data
	string sendFilename (Request);
	sendFilename = sendFilename + ".data"; 

	string sendFile(init.HomeDir);

	sendFile = sendFile + "/files/" + sendFilename;

	struct stat tmp_structure, *buf_stat;
	buf_stat=&tmp_structure;

	stat(sendFile.c_str(), buf_stat);					// Get filesize
   	long fileDataLen=buf_stat->st_size;
	

	fstream fpSend;

	long ReadBytes=0;
	
	fpSend.open(sendFile.c_str(), fstream::in);
	  
	while ( ReadBytes < fileDataLen )							// read all the received bytes	
	{
		int target = fileDataLen - ReadBytes;
		char *readData;    
		if(target > BUFFERSIZE)
		{
			readData = (char *)malloc(BUFFERSIZE+1);				//buffer to read 8192 bytes
			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");}

			memset(readData, '\0', BUFFERSIZE+1);  

			fpSend.read(&readData[0], BUFFERSIZE);

			//Sending Data
			if(write(socDesc, readData, BUFFERSIZE) != BUFFERSIZE)
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
			if(write(socDesc, readData, target) != target)
			{
				signal(SIGPIPE, pipeHandler);
			}


			free(readData); 			 
		} //end else target
		ReadBytes=ReadBytes + BUFFERSIZE;
	}  //end while

	fpSend.close();
}

// Desciption : This function is used to parse metafile
// Start of ParseMetaData()
MetaStruct ParseMetaData(char* metaName)
{
	MetaStruct retMeta;

	dictionary * d ;
    d = iniparser_load(metaName);

	string tempIni="metadata:FileName";	
	retMeta.FileName=iniparser_getstring(d, &tempIni[0],NULL);

	tempIni="metadata:FileSize";	
    retMeta.FileSize=iniparser_getdouble(d, &tempIni[0],-99);

	tempIni="metadata:SHA1";	
	retMeta.SHA1=(unsigned char*)iniparser_getstring(d, &tempIni[0],NULL);

	tempIni="metadata:Nonce";	
	retMeta.Nonce=(unsigned char*)iniparser_getstring(d, &tempIni[0],NULL);

	tempIni="metadata:Keywords";	
	retMeta.Keywords=iniparser_getstring(d, &tempIni[0],NULL);

	tempIni="metadata:Bit-vector";	
	retMeta.BitVector=(unsigned char*)iniparser_getstring(d, &tempIni[0],NULL);

	return retMeta;
}

// Desciption : This function is used to push keywords into a vector
// Start of pushKeywords()
vector<string> pushKeywords(char *trimmedstr) 
{
	size_t found;
	vector<string> KeyWVector;
	string noQuoteKewd (trimmedstr);		//Trims double quotes and removes equal sign

	while ( noQuoteKewd.find(" ") != string::npos)
	{
		found=noQuoteKewd.find(" ");		//Get Filename
		string pushKewd=noQuoteKewd.substr(0, found);
		noQuoteKewd = noQuoteKewd.substr(found+1);
		TrimSpaces(noQuoteKewd);

		KeyWVector.push_back(pushKewd);											
	}

	KeyWVector.push_back(noQuoteKewd);          // for last keyword
	return KeyWVector;
}

// Desciption : This function is used to generate one time password
// Start of GenOTP()
unsigned char* GenOTP(int localFileCount)
{
	char obj_type[] = "file";

	std::ostringstream rawStr;
		rawStr << localFileCount;
	std::string convStr = rawStr.str();

	//Generating one-time-password for the file
	unsigned char pass[SHA_DIGEST_LENGTH];
	GetUOID(node_inst_id, obj_type, (char*)&pass[0], sizeof(pass));

	ComStruct tmpPassStruct;
	tmpPassStruct.fileCount = localFileCount;

	tmpPassStruct.value=(unsigned char*)malloc(21);
	memset(tmpPassStruct.value,'\0', 21);
	memcpy(tmpPassStruct.value, pass, 20);

	pthread_mutex_lock(&storemutex);
		OneTmPwd_vector.push_back(tmpPassStruct);

		unsigned char *charPass = HexadecToChar(pass, 20);
		//cout<<"onetmPwd : "<<charPass<<endl;

	pthread_mutex_unlock(&storemutex);

	return charPass;
}

// Desciption : This function is used to generate nonce
// Start of GenNonce()
unsigned char* GenNonce(int localFileCount, unsigned char* charPass)
{
	unsigned char* pass = (unsigned char*)malloc(21);
	memset(pass,'\0', 21);	
	pass=CharToHexadec(charPass, 40);

	std::ostringstream rawStr;
		rawStr << localFileCount;
	std::string convStr = rawStr.str();

	//Generating nonce for one-time-password
	unsigned char nonce[SHA_DIGEST_LENGTH];
	GetSHA((char*)pass, (char*)&nonce[0], sizeof(nonce));

	ComStruct tmpNonceStruct;
	tmpNonceStruct.fileCount = localFileCount;
	
	tmpNonceStruct.value=(unsigned char*)malloc(21);
	memset(tmpNonceStruct.value,'\0', 21);
	memcpy(tmpNonceStruct.value, nonce, 20);

	pthread_mutex_lock(&storemutex);
		nonce_vector.push_back(tmpNonceStruct);
	
		unsigned char *charnonce = HexadecToChar(nonce, 20);
		//cout<<"nonce : "<<charnonce<<endl;

	pthread_mutex_unlock(&storemutex);

	return charnonce;
}

// Desciption : This function is used to generate SHA1
// Start of GenSHA1()
unsigned char* GenSHA1(int localFileCount, string filename, long file_datalen)
{
	//Get SHA1 for the file content
	SHA_CTX Mp;
	SHA1_Init(&Mp);													//initialize SHA1
	fstream fp;

	std::ostringstream rawStr;
		rawStr << localFileCount;
	std::string convStr = rawStr.str();

	long ReadBytes=0;
	string storeFilename (convStr);
	storeFilename = storeFilename + ".data"; 

	fp.open(filename.c_str(), ios::in | ios::binary);
	  
	while ( ReadBytes < file_datalen )							// read all the received bytes	
	{
		int target = file_datalen -ReadBytes;
		char *readData;    
		if(target > BUFFERSIZE)
		{
			readData = (char *)malloc(BUFFERSIZE+1);				//buffer to read 8192 bytes
			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");}

			memset(readData, '\0', BUFFERSIZE+1);  

			fp.read(&readData[0], BUFFERSIZE);
			DataFileWrite(storeFilename, readData, BUFFERSIZE, 1);

			SHA1_Update(&Mp, readData, BUFFERSIZE);				// update sha1 result

			free(readData); 		 
		}
		else    //target
		{
			readData = (char *)malloc(target+1);					// buffer to read less than 512 bytes

			if(readData == NULL)
			{fprintf(stderr, "malloc() failed\n");  }
			memset(readData, '\0', target+1);
			fp.read(&readData[0], target);
			DataFileWrite(storeFilename, readData, target, 1);
			SHA1_Update(&Mp, readData, target);						// update the sha1 result
			free(readData); 			 
		} //end else target
		ReadBytes=ReadBytes + BUFFERSIZE;
	}  //end while

	fp.close();
	   
	unsigned char *ShaResult = (unsigned char *)malloc(20);
	SHA1_Final(ShaResult, &Mp);										//final sha1 calculation

	ComStruct tmpSha1Struct;
	tmpSha1Struct.fileCount = localFileCount;
	
	tmpSha1Struct.value=(unsigned char*)malloc(21);
	memset(tmpSha1Struct.value,'\0', 21);
	memcpy(tmpSha1Struct.value, ShaResult, 20);

	pthread_mutex_lock(&storemutex);
		sha1_ind_vector.push_back(tmpSha1Struct);
	
		unsigned char *charShaResult = HexadecToChar(ShaResult, 20);
	    //cout<<"SHA1 : "<<charShaResult<<endl;

	pthread_mutex_unlock(&storemutex);

	return charShaResult;
}

// Desciption : This function is used to create meta file
// Start of CreateMetaFile()
string CreateMetaFile(int localFileCount, string extractFname, long file_datalen, unsigned char* charShaResult, unsigned char* charnonce, string trimmedstr, unsigned char* charBitVector)
{
	//Creating Metadata file

	std::ostringstream rawStr;
		rawStr << localFileCount;
	std::string convStr = rawStr.str();

	string storeMetaFilename (convStr);
	storeMetaFilename = storeMetaFilename + ".meta";

	std::ostringstream sizeStr;
		sizeStr << file_datalen;
	std::string szStr = sizeStr.str();

	string metaBuffer("[metadata]");

	metaBuffer = metaBuffer + "\r\n" + "FileName=" + extractFname + "\r\n" + "FileSize=" + szStr + "\r\n" + "SHA1=" + (char*)charShaResult + "\r\n" + "Nonce=" + (char*)charnonce + "\r\n" + "Keywords=" + trimmedstr + "\r\n" + "Bit-vector=" + (char*)charBitVector + "\r\n";

	FileWrite(storeMetaFilename, (char*)metaBuffer.c_str(), 2);

	return storeMetaFilename;
}

// Desciption : This function is used to delete entries from the vectors
// Start of deleteVectorFilesEntry()
void deleteVectorFilesEntry(int localFileCount)
{
	//NOTE: Must acquire a lock on storemutex before calling this function
	std::ostringstream rawStr;
		rawStr << localFileCount;
	std::string convStr = rawStr.str();

	string dataFile (init.HomeDir); 
	dataFile = dataFile + "/files/" + convStr + ".data";
	remove(dataFile.c_str());

	string metaFile (init.HomeDir); 
	metaFile = metaFile  + "/files/" + convStr + ".meta";
	remove(metaFile.c_str());

	for (int i = 0; i < (int)FileID_vector.size(); i++)
	{
		if(FileID_vector[i].fileCount == localFileCount)
		{
			FileID_vector.erase(FileID_vector.begin()+i);
			break;
		}
	}

	for (int i = 0; i < (int)OneTmPwd_vector.size(); i++)
	{
		if(OneTmPwd_vector[i].fileCount == localFileCount)
		{
			OneTmPwd_vector.erase(OneTmPwd_vector.begin()+i);
			break;
		}
	}

	for (int i = 0; i < (int)kwrd_ind_vector.size(); i++)
	{
		if(kwrd_ind_vector[i].fileCount == localFileCount)
		{
			kwrd_ind_vector.erase(kwrd_ind_vector.begin()+i);
			break;
		}
	}

	for (int i = 0; i < (int)name_ind_vector.size(); i++)
	{
		if(name_ind_vector[i].fileCount == localFileCount)
		{
			name_ind_vector.erase(name_ind_vector.begin()+i);
			break;
		}
	}

	for (int i = 0; i < (int)sha1_ind_vector.size(); i++)
	{
		if(sha1_ind_vector[i].fileCount == localFileCount)
		{
			sha1_ind_vector.erase(sha1_ind_vector.begin()+i);
			break;
		}
	}

	for (int i = 0; i < (int)nonce_vector.size(); i++)
	{
		if(nonce_vector[i].fileCount == localFileCount)
		{
			nonce_vector.erase(nonce_vector.begin()+i);
			break;
		}
	}
}

// Desciption : This function is used to update LRU vector
// Start of updateLRUOrder()
void updateLRUOrder(int fileCount)
{
	int filePos=0;
	bool LRU_Found=false;
	LRUStruct tmpLRUStruct;
	for (int j=0;j<(int)LRU_vector.size() ; j++)
	{
		if (LRU_vector[j].localFileCount == fileCount)
		{
			filePos=j;
			LRU_Found=true;
			//Loading vectors
			tmpLRUStruct.localFileCount = LRU_vector[j].localFileCount;
			tmpLRUStruct.FileSize = LRU_vector[j].FileSize;
			break;
		}
	}

    if (LRU_Found==true)
    {
	LRU_vector.erase(LRU_vector.begin()+filePos);
	LRU_vector.push_back(tmpLRUStruct);
	}

}

// Desciption : This function is used to insert file into LRU vector
// Start of addFileToLRU()
int addFileToLRU(MetaStruct rcdMeta)
{
	int localFileCount;
	string strFile(rcdMeta.FileName);

	if (rcdMeta.FileSize > init.CacheSize)
	{
		return -99;
	}
	pthread_mutex_lock(&storemutex);
		while ((long)(LRUcacheSize + rcdMeta.FileSize) > init.CacheSize)
		{
			localFileCount = LRU_vector.front().localFileCount;
			LRUcacheSize = LRUcacheSize - LRU_vector.front().FileSize;
			LRU_vector.erase(LRU_vector.begin());

			deleteVectorFilesEntry(localFileCount);
		}
		LRUcacheSize = LRUcacheSize + rcdMeta.FileSize;
	pthread_mutex_unlock(&storemutex);

	localFileCount = loadAndStoreFiles(strFile);

	pthread_mutex_lock(&storemutex);
		//Loading vectors
		LRUStruct tmpLRUStruct;
		tmpLRUStruct.localFileCount = localFileCount;
		tmpLRUStruct.FileSize = rcdMeta.FileSize;

		LRU_vector.push_back(tmpLRUStruct);
	pthread_mutex_unlock(&storemutex);

	return localFileCount;
}

// Desciption : This function is used to copy data from the meta file
// Start of copyDataFromMetafile()
void copyDataFromMetafile(int localFileCount, MetaStruct rcdMeta)
{
	ComStruct tmpBitStruct;
	tmpBitStruct.fileCount = localFileCount;

	tmpBitStruct.value=(unsigned char*)malloc(129);
	memset(tmpBitStruct.value,'\0',129);
	tmpBitStruct.value=CharToHexadec(rcdMeta.BitVector, 256);

	pthread_mutex_lock(&storemutex);
		kwrd_ind_vector.push_back(tmpBitStruct);
	pthread_mutex_unlock(&storemutex);


	ComStruct tmpNonce;
	tmpNonce.fileCount = localFileCount;

	tmpNonce.value=(unsigned char*)malloc(21);
	memset(tmpNonce.value,'\0',21);
	tmpNonce.value=CharToHexadec(rcdMeta.Nonce, 40);

	pthread_mutex_lock(&storemutex);
		nonce_vector.push_back(tmpNonce);
	pthread_mutex_unlock(&storemutex);


	ComStruct tmpSHA1;
	tmpSHA1.fileCount = localFileCount;

	tmpSHA1.value=(unsigned char*)malloc(21);
	memset(tmpSHA1.value,'\0',21);
	tmpSHA1.value=CharToHexadec(rcdMeta.SHA1, 40);

	pthread_mutex_lock(&storemutex);
		sha1_ind_vector.push_back(tmpSHA1);
	pthread_mutex_unlock(&storemutex);
}

// Desciption : This function is used to handles the store request by putting the request on to the common queue
// Start of HandleStore_REQ()
void HandleStore_REQ(int socDesc,int myCTBPos,struct Header ARH1)
{
	bool uoidFoundFlag = false;

	uint32_t dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;

	string dummyStr = " ";
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, (int)LOGDATALEN, "%s", dummyStr.c_str());

	//Writes into log file
	reportLog("r", "STOR", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort,false );

	//Checks if the request has already been processed
	uoidFoundFlag = pushUOIDStruture(myCTBPos, ARH1);
	
	if(uoidFoundFlag == false)         //UOID not present i.e. it's a new request
	{
		double randomStoreProb=drand48();

			if (randomStoreProb > init.StoreProb)
			{
				discardCompleteData(socDesc, ARH1.nw_datalen, myCTBPos);
				return;
			}

			char *strMetaLength=(char *)malloc(4);
			uint32_t metaLength;

			//Reading Data
			if(read(socDesc, strMetaLength, 4) != 4)
			{
				errorWhileReading(myCTBPos, socDesc);
			}

			memcpy(&metaLength,&strMetaLength[0],4);

            free(strMetaLength);

			char *metaName=(char *)malloc(metaLength+1);
			memset(metaName,'\0',metaLength+1);

			//Reading Data
			if(read(socDesc, metaName, (int)metaLength) != (int)metaLength)
			{
				errorWhileReading(myCTBPos, socDesc);
			}

			MetaStruct rcdMeta;

			rcdMeta=ParseMetaData(metaName);

			int localFileCount=addFileToLRU(rcdMeta);		//addFileToLRU

			if (localFileCount == -99)
			{
				discardData(socDesc, rcdMeta.FileSize, myCTBPos);
				return;
			}
	
			copyDataFromMetafile(localFileCount, rcdMeta);

			std::ostringstream rawStr;
				rawStr << localFileCount;
			std::string convStr = rawStr.str();

			//Call to CreateMetaFile()
			string extractFname (rcdMeta.FileName);
			string storeMetaFilename = CreateMetaFile(localFileCount, extractFname, rcdMeta.FileSize, rcdMeta.SHA1, rcdMeta.Nonce, rcdMeta.Keywords, rcdMeta.BitVector);
			
			//Write Data File

			long ReadBytes=0;
			string storeFilename (convStr);
			storeFilename = storeFilename + ".data"; 

			while (ReadBytes < rcdMeta.FileSize)							// read all the received bytes	
			{
				int target = rcdMeta.FileSize - ReadBytes;
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

					DataFileWrite(storeFilename, readData, BUFFERSIZE, 1);
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
					DataFileWrite(storeFilename, readData, target, 1);
					
					free(readData); 			 
				} //end else target
				ReadBytes=ReadBytes + BUFFERSIZE;
			}  //end while


			//Probabilistic Flooding of STORE Messages
			uint8_t r_storeTTL=ARH1.nw_TTL - 1;

				if(r_storeTTL > 0) 
				{
					char *putBuffer=(char *)malloc(strlen(convStr.c_str())+1);
					memset(putBuffer, '\0', strlen(convStr.c_str())+1);
					memcpy(&putBuffer[0], &convStr[0], strlen(convStr.c_str()));

					string sMetaFile(init.HomeDir);
					sMetaFile = sMetaFile + "/files/" + storeMetaFilename;

					uint32_t dataLength = 4 + strlen(sMetaFile.c_str()) + rcdMeta.FileSize;

					//FLOOD STORE MESSAGE		
					CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_storeTTL,  dataLength, ctb[myCTBPos].ctbPort, putBuffer, 65000);
				}
						
	}	//Else Message Already Seen
	else
	{
	  discardCompleteData(socDesc, ARH1.nw_datalen, myCTBPos);
	}
}

// Desciption : This function puts the store request on the common queue i.e. initiates a store request
// Start of callStore_Files()
void callStore_Files(string readBuffer) 
{
//	int oldState=0;
	size_t found;
	string extractFname;
	int storTTL;

	//Vector to store keywords
	vector<string> KeyWVector;

	TrimSpaces(readBuffer);

	struct stat tmp_structure, *buf_stat;
	buf_stat=&tmp_structure;

	//Parsing store request 
	found=readBuffer.find(" ");		//Get Filename
    if (found!=string::npos)
    {
		extractFname = readBuffer.substr(0, found);
	}

	int statReturn=stat(extractFname.c_str(), buf_stat);					// Get filesize
    if (statReturn!=0)														//valid file
	{
        printMessage("Please check the file name entered... ");   
		return;
	}

	long file_datalen=buf_stat->st_size;

	string tmpTTLStr = readBuffer.substr(found+1);
	TrimSpaces(tmpTTLStr);

	found=tmpTTLStr.find(" ");		//Get Filename
    if (found!=string::npos)
    {
		storTTL = atoi((tmpTTLStr.substr(0, found)).c_str());
	}
	
	string tmpkeywords = tmpTTLStr.substr(found+1);
	TrimSpaces(tmpkeywords);
	
	char *trimmedstr=TrimDoubleQuotes((char*)tmpkeywords.c_str());

	KeyWVector = pushKeywords(trimmedstr);

	//Call to loadAndStoreFiles()
	int localFileCount = loadAndStoreFiles(extractFname);

	//Call to loadBitVector()
	unsigned char *charBitVector = loadBitVector(localFileCount, KeyWVector);

	//Call to GenOTP()
	unsigned char *charPass = GenOTP(localFileCount);

	//Call to GenNonce()
	unsigned char* charnonce = GenNonce(localFileCount, charPass);

	//Call to GenSHA1()
	unsigned char* charShaResult = GenSHA1(localFileCount, extractFname, file_datalen);

	//Call to CreateMetaFile()
	string storeMetaFilename = CreateMetaFile(localFileCount, extractFname, file_datalen, charShaResult, charnonce, trimmedstr, charBitVector);

	std::ostringstream rawStr;
		rawStr << localFileCount;
	std::string convStr = rawStr.str();

	if(storTTL > 0)																// send out message only if TTL > 0
	{
		//SEND STORE MESSAGE
		
		string sMetaFile(init.HomeDir);

		sMetaFile = sMetaFile + "/files/" + storeMetaFilename;

		uint32_t dataLength = 4 + strlen(sMetaFile.c_str()) + file_datalen;

		char *storeHeader = (char*)malloc(COMMON_HEADER_SIZE);
		storeHeader = GetHeader(node_inst_id, STOR, (uint8_t)storTTL, dataLength);

		char *putBuffer=(char *)malloc(strlen(convStr.c_str())+1);
		memset(putBuffer, '\0', strlen(convStr.c_str())+1);
		memcpy(&putBuffer[0], &convStr[0], strlen(convStr.c_str()));

		char *storeUOID = (char*)malloc(21);
		memset(storeUOID, '\0', 21);
		memcpy(&storeUOID[0],&storeHeader[1],20);									// copy to global UOID to decide originator of status message

		CommonData_put(STOR, &(storeUOID[0]), storTTL,  dataLength, uint16_t(init.Port), putBuffer, 65000);

	}	// send message only if TTL > 0

}	




