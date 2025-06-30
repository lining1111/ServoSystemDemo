//
// Created by lining on 12/27/24.
//

#include "myWebsocket/MyWebsocketServer.h"

int main(int argc, char **argv) {
    auto srv = new MyWebsocketServer(10001);
    if (srv->Open() == 0) {
        srv->Run();
    }
//    while (1) {
//        sleep(5);
//    }
    srv->Stop();
    delete srv;

    return 0;
}