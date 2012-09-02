#pragma once

#include <WinSock2.h>
#include "MyException.h"
#include <unordered_set>
#ifdef USE_EXTERNAL_HTTP_PARSER
#include "..\http-parser\http_parser.h"
#endif
#include "MyRequest.h"
#include "MyResponse.h"

#define MAX_SOCKETS 20
#define WEBSERVER_PORT 27015
#define LISTENER_BACKLOG 5

using namespace std;

enum SocketState {
    LISTEN,
    REQUEST,
    REPLY
};

class MySocket
{
private:
    SOCKET socketId;
    SocketState state;
    string requestURL;
    string entityFullPath;
    string entityFileExtension;
    bool entityFileExists;
    long long entityFileSize;

#ifdef USE_EXTERNAL_HTTP_PARSER
    http_parser_settings settings;
    http_parser parser;
#endif

    MyRequest request;
    MyResponse response;

    static unordered_set<MySocket*> sockets;
    static vector<MySocket*> todelete;

public:
    static bool purgeReceived;

private:
    void SendString(const char *str) {
        int l = strlen(str);
        send(socketId, str, l, 0);
    }

    string CanonicalRequestedPath(bool &isInWebRoot);

    void AddToSet(fd_set *set) {
        FD_SET(socketId, set);
    }

    bool IsInSet(fd_set *set) {
        return FD_ISSET(socketId, set);
    }

    static void MarkForDeletion(MySocket *e) {
        todelete.push_back(e);
    }

#ifdef USE_EXTERNAL_HTTP_PARSER
    friend static int on_url_received(http_parser *parser, const char *at, size_t length);
    friend static int on_message_complete(http_parser *parser);

    void OnURLReceived(http_parser *parser, const char *at, size_t length);
    void OnMessageComplete(http_parser *parser);
#endif

    bool CheckPath();
    void HandleBadRequest();
    void HandleTrace();
    void HandleDelete();
    void HandlePut();
    void HandleGetHead();
public:
    MySocket(SOCKET s, bool listener);
    ~MySocket(void);

    static void DeleteAll() {
        for(unordered_set<MySocket*>::iterator e = sockets.begin(); e != sockets.end(); ++e)
            MarkForDeletion(e._Ptr->_Myval);
        DeleteMarkedSockets();
    }

    void Clear() {
        MyException *myEx = NULL;
        if(closesocket(socketId) == SOCKET_ERROR)
            throw MyException(CallType::WSA, "closesocket in Clear", false);
    }

    static void AddAllToRecvSet(fd_set *recvSet) {
        for(unordered_set<MySocket*>::iterator pe = sockets.begin(); pe != sockets.end(); ++pe)
        {
            MySocket *e = *pe;
            if(e->state == SocketState::LISTEN || e->state == SocketState::REQUEST)
                e->AddToSet(recvSet);
        }
    }

    static void AddAllToSendSet(fd_set *sendSet) {
        for(unordered_set<MySocket*>::iterator pe = sockets.begin(); pe != sockets.end(); ++pe)
        {
            MySocket *e = *pe;
            if(e->state == SocketState::REPLY)
                e->AddToSet(sendSet);
        }
    }

    static void ProcessAllInRecvSet(fd_set *recvSet) {
        for(unordered_set<MySocket*>::iterator pe = sockets.begin(); pe != sockets.end(); ++pe)
        {
            MySocket *e = *pe;
            if(e->IsInSet(recvSet)) {
                if(e->state == SocketState::LISTEN) {
                    try { e->Accept(); }
                    catch(MyException myEx) {
                        if(myEx.fatal) throw;
                        else myEx.Print();
                    }
                }
                else {
                    try { e->Receive(); }
                    catch(MyException myEx) { 
                        if(myEx.fatal) throw;
                        else {
                            myEx.Print();
                            MarkForDeletion(e);
                        }
                    }
                }
            }
        }
    }

    static void ProcessAllInSendSet(fd_set *sendSet) {
        for(unordered_set<MySocket*>::iterator pe = sockets.begin(); pe != sockets.end(); ++pe)
        {
            MySocket *e = *pe;
            if(e->IsInSet(sendSet)) {
                try { e->Send(); }
                catch(MyException) { MarkForDeletion(e); }
            }
        }
    }

    static void DeleteMarkedSockets() {
        for(vector<MySocket*>::iterator e = todelete.begin(); e != todelete.end(); ++e)
            delete *e;
        todelete.clear();
    }

    static void PrintStatus();

    void Accept();

    void Receive();

    void Send();
};
