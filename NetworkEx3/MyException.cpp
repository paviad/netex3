#include <WinSock2.h>
#include "MyException.h"
#include <string.h>
#include <errno.h>
#include <iostream>
#include "util.h"
#include "trim.h"

using namespace std;

MyException::MyException(CallType callType, const string &contextMsg, bool fatal, int id)
    : callType(callType), fatal(fatal), id(id)
{
    this->contextMsg = contextMsg;
    switch(callType) {
    case CallType::STDC:
        errorMsg = trim(strerror(errno), " \t\n\r");
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
        errorMsg = trim(tmp, " \t\n\r");
    }
}

void MyException::Print()
{
    Highlight(2);
    cout << id << " Error: " << contextMsg << " - " << errorMsg << endl;
    Highlight(0);
}
