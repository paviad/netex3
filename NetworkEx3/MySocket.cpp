#include "MySocket.h"
#include "MyResponse.h"
#include <iostream>
#include <time.h>
#include <fstream>
#include <streambuf>
#include <Shlwapi.h>
#include <iterator>
#include "util.h"

using namespace std;

unordered_set<MySocket*> MySocket::sockets;
vector<MySocket*> MySocket::todelete;
bool MySocket::purgeReceived = false;

MySocket::MySocket(SOCKET s, bool listener)
{
    if(sockets.size() == MAX_SOCKETS)
        throw MyException(CallType::MY, "No more room for sockets", false, s);
    sockets.emplace(this);
    socketId = s;
    request.socketId = socketId;
    lastSendOrReceive = time(NULL);
    if(listener) {
        state = SocketState::LISTEN;
        sockaddr_in addr;
        int len = sizeof(addr);
        getsockname(s, (sockaddr*)&addr, &len);
        cout << s << " Listening on " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << endl;
    }
    else {
        state = SocketState::REQUEST;
    }
}

MySocket::~MySocket(void)
{
    sockets.erase(this);
    try { Clear(); }
    catch(MyException myEx) {
        cerr << "MySocket destructor threw exception: " << myEx.contextMsg << " - " << myEx.errorMsg << endl;
    }
    catch(...) { }
}

void MySocket::Accept()
{
    struct sockaddr_in from;		// Address of sending partner
    int fromLen = sizeof(from);

    SOCKET msgSocket = accept(socketId, (struct sockaddr *)&from, &fromLen);
    if (INVALID_SOCKET == msgSocket)
        throw MyException(CallType::WSA, "accept in Accept", false, socketId);

    cout << msgSocket << " Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

    //
    // Set the socket to be in non-blocking mode.
    //
    unsigned long flag = 1;
    if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0) {
        if(closesocket(msgSocket) == SOCKET_ERROR)
            throw MyException(CallType::WSA, "closesocket in Accept", false, socketId);
        throw MyException(CallType::WSA, "ioctlsocket in Accept", false, socketId);
    }

    new MySocket(msgSocket, false);
}

void MySocket::Receive()
{
    char buf[4096];
    int recvLen;
    int consumed = 0;

    lastSendOrReceive = time(NULL);

    recvLen = recv(socketId, buf, sizeof(buf) - 1, 0);
    if(recvLen == SOCKET_ERROR) {
        string msg = "recv in Receive (";
        char tmp[30];
        msg += itoa(socketId, tmp, 10);
        msg += ")";
        throw MyException(CallType::WSA, msg, false, socketId);
    }

    buf[recvLen] = '\0';
again:
    consumed += request.ParseChunk(buf + consumed, recvLen - consumed);
    if(request.done || request.aborted) {
        //cout << request.rawHeader;
        requestQueue.push(request);
        bool wasAborted = request.aborted;
        request.Reset();
        if(!wasAborted && consumed < recvLen) goto again;
    }
    if(requestQueue.empty()) {
        if (recvLen == 0) {
            Highlight(1);
            cout << socketId << " Connection closed between requests" << endl;
            Highlight(0);
            MarkForDeletion(this);
        }
    }
    else if(requestQueue.front().done || requestQueue.front().aborted) {
        state = SocketState::REPLY;
    }
    return;
}

/**
* @param location The location of the registry key. For example "Software\\Bethesda Softworks\\Morrowind"
* @param name the name of the registry key, for example "Installed Path"
* @return the value of the key or an empty string if an error occured.
*/
static string getRegKey(const string& location, const string& name){
    HKEY key;
    char value[1024]; 
    DWORD bufLen = 1024*sizeof(TCHAR);
    long ret;
    ret = RegOpenKeyExA(HKEY_CLASSES_ROOT, location.c_str(), 0, KEY_QUERY_VALUE, &key);
    if( ret != ERROR_SUCCESS ){
        return string();
    }
    ret = RegQueryValueExA(key, name.c_str(), 0, 0, (LPBYTE) value, &bufLen);
    RegCloseKey(key);
    if ( (ret != ERROR_SUCCESS) || (bufLen > 1024*sizeof(TCHAR)) ){
        return string();
    }
    string stringValue = string(value, (size_t)bufLen - 1);
    size_t i = stringValue.length();
    while( i > 0 && stringValue[i-1] == '\0' ){
        --i;
    }
    return stringValue.substr(0,i); 
}

