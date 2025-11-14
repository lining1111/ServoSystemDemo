//
// Created by lining on 10/24/24.
//
#include "MyTcpServer.h"
#include "localBusiness/localBusiness.h"

MyTcpServerHandler::MyTcpServerHandler(const StreamSocket &socket) : TCPServerConnection(socket) {
    __socket = socket;
    _peerAddress = socket.peerAddress().toString();
    LOG(WARNING) << "connection from " << _peerAddress;
    recvBuf = new char[1024 * 1024];
    startBusiness();
}

MyTcpServerHandler::~MyTcpServerHandler() {
    LOG(WARNING) << _peerAddress << " disconnected";
    stopBusiness();
    __socket.close();
    delete[] recvBuf;
}

void MyTcpServerHandler::run() {
    memset(recvBuf, 0, 1024 * 1024);
    int recvLen = 1024 * 1024;
    int len = 0;
    auto localBusiness = LocalBusiness::instance();
    localBusiness->addConn(this);
    try {
        do {
            recvLen = (_fsm->GetWriteLen() < (1024 * 1024)) ? _fsm->GetWriteLen() : (1024 * 1024);
            len = __socket.receiveBytes(recvBuf, recvLen);
            _fsm->TriggerAction(recvBuf, len);
        } while (len > 0);
    } catch (Poco::Exception &e) {
        LOG(WARNING) << e.displayText();
    }
    localBusiness->delConn(_peerAddress);
    //!!!Here, do not delete this because Poco will enter the destructor.
}


MyTcpServer::MyTcpServer(int port) : _port(port) {
}

MyTcpServer::~MyTcpServer() {
    Stop();
    LOG(WARNING) << "server:" << to_string(_port) << " close";
}

int MyTcpServer::Open() {
    try {
        srv = new Poco::Net::TCPServer(new MyTcpConnectionFactory, ServerSocket(_port));
    } catch (Poco::Exception &e) {
        LOG(ERROR) << "tcp server err:" << e.displayText();
        return -1;
    }
    isListen = true;
    return 0;
}

int MyTcpServer::ReOpen() {
    try {
        srv = new Poco::Net::TCPServer(new MyTcpConnectionFactory, ServerSocket(_port));
    } catch (Poco::Exception &e) {
        LOG(ERROR) << "tcp server err:" << e.displayText();
        return -1;
    }
    isListen = true;
    return 0;
}


int MyTcpServer::Run() {
    //Starting TCP Server
    srv->start();
    return 0;
}

int MyTcpServer::Stop() {
    srv->stop();
    return 0;
}
