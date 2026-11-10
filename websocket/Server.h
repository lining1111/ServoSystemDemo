//
// Created by lining on 12/27/24.
//

#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "logHelper/LogHelper.h"
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>

using Poco::Net::WebSocket;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;

using namespace Poco::Net;
using namespace std;

namespace websocket {

class Server;

//websocket 连接处理器：一个客户端连接对应一个实例
//构造时登记到所属 Server 的 clients，析构时注销（与 tcp::Server 模式一致）
class RequestHandler : public HTTPRequestHandler {
public:
    std::string _name;
    WebSocket *_ws = nullptr;
    char *recvBuf = nullptr;
    //最近一次收到数据的时间戳(ms)，供超时剔除使用
    uint64_t timeRecv = 0;

private:
    //所属服务器：构造时登记到 clients，析构时注销
    Server *server_ = nullptr;
    //模块日志 handler（ws.log）
    std::shared_ptr<spdlog::logger> log_;
    std::mutex sendMtx_;

public:
    explicit RequestHandler(Server *server = nullptr);

    ~RequestHandler() override;

    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) final;

    //发送报文（一帧数据）
    int SendBase(std::string pkg);
};

class Handler final : public HTTPRequestHandlerFactory {
public:
    explicit Handler(Server *server = nullptr) : server_(server) {
    }

    HTTPRequestHandler *createRequestHandler(const HTTPServerRequest &request) final {
        return new RequestHandler(server_);
    }

private:
    Server *server_ = nullptr;
};


class Server {
    friend class RequestHandler;

public:
    //接入的客户端数组，key 为 peerAddress
    struct ClientInfo {
        //handler 与 _ws 的生命周期由 Poco 管理，这里只存指针，不拥有
        RequestHandler *handler = nullptr;
        WebSocket *ws = nullptr;
        //最近一次收到数据的时间戳(ms)
        uint64_t timeRecv = 0;
    };
    std::map<std::string, ClientInfo> clients;
    std::mutex clients_mutex;
    int _port;
    bool isListen = false;
    Poco::Net::HTTPServer *srv = nullptr;

public:
    explicit Server(int port);

    ~Server();

    int Open();

    int ReOpen();

    int Close();

    /**
     * 设置报文处理回调：入参为一帧完整报文（websocket 帧自带边界，无粘包/断包），返回应答报文(空表示不应答)，由上层设置
     */
    void SetPkgFun(const std::function<std::string(const std::string &pkg)> &fun);

    /**
     * 登记一个客户端连接，key 为 peerAddress
     */
    void AddClient(const std::string &name, RequestHandler *handler, WebSocket *ws);

    /**
     * 移除客户端记录（只从列表删除，不主动关闭连接）
     * @return true 曾存在并已移除 false 不存在
     */
    bool RemoveClient(const std::string &name);

    /**
     * 判断客户端是否在列表中
     */
    bool HasClient(const std::string &name);

    /**
     * 当前在线客户端数量
     */
    int GetClientCount();

    /**
     * 当前在线客户端标识列表（快照，可用于遍历展示）
     */
    std::vector<std::string> GetClientNames();

    /**
     * 向指定客户端发送数据（一帧）
     * @return 0 成功 -1 失败（客户端不存在或发送异常）
     */
    int SendToClient(const std::string &name, const std::string &data);

    /**
     * 向所有客户端广播数据（一帧）
     * @return 发送成功的客户端数量
     */
    int SendToAllClients(const std::string &data);

    /**
     * 关闭并清空所有客户端连接
     */
    void CloseAllClients();

    /**
     * 收到数据时刷新客户端最近收包时间
     */
    void TouchClient(const std::string &name);

    /**
     * 剔除超过 timeout 未收数据的客户端（主动断开连接）
     */
    void KickoffTimeoutClients(uint64_t now, uint64_t timeout);

private:
    //分发完整报文给处理回调，返回应答报文
    std::string OnPkg(const std::string &pkg);

    //模块日志 handler（ws.log）
    std::shared_ptr<spdlog::logger> log_;
    std::function<std::string(const std::string &pkg)> pkgFun_;
    std::mutex pkgFun_mutex_;
};


} // namespace websocket


#endif //WEBSOCKET_SERVER_H
