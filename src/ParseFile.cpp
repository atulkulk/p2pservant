/************************************************************************************************/
/*					Project : CSCI551 Final Part 1 Servant Project		 						*/
/*					Filename: ParseFile.cpp														*/
/*					Credits : Abhishek Deshmukh, Atul Kulkarni									*/
/************************************************************************************************/

//Inclusion of new header file
#include "CommonHeaderFile.h"
#include "iniparser.h"

using namespace std;

//Declartion of variable
typedef struct StInit		//Init File Structure
{
	int Port;
    double Location;
	char *HomeDir;
	char *LogFilename;
    int AutoShutdown;
    int TTL;
    int MsgLifetime;
    int GetMsgLifetime;
    int InitNeighbors;
    int JoinTimeout;
    int KeepAliveTimeout;
    int MinNeighbors;
	int NoCheck;
    double CacheProb;
    double StoreProb;
    double NeighborStoreProb;
    long CacheSize;
	int Retry;
};

struct StInit init;

typedef struct StBeacon		//Beacon Structure
{
	char *HostName;
	int PortNo;
};

struct StBeacon beacon[50];
int BeaconCount=0;

//Description: Removes extra space from the string
//Start of TrimSpaces()
//Reference: Function used from the site "http://codereflect.com/2007/01/31/how-to-trim-leading-or-trailing-spaces-of-string-in-c/"
void TrimSpaces(string& str)
{
	// Trim Both leading and trailing spaces
	size_t startpos = str.find_first_not_of(" \t"); // Find the first character position after excluding leading blank spaces

	size_t endpos = str.find_last_not_of(" \t"); // Find the first character position from reverse af

	// if all spaces or empty return an empty string
	if(( string::npos == startpos ) || ( string::npos == endpos))
	{
	  str = "";
	}
	else
	{
	 str = str.substr( startpos, endpos-startpos+1 );
	}
}
//End of TrimSpaces()

//Description: Removes double quotes and equal sign from the string
//Start of TrimDoubleQuotes()
//Reference: Function used from the site "http://codereflect.com/2007/01/31/how-to-trim-leading-or-trailing-spaces-of-string-in-c/"
char* TrimDoubleQuotes(char* str)
{
	char *buffer=(char*)malloc(strlen(str));

	int keyLen = strlen(str);
	int j=0;
	for (int i = 0; i < keyLen; i++)
	{
		if ((str[i] != '"' ))
		{
			if ((str[i] != '=' )) 
			{
				buffer[j] = str[i];
				j++;
			}
			else 
			{
				buffer[j] = ' ';
				j++;
			}
		}
	}
	buffer[j]='\0';	
	return buffer;
}
//End of TrimDoubleQuotes()



