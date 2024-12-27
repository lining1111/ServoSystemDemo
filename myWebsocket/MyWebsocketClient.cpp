//
// Created by lining on 12/27/24.
//

#include "MyWebsocketClient.h"
#include "common/common.h"
#include "common/config.h"
#include "common/proc.h"

using namespace common;

MyWebsocketClient::MyWebsocketClient(string serverip, int serverport)
        : server_ip(serverip), server_port(serverport) {
    recvBuf = new char[1024 * 1024];
    if (mtx == nullptr) {
        mtx = new std::mutex();
    }
    _fsm = new FSM(BUFFER_SIZE);
    _cs = new Poco::Net::HTTPClientSession(server_ip, server_port);
    _req = new Poco::Net::HTTPRequest(Poco::Net::HTTPRequest::HTTP_GET, "/ws", Poco::Net::HTTPRequest::HTTP_1_1);
    _rsp = new Poco::Net::HTTPResponse;

    startBusiness();
}

MyWebsocketClient::~MyWebsocketClient() {
    LOG(WARNING) << _peerAddress << " disconnected";
    if (_fsm != nullptr) {
        delete _fsm;
        _fsm = nullptr;
    }
    delete mtx;
    delete _cs;
    delete _req;
    delete _rsp;
    stopBusiness();
    delete[]recvBuf;
    delete _ws;
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

    _tHeartbeat = std::thread(ThreadHearbeat, this);
    _tHeartbeat.detach();

    return 0;
}

int MyWebsocketClient::SendBase(string pkg) {
    int ret = 0;
    //阻塞调用，加锁
    std::unique_lock<std::mutex> lock(*mtx);
    //如果添加了分割则不添加分割了
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
    }
    catch (Poco::Exception &exc) {
        LOG(ERROR) << _peerAddress << " send error:" << exc.code() << exc.displayText();
        if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK && exc.code() != POCO_EAGAIN) {
            ret = -2;
        } else {
            ret = -3;
        }
    }
    //记录发送时间
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
    //阻塞调用，加锁
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
    }
    catch (Poco::Exception &exc) {
        LOG(ERROR) << _peerAddress << " send error:" << exc.code() << exc.displayText();
        if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK && exc.code() != POCO_EAGAIN) {
            ret = -2;
        } else {
            ret = -3;
        }
    }

    //记录发送时间
    if (this->timeSend == 0) {
        this->timeRecv = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    }

    this->timeSend = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

    return ret;
}

void MyWebsocketClient::startBusiness() {
    _isRun = true;
    LOG(WARNING) << _peerAddress << " start business";
    isLocalThreadRun = true;
    future_t1 = std::async(std::launch::async, ThreadStateMachine, this);
    future_t2 = std::async(std::launch::async, ThreadProcessPkg, this);
}

void MyWebsocketClient::stopBusiness() {
    _isRun = false;
    if (isLocalThreadRun) {
        isLocalThreadRun = false;
        _fsm->Stop();
        _pkgs.wakeUpAll();
        try {
            future_t1.wait();
        } catch (exception &e) {
            LOG(ERROR) << e.what();
        }
        try {
            future_t2.wait();
        } catch (exception &e) {
            LOG(ERROR) << e.what();
        }
    }
    LOG(WARNING) << _peerAddress << " stop business";
}

void MyWebsocketClient::Action() {
    char value = 0x00;
    if (_fsm->Read(&value, 1) == 1) {
        if (value == '*') {
            //得到分包尾部
            _pkgs.enqueueNotification(Poco::AutoPtr<MsgNotification>(new MsgNotification(_pkgCache)));
            _pkgCache.clear();
        } else {
            _pkgCache.push_back(value);
        }
    }
}

int MyWebsocketClient::ThreadRecv(MyWebsocketClient *local) {
    LOG(WARNING) << local->_peerAddress << " ThreadRecv start";
    while (local->_isRun) {
        memset(local->recvBuf, 0, 1024 * 1024);
        int recvLen = (local->_fsm->GetWriteLen() < (1024 * 1024)) ? local->_fsm->GetWriteLen() : (1024 * 1024);
        int len = local->_ws->receiveBytes(local->recvBuf, recvLen);
        if (len <= 0) {
            LOG(ERROR) << local->_peerAddress << " receiveBytes " << len;;
            local->isNeedReconnect = true;
        } else {
            local->_fsm->TriggerAction(local->recvBuf, len);
        }

    }

    LOG(WARNING) << local->_peerAddress << " ThreadRecv start";
}

int MyWebsocketClient::ThreadStateMachine(MyWebsocketClient *local) {
    LOG(WARNING) << local->_peerAddress << " ThreadStateMachine start";
    local->_pkgCache.clear();
    while (local->_isRun) {
        if (local->_fsm->WaitTriggerAction()) {
            while (local->_fsm->ShouldAction()) {
                local->Action();
            }
        }

    }
    LOG(WARNING) << local->_peerAddress << " ThreadStateMachine end";
    return 0;
}

int MyWebsocketClient::ThreadProcessPkg(MyWebsocketClient *local) {
    LOG(WARNING) << local->_peerAddress << " ThreadProcessPkg start";
    while (local->_isRun) {
        Poco::AutoPtr<MsgNotification> pNf = dynamic_cast<MsgNotification *>(local->_pkgs.waitDequeueNotification());
        if (pNf) {
            string pkg = pNf->message();
            if (pkg.empty()) {
                continue;
            }
            //按照cmd分别处理
            auto guid = common::parseGUID(pkg);
            if (guid.empty()) {
                guid = getGuid();
            }

            auto code = common::parseCode(pkg);
            if (!code.empty()) {
                auto iter = HandleRouter.find(code);
                if (iter != HandleRouter.end()) {
                    LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "local process:" << pkg;
                    iter->second(local->_peerAddress, pkg);
                } else {
                    //最后没有对应的方法名
                    LOG(ERROR) << local->_peerAddress << " 最后没有对应的方法名:" << code << ",内容:" << pkg;
                    Com rsp;
                    rsp.guid = guid;
                    rsp.code = code;
                    rsp.state = State_UnmarshalFail;
                    rsp.param = "code not find:" + code;
                    string rspStr = json::encode(rsp);
                    local->SendBase(rspStr);
                }
            } else {
                Com rsp;
                rsp.guid = guid;
                rsp.code = "";
                rsp.state = State_UnmarshalFail;
                rsp.param = "code empty";
                string rspStr = json::encode(rsp);
                local->SendBase(rspStr);
            }
        }
    }
    LOG(WARNING) << local->_peerAddress << " ThreadProcessPkg end";
    return 0;
}

void MyWebsocketClient::ThreadHearbeat(MyWebsocketClient *local) {
    LOG(WARNING) << local->_peerAddress << " heartbeat thread start";
    while (local->_isRun) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
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
            }
            catch (Poco::Exception &exc) {
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

