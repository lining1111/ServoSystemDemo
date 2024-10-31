//
// Created by lining on 2023/8/21.
//

#ifndef MYTCPCLIENT_H
#define MYTCPCLIENT_H

#include "MyTcpHandler.hpp"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/SocketConnector.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"
#include <Poco/Exception.h>
#include "Poco/Observer.h"
#include "Poco/NObserver.h"
#include <glog/logging.h>
#include <iostream>

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
using Poco::Net::ConnectionRefusedException;
using Poco::Net::InvalidSocketException;
using Poco::TimeoutException;

class MyTcpClient : public MyTcpHandler {
public:
    string server_ip;
    int server_port;
    bool isNeedReconnect = true;
    SocketReactor _reactor;
    Poco::Thread _t;
    char *recvBuf = nullptr;
    std::thread _tRecv;
    std::thread _tHeartbeat;
public:
    MyTcpClient(string serverip, int serverport);

    ~MyTcpClient();

    int Open();

    int Reconnect();

    int Run();
private:
    void onReadable(ReadableNotification *pNf);

    static void ThreadHearbeat(MyTcpClient *local);
};

#endif //MYTCPCLIENT_H
