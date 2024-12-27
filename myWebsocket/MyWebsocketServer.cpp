//
// Created by lining on 12/27/24.
//

#include "MyWebsocketServer.h"
#include "localBusiness/localBusiness.h"
#include "common/common.h"
#include "common/proc.h"
#include "common/config.h"

MyWebSocketRequestHandler::MyWebSocketRequestHandler(size_t bufSize) : _bufSize(bufSize) {
    recvBuf = new char[1024 * 1024];
    mtx = new mutex();
    _fsm = new FSM(_bufSize);
    startBusiness();
}

MyWebSocketRequestHandler::~MyWebSocketRequestHandler() {
    stopBusiness();
    if (mtx != nullptr) {
        delete mtx;
    }
    if (_fsm != nullptr) {
        delete _fsm;
        _fsm = nullptr;
    }
    delete[]recvBuf;
    delete _ws;
}

void MyWebSocketRequestHandler::handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) {
    try {
        _ws = new WebSocket(request, response);
        // 将ws添加到全局数组内
        LOG(WARNING) << "websocket client connect:" << _ws->peerAddress().toString();
        _peerAddress = _ws->peerAddress().toString();
        auto localBusiness = LocalBusiness::instance();
        localBusiness->addConn_ws(this);
        int flags;
        int n;
        do {
            memset(recvBuf, 0, 1024 * 1024);
            int recvLen = (_fsm->GetWriteLen() < (1024 * 1024)) ? _fsm->GetWriteLen() : (1024 * 1024);
            int flags;
            int len = _ws->receiveFrame(recvBuf, recvLen, flags);
            if (len == 0 && flags == 0) {
                break;
            } else {
                _fsm->TriggerAction(recvBuf, len);
            }
        } while (n > 0 || (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);
        //将ws从客户端数组删除
        LOG(WARNING) << "websocket client disconnect:" << _ws->peerAddress().toString();
        localBusiness->delConn_ws(_ws->peerAddress().toString());
        //!!!这里不要delete this 因为Poco会进入析构
    }
    catch (WebSocketException &exc) {
        switch (exc.code()) {
            case WebSocket::WS_ERR_HANDSHAKE_UNSUPPORTED_VERSION:
                response.set("Sec-WebSocket-Version", WebSocket::WEBSOCKET_VERSION);
                // fallthrough
            case WebSocket::WS_ERR_NO_HANDSHAKE:
            case WebSocket::WS_ERR_HANDSHAKE_NO_VERSION:
            case WebSocket::WS_ERR_HANDSHAKE_NO_KEY:
                response.setStatusAndReason(HTTPResponse::HTTP_BAD_REQUEST);
                response.setContentLength(0);
                response.send();
                break;
        }
    }
}

int MyWebSocketRequestHandler::SendBase(string pkg) {
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

int MyWebSocketRequestHandler::Send(char *buf_send, int len_send) {
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

void MyWebSocketRequestHandler::startBusiness() {
    _isRun = true;
    LOG(WARNING) << _peerAddress << " start business";
    isLocalThreadRun = true;
    future_t1 = std::async(std::launch::async, ThreadStateMachine, this);
    future_t2 = std::async(std::launch::async, ThreadProcessPkg, this);
}

void MyWebSocketRequestHandler::stopBusiness() {
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

void MyWebSocketRequestHandler::Action() {
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


int MyWebSocketRequestHandler::ThreadStateMachine(MyWebSocketRequestHandler *local) {
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

int MyWebSocketRequestHandler::ThreadProcessPkg(MyWebSocketRequestHandler *local) {
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

MyWebsocketServer::MyWebsocketServer(int port) : _port(port) {

}

MyWebsocketServer::~MyWebsocketServer() {
    srv->stop();
}

int MyWebsocketServer::Open() {
    try {
        Poco::Net::ServerSocket ss(_port);
        auto param = new Poco::Net::HTTPServerParams();
        srv = new Poco::Net::HTTPServer(new MyWebsocketHandler, ss, param);
    } catch (Poco::Exception &e) {
        LOG(ERROR) << "ws server err:" << e.displayText();
        return -1;
    }
    isListen = true;
    return 0;
}

int MyWebsocketServer::ReOpen() {
    try {
        Poco::Net::ServerSocket ss(_port);
        auto param = new Poco::Net::HTTPServerParams();
        srv = new Poco::Net::HTTPServer(new MyWebsocketHandler, ss, param);
    } catch (Poco::Exception &e) {
        LOG(ERROR) << "ws server err:" << e.displayText();
        return -1;
    }
    isListen = true;
    return 0;
}

int MyWebsocketServer::Run() {
    srv->start();
    return 0;
}

int MyWebsocketServer::Stop() {
    srv->stop();
    return 0;
}
