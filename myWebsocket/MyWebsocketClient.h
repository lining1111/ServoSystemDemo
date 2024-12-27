//
// Created by lining on 12/27/24.
//

#ifndef MYWEBSOCKETCLIENT_H
#define MYWEBSOCKETCLIENT_H

#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/NetException.h"
#include <Poco/Exception.h>
#include "Poco/Observer.h"
#include "Poco/NObserver.h"
#include <glog/logging.h>
#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>

#include "fsm/FSM.h"

using namespace std;

using Poco::Net::SocketReactor;
using Poco::Net::SocketNotification;
using Poco::Net::ReadableNotification;
using Poco::Net::WritableNotification;
using Poco::Net::TimeoutNotification;
using Poco::Net::ShutdownNotification;
using Poco::Observer;
using Poco::Net::NetException;
using Poco::Net::ConnectionRefusedException;
using Poco::Net::InvalidSocketException;
using Poco::TimeoutException;

class MyWebsocketClient {
private:
    Poco::Net::HTTPClientSession *_cs;
    Poco::Net::HTTPRequest *_req;
    Poco::Net::HTTPResponse *_rsp;
    Poco::Net::WebSocket *_ws;
    std::mutex *mtx = nullptr;
    int BUFFER_SIZE = 1024 * 1024 * 4;
    FSM *_fsm = nullptr;
    Poco::NotificationQueue _pkgs;
    string _pkgCache;
public:
    string server_ip;
    int server_port;
    string _peerAddress;
    bool isNeedReconnect = true;
    char *recvBuf = nullptr;
    std::thread _tRecv;
    std::thread _tHeartbeat;
    bool _isRun = false;
    bool isLocalThreadRun = false;
    shared_future<int> future_t1;
    shared_future<int> future_t2;
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

    static void ThreadHearbeat(MyWebsocketClient *local);

};


#endif //MYWEBSOCKETCLIENT_H
