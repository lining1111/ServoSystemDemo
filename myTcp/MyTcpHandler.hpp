//
// Created by lining on 2023/8/22.
//

#ifndef MYTCPHANDLER_H
#define MYTCPHANDLER_H
#include "common/ComHandler.hpp"
#include "Poco/Net/StreamSocket.h"

using namespace std;

using Poco::Net::StreamSocket;


class MyTcpHandler : public common::CommonHandler {
public:
    StreamSocket __socket;

public:
    MyTcpHandler() = default;

    ~MyTcpHandler() = default;

public:
    int SendBase(string pkg) final {
        int ret = 0;
        std::unique_lock<std::mutex> lock(*mtx);
        if (pkg.back() != '*') {
            pkg.push_back('*');
        }
        LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "Rsp:" << pkg;
        try {
            auto len = __socket.sendBytes(pkg.data(), pkg.length());
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

    int Send(char *buf_send, int len_send) final {
        int ret = 0;
        std::unique_lock<std::mutex> lock(*mtx);
        LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "Rsp:" << string(buf_send);
        try {
            auto len = __socket.sendBytes(buf_send, len_send);
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
};


#endif //MYTCPHANDLER_H
