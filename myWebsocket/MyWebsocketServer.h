//
// Created by lining on 12/27/24.
//

#ifndef MYWEBSOCKETSERVER_H
#define MYWEBSOCKETSERVER_H

#include <memory>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include "common/ComHandler.hpp"
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

//websocket request handler
class MyWebSocketRequestHandler : public HTTPRequestHandler, public common::CommonHandler {
private:
    WebSocket *_ws = nullptr;
    char *recvBuf = nullptr;

public:
    MyWebSocketRequestHandler();

    ~MyWebSocketRequestHandler() override;


    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) final;

    int SendBase(string pkg) final;

    int Send(char *buf_send, int len_send) final;
};

class MyWebsocketHandler : public HTTPRequestHandlerFactory {
public:
    MyWebsocketHandler() = default;

    HTTPRequestHandler *createRequestHandler(const HTTPServerRequest &request) final {
        return new MyWebSocketRequestHandler();
    }
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

    int Run() const;

    int Stop() const;
};


#endif //MYWEBSOCKETSERVER_H
