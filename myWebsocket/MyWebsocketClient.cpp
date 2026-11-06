//
// Created by lining on 12/27/24.
//

#include "MyWebsocketClient.h"

#include <utility>
#include "logHelper/LogHelper.h"
#include "common/common.h"
#include "../config/config.h"
#include "common/proc.h"

using namespace common;

MyWebsocketClient::MyWebsocketClient(string serverip, int serverport)
    : CommonHandler("ws_client"), server_ip(std::move(serverip)), server_port(serverport) {
    recvBuf = new char[1024 * 1024];
    _cs = new Poco::Net::HTTPClientSession(server_ip, server_port);
    _req = new Poco::Net::HTTPRequest(Poco::Net::HTTPRequest::HTTP_GET, "/ws", Poco::Net::HTTPRequest::HTTP_1_1);
    _rsp = new Poco::Net::HTTPResponse;
    CommonHandler::startBusiness();
}

MyWebsocketClient::~MyWebsocketClient() {
    LogHelper::Instance()->WsLog(spdlog::level::warn, "{} disconnected", _name);
    CommonHandler::stopBusiness();
    if (_tRecv.joinable()) {
        _tRecv.join();
    }
    if (_tHeartbeat.joinable()) {
        _tHeartbeat.join();
    }
    delete _cs;
    delete _req;
    delete _rsp;
    delete _ws;
    delete[]recvBuf;
}

int MyWebsocketClient::Open() {
    try {
        _ws = new Poco::Net::WebSocket(*_cs, *_req, *_rsp);
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->WsLog(spdlog::level::err, "ws client open fail:{}:{},err:{}",
                                     server_ip, to_string(server_port), e.displayText());
        return -1;
    }
    isNeedReconnect = false;
    _name = _ws->peerAddress().toString();
    LogHelper::Instance()->WsLog(spdlog::level::warn, "connection to {} success", _name);
    return 0;
}

int MyWebsocketClient::Reconnect() {
    try {
        _ws = new Poco::Net::WebSocket(*_cs, *_req, *_rsp);
    } catch (Poco::Exception &e) {
        LogHelper::Instance()->WsLog(spdlog::level::err, "ws client open fail:{}:{},err:{}",
                                     server_ip, to_string(server_port), e.displayText());
        return -1;
    }
    isNeedReconnect = false;
    _name = _ws->peerAddress().toString();
    LogHelper::Instance()->WsLog(spdlog::level::warn, "reconnection to {} success", _name);
    return 0;
}

int MyWebsocketClient::Run() {
    _tRecv = std::thread(ThreadRecv, this);
    _tHeartbeat = std::thread(ThreadHeartbeat, this);
    return 0;
}

int MyWebsocketClient::SendBase(string pkg) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(*mtx);
    if (pkg.back() != '*') {
        pkg.push_back('*');
    }
    if (localConfig.isShowMsgType("COM")) {
        LogHelper::Instance()->WsLog(spdlog::level::info, "Rsp:{}", pkg);
    }
    try {
        auto len = _ws->sendFrame(pkg.data(), pkg.length());
        LogHelper::Instance()->WsLog(spdlog::level::debug, "{} send len:{} len_send:{}", _name, len,
                                     to_string(pkg.length()));
        if (len < 0) {
            LogHelper::Instance()->WsLog(spdlog::level::err, "{} send len < 0", _name);
            ret = -2;
        } else if (len != pkg.size()) {
            LogHelper::Instance()->WsLog(spdlog::level::err, "{} send len !=len_send", _name);
            ret = -2;
        }
    } catch (Poco::Exception &exc) {
        LogHelper::Instance()->WsLog(spdlog::level::err, "{} send error:{}{}", _name, exc.code(),
                                     exc.displayText());
        if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK && exc.code() != POCO_EAGAIN) {
            ret = -2;
        } else {
            ret = -3;
        }
    }
    if (this->timeSend == 0) {
        this->timeRecv = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    this->timeSend = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return ret;
}

int MyWebsocketClient::Send(char *buf_send, int len_send) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(*mtx);
    if (localConfig.isShowMsgType("COM")) {
        LogHelper::Instance()->WsLog(spdlog::level::info, "Rsp:{}", string(buf_send));
    }
    try {
        auto len = _ws->sendFrame(buf_send, len_send);
        LogHelper::Instance()->WsLog(spdlog::level::debug, "{} send len:{} len_send:{}", _name, len, len_send);
        if (len < 0) {
            LogHelper::Instance()->WsLog(spdlog::level::err, "{} send len < 0", _name);
            ret = -2;
        } else if (len != len_send) {
            LogHelper::Instance()->WsLog(spdlog::level::err, "{} send len !=len_send", _name);
            ret = -2;
        }
    } catch (Poco::Exception &exc) {
        LogHelper::Instance()->WsLog(spdlog::level::err, "{} send error:{}{}", _name, exc.code(),
                                     exc.displayText());
        if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK && exc.code() != POCO_EAGAIN) {
            ret = -2;
        } else {
            ret = -3;
        }
    }

    if (this->timeSend == 0) {
        this->timeRecv = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    this->timeSend = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return ret;
}

int MyWebsocketClient::ThreadRecv(MyWebsocketClient *local) {
    LogHelper::Instance()->WsLog(spdlog::level::warn, "{} ThreadRecv start", local->_name);
    while (local->_isRun) {
        std::this_thread::sleep_for(1s);
        if (!local->isNeedReconnect) {
            memset(local->recvBuf, 0, 1024 * 1024);
            int recvLen = (local->_fsm->GetWriteLen() < (1024 * 1024)) ? local->_fsm->GetWriteLen() : (1024 * 1024);
            int flags;
            int len = local->_ws->receiveFrame(local->recvBuf, recvLen, flags);
            if (len == 0 && flags == 0) {
                LogHelper::Instance()->WsLog(spdlog::level::err, "{} receiveBytes {}", local->_name, len);
                local->isNeedReconnect = true;
            } else {
                local->_fsm->TriggerAction(local->recvBuf, len);
            }
        }
    }

    LogHelper::Instance()->WsLog(spdlog::level::warn, "{} ThreadRecv start", local->_name);
    return 0;
}

void MyWebsocketClient::ThreadHeartbeat(MyWebsocketClient *local) {
    LogHelper::Instance()->WsLog(spdlog::level::warn, "{} heartbeat thread start", local->_name);
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
                    LogHelper::Instance()->WsLog(spdlog::level::err, "{} send err", local->_name);
                    local->isNeedReconnect = true;
                }
            } catch (Poco::Exception &exc) {
                LogHelper::Instance()->WsLog(spdlog::level::err, "{} send error:{}{}", local->_name, exc.code(),
                                             exc.displayText());
                if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK &&
                    exc.code() != POCO_EAGAIN) {
                    local->isNeedReconnect = true;
                }
            }
        }
    }
    LogHelper::Instance()->WsLog(spdlog::level::warn, "{} heartbeat thread end", local->_name);
}
