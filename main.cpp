#include <Poco/Util/ServerApplication.h>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include  "version.h"
#include <iostream>
#include <csignal>
#include <gflags/gflags.h>
#include <Poco/Path.h>
#include "logHelper/LogHelper.h"
#include "utils/utils.h"
#include "config/config.h"
#include "localBusiness/localBusiness.h"

DEFINE_bool(isSendSTDOUT, false, "输出到控制台，默认false");
DEFINE_string(logDir, "log", "日志的输出目录,默认log");
DEFINE_string(configFile, "./config.yaml", "配置文件路径，默认./config.yaml");

class MyApp : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string> &args) override {
        LogHelper::Instance()->BizLog(spdlog::level::warn, "获取程序参数:{}", FLAGS_configFile);
        if (localConfig.getDeviceConfigFromYAML(FLAGS_configFile) != 0) {
            LogHelper::Instance()->BizLog(spdlog::level::err, "获取配置文件失败");
            return -1;
        }
        device.Init();

        auto localBusiness = LocalBusiness::instance();
        //Tcp
        {
            auto config = localConfig._config.localTcpServerConfig;
            if (config.isUse) {
                LogHelper::Instance()->BizLog(spdlog::level::warn, "开启本地tcp server通信");
                localBusiness->AddServer(config.name, config.port);
            }
        }
        {
            auto config = localConfig._config.remoteTcpServerConfig;
            if (config.isUse) {
                LogHelper::Instance()->BizLog(spdlog::level::warn, "开启远程tcp server通信");
                localBusiness->AddClient(config.name, config.ip, config.port);
            }
        }
        //WS
        {
            auto config = localConfig._config.localWSServerConfig;
            if (config.isUse) {
                LogHelper::Instance()->BizLog(spdlog::level::warn, "开启本地ws server通信");
                localBusiness->AddServer(config.name, config.port, false);
            }
        }
        {
            auto config = localConfig._config.remoteWSServerConfig;
            if (config.isUse) {
                LogHelper::Instance()->BizLog(spdlog::level::warn, "开启远程ws server通信");
                localBusiness->AddClient(config.name, config.ip, config.port, false);
            }
        }

        localBusiness->Run();
        bool isExit = false;
        std::thread t([&] {
            while (!isExit) {
                std::this_thread::sleep_for(5s);

                {
                    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    auto config = localConfig._config.localTcpServerConfig;
                    if (config.isUse && config.isCheckClient) {
                        localBusiness->kickoff(config.timeout, now_ms, LocalBusiness::CT_REMOTETCP);
                    }
                    auto config_ws = localConfig._config.localWSServerConfig;
                    if (config.isUse && config.isCheckClient) {
                        localBusiness->kickoff(config.timeout, now_ms, LocalBusiness::CT_REMOTEWS);
                    }
                }
                device.Keep();
            }
        });
        t.detach();
        waitForTerminationRequest();
        isExit = true;
        localBusiness->stopAllConns();
        localBusiness->stopAllConns(false);
        localBusiness->Stop();
        delete localBusiness;

        device.Exit();

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

    LogHelper::Instance()->BizLog(spdlog::level::warn, "程序工作目录:{},版本号:{}", Poco::Path::current(), PROJECT_VERSION);
    LogHelper::Instance()->BizLog(spdlog::level::warn, "GIT BRANCH:{},COMMIT:{},COMMIT_TIME:{}", GIT_BRANCH,
                                  GIT_COMMIT, GIT_COMMIT_TIME);
#if defined(__linux__)
    signal(SIGPIPE, SIG_IGN);
#endif
    MyApp app;
    return app.run(argc, argv);
}

// int main(int argc, char **argv) {
//     gflags::SetVersionString(VERSION_BUILD_TIME);
//     gflags::ParseCommandLineFlags(&argc, &argv, true);
// #ifdef WIN32
//     // 设置控制台输出代码页为 UTF-8
//     SetConsoleOutputCP(65001);
// #endif
//     LogHelper::Instance()->Init(FLAGS_logDir, FLAGS_isSendSTDOUT);
//
//     LogHelper::Instance()->BizLog(spdlog::level::warn, "程序工作目录:{},版本号:{}", Poco::Path::current(), VERSION_BUILD_TIME);
//     LogHelper::Instance()->BizLog(spdlog::level::warn, "获取程序参数:{}", FLAGS_configFile);
//     if (localConfig.getDeviceConfigFromYAML(FLAGS_configFile) != 0) {
//         LogHelper::Instance()->BizLog(spdlog::level::err, "获取配置文件失败");
//         return -1;
//     }
//
//     device.Init();
// #if defined(__linux__)
//     signal(SIGPIPE, SIG_IGN);
//     signal(SIGUSR1, handleSignalUSR1); //USR1 show local info
// #endif
//     signal(SIGINT, handleSignal); //Ctrl+C
//     signal(SIGTERM, handleSignal); //kill
//
//     auto localBusiness = LocalBusiness::instance();
//     //Tcp
//     {
//         auto config = localConfig._config.localTcpServerConfig;
//         if (config.isUse) {
//             LogHelper::Instance()->BizLog(spdlog::level::warn, "开启本地tcp server通信");
//             localBusiness->AddServer(config.name, config.port);
//         }
//     }
//     {
//         auto config = localConfig._config.remoteTcpServerConfig;
//         if (config.isUse) {
//             LogHelper::Instance()->BizLog(spdlog::level::warn, "开启远程tcp server通信");
//             localBusiness->AddClient(config.name, config.ip, config.port);
//         }
//     }
//     //WS
//     {
//         auto config = localConfig._config.localWSServerConfig;
//         if (config.isUse) {
//             LogHelper::Instance()->BizLog(spdlog::level::warn, "开启本地ws server通信");
//             localBusiness->AddServer(config.name, config.port, false);
//         }
//     }
//     {
//         auto config = localConfig._config.remoteWSServerConfig;
//         if (config.isUse) {
//             LogHelper::Instance()->BizLog(spdlog::level::warn, "开启远程ws server通信");
//             localBusiness->AddClient(config.name, config.ip, config.port, false);
//         }
//     }
//
//     localBusiness->Run();
//
//     while (true) {
//         if (isExit) {
//             break;
//         }
//         std::this_thread::sleep_for(5s);
//
//         {
//             auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
//                 std::chrono::system_clock::now().time_since_epoch()).count();
//             auto config = localConfig._config.localTcpServerConfig;
//             if (config.isUse && config.isCheckClient) {
//                 localBusiness->kickoff(config.timeout, now_ms, LocalBusiness::CT_REMOTETCP);
//             }
//             auto config_ws = localConfig._config.localWSServerConfig;
//             if (config.isUse && config.isCheckClient) {
//                 localBusiness->kickoff(config.timeout, now_ms, LocalBusiness::CT_REMOTEWS);
//             }
//         }
//         device.Keep();
//     }
//     localBusiness->stopAllConns();
//     localBusiness->stopAllConns(false);
//     localBusiness->Stop();
//     delete localBusiness;
//
//     device.Exit();
//
//     return 0;
// }
