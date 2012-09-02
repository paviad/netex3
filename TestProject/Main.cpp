#include "Main.h"
#include <WinSock2.h>
#include <iostream>
#include <string>

using namespace std;

SOCKET s;

void SendString(string str)
{
    send(s, str.c_str(), str.length(), 0);
}

string GetResponse() {
    string rc;
    char buf[10000];
    int bytesRead;
    while(true) {
        bytesRead = recv(s, buf, sizeof(buf) - 1, 0);
        if(bytesRead == 0) break;
        buf[bytesRead] = '\0';
        rc.append(buf);
    }
    return rc;
}

bool PerformTest(string testName, string request, string expected)
{
    cout << "Running " << testName << endl << "===========================" << endl;
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(27015);
    connect(s, (sockaddr*)&addr, sizeof(addr));

    string response;

    cout << request << "----" << endl;
    SendString(request);
    response = GetResponse();
    cout << response << "----" << endl;

    closesocket(s);

    bool rc = response.substr(0, expected.length()) == expected;
    if(!rc) cout << ">>>>>>>>>>>>>>>>FAILED<<<<<<<<<<<<<<<<";
    cout << endl << endl << endl;
    return rc;
}

bool Test1()
{
    string request = "GET / HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 403 Forbidden\r\n";
    
    return PerformTest("Test1", request, expected);
}

bool Test2()
{
    string request = "GET /notthere.html HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 404 Not Found\r\n";
    
    return PerformTest("Test2", request, expected);
}

bool Test3()
{
    string request = "GET /test.htm HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 200 OK\r\n";
    
    return PerformTest("Test3", request, expected);
}

bool Test4()
{
    string request = "PUT /notthere.html HTTP/1.1\r\nContent-Length: 5\r\n\r\nHello";
    string expected = "HTTP/1.1 201 Created\r\n";
    
    return PerformTest("Test4", request, expected);
}

bool Test5()
{
    string request = "GET /notthere.html HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 200 OK\r\n";
    
    return PerformTest("Test5", request, expected);
}

bool Test6()
{
    string request = "PUT /notthere.html HTTP/1.1\r\nContent-Length: 6\r\n\r\nWorld!";
    string expected = "HTTP/1.1 204 No Content\r\n";
    
    return PerformTest("Test6", request, expected);
}

bool Test7()
{
    string request = "GET /notthere.html HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 200 OK\r\n";
    
    return PerformTest("Test7", request, expected);
}

bool Test8()
{
    string request = "DELETE /notthere.html HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 204 No Content\r\n";
    
    return PerformTest("Test8", request, expected);
}

bool Test9()
{
    string request = "GET /notthere.html HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 404 Not Found\r\n";
    
    return PerformTest("Test9", request, expected);
}

bool Test10()
{
    string request = "DELETE /notthere.html HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 404 Not Found\r\n";
    
    return PerformTest("Test10", request, expected);
}

bool Test11()
{
    string request = "PUT /readonly.txt HTTP/1.1\r\nContent-Length: 5\r\n\r\nHello";
    string expected = "HTTP/1.1 500 Internal Server Error\r\n";
    
    return PerformTest("Test11", request, expected);
}

bool Test12()
{
    string request = "DELETE /readonly.txt HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 500 Internal Server Error\r\n";
    
    return PerformTest("Test12", request, expected);
}

bool Test13()
{
    string request = "OPTIONS * HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 501 Not Implemented\r\n";
    
    return PerformTest("Test13", request, expected);
}

bool Test14()
{
    string request = "GET % HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 400 Bad Request\r\n";
    
    return PerformTest("Test14", request, expected);
}

bool Test15()
{
    string request = "GET /test.htm HTTP/1.2\r\n\r\n";
    string expected = "HTTP/1.1 505 HTTP Version Not Supported\r\n";
    
    return PerformTest("Test15", request, expected);
}

bool Test16()
{
    string request = "GET /file+with%20spaces.htm HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 200 OK\r\n";
    
    return PerformTest("Test16", request, expected);
}

bool Test17()
{
    string request = "TRACE /file+with%20spaces.htm HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 200 OK\r\n";
    
    return PerformTest("Test17", request, expected);
}

bool Test18()
{
    string request = "GET /../lala.htm HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 403 Forbidden\r\n";
    
    return PerformTest("Test18", request, expected);
}

bool Test19()
{
    string request = "GET /ex03.doc HTTP/1.1\r\n\r\n";
    string expected = "HTTP/1.1 200 OK\r\n";
    
    return PerformTest("Test19", request, expected);
}

int main(int argc, char *argv[])
{
    bool testsOk = true;
    WSADATA wsaData;

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    testsOk &= Test1();
    testsOk &= Test2();
    testsOk &= Test3();
    testsOk &= Test4();
    testsOk &= Test5();
    testsOk &= Test6();
    testsOk &= Test7();
    testsOk &= Test8();
    testsOk &= Test9();
    testsOk &= Test10();
    testsOk &= Test11();
    testsOk &= Test12();
    testsOk &= Test13();
    testsOk &= Test14();
    testsOk &= Test15();
    testsOk &= Test16();
    testsOk &= Test17();
    testsOk &= Test18();
    testsOk &= Test19();

    WSACleanup();

    if(testsOk)
        cout << "All tests ok" << endl;
    else
        cout << "Some tests failed" << endl;
    
    return 0;
}
