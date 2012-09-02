#include <WinSock2.h>
#include "MyException.h"
#include <string.h>
#include <errno.h>
#include <iostream>

using namespace std;

MyException::MyException(CallType callType, char *contextMsg, bool fatal) 
    : callType(callType), fatal(fatal)
{
    this->contextMsg = contextMsg;
    switch(callType) {
    case CallType::STDC:
        errorMsg = strerror(errno);
        break;
    case CallType::WSA:
        FillWSAErrorMessage();
        break;
    case CallType::MY:
        break;
    }
}

MyException::~MyException(void)
{
}

void MyException::FillWSAErrorMessage()
{
    char tmp[200];
    int errCode = WSAGetLastError(), size;

    if(errCode != 0) {
        size = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM, // use windows internal message table
            0,       
            errCode, 
            0,       
            tmp,
            sizeof(tmp),
            0);
        errorMsg = tmp;
    }
}

void MyException::Print()
{
    cerr << "Error: " << contextMsg << " - " << errorMsg << endl;
}
