//
// Created by lining on 10/24/24.
//
#include "MyTcpServer.h"
#include "localBusiness/localBusiness.h"

MyTcpServerHandler::MyTcpServerHandler(StreamSocket &socket, SocketReactor &reactor) :
        _reactor(reactor) {
    _socket = socket;
    _socket.setLinger(true, 0);
    _peerAddress = socket.peerAddress().toString();
    LOG(WARNING) << "connection from " << _peerAddress;
    auto localBusiness = LocalBusiness::instance();
    localBusiness->addConn(this);
    _reactor.addEventHandler(_socket, Observer<MyTcpServerHandler, ShutdownNotification>(*this,
                                                                                         &MyTcpServerHandler::onSocketShutdown));
    _reactor.addEventHandler(_socket, Observer<MyTcpServerHandler, ReadableNotification>(*this,
                                                                                         &MyTcpServerHandler::onReadable));
    recvBuf = new char[1024 * 1024];
    startBusiness();
}

MyTcpServerHandler::~MyTcpServerHandler() {
    LOG(WARNING) << _peerAddress << " disconnected";
    stopBusiness();
    _reactor.removeEventHandler(_socket, Observer<MyTcpServerHandler, ShutdownNotification>(*this,
                                                                                            &MyTcpServerHandler::onSocketShutdown));
    _reactor.removeEventHandler(_socket, Observer<MyTcpServerHandler, ReadableNotification>(*this,
                                                                                            &MyTcpServerHandler::onReadable));
//    _socket.close();
    delete[] recvBuf;
}

void MyTcpServerHandler::onReadable(ReadableNotification *pNf) {
    pNf->release();
    if (_fsm == nullptr) {
        LOG(ERROR) << _peerAddress << " _fsm null";
        return;
    }

    memset(recvBuf, 0, 1024 * 1024);
    int recvLen = (_fsm->GetWriteLen() < (1024 * 1024)) ? _fsm->GetWriteLen() : (1024 * 1024);
    try {
        int len = _socket.receiveBytes(recvBuf, recvLen);
        if (len <= 0) {
            LOG(WARNING) << _peerAddress << " receiveBytes " << len;
            auto localBusiness = LocalBusiness::instance();
            localBusiness->delConn(_peerAddress);
            delete this;
        } else {
            _fsm->TriggerAction(recvBuf, len);
        }
    }
    catch (Poco::Exception &e) {
        LOG(WARNING) << e.displayText();
        auto localBusiness = LocalBusiness::instance();
        localBusiness->delConn(_peerAddress);
        delete this;
    }
}

void MyTcpServerHandler::onSocketShutdown(ShutdownNotification *pNf) {
    pNf->release();
    auto localBusiness = LocalBusiness::instance();
    localBusiness->delConn(_peerAddress);
    delete this;
}


MyTcpServer::MyTcpServer(int port) : _port(port) {

}

MyTcpServer::~MyTcpServer() {
    _reactor.stop();
    _s.close();
    delete _acceptor;
    LOG(WARNING) << "server:" << to_string(_port) << " close";
}

int MyTcpServer::Open() {
    try {
        _s.bind(Poco::Net::SocketAddress(_port));
        _s.listen();
    } catch (Poco::Exception &e) {
        LOG(ERROR) << e.displayText();
        isListen = false;
        return -1;
    }
    _s.setReusePort(true);
    _s.setLinger(true, 0);
    _s.setKeepAlive(true);
    _acceptor = new Poco::Net::SocketAcceptor<MyTcpServerHandler>(_s, _reactor);
    isListen = true;
    return 0;
}

int MyTcpServer::ReOpen() {
    try {
        _s.close();
        _s.bind(Poco::Net::SocketAddress(_port));
        _s.listen();
    } catch (Poco::Exception &e) {
        LOG(ERROR) << e.displayText();
        isListen = false;
        return -1;
    }
    if (_acceptor != nullptr) {
        delete _acceptor;
    }
    _s.setReusePort(true);
    _s.setLinger(true, 0);
    _s.setKeepAlive(true);
    _acceptor = new Poco::Net::SocketAcceptor<MyTcpServerHandler>(_s, _reactor);
    isListen = true;
    return 0;
}


int MyTcpServer::Run() {
    //Starting TCP Server
    _t.start(_reactor);
    LOG(WARNING) << _port << "-Server Started";
    LOG(WARNING) << _port << "-Ready To Accept the connections";
    return 0;
}