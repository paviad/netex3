#include <WinSock2.h>
#include <iostream>
#include "MySocket.h"

using namespace std;

static int applicationExitCode = 0;

SOCKET CreateListener() {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener == INVALID_SOCKET)
        throw MyException(CallType::WSA, "creating listener socket", true);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(WEBSERVER_PORT);
    if(bind(listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
        throw MyException(CallType::WSA, "binding listener socket", true);

    if(listen(listener, LISTENER_BACKLOG) == SOCKET_ERROR)
        throw MyException(CallType::WSA, "listen", true);

    return listener;
}

void Startup() {
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        throw MyException(CallType::WSA, "WSAStartup", true);
}

void Cleanup() {
    if(WSACleanup() != 0) {
        applicationExitCode = -1;
        MyException myEx(CallType::WSA, "WSACleanup", true);
        myEx.Print();
    }
}

int main(int argc, char *argv[]) {
    fd_set recvSet, sendSet;

    try {
        Startup();
        new MySocket(CreateListener(), true);
    }
    catch(MyException myEx) {
        myEx.Print();
        if(myEx.fatal) {
            applicationExitCode = -1;
            goto cleanup;
        }
    }

    while(!MySocket::purgeReceived) {
        try {
            FD_ZERO(&recvSet);
            FD_ZERO(&sendSet);
            MySocket::AddAllToRecvSet(&recvSet);
            MySocket::AddAllToSendSet(&sendSet);

            //MySocket::PrintStatus();
            if(select(0, &recvSet, &sendSet, NULL, NULL) == SOCKET_ERROR)
                throw MyException(CallType::WSA, "select", true);

            MySocket::ProcessAllInRecvSet(&recvSet);
            MySocket::ProcessAllInSendSet(&sendSet);
            MySocket::DeleteMarkedSockets();
        }
        catch(MyException myEx) {
            myEx.Print();
            if(myEx.fatal) {
                applicationExitCode = -1;
                goto cleanup;
            }
        }
    }

    // We exit on any request that uses the PURGE method.
    // This is just for testing purposes.
    cout << "Normal exit" << endl;
cleanup:
    MySocket::DeleteAll();
    Cleanup();

    return applicationExitCode;
}
