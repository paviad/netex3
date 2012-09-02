#pragma once

#include <string>

using namespace std;

enum CallType { STDC, WSA, MY };

class MyException
{
private:
    void FillWSAErrorMessage();
public:
    const CallType callType;
    const bool fatal;
    string contextMsg;
    string errorMsg;

    MyException(CallType callType, char *contextMsg, bool fatal);
    ~MyException(void);

    void Print();
};

