//
// Created by lining on 2023/8/21.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "MyTcpHandler.hpp"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/SocketConnector.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/Observer.h"
#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>

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

class MyTcpServerHandler : public MyTcpHandler, public Poco::Net::TCPServerConnection {
public:
    char *recvBuf = nullptr;

public:
    explicit MyTcpServerHandler(const StreamSocket &socket);

    ~MyTcpServerHandler() override;

    void run() override;
};


class MyTcpConnectionFactory final : public Poco::Net::TCPServerConnectionFactory {
public:
    MyTcpConnectionFactory() = default;

    Poco::Net::TCPServerConnection *createConnection(const StreamSocket &socket) override {
        return new MyTcpServerHandler(socket);
    }
};


class MyTcpServer {
public:
    int _port;
    bool isListen = false;
    Poco::Net::TCPServer *srv = nullptr;

public:
    explicit MyTcpServer(int port);

    ~MyTcpServer();

    int Open();

    int ReOpen();
};


#endif //TCPSERVER_H
