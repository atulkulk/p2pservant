/************************************************************************************************************/
/*												Project : CSCI551 Final Part 1 Project 						*/
/*												Filename: ParseCmd.cpp (Servant Implementation)				*/
/*												Name: Atul Kulkarni, Abhishek Deshmukh						*/
/************************************************************************************************************/
#include "CommonHeaderFile.h"

using namespace std;

bool ResetFlag;
char *iniName;

// Print out error messages ...Used from warmup1 and warmup 2
void Usage(int SwitchUse)															 // print out specific error messages
    {
		printf("\n");
		switch (SwitchUse)
		{
		case 1:																		//Insufficient arguments
		   {
	        printf("\tonly valid option is -reset.... \n");
			printf("\t.ini filename is mandatory.... \n");
 		    break;
		   }
   
         default : break;
		} //exit switch  
        
        exit(1);
    }

// function modified from the FAQ given for project 1
void ParseCommandLine(int argc, char *argv[])									// called from Client.cpp			
    {
	ResetFlag=false;

        argc--, argv++; /* skip the original argv[0] */
         
        for (argc=argc, argv=argv; argc > 0; argc--, argv++) 
		{
            if (*argv[0] == '-') 
			{
				    if (strcmp(*argv,"-reset") == 0)
					{
                      ResetFlag=true;
			        }	
					else
				    {
					  Usage(1);
					}
            }  // end if argv[0]
		    else 
		    {
				iniName=(char*)malloc(strlen(*argv));
				strncpy(iniName,*argv,strlen(*argv)); 
            }
        } // end for

		if (iniName==NULL)
		{
			printf("Please enter the .ini file name...\n");
			exit(1);
		}

//	    printf("ResetFlag = %d\n", ResetFlag);
//		printf("INIFILE = %s\n", iniName);

		return;

    } // end function

