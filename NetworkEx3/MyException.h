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
    int id;

    MyException(CallType callType, const string &contextMsg, bool fatal, int id);
    ~MyException(void);

    void Print();
};

