//
// Created by lining on 2023/8/21.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <memory>
#include "MyTcpHandler.hpp"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/SocketConnector.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/Observer.h"
#include <glog/logging.h>

using Poco::Net::SocketReactor;
using Poco::Net::SocketConnector;
using Poco::Net::SocketAcceptor;
using Poco::Net::StreamSocket;
using Poco::Net::ServerSocket;
using Poco::Net::SocketAddress;
using Poco::Net::SocketNotification;
using Poco::Net::ReadableNotification;
using Poco::Net::WritableNotification;
using Poco::Net::TimeoutNotification;
using Poco::Net::ShutdownNotification;
using Poco::Observer;
using Poco::Net::NetException;

class MyTcpServerHandler : public MyTcpHandler{
public:
    SocketReactor &_reactor;
    char *recvBuf = nullptr;
public:
    MyTcpServerHandler(StreamSocket &socket, SocketReactor &reactor);

    ~MyTcpServerHandler();

    void onReadable(ReadableNotification *pNf);

    void onSocketShutdown(ShutdownNotification *pNf);

};

class MyTcpServer {
public:
    int _port;
    Poco::Net::ServerSocket _s;
    Poco::Net::SocketReactor _reactor;
    Poco::Thread _t;
    Poco::Net::SocketAcceptor<MyTcpServerHandler> *_acceptor = nullptr;
    bool isListen = false;
public:
    MyTcpServer(int port);

    ~MyTcpServer();

    int Open();

    int ReOpen();

    int Run();

};


#endif //TCPSERVER_H
