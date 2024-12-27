//
// Created by lining on 12/27/24.
//

#ifndef MYWEBSOCKETSERVERHANDLER_H
#define MYWEBSOCKETSERVERHANDLER_H

#include <mutex>
#include <iostream>
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerSession.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Util/ServerApplication.h"
#include "common/common.h"
#include "common/proc.h"
#include "common/config.h"
#include "localBusiness/localBusiness.h"

using namespace Poco::Net;
using namespace std;

//websocket请求的处理
class WebSocketRequestHandler : public Poco::Net::HTTPRequestHandler {
public:
    WebSocketRequestHandler(size_t bufSize = 1024)
            : _bufSize(bufSize) {
    }

    virtual void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) {
        try {
            ws = new WebSocket(request, response);
            //TODO 将ws添加到全局数组内
            Poco::Buffer<char> buffer(_bufSize);
            int flags;
            int n;
            do {
                n = ws->receiveFrame(buffer.begin(), static_cast<int>(buffer.size()), flags);
                std::cout << "服务端接收到数据：" << buffer.begin() << std::endl;
                proContent(buffer.begin());
//                std::string sSend = "hello, I'm WebSocket Server!";
//                ws.sendFrame(sSend.c_str(), sSend.size(), flags);
            } while (n > 0 || (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);
            //将ws从客户端数组删除
            auto localBusiness = LocalBusiness::instance();
            localBusiness->delConn(ws->peerAddress().toString());
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

private:
    size_t _bufSize;
    WebSocket *ws;

    int proContent(const string &content) {
        string peerAddress = ws->peerAddress().toString();
        string pkg = content;
        if (pkg.empty()) {
            return -1;
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
                iter->second(peerAddress, pkg);
            } else {
                //最后没有对应的方法名
                LOG(ERROR) << peerAddress << " 最后没有对应的方法名:" << code << ",内容:" << pkg;
                Com rsp;
                rsp.guid = guid;
                rsp.code = code;
                rsp.state = State_UnmarshalFail;
                rsp.param = "code not find:" + code;
                string rspStr = json::encode(rsp);
                ws->sendFrame(rspStr.data(), rspStr.length());
            }
        } else {
            Com rsp;
            rsp.guid = guid;
            rsp.code = "";
            rsp.state = State_UnmarshalFail;
            rsp.param = "code empty";
            string rspStr = json::encode(rsp);
            ws->sendFrame(rspStr.data(), rspStr.length());
        }

    }
};

class MyWebsocketHandler : public Poco::Net::HTTPRequestHandlerFactory {
public:
    MyWebsocketHandler(std::size_t bufSize = 1024) : _bufSize(bufSize) {

    }

    Poco::Net::HTTPRequestHandler *createRequestHandler(const HTTPServerRequest &request) {
        return new WebSocketRequestHandler(_bufSize);
    }

private:
    std::size_t _bufSize;
};


#endif //MYWEBSOCKETSERVERHANDLER_H
