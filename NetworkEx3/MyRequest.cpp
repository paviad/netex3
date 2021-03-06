#include "MyRequest.h"
#include <iostream>

using namespace std;

unordered_set<string> MyRequest::allowedMethods;
unordered_set<string> MyRequest::commentedHeaders;
char MyRequest::invalidPathChars[] = "<>:\"/\\|?*";

MyRequest::__initializer MyRequest::__initializerInstance;

MyRequest::__initializer::__initializer()
{
    allowedMethods.emplace("GET");
    allowedMethods.emplace("PUT");
    allowedMethods.emplace("HEAD");
    allowedMethods.emplace("DELETE");
    allowedMethods.emplace("TRACE");

    commentedHeaders.emplace("Server");
    commentedHeaders.emplace("User-Agent");
    commentedHeaders.emplace("Via");
}

static bool ieq(const string &a, const string &b) {
    return _stricmp(a.c_str(), b.c_str()) == 0;
}

void MyRequest::ProcessHeader(string &hdr, string &value)
{
    //cout << socketId << " >>> " << hdr << ": " << value << endl;
    if(ieq(hdr, "Content-Length")) 
        contentLength = atoi(value.c_str());
    else if(ieq(hdr, "Connection") && ieq(value, "Keep-Alive"))
        keepAlive = true;
    else if(ieq(hdr, "User-Agent")) {
    }
}

#ifdef USE_EXTERNAL_HTTP_PARSER
int on_message_complete(http_parser *parser)
{
    MyRequest *e = (MyRequest*)parser->data;
    e->OnMessageComplete(parser);
    return 0;
}

void MyRequest::OnMessageComplete(http_parser *parser)
{
    http_method method = (http_method)parser->method;
    this->method = http_method_str(method);
    char tmp[20];
    sprintf(tmp, "HTTP/%d.%d", parser->http_major, parser->http_minor);
    httpVersion = tmp;
    CheckURL(true);
    CheckMethod();
    CheckHTTPVersion();
    valid = urlValid && httpVersionValid && methodValid;
    if(contentLength != data.size()) {
        done = false;
        aborted = true;
    }
    else {
        done = valid;
        aborted = !done;
    }
}

int on_url_received(http_parser *parser, const char *at, size_t length)
{
    MyRequest *e = (MyRequest*)parser->data;
    e->OnURLReceived(parser, at, length);
    return 0;
}

void MyRequest::OnURLReceived(http_parser *parser, const char *at, size_t length)
{
    url.append(at, length);
}

int on_body_received(http_parser *parser, const char *at, size_t length)
{
    MyRequest *e = (MyRequest*)parser->data;
    e->OnBodyReceived(parser, at, length);
    return 0;
}

void MyRequest::OnBodyReceived(http_parser *parser, const char *at, size_t length)
{
    if(!tmpHeader.empty()) {
        ProcessHeader(tmpHeader, trim(tmpHeaderValue));
        tmpHeader.clear();
        tmpHeaderValue.clear();
    }
    data.insert(data.end(), at, at + length);
}

int on_header_field_received(http_parser *parser, const char *at, size_t length)
{
    MyRequest *e = (MyRequest*)parser->data;
    e->OnHeaderFieldReceived(parser, at, length);
    return 0;
}

void MyRequest::OnHeaderFieldReceived(http_parser *parser, const char *at, size_t length)
{
    if(!tmpHeader.empty()) {
        ProcessHeader(tmpHeader, tmpHeaderValue);
        tmpHeader.clear();
        tmpHeaderValue.clear();
    }
    tmpHeader.append(at, at + length);
}

int on_header_value_received(http_parser *parser, const char *at, size_t length)
{
    MyRequest *e = (MyRequest*)parser->data;
    e->OnHeaderValueReceived(parser, at, length);
    return 0;
}

void MyRequest::OnHeaderValueReceived(http_parser *parser, const char *at, size_t length)
{
    tmpHeaderValue.append(at, at + length);
}
#endif
