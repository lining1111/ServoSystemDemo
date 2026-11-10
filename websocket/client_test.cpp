//
// Created by lining on 12/27/24.
//
// websocket 客户端测试：连接服务端后发送报文，收到的应答打印到控制台
// 用法: test_websocketClient [ip] [port] [runSeconds]，默认 127.0.0.1:10001 运行 10 秒

#include <iostream>
#include <thread>

#include "websocket/client.h"
#include "logHelper/logHelper.h"

int main(int argc, char **argv) {
    std::string ip = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? std::stoi(argv[2]) : 10001;
    int runSeconds = argc > 3 ? std::stoi(argv[3]) : 10;

    LogHelper::Instance()->Init("log", true);

    websocket::client wsClient(ip, port);
    wsClient.SetPkgFun([](const std::string &pkg) -> std::string {
        std::cout << "recv rsp: " << pkg << std::endl;
        return "";
    });

    if (wsClient.Open() == 0) {
        wsClient.Run();
        std::thread sendTask([&wsClient] {
            for (int i = 1; i <= 3; i++) {
                wsClient.SendBase("hello " + std::to_string(i));
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
            }
        });
        sendTask.detach();
        std::this_thread::sleep_for(std::chrono::seconds(runSeconds));
    }

    return 0;
}
