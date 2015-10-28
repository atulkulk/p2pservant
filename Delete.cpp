/*************************************************************************************************************/
/*												Project : CSCI551 Final Part 2 Project 						*/
/*												Filename: Delete.cpp (Server Implementation)				*/
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/

#include "CommonHeaderFile.h"
#include "DoHello.h"
#include "ParseFile.h"
#include "Store.h"
#include "HandleRead.h"
#include "Connection.h"

using namespace std;


// Desciption : Function deletes data from the index data structures
// Start of PerformDeletion()

void PerformDeletion(char *FileName, char *charSHA, char *charNonce, char *charPwd )
{

	unsigned char* hexSHA = (unsigned char*)malloc(21);
	memset(hexSHA, '\0', 21);
	hexSHA = CharToHexadec((unsigned char *)charSHA, 40);
	
	unsigned char* hexNonce = (unsigned char*)malloc(21);
	memset(hexNonce, '\0', 21);
	hexNonce = CharToHexadec((unsigned char *)charNonce, 40);

	unsigned char* hexPwd = (unsigned char*)malloc(21);
	memset(hexPwd, '\0', 21);
	hexPwd = CharToHexadec((unsigned char *)charPwd, 40);

	unsigned char storedNonce[20];
	GetSHA((char*)hexPwd, (char*)&storedNonce[0], 20);

    // first check if the provided password is correct or not.. 
	size_t checkStr=memcmp(storedNonce, hexNonce, 20);
    if(checkStr!=0)
	{
		return;											//if not return 				
	}


	bool NonceFound = false;
	bool ShaFound = false;
	bool NameFound = false;

	int filePos=0;

	//check all data structures and see if provided name,sha1,nonce exist
	pthread_mutex_lock(&storemutex);

	for (int i = 0; i < (int)nonce_vector.size(); i++)
	{
		size_t newcheckStr=memcmp(nonce_vector[i].value, hexNonce, 20);
		if(newcheckStr==0)
		{
			filePos=i;
			NonceFound = true;
			break;
		}
	}


	for (int i = 0; i < (int)name_ind_vector.size(); i++)
	{
		string tmpLowerName((char*)name_ind_vector[i].value);
		convert_lower(&tmpLowerName[0]);
		size_t newcheckStr=memcmp (&tmpLowerName[0], FileName, strlen(FileName) );
		
		if(newcheckStr==0)
		{
			NameFound = true;
			break;
		}
	}


	for (int i = 0; i < (int)sha1_ind_vector.size(); i++)
	{
		size_t newcheckStr=memcmp(sha1_ind_vector[i].value, hexSHA, 20);
		if(newcheckStr==0)
		{
			ShaFound = true;
			break;
		}
	}

	pthread_mutex_unlock(&storemutex);

    // delete entries only if all data is correct
	if (NonceFound == true && NameFound == true && ShaFound == true  )
	{
		
	pthread_mutex_lock(&storemutex);

	deleteVectorFilesEntry(nonce_vector[filePos].fileCount);

	for (int i = 0; i < (int)LRU_vector.size(); i++)
	{
		if(LRU_vector[i].localFileCount == nonce_vector[filePos].fileCount)
		{
			LRUcacheSize = LRUcacheSize - LRU_vector[i].FileSize;
			LRU_vector.erase(LRU_vector.begin()+i);
			break;
		}
	}

    pthread_mutex_unlock(&storemutex);
	
	}

}


// Desciption : Function used to read delete request from the network and parse the data
// Start of HandleDel_REQ()

void HandleDel_REQ(int socDesc, int myCTBPos, struct Header ARH1, char *readBuffer)
{
	bool uoidFoundFlag = false;

	int dataSize = COMMON_HEADER_SIZE + ARH1.nw_datalen;

	string dummyStr = " ";
	char *logData = (char*)malloc((int)LOGDATALEN);
	snprintf(logData, ((int)LOGDATALEN), "%s", dummyStr.c_str());

	//Writes into log file
	reportLog("r", "DELT", dataSize, ARH1.nw_TTL, ARH1.UOID, logData, ctb[myCTBPos].ctbPort,false);

	//Checks if the request has already been processed
	uoidFoundFlag = pushUOIDStruture(myCTBPos, ARH1);
	
	if(uoidFoundFlag == false)         //UOID not present i.e. it's a new request
	{
		uint8_t r_deleteTTL=ARH1.nw_TTL - 1;

		string strReadBuffer(readBuffer);
		string fileStr, SHA1Str, nonceStr, outPwdStr;
		string fileName, SHA1, Nonce, outPwd;

		//Parsing delete request 
		size_t found=strReadBuffer.find("\r\n");	//Get fileStr
		if (found!=string::npos)
		{
			fileStr = strReadBuffer.substr(0, found);
			size_t found1=fileStr.find("=");		//Get filename
			if (found1!=string::npos)
			{
				fileName = fileStr.substr(found1+1);
			}
			strReadBuffer = strReadBuffer.substr(found+1);
		}

		found=strReadBuffer.find("\r\n");			//Get SHA1Str
		if (found!=string::npos)
		{
			SHA1Str = strReadBuffer.substr(0, found);
			size_t found1=SHA1Str.find("=");		//Get SHA1
			if (found1!=string::npos)
			{
				SHA1 = SHA1Str.substr(found1+1);
			}
			strReadBuffer = strReadBuffer.substr(found+1);
		}

		found=strReadBuffer.find("\r\n");			//Get NonceStr
		if (found!=string::npos)
		{
			nonceStr = strReadBuffer.substr(0, found);
			size_t found1=nonceStr.find("=");		//Get Nonce
			if (found1!=string::npos)
			{
				Nonce = nonceStr.substr(found1+1);
			}
			strReadBuffer = strReadBuffer.substr(found+1);
		}

		found=strReadBuffer.find("\r\n");			//Get outPwdStr
		if (found!=string::npos)
		{
			outPwdStr = strReadBuffer.substr(0, found);
			size_t found1=outPwdStr.find("=");		//Get outPwd
			if (found1!=string::npos)
			{
				outPwd = outPwdStr.substr(found1+1);
			}
		}		
		
		//call to perform deletion on this node
	    PerformDeletion((char*)fileName.c_str(), (char*)SHA1.c_str(), (char*)Nonce.c_str(), (char*)outPwd.c_str());		

		//Inserts the request into queue for flooding
		if (r_deleteTTL > 0)
		{
		   CommonData_put(ARH1.nw_msg_type, &(ARH1.UOID[0]), r_deleteTTL,  ARH1.nw_datalen, ctb[myCTBPos].ctbPort, readBuffer, 65000);
		}
	}	//Else Message Already Seen
}


