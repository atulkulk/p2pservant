//StatusFile Header file
#include "CommonHeaderFile.h"

using namespace std;

void writeStatusMetadata(string metaFile);
void HandleStatusFiles_REQ(int socDesc, int myCTBPos, struct Header ARH1, char *readBuffer);
