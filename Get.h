using namespace std;

void callGet(string readBuffer);
void HandleGet_REQ(int socDesc, int myCTBPos,struct Header ARH1, char *readBuffer);
void HandleGet_RSP(int socDesc,int myCTBPos,struct Header ARH1);
void sendGetResponseMessage(char *msg_header, char *Request, int socDesc, uint32_t dataLen);
