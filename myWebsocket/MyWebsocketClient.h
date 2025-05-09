//
// Created by lining on 12/27/24.
//

#ifndef MYWEBSOCKETCLIENT_H
#define MYWEBSOCKETCLIENT_H


#include <glog/logging.h>
#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <Poco/Exception.h>
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/HTTPClientSession.h"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>

#include "fsm/FSM.h"

using namespace std;

using Poco::Net::WebSocket;
using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;

class MyWebsocketClient {
private:
    HTTPClientSession *_cs;
    HTTPRequest *_req;
    HTTPResponse *_rsp;
    WebSocket *_ws;
    std::mutex *mtx = nullptr;
    int BUFFER_SIZE = 1024 * 1024 * 4;
    FSM *_fsm = nullptr;
    Poco::NotificationQueue _pkgs;
    string _pkgCache;
    char *recvBuf = nullptr;
    bool _isRun = false;
    bool isLocalThreadRun = false;
    shared_future<int> future_t1;
    shared_future<int> future_t2;
public:
    string server_ip;
    int server_port;
    string _peerAddress;
    bool isNeedReconnect = true;
    std::thread _tRecv;
    std::thread _tHeartbeat;
    uint64_t timeSend = 0;
    uint64_t timeRecv = 0;
public:
    MyWebsocketClient(string serverip, int serverport);

    ~MyWebsocketClient();

    int Open();

    int Reconnect();

    int Run();

    int SendBase(string pkg);

    int Send(char *buf_send, int len_send);

private:
    void startBusiness();

    void stopBusiness();

    void Action();

    static int ThreadRecv(MyWebsocketClient *local);

    static int ThreadStateMachine(MyWebsocketClient *local);

    static int ThreadProcessPkg(MyWebsocketClient *local);

    static void ThreadHeartbeat(MyWebsocketClient *local);

};


#endif //MYWEBSOCKETCLIENT_H
