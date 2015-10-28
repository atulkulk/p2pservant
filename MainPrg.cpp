/*************************************************************************************************************/
/*												Project : CSCI551 Final Part 1 Project 						*/
/*												Filename: MainPrg.cpp (Servant Implementation)				*/
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/
#include "CommonHeaderFile.h"
#include "ParseFile.h"
#include "DoHello.h"
#include "ProcessWrite.h"

using namespace std;

// Common variables used for maintaing state
bool gnShutdown;
bool MeBeacon;
double StartTime;
struct timeval SimStart;
long int SimStart_Sec;
long int SimStart_usec;

sigset_t newSet;
struct sigaction act;

string tmplogfiles;

int main(int argc, char *argv[])
{
	struct stat tmp_struct, tmpServ_struct, *ini_neighbour_stat, *servStat;
	ini_neighbour_stat=&tmp_struct;
	servStat = &tmpServ_struct;

	MeBeacon = false;
	gnShutdown = false;

	// get the current time from the system
	pthread_mutex_lock(&logmutex);

	gettimeofday(&SimStart,NULL);
    SimStart_Sec=SimStart.tv_sec;
    SimStart_usec=SimStart.tv_usec;
	
	pthread_mutex_unlock(&logmutex);

	// Parse the command line arguments
	ParseCommandLine(argc,argv);
  
	sigemptyset(&newSet);									// block the SIGINT signals
	sigaddset(&newSet, SIGINT);
	pthread_sigmask(SIG_BLOCK, &newSet, NULL);

 	// Main while loop....All functions are called from this loop. Used for self restart
	while (!gnShutdown)
	{

		node_inst_id = getNodeInstanceId();

		InitRandom();

		ParseINIFile(iniName);					// parse the Ini file

		if(ResetFlag == true)	
		{
			resetNode();			//Resetting Node
		}

		loadFiles();				//Loading all the index files

		tmplogfiles = init.HomeDir;
		tmplogfiles=tmplogfiles+"/"+init.LogFilename;
		
		if(stat(&tmplogfiles[0], servStat) != 0)   // check if the servant is present
		{
			fLog.open(tmplogfiles.c_str(), fstream::in | fstream::out | fstream::app);
			fLog.close();	
		}

		// Check if the current node is beacon node or an ordinary node...
		for (int i=0;i<BeaconCount ;i++ )
		{
		  if(init.Port==beacon[i].PortNo)
		  {
			  MeBeacon=true;								// flag set if current node is beacon node
		  }
		}

		if (MeBeacon == false)							// not a beacon node
		{
			string tmpfiles (init.HomeDir);
	        tmpfiles=tmpfiles+"/init_neighbor_list";

			if(stat(&tmpfiles[0],ini_neighbour_stat) == 0)                                 // check if the init_neighbor_list is present
			{
				HelloInit(tmpfiles);
			}
			else
			{
			    JoinInit();
			}
		}
		else                                                // current node is a beacon node
		{
		     BeaconInit();
		}
		
	}

	return 0;
}

