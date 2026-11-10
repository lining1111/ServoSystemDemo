//
// Created by lining on 2023/8/21.
//

#include "Server.h"

#include "logHelper/LogHelper.h"
#include "utils/utils.h"

namespace tcp {

ServerHandler::ServerHandler(const StreamSocket &socket, Server *server)
        : TCPServerConnection(socket), server_(server) {
    //获取 tcp 模块日志 handler
    log_ = LogHelper::Instance()->GetLogger("tcp");
    socket_ = socket;
    _name = socket.peerAddress().toString();
    LOG_TO(log_, spdlog::level::warn, "connection from {}", _name);
    recvBuf = new char[1024 * 1024];
    //注入分包处理的报文路由与应答发送
    helper_.SetPkgFun([this](const std::string &pkg) -> std::string {
        return server_ ? server_->OnPkg(pkg) : std::string();
    });
    helper_.SetSendFun([this](const std::string &rsp) -> int {
        return SendBase(rsp);
    });
    //连接建立即登记，供 SendToClient / SendToAllClients / GetClientCount 等使用
    if (server_) {
        server_->AddClient(_name, socket_);
    }
}

ServerHandler::~ServerHandler() {
    LOG_TO(log_, spdlog::level::warn, "{} disconnected", _name);
    //先注销再关闭，避免其他线程拿到已失效的连接
    if (server_) {
        server_->RemoveClient(_name);
    }
    socket_.close();
    delete[] recvBuf;
}

void ServerHandler::run() {
    memset(recvBuf, 0, 1024 * 1024);
    int recvLen = 1024 * 1024;
    try {
        int len = 0;
        do {
            len = socket_.receiveBytes(recvBuf, recvLen);
            if (len <= 0) {
                break;
            }
            //刷新最近收包时间，供超时剔除使用
            timeRecv = getTimestampMs();
            if (server_) {
                server_->TouchClient(_name);
            }
            //第一步：分包处理（'*'为包尾），完整包进入待处理队列
            helper_.SplitPkg(recvBuf, static_cast<size_t>(len));
            //第二步：轮询待处理缓存，逐包处理
            helper_.ProcessPkgQueue();
        } while (len > 0);
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::warn, "{}", e.displayText());
    }
    //!!!Here, do not delete this because Poco will enter the destructor.
}

int ServerHandler::SendBase(std::string pkg) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(send_mutex_);
    if (pkg.empty()) {
        return 0;
    }
    //补包尾
    if (pkg.back() != '*') {
        pkg.push_back('*');
    }
    try {
        auto len = socket_.sendBytes(pkg.data(), pkg.length());
        LOG_TO(log_, spdlog::level::debug, "{} send len:{} len_send:{}", _name, len,
                                      to_string(pkg.length()));
        if (len < 0 || len != (int) pkg.size()) {
            LOG_TO(log_, spdlog::level::err, "{} send len error", _name);
            ret = -2;
        }
    } catch (Poco::Exception &exc) {
        LOG_TO(log_, spdlog::level::err, "{} send error:{}{}", _name, exc.code(),
                                      exc.displayText());
        if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK && exc.code() != POCO_EAGAIN) {
            ret = -2;
        } else {
            ret = -3;
        }
    }
    return ret;
}

Server::Server(const int port) : _port(port) {
    //获取 tcp 模块日志 handler
    log_ = LogHelper::Instance()->GetLogger("tcp");
}

Server::~Server() {
    if (srv) {
        srv->stop();
        delete srv;
    }
    LOG_TO(log_, spdlog::level::warn, "tcp server:{} close", _port);
}

int Server::Open() {
    try {
        srv = new Poco::Net::TCPServer(new ConnectionFactory(this), ServerSocket(_port));
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "tcp server err:{}", e.displayText());
        return -1;
    }
    srv->start();
    isListen = true;
    return 0;
}

int Server::ReOpen() {
    try {
        srv = new Poco::Net::TCPServer(new ConnectionFactory(this), ServerSocket(_port));
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "tcp server err:{}", e.displayText());
        return -1;
    }
    srv->start();
    isListen = true;
    return 0;
}

int Server::Close() {
    isListen = false;
    if (srv) {
        srv->stop();
        delete srv;
        //置空避免析构函数再次 delete
        srv = nullptr;
    }
    CloseAllClients();
    return 0;
}

void Server::SetPkgFun(const std::function<std::string(const std::string &pkg)> &fun) {
    std::lock_guard<std::mutex> lock(pkgFun_mutex_);
    pkgFun_ = fun;
}

