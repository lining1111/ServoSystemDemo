//
// Created by lining on 12/27/24.
//
// websocket 服务端测试：echo 模式，收到完整报文后回发 "echo:<报文>"
// 用法: test_websocketServer [port] [runSeconds]，默认 10001 端口运行 10 秒

#include <thread>

#include "websocket/server.h"
#include "logHelper/logHelper.h"

int main(int argc, char **argv) {
    int port = argc > 1 ? std::stoi(argv[1]) : 10001;
    int runSeconds = argc > 2 ? std::stoi(argv[2]) : 10;

    LogHelper::Instance()->Init("log", true);

    websocket::server srv(port);
    srv.SetPkgFun([](const std::string &pkg) -> std::string {
        return "echo:" + pkg;
    });

    if (srv.Open() == 0) {
        WS_LOG(spdlog::level::warn, "ws server running {}s, port:{}", runSeconds, port);
        std::this_thread::sleep_for(std::chrono::seconds(runSeconds));
        WS_LOG(spdlog::level::warn, "ws server exit, clientCount:{}", srv.GetClientCount());
    }
    srv.Close();

    return 0;
}
