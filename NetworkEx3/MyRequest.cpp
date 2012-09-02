#include "MyRequest.h"
#include <iostream>

using namespace std;

unordered_set<string> MyRequest::allowedMethods;
char MyRequest::invalidPathChars[] = "<>:\"/\\|?*";

MyRequest::__initializer MyRequest::__initializerInstance;

MyRequest::__initializer::__initializer()
{
    allowedMethods.emplace("GET");
    allowedMethods.emplace("PUT");
    allowedMethods.emplace("HEAD");
    allowedMethods.emplace("DELETE");
    allowedMethods.emplace("TRACE");
}

void MyRequest::ProcessHeader(string hdr)
{
    string contentLengthHeaderField = "Content-Length: ";
    int l = contentLengthHeaderField.length();
    if(hdr.substr(0, l) == contentLengthHeaderField) {
        contentLength = atoi(hdr.substr(l).c_str());
    }
}
