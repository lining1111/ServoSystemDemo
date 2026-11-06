//
// Created by lining on 10/24/24.
//
#include "MyTcpServer.h"
#include "logHelper/LogHelper.h"
#include "localBusiness/localBusiness.h"

MyTcpServerHandler::MyTcpServerHandler(const StreamSocket &socket) : TCPServerConnection(socket) {
    socket_ = socket;
    _name = socket.peerAddress().toString();
    LogHelper::Instance()->TcpLog(spdlog::level::warn, "connection from {}", _name);
    recvBuf = new char[1024 * 1024];
    CommonHandler::startBusiness();
}

MyTcpServerHandler::~MyTcpServerHandler() {
    LogHelper::Instance()->TcpLog(spdlog::level::warn, "{} disconnected", _name);
    CommonHandler::stopBusiness();
    socket_.close();
    delete[] recvBuf;
}

void MyTcpServerHandler::run() {
    memset(recvBuf, 0, 1024 * 1024);
    int recvLen = 1024 * 1024;
    const auto localBusiness = LocalBusiness::instance();
    localBusiness->addConn(this);
    try {
        int len = 0;
        do {
            recvLen = (_fsm->GetWriteLen() < (1024 * 1024)) ? _fsm->GetWriteLen() : (1024 * 1024);
            len = socket_.receiveBytes(recvBuf, recvLen);
            _fsm->TriggerAction(recvBuf, len);
        } while (len > 0);
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::warn, "{}", e.displayText());
    }
    localBusiness->delConn(_name);
    //!!!Here, do not delete this because Poco will enter the destructor.
}


MyTcpServer::MyTcpServer(const int port) : _port(port) {
}

MyTcpServer::~MyTcpServer() {
    if (srv) {
        srv->stop();
        delete srv;
    }
    LogHelper::Instance()->TcpLog(spdlog::level::warn, "tcp server:{} close", _port);
}

int MyTcpServer::Open() {
    try {
        srv = new Poco::Net::TCPServer(new MyTcpConnectionFactory, ServerSocket(_port));
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "tcp server err:{}", e.displayText());
        return -1;
    }
    srv->start();
    isListen = true;
    return 0;
}

int MyTcpServer::ReOpen() {
    try {
        srv = new Poco::Net::TCPServer(new MyTcpConnectionFactory, ServerSocket(_port));
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "tcp server err:{}", e.displayText());
        return -1;
    }
    srv->start();
    isListen = true;
    return 0;
}
