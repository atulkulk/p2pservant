sv_node: dictionary.o strlib.o iniparser.o HandleRead.o ProcessWrite.o Delete.o Get.o StatusFile.o Search.o Store.o Connection.o ParseFile.o ParseCmd.o DoHello.o MainPrg.o
	g++ -Wall -lnsl -lpthread -lsocket dictionary.o strlib.o iniparser.o HandleRead.o Delete.o Get.o Search.o Store.o StatusFile.o ProcessWrite.o Connection.o ParseFile.o ParseCmd.o DoHello.o MainPrg.o -o sv_node -L/home/scf-22/csci551b/openssl/lib -lcrypto
MainPrg.o: CommonHeaderFile.h DoHello.h ParseFile.h MainPrg.cpp
	g++ -Wall -c MainPrg.cpp
DoHello.o: CommonHeaderFile.h MainPrg.h Connection.h ParseFile.h ProcessWrite.h HandleRead.h DoHello.cpp
	g++ -Wall -c DoHello.cpp -I/home/scf-22/csci551b/openssl/include
Connection.o: CommonHeaderFile.h ParseFile.h MainPrg.h DoHello.h ProcessWrite.h HandleRead.h Connection.cpp
	g++ -Wall -c Connection.cpp
Delete.o: CommonHeaderFile.h DoHello.h ParseFile.h Store.h HandleRead.h Connection.h Delete.cpp
	g++ -Wall -c Delete.cpp
Get.o: CommonHeaderFile.h ParseFile.h Search.h Store.h DoHello.h Connection.h HandleRead.h ProcessWrite.h Get.cpp
	g++ -Wall -c Get.cpp
Search.o: CommonHeaderFile.h Store.h ParseFile.h ProcessWrite.h HandleRead.h DoHello.h Connection.h MainPrg.h Search.cpp
	g++ -Wall -c Search.cpp
Store.o: CommonHeaderFile.h ProcessWrite.h HandleRead.h DoHello.h Connection.h ParseFile.h MainPrg.h iniparser.h Store.cpp
	g++ -Wall -c Store.cpp -I/home/scf-22/csci551b/openssl/include
StatusFile.o: CommonHeaderFile.h ProcessWrite.h DoHello.h Store.h ParseFile.h HandleRead.h Connection.h StatusFile.cpp
	g++ -Wall -c StatusFile.cpp
ProcessWrite.o: CommonHeaderFile.h MainPrg.h Connection.h DoHello.h ParseFile.h HandleRead.h Store.h ProcessWrite.cpp
	g++ -Wall -c ProcessWrite.cpp
HandleRead.o: CommonHeaderFile.h Connection.h MainPrg.h DoHello.h ParseFile.h ProcessWrite.h HandleRead.cpp
	g++ -Wall -c HandleRead.cpp
ParseCmd.o: CommonHeaderFile.h ParseCmd.cpp 
	g++ -Wall -c ParseCmd.cpp
ParseFile.o: CommonHeaderFile.h iniparser.h ParseFile.cpp
	g++ -Wall -c ParseFile.cpp
iniparser.o: iniparser.h strlib.h dictionary.h iniparser.cpp 
	g++ -Wall -c iniparser.cpp
strlib.o: strlib.h strlib.cpp 
	g++ -Wall -c strlib.cpp
dictionary.o: dictionary.h dictionary.cpp 
	g++ -Wall -c dictionary.cpp

clean: 
	rm -rf *.o sv_node

