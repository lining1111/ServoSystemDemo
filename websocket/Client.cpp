//
// Created by lining on 12/27/24.
//

#include "Client.h"

#include <utility>

#include "common/common.h"

namespace websocket {

Client::Client(std::string serverip, int serverport)
        : server_ip(std::move(serverip)), server_port(serverport) {
    //获取 ws 模块日志 handler
    log_ = LogHelper::Instance()->GetLogger("ws");
    recvBuf = new char[1024 * 1024];
    _cs = new Poco::Net::HTTPClientSession(server_ip, server_port);
    _req = new Poco::Net::HTTPRequest(Poco::Net::HTTPRequest::HTTP_GET, "/ws", Poco::Net::HTTPRequest::HTTP_1_1);
    _rsp = new Poco::Net::HTTPResponse;
    _isRun = true;
}

Client::~Client() {
    LOG_TO(log_, spdlog::level::warn, "{} disconnected", _name);

    _isRun = false;
    if (_tRecv.joinable()) {
        _tRecv.join();
    }
    if (_tHeartbeat.joinable()) {
        _tHeartbeat.join();
    }

    if (_ws) {
        try {
            _ws->close();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::err, "{}", e.displayText());
        }
    }
    delete _cs;
    delete _req;
    delete _rsp;
    delete _ws;
    delete[] recvBuf;
}

int Client::Open() {
    try {
        _ws = new Poco::Net::WebSocket(*_cs, *_req, *_rsp);
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "ws client open fail:{}:{},err:{}",
                                     server_ip, to_string(server_port), e.displayText());
        return -1;
    }
    isNeedReconnect = false;
    _name = _ws->peerAddress().toString();
    LOG_TO(log_, spdlog::level::warn, "connection to {} success", _name);
    return 0;
}

int Client::Reconnect() {
    try {
        _ws = new Poco::Net::WebSocket(*_cs, *_req, *_rsp);
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "ws client open fail:{}:{},err:{}",
                                     server_ip, to_string(server_port), e.displayText());
        return -1;
    }
    isNeedReconnect = false;
    _name = _ws->peerAddress().toString();
    LOG_TO(log_, spdlog::level::warn, "reconnection to {} success", _name);
    return 0;
}

int Client::Run() {
    _tRecv = std::thread(ThreadRecv, this);
    _tHeartbeat = std::thread(ThreadHeartbeat, this);
    return 0;
}

int Client::SendBase(std::string pkg) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(sendMtx_);
    if (_ws == nullptr || pkg.empty()) {
        return 0;
    }
    try {
        auto len = _ws->sendFrame(pkg.data(), pkg.length());
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

int Client::ThreadRecv(Client *local) {
    LOG_TO(local->log_, spdlog::level::warn, "{} ThreadRecv start", local->_name);
    while (local->_isRun) {
        std::this_thread::sleep_for(1s);
        if (local->isNeedReconnect || local->_ws == nullptr) {
            continue;
        }
        memset(local->recvBuf, 0, 1024 * 1024);
        int flags;
        try {
            int len = local->_ws->receiveFrame(local->recvBuf, 1024 * 1024, flags);
            if (len <= 0 && flags == 0) {
                LOG_TO(local->log_, spdlog::level::err, "{} receiveBytes {}", local->_name, len);
                local->isNeedReconnect = true;
                continue;
            }
            //websocket 帧自带消息边界（无粘包/断包），一帧即一条完整报文
            if (len > 0) {
                std::string rsp = local->OnPkg(std::string(local->recvBuf, len));
                if (!rsp.empty()) {
                    local->SendBase(rsp);
                }
            }
        } catch (Poco::Exception &e) {
            LOG_TO(local->log_, spdlog::level::err, "{} receive error:{},{}", local->_name, e.code(),
                                         e.displayText());
            if (e.code() != POCO_ETIMEDOUT && e.code() != POCO_EWOULDBLOCK &&
                e.code() != POCO_EAGAIN) {
                local->isNeedReconnect = true;
            }
        }
    }
    LOG_TO(local->log_, spdlog::level::warn, "{} ThreadRecv end", local->_name);
    return 0;
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
            } catch (Poco::Exception &exc) {
                LOG_TO(local->log_, spdlog::level::err, "{} send error:{}{}", local->_name, exc.code(),
                                             exc.displayText());
                if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK &&
                    exc.code() != POCO_EAGAIN) {
                    local->isNeedReconnect = true;
                }
            }
        }
    }
    LOG_TO(local->log_, spdlog::level::warn, "{} heartbeat thread end", local->_name);
}

} // namespace websocket