//
// Created by lining on 2023/8/21.
//

#include "Client.h"

#include <utility>

#include "common/common.h"

namespace tcp {

Client::Client(std::string serverip, int serverport) : server_ip(std::move(serverip)),
                                                             server_port(serverport) {
    //获取 tcp 模块日志 handler
    log_ = LogHelper::Instance()->GetLogger("tcp");
    recvBuf = new char[1024 * 1024];
    //注入分包处理的报文路由与应答发送
    helper_.SetPkgFun([this](const std::string &pkg) -> std::string {
        return OnPkg(pkg);
    });
    helper_.SetSendFun([this](const std::string &rsp) -> int {
        return SendBase(rsp);
    });
    _isRun = true;
}

Client::~Client() {
    LOG_TO(log_, spdlog::level::warn, "{} disconnected", _name);

    _isRun = false;
    try {
        _reactor.removeEventHandler(socket_, Observer<Client, ReadableNotification>(*this,
                                        &Client::onReadable));
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "{}", e.displayText());
    }

    if (_tHeartbeat.joinable()) {
        _tHeartbeat.join();
    }

    try {
        socket_.close();
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "{}", e.displayText());
    }

    delete[] recvBuf;
}

int Client::Open() {
    SocketAddress sa(server_ip, server_port);
    try {
        Poco::Timespan ts(1000 * 1000);
        socket_.connect(sa, ts);
    } catch (ConnectionRefusedException &) {
        LOG_TO(log_, spdlog::level::err, "{}:{} connect refuse", server_ip, server_port);
        return -1;
    } catch (TimeoutException &) {
        LOG_TO(log_, spdlog::level::err, "{}:{} connect time out", server_ip, server_port);
        return -1;
    } catch (NetException &) {
        LOG_TO(log_, spdlog::level::err, "{}:{} net exception", server_ip, server_port);
        return -1;
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "{}:{} exception:{}", server_ip, server_port,
                                      e.displayText());
        return -1;
    }

    _name = socket_.peerAddress().toString();
    LOG_TO(log_, spdlog::level::warn, "connection to {} success", _name);
    Poco::Timespan ts1(1000 * 100);
    socket_.setSendTimeout(ts1);
    socket_.setKeepAlive(true);
    socket_.setLinger(true, 0);

    try {
        _reactor.addEventHandler(socket_, Observer<Client, ReadableNotification>(*this,
                                     &Client::onReadable));
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "{}", e.displayText());
    }

    isNeedReconnect = false;
    return 0;
}

int Client::Reconnect() {
    socket_.close();
    SocketAddress sa(server_ip, server_port);
    try {
        Poco::Timespan ts(1000 * 1000);
        socket_.connect(sa, ts);
    } catch (ConnectionRefusedException &) {
        LOG_TO(log_, spdlog::level::err, "{}:{} connect refuse", server_ip, server_port);
        return -1;
    } catch (TimeoutException &) {
        LOG_TO(log_, spdlog::level::err, "{}:{} connect time out", server_ip, server_port);
        return -1;
    } catch (NetException &) {
        LOG_TO(log_, spdlog::level::err, "{}:{} net exception", server_ip, server_port);
        return -1;
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "{}:{} exception:{}", server_ip, server_port,
                                      e.displayText());
        return -1;
    }

    _name = socket_.peerAddress().toString();
    LOG_TO(log_, spdlog::level::warn, "reconnection to {} success", _name);
    Poco::Timespan ts1(1000 * 100);
    socket_.setSendTimeout(ts1);
    socket_.setKeepAlive(true);
    socket_.setLinger(true, 0);

    try {
        _reactor.addEventHandler(socket_, Observer<Client, ReadableNotification>(*this,
                                     &Client::onReadable));
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "{}", e.displayText());
    }

    isNeedReconnect = false;
    return 0;
}

int Client::Run() {
    _t.start(_reactor);
    _tHeartbeat = std::thread(ThreadHeartbeat, this);

    return 0;
}

int Client::SendBase(std::string pkg) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(send_mutex_);
    if (pkg.empty()) {
        return 0;
    }
    //补包尾
    if (pkg.back() != '*') {
        pkg.push_back('*');
    }
    try {
        auto len = socket_.sendBytes(pkg.data(), pkg.length());
        LOG_TO(log_, spdlog::level::debug, "{} send len:{} len_send:{}", _name, len,
                                      to_string(pkg.length()));
        if (len < 0 || len != (int) pkg.size()) {
            LOG_TO(log_, spdlog::level::err, "{} send len error", _name);
            ret = -2;
        }
    } catch (Poco::Exception &exc) {
        LOG_TO(log_, spdlog::level::err, "{} send error:{}{}", _name, exc.code(),
                                      exc.displayText());
        if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK && exc.code() != POCO_EAGAIN) {
            ret = -2;
        } else {
            ret = -3;
        }
    }
    return ret;
}

void Client::SetPkgFun(const std::function<std::string(const std::string &pkg)> &fun) {
    std::lock_guard<std::mutex> lock(pkgFun_mutex_);
    pkgFun_ = fun;
}

std::string Client::OnPkg(const std::string &pkg) {
    std::function<std::string(const std::string &pkg)> fun;
    {
        std::lock_guard<std::mutex> lock(pkgFun_mutex_);
        fun = pkgFun_;
    }
    if (!fun) {
        LOG_TO(log_, spdlog::level::err, "{} 未设置报文处理回调,丢弃报文:{}", _name, pkg);
        return "";
    }
    return fun(pkg);
}

void Client::onReadable(ReadableNotification *pNf) {
    pNf->release();
    if (isNeedReconnect) {
        return;
    }

    memset(recvBuf, 0, 1024 * 1024);
    int recvLen = 1024 * 1024;
    try {
        int len = socket_.receiveBytes(recvBuf, recvLen);
        if (len <= 0) {
            LOG_TO(log_, spdlog::level::err, "{} receiveBytes {}", _name, len);
            isNeedReconnect = true;
            return;
        }
        //第一步：分包处理（'*'为包尾），完整包进入待处理队列
        helper_.SplitPkg(recvBuf, static_cast<size_t>(len));
        //第二步：轮询待处理缓存，逐包处理
        helper_.ProcessPkgQueue();
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "{} receive error:{},{}", _name, e.code(),
                                      e.displayText());
        if (e.code() != POCO_ETIMEDOUT && e.code() != POCO_EWOULDBLOCK &&
            e.code() != POCO_EAGAIN) {
            isNeedReconnect = true;
        }
    }
}

void Client::ThreadHeartbeat(Client *local) {
    LOG_TO(local->log_, spdlog::level::warn, "{} heartbeat thread start", local->_name);
    while (local->_isRun) {
        std::this_thread::sleep_for(5s);
        if (!local->isNeedReconnect) {
            try {
                common::Com req;
                req.code = "Heartbeat";
                req.param = "req";
                string msg = json::encode(req);
                int ret = local->SendBase(msg);
                if (ret < 0) {
                    LOG_TO(local->log_, spdlog::level::err, "{} send err", local->_name);
                    local->isNeedReconnect = true;
                }
            } catch (Poco::Exception &e) {
                LOG_TO(local->log_, spdlog::level::err, "{} send error:{}{}", local->_name, e.code(),
                                              e.displayText());
                if (e.code() != POCO_ETIMEDOUT && e.code() != POCO_EWOULDBLOCK &&
                    e.code() != POCO_EAGAIN) {
                    local->isNeedReconnect = true;
                }
            }
        }
    }
    LOG_TO(local->log_, spdlog::level::warn, "{} heartbeat thread end", local->_name);
}

} // namespace tcp