//Description: Parses the init file of the node
//Start of ParseINIFile()
//Reference: INI Files Parser http://merlot.usc.edu/cs551-f09/projects/iniparser/
void ParseINIFile(char *IniName)
{
	struct stat tmp_structure, *buf_stat;
	buf_stat=&tmp_structure;

	string tempIni;

	init.Port=0;
    init.Location=0;

	tempIni="";
	init.HomeDir=&tempIni[0];

    tempIni="servant.log";

    init.LogFilename=(char*)malloc(strlen(tempIni.c_str()));
	strncpy(init.LogFilename,tempIni.c_str(),strlen(tempIni.c_str())); 
	init.LogFilename[strlen(tempIni.c_str())]='\0';

	//Default values
	init.AutoShutdown=900;
    init.TTL=30;
    init.MsgLifetime=30;
    init.GetMsgLifetime=300;
    init.InitNeighbors=3;
    init.JoinTimeout=15;
    init.KeepAliveTimeout=60;
    init.MinNeighbors=2;
	init.NoCheck=0;
    init.CacheProb=0.1;
    init.StoreProb=0.1;
    init.NeighborStoreProb=0.2;
    init.CacheSize=(500*1024);
	init.Retry=30;

    dictionary * d ;

    d = iniparser_load(IniName);
    ifstream myfile (IniName);
	
	tempIni="init:Port";		//Port
	int tmpPort=iniparser_getint(d, &tempIni[0],-99);
	if(tmpPort!= -99)
	{
	  init.Port=tmpPort;
	}
	else
	{
	  printf(".ini file must contain port number...\n");
	  exit(1);
	}

    tempIni="init:Location";	//Location
    double tmpLocation=iniparser_getdouble(d, &tempIni[0],-99);
	if(tmpLocation!= -99)
	{
	  init.Location=tmpLocation;
	}
	else
	{
	  printf(".ini file must contain location...\n");
	  exit(1);
	}

    tempIni="init:HomeDir";		//Home Directory
    char *tmpHomeDir=iniparser_getstring(d, &tempIni[0],NULL);
	if(tmpHomeDir != NULL)
	{
	  init.HomeDir=&tmpHomeDir[0];

	  if(stat(init.HomeDir,buf_stat) != 0)               // check if the HomeDir is present
	  {
	    printf("The HomeDir must be present...Please check the home directory in the .ini file...\n");
		exit(1);
	  }

	  string tmpfiles (init.HomeDir);
	  tmpfiles=tmpfiles+"/files";
  	  string tmpdir (tmpfiles);
      tmpdir = tmpdir + "/tmp";

      if(stat(&tmpfiles[0],buf_stat) != 0)               // check if the files direcory is present
	  {
		  if(mkdir(&tmpfiles[0],0777) == -1)
		  {
		   printf("files direcory can not be created...\n");
		   exit(1);
		  }          
	  }

		if(stat(&tmpdir[0],buf_stat) != 0)               // check if the files direcory is present
		{
			mkdir(&tmpdir[0],0777);
		}

	  
	}
	else
	{
	  printf(".ini file must contain HomeDir...\n");
	  exit(1);
	}

    tempIni="init:LogFilename";			//Log Filename
	char *tmpLogFilename=iniparser_getstring(d, &tempIni[0],NULL);
	if(tmpLogFilename != NULL)
	{
	  init.LogFilename=&tmpLogFilename[0];
	}

	tempIni="init:AutoShutdown";		//Auto Shutdown
	int tmpAutoShutdown=iniparser_getint(d, &tempIni[0],-99);
	if(tmpAutoShutdown!= -99)
	{
	  init.AutoShutdown=tmpAutoShutdown;
	}

	tempIni="init:TTL";					//TTL
	int tmpTTL=iniparser_getint(d, &tempIni[0],-99);
	if(tmpTTL!= -99)
	{
	  init.TTL=tmpTTL;
	}

	tempIni="init:MsgLifetime";			//Message Life Time
	int tmpMsgLifetime=iniparser_getint(d, &tempIni[0],-99);
	if(tmpMsgLifetime!= -99)
	{
	  init.MsgLifetime=tmpMsgLifetime;
	}

	tempIni="init:GetMsgLifetime";		//Get Message Life Time
	int tmpGetMsgLifetime=iniparser_getint(d, &tempIni[0],-99);
	if(tmpGetMsgLifetime!= -99)
	{
	  init.GetMsgLifetime=tmpGetMsgLifetime;
	}

	tempIni="init:InitNeighbors";		//# of Initial Neighbors
	int tmpInitNeighbors=iniparser_getint(d, &tempIni[0],-99);
	if(tmpInitNeighbors!= -99)
	{
	  init.InitNeighbors=tmpInitNeighbors;
	}

	tempIni="init:JoinTimeout";			//Join Timeout
	int tmpJoinTimeout=iniparser_getint(d, &tempIni[0],-99);
	if(tmpJoinTimeout!= -99)
	{
	  init.JoinTimeout=tmpJoinTimeout;
	}

	tempIni="init:KeepAliveTimeout";	//Keepalive Timeout
	int tmpKeepAliveTimeout=iniparser_getint(d, &tempIni[0],-99);
	if(tmpKeepAliveTimeout!= -99)
	{
	  init.KeepAliveTimeout=tmpKeepAliveTimeout;
	}

	tempIni="init:MinNeighbors";		//Minimum # of neighbors
	int tmpMinNeighbors=iniparser_getint(d, &tempIni[0],-99);
	if(tmpMinNeighbors!= -99)
	{
	  init.MinNeighbors=tmpMinNeighbors;
	}

	tempIni="init:NoCheck";				//No Check Flag
	int tmpNoCheck=iniparser_getint(d, &tempIni[0],-99);
	if(tmpNoCheck != -99)
	{
	  init.NoCheck=tmpNoCheck;
	}

	tempIni="init:CacheProb";			//Cache Probability
	double tmpCacheProb=iniparser_getdouble(d, &tempIni[0],-99);
	if(tmpCacheProb != -99)
	{
	  init.CacheProb=tmpCacheProb;
	}

	tempIni="init:StoreProb";			//Store Probability
	double tmpStoreProb=iniparser_getdouble(d, &tempIni[0],-99);
	if(tmpStoreProb != -99)
	{
	  init.StoreProb=tmpStoreProb;
	}

	tempIni="init:NeighborStoreProb";	//Neighbore Store Probability
	double tmpNeighborStoreProb=iniparser_getdouble(d, &tempIni[0],-99);
	if(tmpNeighborStoreProb != -99)
	{
	  init.NeighborStoreProb=tmpNeighborStoreProb;
	}

	tempIni="init:CacheSize";			//Cache Size
	int tmpCacheSize=iniparser_getint(d, &tempIni[0],-99);
	if(tmpCacheSize != -99)
	{
	  init.CacheSize=(tmpCacheSize*1024);
	}

	tempIni="beacons:Retry";			//Retry Time
	int tmpRetry=iniparser_getint(d, &tempIni[0],-99);
	if(tmpRetry != -99)
	{
	  init.Retry=tmpRetry;
	}


	//Extracting Beacon details from the file
	bool beacon_found=false;
	string line;
	if (myfile.is_open())                            // read the file and send to user 
	{
		while (! myfile.eof() )
		{
	        if(beacon_found==false)
			{
			  getline (myfile,line);
			  TrimSpaces(line);

	          int beacon_pos = line.find ("[beacons]");
		      if (beacon_pos != -1)
	          {
		       beacon_found=true;
	          }
			}

			if(beacon_found==true)
			{
			  getline (myfile,line);
			  TrimSpaces(line);

			  int colon_pos = line.find (":");
		      if (colon_pos != -1)					// valid line having hostname and port number
	          {
				 string str_host=line.substr (0,colon_pos);
				 TrimSpaces(str_host);
				 
				 beacon[BeaconCount].HostName=(char*)malloc(strlen(str_host.c_str()));
				 strncpy(beacon[BeaconCount].HostName,str_host.c_str(),strlen(str_host.c_str())); 
				 beacon[BeaconCount].HostName[strlen(str_host.c_str())]='\0';

				 int equal_pos = line.find ("=");		// find the seperator for port number
		         if (equal_pos != -1)
	             {
                   string str_port=line.substr (colon_pos+1,equal_pos);
                   TrimSpaces(str_port);
				  
				   beacon[BeaconCount].PortNo=atoi(str_port.c_str());

					BeaconCount=BeaconCount+1;

				 }		  

				}    // end colon_pos != -1

				int bracket_pos = line.find ("[");
		        if (bracket_pos != -1)
	            {
                  beacon_found=false;
				}

			}         // end if beacon found=true
	   
	  }													// end while file != EOF
	   myfile.close();
	   if (BeaconCount==0)
	   {
		   printf("There must be at least one beacon node for this peer to function....\n");
		   exit(1);
	   }
	 }	 
	 else                                                  // .ini file is not open
	{
	  printf("Invalid .ini file...Please check the file specified..."); 
	  exit(1);
	}

}              // end function 
//End of ParseINIFile()


