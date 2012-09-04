#pragma once

#include <string>
#include <time.h>

using namespace std;

class MyResponse
{
public:
    int code;
    string responseText;
    string content;
    string contentType;
    long long contentLength;
    bool manuallySent;
public:
    MyResponse(void); // Calls Clear() and nothing more.
    ~MyResponse(void);

    void Clear() {
        contentType = "text/html";
        contentLength = 0;
        manuallySent = false;
    }

    string GetResponseHeader() {
        char tmp[20], headerTmp[200];
        string rc;
        rc.append("HTTP/1.1 ");
        _itoa_s(code, tmp, sizeof(tmp) - 1, 10);
        rc.append(tmp);
        rc.push_back(' ');
        rc.append(responseText);
        rc.append("\r\n");

        time_t t;
        struct tm *tmp_tm;
        char dateBuf[80];

        t = time(NULL);
        tmp_tm = gmtime(&t);
        strftime(dateBuf, sizeof(dateBuf), "%a, %d %b %Y %H:%M:%S", tmp_tm);
        sprintf(headerTmp, "Date: %s GMT\r\n", dateBuf);
        rc.append(headerTmp);

        //rc.append("Connection: close\r\n");
        rc.append("Connection: Keep-Alive\r\n");
        if(contentLength > 0) {
            sprintf(headerTmp, "Content-Length: %I64d\r\n", contentLength);
            rc.append(headerTmp);
            rc.append("Content-Type: ");
            rc.append(contentType);
            rc.append("\r\n");
        }
        rc.append("\r\n");
        return rc;
    }

    string GetResponseText() {
        string rc = GetResponseHeader();

        if(contentLength > 0) {
            rc.append(content.c_str());
        }

        return rc;
    }
};
