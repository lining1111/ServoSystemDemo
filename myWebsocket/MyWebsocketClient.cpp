//
// Created by lining on 12/27/24.
//

#include "MyWebsocketClient.h"
#include "common/common.h"
#include "../config/config.h"
#include "common/proc.h"

using namespace common;

MyWebsocketClient::MyWebsocketClient(string serverip, int serverport)
    : server_ip(serverip), server_port(serverport) {
    recvBuf = new char[1024 * 1024];
    _cs = new Poco::Net::HTTPClientSession(server_ip, server_port);
    _req = new Poco::Net::HTTPRequest(Poco::Net::HTTPRequest::HTTP_GET, "/ws", Poco::Net::HTTPRequest::HTTP_1_1);
    _rsp = new Poco::Net::HTTPResponse;
    startBusiness();
}

MyWebsocketClient::~MyWebsocketClient() {
    LOG(WARNING) << _peerAddress << " disconnected";
    stopBusiness();
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
        LOG(ERROR) << "ws client open fail:" << server_ip << ":" << to_string(server_port) << ",err:"
                << e.displayText();
        return -1;
    }
    isNeedReconnect = false;
    _peerAddress = _ws->peerAddress().toString();
    LOG(WARNING) << "connection to " << _peerAddress << " success";
    return 0;
}

int MyWebsocketClient::Reconnect() {
    try {
        _ws = new Poco::Net::WebSocket(*_cs, *_req, *_rsp);
    } catch (Poco::Exception &e) {
        LOG(ERROR) << "ws client open fail:" << server_ip << ":" << to_string(server_port) << ",err:"
                << e.displayText();
        return -1;
    }
    isNeedReconnect = false;
    _peerAddress = _ws->peerAddress().toString();
    LOG(WARNING) << "reconnection to " << _peerAddress << " success";
    return 0;
}

int MyWebsocketClient::Run() {
    _tRecv = std::thread(ThreadRecv, this);
    _tRecv.detach();

    _tHeartbeat = std::thread(ThreadHeartbeat, this);
    _tHeartbeat.detach();

    return 0;
}

int MyWebsocketClient::SendBase(string pkg) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(*mtx);
    if (pkg.back() != '*') {
        pkg.push_back('*');
    }
    LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "Rsp:" << pkg;
    try {
        auto len = _ws->sendFrame(pkg.data(), pkg.length());
        VLOG(2) << _peerAddress << " send len:" << len << " len_send:" << to_string(pkg.length());
        if (len < 0) {
            LOG(ERROR) << _peerAddress << " send len < 0";
            ret = -2;
        } else if (len != pkg.size()) {
            LOG(ERROR) << _peerAddress << " send len !=len_send";
            ret = -2;
        }
    } catch (Poco::Exception &exc) {
        LOG(ERROR) << _peerAddress << " send error:" << exc.code() << exc.displayText();
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
    LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "Rsp:" << string(buf_send);
    try {
        auto len = _ws->sendFrame(buf_send, len_send);
        VLOG(2) << _peerAddress << " send len:" << len << " len_send:" << len_send;
        if (len < 0) {
            LOG(ERROR) << _peerAddress << " send len < 0";
            ret = -2;
        } else if (len != len_send) {
            LOG(ERROR) << _peerAddress << " send len !=len_send";
            ret = -2;
        }
    } catch (Poco::Exception &exc) {
        LOG(ERROR) << _peerAddress << " send error:" << exc.code() << exc.displayText();
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
    LOG(WARNING) << local->_peerAddress << " ThreadRecv start";
    while (local->_isRun) {
        std::this_thread::sleep_for(1s);
        if (!local->isNeedReconnect) {
            memset(local->recvBuf, 0, 1024 * 1024);
            int recvLen = (local->_fsm->GetWriteLen() < (1024 * 1024)) ? local->_fsm->GetWriteLen() : (1024 * 1024);
            int flags;
            int len = local->_ws->receiveFrame(local->recvBuf, recvLen, flags);
            if (len == 0 && flags == 0) {
                LOG(ERROR) << local->_peerAddress << " receiveBytes " << len;;
                local->isNeedReconnect = true;
            } else {
                local->_fsm->TriggerAction(local->recvBuf, len);
            }
        }
    }

    LOG(WARNING) << local->_peerAddress << " ThreadRecv start";
    return 0;
}

void MyWebsocketClient::ThreadHeartbeat(MyWebsocketClient *local) {
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
            } catch (Poco::Exception &exc) {
                LOG(ERROR) << local->_peerAddress << " send error:" << exc.code()
                        << exc.displayText();
                if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK &&
                    exc.code() != POCO_EAGAIN) {
                    local->isNeedReconnect = true;
                }
            }
        }
    }
    LOG(WARNING) << local->_peerAddress << " heartbeat thread end";
}