std::string Server::OnPkg(const std::string &pkg) {
    std::function<std::string(const std::string &pkg)> fun;
    {
        std::lock_guard<std::mutex> lock(pkgFun_mutex_);
        fun = pkgFun_;
    }
    if (!fun) {
        LOG_TO(log_, spdlog::level::err, "tcp server:{} 未设置报文处理回调,丢弃报文:{}", _port, pkg);
        return "";
    }
    return fun(pkg);
}

void Server::AddClient(const std::string &name, const StreamSocket &socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    //StreamSocket 是引用计数的句柄，拷贝开销很小
    clients[name] = {socket, getTimestampMs()};
}

bool Server::RemoveClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    return clients.erase(name) > 0;
}

bool Server::HasClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    return clients.find(name) != clients.end();
}

int Server::GetClientCount() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    return static_cast<int>(clients.size());
}

std::vector<std::string> Server::GetClientNames() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::vector<std::string> names;
    names.reserve(clients.size());
    for (const auto &iter: clients) {
        names.push_back(iter.first);
    }
    return names;
}

int Server::SendToClient(const std::string &name, const std::string &data) {
    StreamSocket socket;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto iter = clients.find(name);
        if (iter == clients.end()) {
            LOG_TO(log_, spdlog::level::warn, "tcp client not found:{}", name);
            return -1;
        }
        socket = iter->second.socket;
    }
    //取到句柄后在锁外发送，避免阻塞 IO 影响其他线程访问 clients
    try {
        std::string pkg = data;
        //补包尾
        if (!pkg.empty() && pkg.back() != '*') {
            pkg.push_back('*');
        }
        int sent = socket.sendBytes(pkg.data(), static_cast<int>(pkg.size()));
        if (sent == static_cast<int>(pkg.size())) {
            LOG_TO(log_, spdlog::level::debug, "tcp send to {}:{}", name, pkg);
            return 0;
        }
        LOG_TO(log_, spdlog::level::err, "tcp send to {} incomplete:{}/{}", name, sent, pkg.size());
        return -1;
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "tcp send to {} err:{}", name, e.displayText());
        return -1;
    }
}

int Server::SendToAllClients(const std::string &data) {
    //先做快照，发送期间不持锁
    std::vector<std::pair<std::string, StreamSocket>> snapshot;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        snapshot.reserve(clients.size());
        for (const auto &iter: clients) {
            snapshot.emplace_back(iter.first, iter.second.socket);
        }
    }
    std::string pkg = data;
    //补包尾
    if (!pkg.empty() && pkg.back() != '*') {
        pkg.push_back('*');
    }
    int okCount = 0;
    for (auto &iter: snapshot) {
        try {
            if (iter.second.sendBytes(pkg.data(), static_cast<int>(pkg.size()))
                == static_cast<int>(pkg.size())) {
                ++okCount;
            }
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::err, "tcp send to {} err:{}", iter.first, e.displayText());
        }
    }
    return okCount;
}

void Server::CloseAllClients() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &iter: clients) {
        try {
            iter.second.socket.shutdown();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::debug, "tcp client {} shutdown err:{}", iter.first,
                                          e.displayText());
        }
        try {
            iter.second.socket.close();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::debug, "tcp client {} close err:{}", iter.first,
                                          e.displayText());
        }
    }
    clients.clear();
}

void Server::TouchClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto iter = clients.find(name);
    if (iter != clients.end()) {
        iter->second.timeRecv = getTimestampMs();
    }
}

void Server::KickoffTimeoutClients(uint64_t now, uint64_t timeout) {
    std::vector<std::string> stale;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto &iter: clients) {
            if (iter.second.timeRecv > 0
                && now > iter.second.timeRecv
                && now - iter.second.timeRecv > timeout) {
                stale.push_back(iter.first);
            }
        }
    }
    //锁外关闭连接，析构时 Handler 会自行 RemoveClient
    for (const auto &name: stale) {
        LOG_TO(log_, spdlog::level::warn, "接收超时,主动断开客户端:{}", name);
        StreamSocket socket;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto iter = clients.find(name);
            if (iter == clients.end()) {
                continue;
            }
            socket = iter->second.socket;
        }
        try {
            socket.shutdown();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::debug, "tcp client {} shutdown err:{}", name,
                                          e.displayText());
        }
        try {
            socket.close();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::debug, "tcp client {} close err:{}", name,
                                          e.displayText());
        }
    }
}

} // namespace tcp