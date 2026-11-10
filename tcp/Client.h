//
// Created by lining on 2023/8/21.
//

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/SocketConnector.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"
#include <Poco/Exception.h>
#include "Poco/Observer.h"
#include "logHelper/LogHelper.h"
#include "spdlog/spdlog.h"
#include "Helper.hpp"

using Poco::Net::SocketReactor;
using Poco::Net::SocketConnector;
using Poco::Net::SocketAcceptor;
using Poco::Net::StreamSocket;
using Poco::Net::ServerSocket;
using Poco::Net::SocketAddress;
using Poco::Net::SocketNotification;
using Poco::Net::ReadableNotification;
using Poco::Net::WritableNotification;
using Poco::Net::TimeoutNotification;
using Poco::Net::ShutdownNotification;
using Poco::Observer;
using Poco::Net::NetException;
using Poco::Net::ConnectionRefusedException;
using Poco::Net::InvalidSocketException;
using Poco::TimeoutException;


namespace tcp {
class Client {
public:
    std::string server_ip;
    int server_port;
    bool isNeedReconnect = true;
    std::string _name;
    StreamSocket socket_;
    bool _isRun = false;
    SocketReactor _reactor;
    Poco::Thread _t;
    char *recvBuf = nullptr;
    std::thread _tHeartbeat;

public:
    Client(std::string serverip, int serverport);

    ~Client();

    int Open();

    int Reconnect();

    int Run();

    //发送报文，自动补包尾'*'
    int SendBase(std::string pkg);

    /**
     * 设置完整报文处理回调：入参为去掉包尾'*'的报文，返回应答报文(空表示不应答)，由上层设置
     */
    void SetPkgFun(const std::function<std::string(const std::string &pkg)> &fun);

private:
    void onReadable(ReadableNotification *pNf);

    //分发完整报文给处理回调，返回应答报文
    std::string OnPkg(const std::string &pkg);

    static void ThreadHeartbeat(Client *local);

private:
    //模块日志 handler（tcp.log）
    std::shared_ptr<spdlog::logger> log_;
    std::mutex send_mutex_;
    std::mutex pkgFun_mutex_;
    std::function<std::string(const std::string &pkg)> pkgFun_;
    //分包与轮询处理辅助类（与 Server 共用）
    Helper helper_;
};

} // namespace tcp

#endif //TCP_CLIENT_H
