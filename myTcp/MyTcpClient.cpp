//
// Created by lining on 10/24/24.
//
#include "MyTcpClient.h"

MyTcpClient::MyTcpClient(string serverip, int serverport) : server_ip(serverip), server_port(serverport) {
    recvBuf = new char[1024 * 1024];
    //    _reactor.addEventHandler(_socket, Observer<MyTcpClient, ReadableNotification>(*this,
    //                                                                                  &MyTcpClient::onReadable));
    startBusiness();
}

MyTcpClient::~MyTcpClient() {
    LOG(WARNING) << _peerAddress << " disconnected";

    try {
        _reactor.removeEventHandler(__socket, Observer<MyTcpClient, ReadableNotification>(*this,
                                        &MyTcpClient::onReadable));
    } catch (Poco::Exception &e) {
        LOG(ERROR) << e.displayText();
    }

    stopBusiness();
    delete[]recvBuf;
}

int MyTcpClient::Open() {
    SocketAddress sa(server_ip, server_port);
    try {
        Poco::Timespan ts(1000 * 1000);
        __socket.connect(sa, ts);
    } catch (ConnectionRefusedException &) {
        LOG(ERROR) << server_ip << ":" << server_port << " connect refuse";
        return -1;
    } catch (TimeoutException &) {
        LOG(ERROR) << server_ip << ":" << server_port << " connect time out";
        return -1;
    } catch (NetException &) {
        LOG(ERROR) << server_ip << ":" << server_port << " net exception";
        return -1;
    } catch (Poco::Exception &e) {
        LOG(ERROR) << server_ip << ":" << server_port << " exception:" << e.displayText();
        return -1;
    }

    _peerAddress = __socket.peerAddress().toString();
    LOG(WARNING) << "connection to " << _peerAddress << " success";
    Poco::Timespan ts1(1000 * 100);
    __socket.setSendTimeout(ts1);
    __socket.setKeepAlive(true);
    __socket.setLinger(true, 0);

    try {
        _reactor.addEventHandler(__socket, Observer<MyTcpClient, ReadableNotification>(*this,
                                     &MyTcpClient::onReadable));
    } catch (Poco::Exception &e) {
        LOG(ERROR) << e.displayText();
    }

    isNeedReconnect = false;
    timeSend = 0;
    timeRecv = 0;
    return 0;
}

int MyTcpClient::Reconnect() {
    __socket.close();
    SocketAddress sa(server_ip, server_port);
    try {
        Poco::Timespan ts(1000 * 1000);
        __socket.connect(sa, ts);
    } catch (ConnectionRefusedException &) {
        LOG(ERROR) << server_ip << ":" << server_port << " connect refuse";
        return -1;
    } catch (TimeoutException &) {
        LOG(ERROR) << server_ip << ":" << server_port << " connect time out";
        return -1;
    } catch (NetException &) {
        LOG(ERROR) << server_ip << ":" << server_port << " net exception";
        return -1;
    } catch (Poco::Exception &e) {
        LOG(ERROR) << server_ip << ":" << server_port << " exception:" << e.displayText();
        return -1;
    }

    _peerAddress = __socket.peerAddress().toString();
    LOG(WARNING) << "reconnection to " << _peerAddress << " success";
    Poco::Timespan ts1(1000 * 100);
    __socket.setSendTimeout(ts1);
    __socket.setKeepAlive(true);
    __socket.setLinger(true, 0);

    try {
        _reactor.addEventHandler(__socket, Observer<MyTcpClient, ReadableNotification>(*this,
                                     &MyTcpClient::onReadable));
    } catch (Poco::Exception &e) {
        LOG(ERROR) << e.displayText();
    }

    isNeedReconnect = false;
    timeSend = 0;
    timeRecv = 0;
    return 0;
}

int MyTcpClient::Run() {
    _t.start(_reactor);
    _tHeartbeat = std::thread(ThreadHeartbeat, this);
    _tHeartbeat.detach();

    return 0;
}

void MyTcpClient::onReadable(ReadableNotification *pNf) {
    pNf->release();
    if (_fsm == nullptr) {
        LOG(ERROR) << _peerAddress << " _fsm null";
        return;
    }
    if (isNeedReconnect) {
        return;
    }

    memset(recvBuf, 0, 1024 * 1024);
    int recvLen = (_fsm->GetWriteLen() < (1024 * 1024)) ? _fsm->GetWriteLen() : (1024 * 1024);
    try {
        int len = __socket.receiveBytes(recvBuf, recvLen);
        if (len <= 0) {
            LOG(ERROR) << _peerAddress << " receiveBytes " << len;
            isNeedReconnect = true;
        } else {
            _fsm->TriggerAction(recvBuf, len);
        }
    } catch (Poco::Exception &e) {
        LOG(ERROR) << _peerAddress << " receive error:" << e.code() << ","
                << e.displayText();
        if (e.code() != POCO_ETIMEDOUT && e.code() != POCO_EWOULDBLOCK &&
            e.code() != POCO_EAGAIN) {
            isNeedReconnect = true;
        }
    }
}


void MyTcpClient::ThreadHeartbeat(MyTcpClient *local) {
    LOG(WARNING) << local->_peerAddress << " heartbeat thread start";
    while (local->_isRun) {
        std::this_thread::sleep_for(5s);
        if (!local->isNeedReconnect) {
            try {
                Com req;
                req.code = "Heartbeat";
                req.param = "req";
                string msg = json::encode(req);
                int ret = local->SendBase(msg);
                if (ret < 0) {
                    LOG(ERROR) << local->_peerAddress << " send err";
                    local->isNeedReconnect = true;
                }
            } catch (Poco::Exception &e) {
                LOG(ERROR) << local->_peerAddress << " send error:" << e.code()
                        << e.displayText();
                if (e.code() != POCO_ETIMEDOUT && e.code() != POCO_EWOULDBLOCK &&
                    e.code() != POCO_EAGAIN) {
                    local->isNeedReconnect = true;
                }
            }
        }
    }
    LOG(WARNING) << local->_peerAddress << " heartbeat thread end";
}
