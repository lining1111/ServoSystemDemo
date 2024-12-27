//
// Created by lining on 12/27/24.
//

#include "myWebsocket/MyWebsocketClient.h"

int main(int argc, char **argv) {
    MyWebsocketClient *wsClient = new MyWebsocketClient("127.0.0.1", 10001);

    if (wsClient->Open() == 0) {
        wsClient->Run();
    }

    while (1) {
        sleep(5);
    }
    return 0;
}