//
// Created by lining on 12/27/24.
//

#ifndef MYWEBSOCKETCLIENT_H
#define MYWEBSOCKETCLIENT_H
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include <string>
#include <thread>
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/HTTPClientSession.h"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include "common/ComHandler.hpp"

using namespace std;

using Poco::Net::WebSocket;
using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;

class MyWebsocketClient : public common::CommonHandler {
private:
    HTTPClientSession *_cs = nullptr;
    HTTPRequest *_req = nullptr;
    HTTPResponse *_rsp = nullptr;
    WebSocket *_ws = nullptr;
    char *recvBuf = nullptr;

public:
    string server_ip;
    int server_port;
    bool isNeedReconnect = true;
    std::thread _tRecv;
    std::thread _tHeartbeat;

public:
    MyWebsocketClient(string serverip, int serverport);

    ~MyWebsocketClient() override;

    int Open();

    int Reconnect();

    int Run();

    int SendBase(string pkg) final;

    int Send(char *buf_send, int len_send) final;

private:
    static int ThreadRecv(MyWebsocketClient *local);

    static void ThreadHeartbeat(MyWebsocketClient *local);
};


#endif //MYWEBSOCKETCLIENT_H
