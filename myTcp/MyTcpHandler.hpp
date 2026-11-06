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
    StreamSocket socket_;

public:
    MyTcpHandler() : CommonHandler("MyTcpHandler") {
    }

    ~MyTcpHandler() override = default;

public:
    int SendBase(string pkg) final {
        int ret = 0;
        std::unique_lock<std::mutex> lock(*mtx);
        if (pkg.back() != '*') {
            pkg.push_back('*');
        }
        if (localConfig.isShowMsgType("COM")) {
            LogHelper::Instance()->TcpLog(spdlog::level::info, "Rsp:{}", pkg);
        }
        try {
            auto len = socket_.sendBytes(pkg.data(), pkg.length());
            LogHelper::Instance()->TcpLog(spdlog::level::debug, "{} send len:{} len_send:{}", _name, len,
                                          to_string(pkg.length()));
            if (len < 0) {
                LogHelper::Instance()->TcpLog(spdlog::level::err, "{} send len < 0", _name);
                ret = -2;
            } else if (len != pkg.size()) {
                LogHelper::Instance()->TcpLog(spdlog::level::err, "{} send len !=len_send", _name);
                ret = -2;
            }
        } catch (Poco::Exception &exc) {
            LogHelper::Instance()->TcpLog(spdlog::level::err, "{} send error:{}{}", _name, exc.code(),
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

    int Send(char *buf_send, int len_send) final {
        int ret = 0;
        std::unique_lock<std::mutex> lock(*mtx);
        if (localConfig.isShowMsgType("COM")) {
            LogHelper::Instance()->TcpLog(spdlog::level::info, "Rsp:{}", string(buf_send));
        }
        try {
            auto len = socket_.sendBytes(buf_send, len_send);
            LogHelper::Instance()->TcpLog(spdlog::level::debug, "{} send len:{} len_send:{}", _name, len, len_send);
            if (len < 0) {
                LogHelper::Instance()->TcpLog(spdlog::level::err, "{} send len < 0", _name);
                ret = -2;
            } else if (len != len_send) {
                LogHelper::Instance()->TcpLog(spdlog::level::err, "{} send len !=len_send", _name);
                ret = -2;
            }
        } catch (Poco::Exception &exc) {
            LogHelper::Instance()->TcpLog(spdlog::level::err, "{} send error:{}{}", _name, exc.code(),
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
};


#endif //MYTCPHANDLER_H