// Desciption : Function used to parse commandline data and call perform delete
// Start of callDelete()

void callDelete(string readBuffer)
{
//	int oldState=0;
	size_t found,found1;
	string fileStr, SHA1Str, nonceStr;
	string fileName, SHA1, Nonce;
    unsigned char *outPwd;

	TrimSpaces(readBuffer);
	convert_lower(&readBuffer[0]);

	//delete FileName=foo SHA1=6b6c... Nonce=fe18...

	//Parsing delete request 
	found=readBuffer.find(" ");			//Get fileStr
    if (found!=string::npos)
    {
		fileStr = readBuffer.substr(0, found);
		TrimSpaces(fileStr);
		found1=fileStr.find("=");		//Get filename
		if (found1!=string::npos)
		{
			fileName = fileStr.substr(found1+1);
		}
		readBuffer = readBuffer.substr(found+1);
	}

	found=readBuffer.find(" ");			//Get SHA1Str
    if (found!=string::npos)
    {
		SHA1Str = readBuffer.substr(0, found);
		TrimSpaces(SHA1Str);
		found1=SHA1Str.find("=");		//Get SHA1
		if (found1!=string::npos)
		{
			SHA1 = SHA1Str.substr(found1+1);
		}
		readBuffer = readBuffer.substr(found+1);
	}

	found=readBuffer.find("=");			//Get nonceStr
    if (found!=string::npos)
    {
		Nonce = readBuffer.substr(found+1);
		TrimSpaces(Nonce);
	}
   
	unsigned char* hexNonce = (unsigned char*)malloc(21);
	memset(hexNonce, '\0', 21);
	hexNonce = CharToHexadec((unsigned char *)Nonce.c_str(), 40);
   
	bool OTPFound = false;

	char ans[100];
	int filePos=0;

	pthread_mutex_lock(&storemutex);

	for (int i = 0; i < (int)OneTmPwd_vector.size(); i++)
	{
		//Generating nonce for all the one-time-password
		unsigned char storedNonce[20];
		GetSHA((char*)OneTmPwd_vector[i].value, (char*)&storedNonce[0], 20);

		size_t checkStr=memcmp(storedNonce, hexNonce, 20);		//check if OTP is present
		if(checkStr==0)
		{
			filePos=i;
			OTPFound = true;
			break;
		}
	}

	pthread_mutex_unlock(&storemutex);

   outPwd=(unsigned char*)malloc(41);
   memset(outPwd,'\0',41);

	//generate a random password in case OTP is not present
	if (OTPFound == false)
	{
		printf("No one-time password found.\n");
		printf("Okay to use a random password [yes/no]? ");
		gets(ans);
		string userans (ans);
		size_t ansfound1=userans.find("y");
		size_t ansfound2=userans.find("Y");
		if (ansfound1 != string::npos || ansfound2 != string::npos)
		{
			char obj_type[] = "file";
			//Generating one-time-password for the file
			unsigned char randpass[20];
			GetUOID(node_inst_id, obj_type, (char*)&randpass[0], 20);
	
			outPwd=HexadecToChar((unsigned char *)randpass, 20);
		}
		else
		{
		   return;
		}
	}
	else            // OTP is present
	{
	    outPwd=HexadecToChar((unsigned char *)OneTmPwd_vector[filePos].value, 20);	
	}

    PerformDeletion((char*)fileName.c_str(), (char*)SHA1.c_str(), (char*)Nonce.c_str(), (char*)outPwd );

	 if (init.TTL > 0)
	 {
		// put delete message on the queue

		string strOutpwd((char*)outPwd);

		//append data in a string
		string sendString ="FileName=";
		sendString=sendString+fileName+"\r\nSHA1="+SHA1+"\r\nNonce="+Nonce+"\r\nPassword="+strOutpwd+"\r\n";

		uint32_t dataLength = strlen(sendString.c_str());

		char *deleteHeader = (char*)malloc(COMMON_HEADER_SIZE);
		deleteHeader = GetHeader(node_inst_id, DELT, (uint8_t)init.TTL, dataLength);

		char *deleteUOID = (char*)malloc(21);
		memset(deleteUOID, '\0', 21);
		memcpy(&deleteUOID[0],&deleteHeader[1],20);	// copy to global UOID to decide originator of search message

		char *putBuffer=(char *)malloc(dataLength+1);
		memset(putBuffer, '\0', dataLength+1);
		memcpy(&putBuffer[0], &sendString[0], dataLength);

		//put data on the queue	
		CommonData_put(DELT, &(deleteUOID[0]), init.TTL,  dataLength, uint16_t(init.Port), putBuffer, 65000);

	 }

}



