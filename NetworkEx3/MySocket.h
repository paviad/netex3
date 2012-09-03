#pragma once

#include <WinSock2.h>
#include "MyException.h"
#include <unordered_set>
#include "MyRequest.h"
#include "MyResponse.h"
#include <queue>

#define MAX_SOCKETS 20
#define WEBSERVER_PORT 27015
#define LISTENER_BACKLOG 5
#define MY_TIMEOUT 120

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
    time_t lastSendOrReceive;

    queue<MyRequest> requestQueue;
    MyRequest request;
    MyResponse response;

    static unordered_set<MySocket*> sockets;
    static vector<MySocket*> todelete;

public:
    static bool purgeReceived;

private:
    void SendString(const char *str) {
        int l = strlen(str);
        if(send(socketId, str, l, 0) == SOCKET_ERROR)
            throw MyException(CallType::WSA, "send in SendString", false, socketId);
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
        cout << socketId << " Closing" << endl;
        MyException *myEx = NULL;
        if(closesocket(socketId) == SOCKET_ERROR)
            throw MyException(CallType::WSA, "closesocket in Clear", false, socketId);
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

    static TIMEVAL *CalculateTimeout() {
        time_t t = time(NULL), min = t;
        bool anyClients = false;
        static TIMEVAL rc;
        for(unordered_set<MySocket*>::iterator pe = sockets.begin(); pe != sockets.end(); ++pe)
        {
            MySocket *e = *pe;
            if(e->state == SocketState::LISTEN) continue;
            anyClients = true;
            if(e->lastSendOrReceive < min)
                min = e->lastSendOrReceive;
        }
        if(!anyClients) return NULL;
        rc.tv_sec = MY_TIMEOUT - (t - min);
        rc.tv_usec = 0;
        return &rc;
    }

    static void RemoveTimeoutConnections() {
        time_t t = time(NULL);
        for(unordered_set<MySocket*>::iterator pe = sockets.begin(); pe != sockets.end(); ++pe)
        {
            MySocket *e = *pe;
            if(e->state == SocketState::LISTEN) continue;
            if(t - e->lastSendOrReceive >= MY_TIMEOUT) {
                cout << e->socketId << " timeout" << endl;
                MarkForDeletion(e);
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
