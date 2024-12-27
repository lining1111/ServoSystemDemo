//
// Created by lining on 12/27/24.
//

#ifndef MYWEBSOCKETSERVERHANDLER_H
#define MYWEBSOCKETSERVERHANDLER_H

#include <mutex>
#include <iostream>
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerSession.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Util/ServerApplication.h"
#include "common/common.h"
#include "common/proc.h"
#include "common/config.h"
#include "fsm/FSM.h"
#include <future>

using namespace Poco::Net;
using namespace std;

//websocket请求的处理
class MyWebSocketRequestHandler : public Poco::Net::HTTPRequestHandler {
private:
    std::mutex *mtx = nullptr;
    int BUFFER_SIZE = 1024 * 1024 * 4;
    FSM *_fsm = nullptr;
    Poco::NotificationQueue _pkgs;
    string _pkgCache;
public:
    string _peerAddress;
    char *recvBuf = nullptr;
    bool _isRun = false;
    bool isLocalThreadRun = false;
    shared_future<int> future_t1;
    shared_future<int> future_t2;
    uint64_t timeRecv = 0;
    uint64_t timeSend = 0;
    MyWebSocketRequestHandler(size_t bufSize = 1024);

    ~MyWebSocketRequestHandler();


    virtual void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response);

    int SendBase(string pkg);

    int Send(char *buf_send, int len_send);

private:
    size_t _bufSize;
    WebSocket *_ws;

    void startBusiness();

    void stopBusiness();

    void Action();

    //获取包
    static int ThreadStateMachine(MyWebSocketRequestHandler *local);

    //处理包
    static int ThreadProcessPkg(MyWebSocketRequestHandler *local);
};

class MyWebsocketHandler : public Poco::Net::HTTPRequestHandlerFactory {
public:
    MyWebsocketHandler(std::size_t bufSize = 1024) : _bufSize(bufSize) {

    }

    Poco::Net::HTTPRequestHandler *createRequestHandler(const HTTPServerRequest &request) {
        return new MyWebSocketRequestHandler(_bufSize);
    }

private:
    std::size_t _bufSize;
};


#endif //MYWEBSOCKETSERVERHANDLER_H