static int repl(string &str, const string &find, const string &replace)
{
    int cnt = 0;
    size_t start_pos = 0;
    while((start_pos = str.find(find, start_pos)) != std::string::npos) {
        str.replace(start_pos, find.length(), replace);
        start_pos += replace.length();
        cnt++;
    }
    return cnt;
}

string MySocket::CanonicalRequestedPath(bool &isInWebRoot)
{
    // Get current directory of server, append /wwwroot to it this is our base path
    // We will not serve files not under this directory.
    char curDir[MAX_PATH];
    GetCurrentDirectoryA(sizeof(curDir), curDir);
    PathAppendA(curDir, "wwwroot");
    string basePath = curDir;

    // Clean up the request URL by converting forward slashes to backslashes, and compacting
    // multiple consecutive backslashes to one.
    string strClean = requestQueue.front().url;
    // First make all forward slashes in this path, backslashes
    int cnt = repl(strClean, "/", "\\");
    // Now collapse all consecutive backslashes to a single one
    do
    {
        cnt = repl(strClean, "\\\\", "\\");
    } while (cnt);

    // Compose the full path of the requested object
    PathAppendA(curDir, strClean.c_str());
    string fullPath = curDir;

    // Canonicalize the full path by considering ./ and ../ elements.
    char canonicalPath[MAX_PATH];
    PathCanonicalizeA(canonicalPath, fullPath.c_str());

    isInWebRoot = fullPath.substr(0, basePath.length()) == basePath;
    return canonicalPath;
}

void MySocket::HandleBadRequest()
{
    response.code = 400;
    response.responseText = "Bad Request";

    string content;
    if(!requestQueue.front().methodValid) {
        response.code = 501;
        response.responseText = "Not Implemented";
        content = "<h1>Method Not Implemented (" + requestQueue.front().method + ")</h1>";
    }
    else if(!requestQueue.front().urlValid) content = "<h1>Bad Request (Invalid URL: '" + requestQueue.front().url + "')</h1>";
    else if(!requestQueue.front().httpVersionValid) {
        response.code = 505;
        response.responseText = "HTTP Version Not Supported";
        content = "<h1>HTTP Version Not Supported (" + requestQueue.front().httpVersion + ")</h1>";
    }
    else content = "<h1>Bad Request (Malformed Request)</h1>";

    Highlight(1);
    cout << socketId << " Bad request: " << response.responseText << endl;
    Highlight(0);

    response.content = content;
    response.contentLength = content.length();
}

void MySocket::HandleTrace()
{
    response.code = 200;
    response.responseText = "OK";
    response.content = requestQueue.front().rawHeader;
    response.contentLength = response.content.length();
    response.contentType = "text/plain";
}

void MySocket::HandleDelete()
{
    if(!CheckPath()) return;
    if(!entityFileExists) {
        response.code = 404;
        response.responseText = "Not Found";
        response.content = "<h1>Sorry!</h1>";
        response.contentLength = response.content.length();
    }
    else if(DeleteFileA(entityFullPath.c_str()) != 0) { // success
        response.code = 204;
        response.responseText = "No Content";
    }
    else {
        MyException myEx(CallType::WSA, "DeleteFile", false, socketId);
        response.code = 500;
        response.responseText = "Internal Server Error";
        response.content = "<h1>Internal Error (DELETE)</h1>Couldn't delete file: " + myEx.errorMsg;
        response.contentLength = response.content.length();
    }
}

