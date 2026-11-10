//
// Created by lining on 2023/8/21.
//

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "Helper.hpp"
#include "logHelper/LogHelper.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/SocketConnector.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/Observer.h"
#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>

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


namespace tcp {
class Server;

//TCP 服务端连接处理器：一个客户端连接对应一个实例
//构造时登记到所属 Server 的 clients，析构时注销（参考 GigECameraTest/tcpServer/Server）
class ServerHandler : public Poco::Net::TCPServerConnection {
public:
    std::string _name;
    StreamSocket socket_;
    //最近一次收到数据的时间戳(ms)，供超时剔除使用
    uint64_t timeRecv = 0;
    char *recvBuf = nullptr;

private:
    //所属服务器：构造时登记到 clients，析构时注销
    Server *server_ = nullptr;
    //模块日志 handler（tcp.log）
    std::shared_ptr<spdlog::logger> log_;
    //分包与轮询处理辅助类（与 Client 共用）
    Helper helper_;
    std::mutex send_mutex_;

public:
    explicit ServerHandler(const StreamSocket &socket, Server *server = nullptr);

    ~ServerHandler() override;

    void run() override;

    //发送报文，自动补包尾'*'
    int SendBase(std::string pkg);
};


class ConnectionFactory final : public Poco::Net::TCPServerConnectionFactory {
public:
    explicit ConnectionFactory(Server *server = nullptr) : server_(server) {
    }

    Poco::Net::TCPServerConnection *createConnection(const StreamSocket &socket) override {
        return new ServerHandler(socket, server_);
    }

private:
    Server *server_ = nullptr;
};


class Server {
    friend class ServerHandler;

public:
    //接入的客户端数组，key 为 peerAddress
    struct ClientInfo {
        StreamSocket socket;
        //最近一次收到数据的时间戳(ms)
        uint64_t timeRecv = 0;
    };
    std::map<std::string, ClientInfo> clients;
    std::mutex clients_mutex;
    int _port;
    bool isListen = false;
    Poco::Net::TCPServer *srv = nullptr;

public:
    explicit Server(int port);

    ~Server();

    int Open();

    int ReOpen();

    int Close();

    /**
     * 设置完整报文处理回调：入参为去掉包尾'*'的报文，返回应答报文(空表示不应答)，由上层设置
     */
    void SetPkgFun(const std::function<std::string(const std::string &pkg)> &fun);

    /**
     * 登记一个客户端连接，key 为 peerAddress
     */
    void AddClient(const std::string &name, const StreamSocket &socket);

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
     * 向指定客户端发送数据(自动补包尾'*')
     * @return 0 成功 -1 失败（客户端不存在或发送异常）
     */
    int SendToClient(const std::string &name, const std::string &data);

    /**
     * 向所有客户端广播数据(自动补包尾'*')
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

    //模块日志 handler（tcp.log）
    std::shared_ptr<spdlog::logger> log_;
    std::function<std::string(const std::string &pkg)> pkgFun_;
    std::mutex pkgFun_mutex_;
};


} // namespace tcp

#endif //TCP_SERVER_H
