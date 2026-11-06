//
// Created by lining on 10/24/24.
//
#include "MyTcpClient.h"
#include <utility>

MyTcpClient::MyTcpClient(string serverip, int serverport) : server_ip(std::move(serverip)),
                                                            server_port(serverport) {
    recvBuf = new char[1024 * 1024];
    //    _reactor.addEventHandler(_socket, Observer<MyTcpClient, ReadableNotification>(*this,
    //                                                                                  &MyTcpClient::onReadable));
    CommonHandler::startBusiness();
}

MyTcpClient::~MyTcpClient() {
    LogHelper::Instance()->TcpLog(spdlog::level::warn, "{} disconnected", _name);

    try {
        _reactor.removeEventHandler(socket_, Observer<MyTcpClient, ReadableNotification>(*this,
                                        &MyTcpClient::onReadable));
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}", e.displayText());
    }

    CommonHandler::stopBusiness();
    if (_tHeartbeat.joinable()) {
        _tHeartbeat.join();
    }

    delete[]recvBuf;
}

int MyTcpClient::Open() {
    SocketAddress sa(server_ip, server_port);
    try {
        Poco::Timespan ts(1000 * 1000);
        socket_.connect(sa, ts);
    } catch (ConnectionRefusedException &) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}:{} connect refuse", server_ip, server_port);
        return -1;
    } catch (TimeoutException &) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}:{} connect time out", server_ip, server_port);
        return -1;
    } catch (NetException &) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}:{} net exception", server_ip, server_port);
        return -1;
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}:{} exception:{}", server_ip, server_port,
                                      e.displayText());
        return -1;
    }

    _name = socket_.peerAddress().toString();
    LogHelper::Instance()->TcpLog(spdlog::level::warn, "connection to {} success", _name);
    Poco::Timespan ts1(1000 * 100);
    socket_.setSendTimeout(ts1);
    socket_.setKeepAlive(true);
    socket_.setLinger(true, 0);

    try {
        _reactor.addEventHandler(socket_, Observer<MyTcpClient, ReadableNotification>(*this,
                                     &MyTcpClient::onReadable));
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}", e.displayText());
    }

    isNeedReconnect = false;
    timeSend = 0;
    timeRecv = 0;
    return 0;
}

int MyTcpClient::Reconnect() {
    socket_.close();
    SocketAddress sa(server_ip, server_port);
    try {
        Poco::Timespan ts(1000 * 1000);
        socket_.connect(sa, ts);
    } catch (ConnectionRefusedException &) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}:{} connect refuse", server_ip, server_port);
        return -1;
    } catch (TimeoutException &) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}:{} connect time out", server_ip, server_port);
        return -1;
    } catch (NetException &) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}:{} net exception", server_ip, server_port);
        return -1;
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}:{} exception:{}", server_ip, server_port,
                                      e.displayText());
        return -1;
    }

    _name = socket_.peerAddress().toString();
    LogHelper::Instance()->TcpLog(spdlog::level::warn, "reconnection to {} success", _name);
    Poco::Timespan ts1(1000 * 100);
    socket_.setSendTimeout(ts1);
    socket_.setKeepAlive(true);
    socket_.setLinger(true, 0);

    try {
        _reactor.addEventHandler(socket_, Observer<MyTcpClient, ReadableNotification>(*this,
                                     &MyTcpClient::onReadable));
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{}", e.displayText());
    }

    isNeedReconnect = false;
    timeSend = 0;
    timeRecv = 0;
    return 0;
}

int MyTcpClient::Run() {
    _t.start(_reactor);
    _tHeartbeat = std::thread(ThreadHeartbeat, this);

    return 0;
}

void MyTcpClient::onReadable(ReadableNotification *pNf) {
    pNf->release();
    if (_fsm == nullptr) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{} _fsm null", _name);
        return;
    }
    if (isNeedReconnect) {
        return;
    }

    memset(recvBuf, 0, 1024 * 1024);
    int recvLen = (_fsm->GetWriteLen() < (1024 * 1024)) ? _fsm->GetWriteLen() : (1024 * 1024);
    try {
        int len = socket_.receiveBytes(recvBuf, recvLen);
        if (len <= 0) {
            LogHelper::Instance()->TcpLog(spdlog::level::err, "{} receiveBytes {}", _name, len);
            isNeedReconnect = true;
        } else {
            _fsm->TriggerAction(recvBuf, len);
        }
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->TcpLog(spdlog::level::err, "{} receive error:{},{}", _name, e.code(),
                                      e.displayText());
        if (e.code() != POCO_ETIMEDOUT && e.code() != POCO_EWOULDBLOCK &&
            e.code() != POCO_EAGAIN) {
            isNeedReconnect = true;
        }
    }
}


void MyTcpClient::ThreadHeartbeat(MyTcpClient *local) {
    LogHelper::Instance()->TcpLog(spdlog::level::warn, "{} heartbeat thread start", local->_name);
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
                    LogHelper::Instance()->TcpLog(spdlog::level::err, "{} send err", local->_name);
                    local->isNeedReconnect = true;
                }
            } catch (Poco::Exception &e) {
                LogHelper::Instance()->TcpLog(spdlog::level::err, "{} send error:{}{}", local->_name, e.code(),
                                              e.displayText());
                if (e.code() != POCO_ETIMEDOUT && e.code() != POCO_EWOULDBLOCK &&
                    e.code() != POCO_EAGAIN) {
                    local->isNeedReconnect = true;
                }
            }
        }
    }
    LogHelper::Instance()->TcpLog(spdlog::level::warn, "{} heartbeat thread end", local->_name);
}
