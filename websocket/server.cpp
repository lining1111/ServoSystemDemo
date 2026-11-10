//
// Created by lining on 12/27/24.
//

#include "server.h"

#include "logHelper/logHelper.h"
#include "utils/utils.h"

namespace websocket {

RequestHandler::RequestHandler(server *server) : server_(server) {
    //获取 ws 模块日志 handler
    log_ = LogHelper::Instance()->GetLogger("ws");
    recvBuf = new char[1024 * 1024];
}

RequestHandler::~RequestHandler() {
    if (!_name.empty()) {
        LOG_TO(log_, spdlog::level::warn, "{} disconnected", _name);
        //先注销再关闭，避免其他线程拿到已失效的连接
        if (server_) {
            server_->RemoveClient(_name);
        }
    }
    if (_ws) {
        try {
            _ws->close();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::debug, "{} close err:{}", _name, e.displayText());
        }
    }
    delete _ws;
    delete[] recvBuf;
}

void RequestHandler::handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) {
    try {
        _ws = new WebSocket(request, response);
        _name = _ws->peerAddress().toString();
        LOG_TO(log_, spdlog::level::warn, "websocket client connect:{}", _name);
        //连接建立即登记，供 SendToClient / SendToAllClients / GetClientCount 等使用
        if (server_) {
            server_->AddClient(_name, this, _ws);
        }
        int flags = 0;
        int len = 0;
        do {
            memset(recvBuf, 0, 1024 * 1024);
            len = _ws->receiveFrame(recvBuf, 1024 * 1024, flags);
            if (len == 0 && flags == 0) {
                break;
            }
            //刷新最近收包时间，供超时剔除使用
            timeRecv = getTimestampMs();
            if (server_) {
                server_->TouchClient(_name);
            }
            //websocket 帧自带消息边界（无粘包/断包），一帧即一条完整报文
            if (len > 0) {
                std::string rsp = server_ ? server_->OnPkg(std::string(recvBuf, len)) : std::string();
                if (!rsp.empty()) {
                    SendBase(rsp);
                }
            }
        } while (len > 0 || (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);
        LOG_TO(log_, spdlog::level::warn, "websocket client disconnect:{}", _name);
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
    //!!!Here, do not delete this because Poco will enter the destructor.
}

int RequestHandler::SendBase(std::string pkg) {
    int ret = 0;
    std::unique_lock<std::mutex> lock(sendMtx_);
    if (_ws == nullptr || pkg.empty()) {
        return 0;
    }
    try {
        auto len = _ws->sendFrame(pkg.data(), pkg.length());
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

server::server(const int port) : _port(port) {
    //获取 ws 模块日志 handler
    log_ = LogHelper::Instance()->GetLogger("ws");
}

server::~server() {
    if (srv != nullptr) {
        srv->stop();
        delete srv;
    }
    LOG_TO(log_, spdlog::level::warn, "websocket server:{} close", _port);
}

int server::Open() {
    try {
        srv = new Poco::Net::HTTPServer(new Handler(this), _port);
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "ws server err:{}", e.displayText());
        return -1;
    }
    srv->start();
    isListen = true;
    return 0;
}

int server::ReOpen() {
    try {
        srv = new Poco::Net::HTTPServer(new Handler(this), _port);
    } catch (Poco::Exception &e) {
        LOG_TO(log_, spdlog::level::err, "ws server err:{}", e.displayText());
        return -1;
    }
    srv->start();
    isListen = true;
    return 0;
}

int server::Close() {
    isListen = false;
    if (srv != nullptr) {
        srv->stop();
        delete srv;
        //置空避免析构函数再次 delete
        srv = nullptr;
    }
    CloseAllClients();
    return 0;
}

void server::SetPkgFun(const std::function<std::string(const std::string &pkg)> &fun) {
    std::lock_guard<std::mutex> lock(pkgFun_mutex_);
    pkgFun_ = fun;
}

std::string server::OnPkg(const std::string &pkg) {
    std::function<std::string(const std::string &pkg)> fun;
    {
        std::lock_guard<std::mutex> lock(pkgFun_mutex_);
        fun = pkgFun_;
    }
    if (!fun) {
        LOG_TO(log_, spdlog::level::err, "ws server:{} 未设置报文处理回调,丢弃报文:{}", _port, pkg);
        return "";
    }
    return fun(pkg);
}

void server::AddClient(const std::string &name, RequestHandler *handler, WebSocket *ws) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients[name] = {handler, ws, getTimestampMs()};
}

bool server::RemoveClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    return clients.erase(name) > 0;
}

bool server::HasClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    return clients.find(name) != clients.end();
}

int server::GetClientCount() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    return static_cast<int>(clients.size());
}

std::vector<std::string> server::GetClientNames() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::vector<std::string> names;
    names.reserve(clients.size());
    for (const auto &iter: clients) {
        names.push_back(iter.first);
    }
    return names;
}

int server::SendToClient(const std::string &name, const std::string &data) {
    RequestHandler *handler = nullptr;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto iter = clients.find(name);
        if (iter == clients.end()) {
            LOG_TO(log_, spdlog::level::warn, "ws client not found:{}", name);
            return -1;
        }
        handler = iter->second.handler;
    }
    //取到句柄后在锁外发送，避免阻塞 IO 影响其他线程访问 clients
    if (handler == nullptr) {
        return -1;
    }
    return handler->SendBase(data) == 0 ? 0 : -1;
}

int server::SendToAllClients(const std::string &data) {
    //先做快照，发送期间不持锁
    std::vector<std::pair<std::string, RequestHandler *>> snapshot;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        snapshot.reserve(clients.size());
        for (const auto &iter: clients) {
            snapshot.emplace_back(iter.first, iter.second.handler);
        }
    }
    int okCount = 0;
    for (auto &iter: snapshot) {
        if (iter.second != nullptr && iter.second->SendBase(data) == 0) {
            ++okCount;
        }
    }
    return okCount;
}

void server::CloseAllClients() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &iter: clients) {
        if (iter.second.ws == nullptr) {
            continue;
        }
        try {
            iter.second.ws->shutdown();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::debug, "ws client {} shutdown err:{}", iter.first,
                                         e.displayText());
        }
        try {
            iter.second.ws->close();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::debug, "ws client {} close err:{}", iter.first,
                                         e.displayText());
        }
    }
    clients.clear();
}

void server::TouchClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto iter = clients.find(name);
    if (iter != clients.end()) {
        iter->second.timeRecv = getTimestampMs();
    }
}

void server::KickoffTimeoutClients(uint64_t now, uint64_t timeout) {
    std::vector<std::pair<std::string, WebSocket *>> stale;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto &iter: clients) {
            if (iter.second.timeRecv > 0
                && now > iter.second.timeRecv
                && now - iter.second.timeRecv > timeout) {
                stale.emplace_back(iter.first, iter.second.ws);
            }
        }
    }
    //锁外关闭连接，handler 析构时会自行 RemoveClient
    for (const auto &iter: stale) {
        LOG_TO(log_, spdlog::level::warn, "接收超时,主动断开客户端:{}", iter.first);
        if (iter.second == nullptr) {
            continue;
        }
        try {
            iter.second->shutdown();
        } catch (Poco::Exception &e) {
            LOG_TO(log_, spdlog::level::debug, "ws client {} shutdown err:{}", iter.first,
                                         e.displayText());
        }
    }
}

} // namespace websocket