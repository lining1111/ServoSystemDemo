//
// Created by lining on 12/27/24.
//

#include "MyWebsocketServer.h"
#include "localBusiness/localBusiness.h"

MyWebSocketRequestHandler::MyWebSocketRequestHandler() : CommonHandler("ws_client") {
    recvBuf = new char[1024 * 1024];
    CommonHandler::startBusiness();
}

MyWebSocketRequestHandler::~MyWebSocketRequestHandler() {
    CommonHandler::stopBusiness();
    delete[]recvBuf;
    delete _ws;
}

void MyWebSocketRequestHandler::handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) {
    try {
        _ws = new WebSocket(request, response);
        LOG(WARNING) << "websocket client connect:" << _ws->peerAddress().toString();
        _name = _ws->peerAddress().toString();
        auto localBusiness = LocalBusiness::instance();
        localBusiness->addConn_ws(this);
        int flags = 0;
        int len = 0;
        do {
            memset(recvBuf, 0, 1024 * 1024);
            int recvLen = (_fsm->GetWriteLen() < (1024 * 1024)) ? _fsm->GetWriteLen() : (1024 * 1024);
            len = _ws->receiveFrame(recvBuf, recvLen, flags);
            if (len == 0 && flags == 0) {
                break;
            }
            _fsm->TriggerAction(recvBuf, len);
        } while (len > 0 || (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);
        //Remove ws from the client array.
        LOG(WARNING) << "websocket client disconnect:" << _ws->peerAddress().toString();
        localBusiness->delConn(_ws->peerAddress().toString(), false);
        //!!!Here, do not delete this because Poco will enter the destructor.
    } catch (WebSocketException &exc) {
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
            default:
                break;
        }
    }
}

int MyWebSocketRequestHandler::SendBase(string pkg) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(*mtx);
    if (pkg.back() != '*') {
        pkg.push_back('*');
    }
    LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "Rsp:" << pkg;
    try {
        auto len = _ws->sendFrame(pkg.data(), pkg.length());
        VLOG(2) << _name << " send len:" << len << " len_send:" << to_string(pkg.length());
        if (len < 0) {
            LOG(ERROR) << _name << " send len < 0";
            ret = -2;
        } else if (len != pkg.size()) {
            LOG(ERROR) << _name << " send len !=len_send";
            ret = -2;
        }
    } catch (Poco::Exception &exc) {
        LOG(ERROR) << _name << " send error:" << exc.code() << exc.displayText();
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

int MyWebSocketRequestHandler::Send(char *buf_send, int len_send) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(*mtx);
    LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "Rsp:" << string(buf_send);
    try {
        auto len = _ws->sendFrame(buf_send, len_send);
        VLOG(2) << _name << " send len:" << len << " len_send:" << len_send;
        if (len < 0) {
            LOG(ERROR) << _name << " send len < 0";
            ret = -2;
        } else if (len != len_send) {
            LOG(ERROR) << _name << " send len !=len_send";
            ret = -2;
        }
    } catch (Poco::Exception &exc) {
        LOG(ERROR) << _name << " send error:" << exc.code() << exc.displayText();
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

MyWebsocketServer::MyWebsocketServer(const int port) : _port(port) {
}

MyWebsocketServer::~MyWebsocketServer() {
    if (srv != nullptr) {
        srv->stop();
        delete srv;
    }
}

int MyWebsocketServer::Open() {
    try {
        srv = new Poco::Net::HTTPServer(new MyWebsocketHandler, _port);
    } catch (Poco::Exception &e) {
        LOG(ERROR) << "ws server err:" << e.displayText();
        return -1;
    }
    srv->start();
    isListen = true;
    return 0;
}

int MyWebsocketServer::ReOpen() {
    try {
        srv = new Poco::Net::HTTPServer(new MyWebsocketHandler, _port);
    } catch (Poco::Exception &e) {
        LOG(ERROR) << "ws server err:" << e.displayText();
        return -1;
    }
    srv->start();
    isListen = true;
    return 0;
}
