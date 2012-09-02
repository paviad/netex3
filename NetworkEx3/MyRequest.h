#pragma once

#include <string>
#include <cctype>
#include <unordered_set>

using namespace std;

class MyRequest {
private:
    int state;

    static unordered_set<string> allowedMethods;
    static char invalidPathChars[];

    static class __initializer {
    public:
        __initializer();
    };
    static __initializer __initializerInstance;

public:
    bool methodValid;
    string method;
    bool urlValid;
    string url;
    bool httpVersionValid;
    string httpVersion;
    bool valid;
    bool aborted;
    bool done;
    vector<char> data;
    int contentLength;
    string rawHeader;

    string tmpHeader;

private:
    void CheckMethod() {
        methodValid = allowedMethods.find(method) != allowedMethods.end();
    }

    void CheckURL() {
        // First, decode the URL by handling "%xx" (xx = two digit hex number), and "+" occurrences.
        string newurl;
        char hex[3] = { '\0' };
        int tmp = 0;
        for(string::iterator it = url.begin(); it != url.end(); ++it) {
            if(tmp == 0 && *it == '+') newurl.push_back(' ');
            else if(tmp == 0 && *it != '%') newurl.push_back(*it);
            else if(tmp == 0 && *it == '%') tmp = 1;
            else if(tmp == 1 && isxdigit(*it)) {
                hex[0] = *it;
                tmp = 2;
            }
            else if(tmp == 1 && *it == '%') {
                newurl.push_back('%');
                tmp = 0;
            }
            else if(tmp == 2 && isxdigit(*it)) {
                hex[1] = *it;
                char newch = strtol(hex, NULL, 16);
                newurl.push_back(newch);
                tmp = 0;
            }
            else return;
        }
        if(tmp == 0) {
            url = newurl;
            urlValid = true;
        }
    }

    void CheckHTTPVersion() {
        // We only support HTTP/1.1
        if(httpVersion != "HTTP/1.1") return;
        httpVersionValid = true;
    }

public:
    MyRequest() {
        state = 0;
        contentLength = 0;
        done = aborted = httpVersionValid = methodValid = urlValid = valid = false;
    }

    void AppendData(char *buf, int length) {
        vector<char> newData;
        int newLength = length;
        if(data.size() + newLength > contentLength) {
            aborted = true;
        }
        else {
            newData.assign(buf, buf + newLength);
            data.insert(data.end(), newData.begin(), newData.end());
            if(data.size() == contentLength)
                done = true;
        }
    }

    void ParseChunk(char *buf, int length) {
        if(length == 0) {
            return;
        }
        if(state == 999) { // Just append the data
            AppendData(buf, length);
            return;
        }
        for(int i = 0; i < length; i++) {
            char ch = buf[i];
            rawHeader.push_back(ch);
            again:
            switch(state) {
            case 0: // expect method
                if(!isupper(ch)) state = -1; // any non upper case character is illegal here, terminate.
                else {
                    state = 1;
                    goto again;
                }
                break;
            case 1: // read method, expect whitespace
                if(ch == '\r' || ch == '\n') state = -1; // new line is illegal here, terminate.
                else if(isspace(ch)) {
                    CheckMethod();
                    if(methodValid) state = 2;
                    else state = -1; // illegal method received, terminate
                }
                else method.push_back(ch);
                break;
            case 2: // eat whitespace, expect non-whitespace
                if(ch == '\r' || ch == '\n') state = -1; // new line is illegal here, terminate.
                else if(!isspace(ch)) {
                    state = 3;
                    goto again;
                }
                break;
            case 3: // read url, expect whitespace
                if(ch == '\r' || ch == '\n') state = -1; // new line is illegal here, terminate.
                else if(isspace(ch)) {
                    CheckURL();
                    if(urlValid) state = 4;
                    else state = -1; // illegal url received, terminate
                }
                else url.push_back(ch);
                break;
            case 4: // eat whitespace, expect http version
                if(ch == '\r' || ch == '\n') state = -1; // new line is illegal here, terminate.
                else if(!isspace(ch)) {
                    state = 5;
                    goto again;
                }
                break;
            case 5: // read http version, expect whitespace or newline
                if(ch == '\r') state = 1005; // new line means we're past the first line, start parsing headers.
                else if(ch == '\n') state = -1; // lf here is illegal.
                else if(isspace(ch)) {
                    CheckHTTPVersion();
                    if(httpVersionValid) state = 6;
                    else state = -1; // illegal HTTP version received, terminate.
                }
                else httpVersion.push_back(ch);
                break;
            case 1005: // expect \n
                if(ch != '\n') state = -1; // always expect \n after \r, otherwise terminate.
                else {
                    CheckHTTPVersion();
                    if(httpVersionValid) state = 7;
                    else state = -1; // illegal HTTP version received, terminate.
                }
                break;
            case 6: // eat whitespace, expect newline
                if(ch == '\r') state = 1005; // new line means we're now in the headers section.
                else if(ch == '\n') state = -1; // lf here is illegal, terminate.
                else if(!isspace(ch)) state = -1; // any non-whitespace character is illegal here, terminate.
                break;
            case 7: // expect header or empty line.
                if(ch == '\r') state = 1007;
                else if(ch == '\n') state = -1; // lf here is illegal, terminate.
                else {
                    if(!tmpHeader.empty() && isspace(ch)) tmpHeader.push_back(' '); // folding
                    else if(!tmpHeader.empty()) {
                        ProcessHeader(tmpHeader);
                        tmpHeader.clear();
                    }
                    state = 8;
                    goto again;
                }
                break;
            case 1007: // expect \n
                if(ch != '\n') state = -1; // always expect \n after \r, otherwise terminate.
                else {
                    if(!tmpHeader.empty()) ProcessHeader(tmpHeader);
                    state = 999; // empty header line, request header ended.
                }
                break;
            case 8: // eat header line, expect newline
                if(ch == '\r') state = 1008;
                else if(ch == '\n') state = -1; // lf here is illegal, terminate.
                else tmpHeader.push_back(ch);
                break;
            case 1008: // expect \n
                if(ch != '\n') state = -1; // always expect \n after \r, otherwise terminate.
                else state = 7;
                break;
            case 999:
                valid = true;
                AppendData(buf + i, length - i);
                return;
            }
        }
        if(state == 999) {
            valid = true;
            if(contentLength == 0) done = true;
        }
        if(state == -1) aborted = true;
    }

    void ProcessHeader(string hdr);
};
