//
// Created by lining on 12/27/24.
//

#ifndef MYWEBSOCKETSERVER_H
#define MYWEBSOCKETSERVER_H

#include <mutex>
#include <iostream>
#include "fsm/FSM.h"
#include <future>
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>

using Poco::Net::WebSocket;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;

using namespace Poco::Net;
using namespace std;

//websocket请求的处理
class MyWebSocketRequestHandler : public HTTPRequestHandler {
private:
    size_t _bufSize;
    WebSocket *_ws;
    std::mutex *mtx = nullptr;
    FSM *_fsm = nullptr;
    Poco::NotificationQueue _pkgs;
    string _pkgCache;
    char *recvBuf = nullptr;
    bool _isRun = false;
    bool isLocalThreadRun = false;
    shared_future<int> future_t1;
    shared_future<int> future_t2;
public:
    string _peerAddress;
    uint64_t timeRecv = 0;
    uint64_t timeSend = 0;

    MyWebSocketRequestHandler(size_t bufSize = 1024 * 1024 * 4);

    ~MyWebSocketRequestHandler();


    virtual void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response);

    int SendBase(string pkg);

    int Send(char *buf_send, int len_send);

private:
    void startBusiness();

    void stopBusiness();

    void Action();

    //获取包
    static int ThreadStateMachine(MyWebSocketRequestHandler *local);

    //处理包
    static int ThreadProcessPkg(MyWebSocketRequestHandler *local);
};

class MyWebsocketHandler : public HTTPRequestHandlerFactory {
public:
    MyWebsocketHandler(std::size_t bufSize = 1024 * 1024 * 4) : _bufSize(bufSize) {

    }

    HTTPRequestHandler *createRequestHandler(const HTTPServerRequest &request) {
        return new MyWebSocketRequestHandler(_bufSize);
    }

private:
    std::size_t _bufSize;
};

class MyWebsocketServer {
public:
    int _port;
    bool isListen = false;
    Poco::Net::HTTPServer *srv = nullptr;
    MyWebsocketServer(int port);

    ~MyWebsocketServer();

    int Open();

    int ReOpen();

    int Run();

    int Stop();
};


#endif //MYWEBSOCKETSERVER_H
