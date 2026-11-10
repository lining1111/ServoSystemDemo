//
// Created by lining on 12/27/24.
//

#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "Poco/Net/WebSocket.h"
#include "Poco/Net/HTTPClientSession.h"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include "logHelper/LogHelper.h"
#include "spdlog/spdlog.h"

using Poco::Net::WebSocket;
using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;


namespace websocket {
class Client {
public:
    std::string server_ip;
    int server_port;
    bool isNeedReconnect = true;
    std::string _name;
    WebSocket *_ws = nullptr;
    bool _isRun = false;
    std::thread _tRecv;
    std::thread _tHeartbeat;

public:
    Client(std::string serverip, int serverport);

    ~Client();

    int Open();

    int Reconnect();

    int Run();

    //发送报文（一帧数据）
    int SendBase(std::string pkg);

    /**
     * 设置报文处理回调：入参为一帧完整报文（websocket 帧自带边界，无粘包/断包），返回应答报文(空表示不应答)，由上层设置
     */
    void SetPkgFun(const std::function<std::string(const std::string &pkg)> &fun);

private:
    static int ThreadRecv(Client *local);

    //分发完整报文给处理回调，返回应答报文
    std::string OnPkg(const std::string &pkg);

    static void ThreadHeartbeat(Client *local);

private:
    HTTPClientSession *_cs = nullptr;
    HTTPRequest *_req = nullptr;
    HTTPResponse *_rsp = nullptr;
    char *recvBuf = nullptr;
    //模块日志 handler（ws.log）
    std::shared_ptr<spdlog::logger> log_;
    std::mutex sendMtx_;
    std::mutex pkgFun_mutex_;
    std::function<std::string(const std::string &pkg)> pkgFun_;
};

} // namespace websocket

#endif //WEBSOCKET_CLIENT_H
