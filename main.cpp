#include <Poco/Util/ServerApplication.h>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include  "version.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <thread>
#include <gflags/gflags.h>
#include <Poco/Path.h>
#include "logHelper/LogHelper.h"
#include "utils/utils.h"
#include "config/config.h"
#include "common/proc.h"
#include "tcp/Server.h"
#include "tcp/Client.h"
#include "websocket/Server.h"
#include "websocket/Client.h"

DEFINE_bool(isSendSTDOUT, false, "输出到控制台，默认false");
DEFINE_string(logDir, "log", "日志的输出目录,默认log");
DEFINE_string(configFile, "./config.yaml", "配置文件路径，默认./config.yaml");

class MyApp : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string> &args) override {
        BIZ_LOG(spdlog::level::warn, "获取程序参数:{}", FLAGS_configFile);
        if (localConfig.getDeviceConfigFromYAML(FLAGS_configFile) != 0) {
            BIZ_LOG(spdlog::level::err, "获取配置文件失败");
            return -1;
        }

        //报文处理回调：分包后的完整报文路由到 HandleRouter，返回应答报文
        auto pkgFun = [](const std::string &pkg) -> std::string {
            return common::ProcessPkg(pkg);
        };

        //根据配置启动相应的服务/客户端
        shared_ptr<tcp::Server> tcpServer;
        shared_ptr<tcp::Client> tcpClient;
        shared_ptr<websocket::Server> wsServer;
        shared_ptr<websocket::Client> wsClient;

        //本地 tcp server
        {
            const auto &config = localConfig._config.localTcpServer;
            if (config.isUse) {
                BIZ_LOG(spdlog::level::warn, "开启本地tcp server通信:{}:{}", config.name, config.port);
                tcpServer = make_shared<tcp::Server>(config.port);
                tcpServer->SetPkgFun(pkgFun);
                if (tcpServer->Open() != 0) {
                    BIZ_LOG(spdlog::level::err, "tcp server {}:{} 启动失败", config.name, config.port);
                    tcpServer.reset();
                }
            }
        }
        //远程 tcp server（本地作为 client 连接）
        {
            const auto &config = localConfig._config.remoteTcpServer;
            if (config.isUse) {
                BIZ_LOG(spdlog::level::warn, "开启远程tcp client通信:{}:{}:{}", config.name, config.ip, config.port);
                tcpClient = make_shared<tcp::Client>(config.ip, config.port);
                tcpClient->SetPkgFun(pkgFun);
                if (tcpClient->Open() != 0) {
                    BIZ_LOG(spdlog::level::err, "tcp client {}:{}:{} 首次连接失败,等待自动重连",
                                                  config.name, config.ip, config.port);
                }
                //无论首次是否连上，Run 启动收发线程，断线由保活线程重连
                tcpClient->Run();
            }
        }
        //本地 websocket server
        {
            const auto &config = localConfig._config.localWSServer;
            if (config.isUse) {
                BIZ_LOG(spdlog::level::warn, "开启本地ws server通信:{}:{}", config.name, config.port);
                wsServer = make_shared<websocket::Server>(config.port);
                wsServer->SetPkgFun(pkgFun);
                if (wsServer->Open() != 0) {
                    BIZ_LOG(spdlog::level::err, "ws server {}:{} 启动失败", config.name, config.port);
                    wsServer.reset();
                }
            }
        }
        //远程 websocket server（本地作为 client 连接）
        {
            const auto &config = localConfig._config.remoteWSServer;
            if (config.isUse) {
                BIZ_LOG(spdlog::level::warn, "开启远程ws client通信:{}:{}:{}", config.name, config.ip, config.port);
                wsClient = make_shared<websocket::Client>(config.ip, config.port);
                wsClient->SetPkgFun(pkgFun);
                if (wsClient->Open() != 0) {
                    BIZ_LOG(spdlog::level::err, "ws client {}:{}:{} 首次连接失败,等待自动重连",
                                                  config.name, config.ip, config.port);
                }
                wsClient->Run();
            }
        }

        //保活线程：服务端客户端超时剔除 + 客户端断线重连
        bool isExit = false;
        std::thread keepThread([&] {
            while (!isExit) {
                std::this_thread::sleep_for(5s);
                auto now = getTimestampMs();
                //超时剔除
                {
                    const auto &config = localConfig._config.localTcpServer;
                    if (tcpServer && config.isUse && config.isCheckClient) {
                        tcpServer->KickoffTimeoutClients(now, config.timeout);
                    }
                    const auto &config_ws = localConfig._config.localWSServer;
                    if (wsServer && config_ws.isUse && config_ws.isCheckClient) {
                        wsServer->KickoffTimeoutClients(now, config_ws.timeout);
                    }
                }
                //断线重连
                if (tcpClient && tcpClient->isNeedReconnect) {
                    tcpClient->Reconnect();
                }
                if (wsClient && wsClient->isNeedReconnect) {
                    wsClient->Reconnect();
                }
            }
        });
        keepThread.detach();

        waitForTerminationRequest();
        isExit = true;

        //停止服务与客户端（客户端析构会收线程并关闭连接）
        if (tcpServer) {
            tcpServer->Close();
        }
        if (wsServer) {
            wsServer->Close();
        }
        tcpClient.reset();
        wsClient.reset();
        tcpServer.reset();
        wsServer.reset();

        //刷盘并关闭日志文件句柄（对齐 StorageService 的 defer logHelper.Close()）
        LogHelper::Instance()->Close();

        return Poco::Util::Application::EXIT_OK;
    }
};

int main(int argc, char **argv) {
    gflags::SetVersionString(PROJECT_VERSION);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
#ifdef WIN32
    // 设置控制台输出代码页为 UTF-8
    SetConsoleOutputCP(65001);
#endif
    LogHelper::Instance()->Init(FLAGS_logDir, FLAGS_isSendSTDOUT);

    BIZ_LOG(spdlog::level::warn, "程序工作目录:{},版本号:{}", Poco::Path::current(), PROJECT_VERSION);
    BIZ_LOG(spdlog::level::warn, "GIT BRANCH:{},COMMIT:{},COMMIT_TIME:{}", GIT_BRANCH,
                                  GIT_COMMIT, GIT_COMMIT_TIME);
#if defined(__linux__)
    signal(SIGPIPE, SIG_IGN);
#endif
    MyApp app;
    return app.run(argc, argv);
}