void MySocket::HandlePut()
{
    if(!CheckPath()) return;
    if(!entityFileExists) {
        response.code = 201;
        response.responseText = "Created";
    }
    else {
        response.code = 204;
        response.responseText = "No Content";
    }
    ofstream ofst(entityFullPath, ios::binary | ios::trunc);
    if(ofst.fail()) {
        response.code = 500;
        response.responseText = "Internal Server Error";
        response.content = "<h1>Internal Error (PUT)</h1>Couldn't create file";
        response.contentLength = response.content.length();
    }
    else {
        ostream_iterator<char> oi(ofst);
        copy(requestQueue.front().data.begin(), requestQueue.front().data.end(), oi);
    }
    ofst.close();
}

void MySocket::HandleGetHead()
{
    if(!CheckPath()) return;
    if(!entityFileExists) {
        response.code = 404;
        response.responseText = "Not Found";
        response.content = "<h1>Sorry!</h1>";
        response.contentLength = response.content.length();
    }
    else {
        response.code = 200;
        response.responseText = "OK";
        ifstream t(entityFullPath, ios::binary);

        string contentType = getRegKey(entityFileExtension, "Content Type");
        response.contentType = contentType;
        response.contentLength = entityFileSize;
        //cout << response.GetResponseHeader() << endl;

        SendString(response.GetResponseHeader().c_str());
        SendString("\r\n");

        if(requestQueue.front().method == "GET") {
            int readCount, total = 0;
            vector<char> buf(response.contentLength, 0);
            t.read(&buf[0], buf.size());
            readCount = t.gcount();
            t.close();
            if(send(socketId, &buf[0], readCount, 0) == SOCKET_ERROR)
                throw MyException(CallType::WSA, "send in HandleGetHead (GET)", false, socketId);
            total += readCount;
            cout << socketId << " Sent a total of " << total << " bytes" << endl;
        }
        response.manuallySent = true;
    }
}

bool MySocket::CheckPath()
{
    bool isInWebRoot;
    entityFullPath = CanonicalRequestedPath(isInWebRoot);

    cout << socketId << " Request canonical path: " << entityFullPath << endl;
    if(!isInWebRoot) {
        response.code = 403;
        response.responseText = "Forbidden";
        response.content = "<h1>Sorry the specified URL lies outside of the web root!</h1>";
        response.contentLength = response.content.length();
        return false;
    }
    else if(PathIsDirectoryA(entityFullPath.c_str())) {
        response.code = 403;
        response.responseText = "Forbidden";
        response.content = "<h1>Requested entity is a directory.</h1>";
        response.contentLength = response.content.length();
        return false;
    }
    entityFileExtension = PathFindExtensionA(entityFullPath.c_str());
    WIN32_FILE_ATTRIBUTE_DATA fileAttributes;
    entityFileExists = GetFileAttributesExA(entityFullPath.c_str(), GET_FILEEX_INFO_LEVELS::GetFileExInfoStandard, &fileAttributes) != 0;
    if(entityFileExists) {
        long long l = fileAttributes.nFileSizeLow;
        long long h = fileAttributes.nFileSizeHigh;
        entityFileSize = (h << 32) + l;
    }
    return true;
}

void MySocket::Send()
{
    lastSendOrReceive = time(NULL);

    response.Clear();
    cout << socketId << " processing " << requestQueue.front().method << " " << requestQueue.front().url << endl;
    if(!requestQueue.front().done) {
        HandleBadRequest();
    }
    else if(requestQueue.front().method == "TRACE") {
        HandleTrace();
    }
    else if(requestQueue.front().method == "DELETE") {
        HandleDelete();
    }
    else if(requestQueue.front().method == "PUT") {
        HandlePut();
    }
    else { // GET or HEAD
        HandleGetHead();
    }
    if(!response.manuallySent)
        SendString(response.GetResponseText().c_str());
    bool keepAlive = requestQueue.front().keepAlive;
    requestQueue.pop();
    if(!keepAlive)
        MarkForDeletion(this);
    else if(requestQueue.empty())
        state = SocketState::REQUEST;
    //MarkForDeletion(this);
}

void MySocket::PrintStatus()
{
    cout << "Total sockets: " << sockets.size() << endl;
}
