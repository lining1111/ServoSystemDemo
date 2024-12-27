//
// Created by lining on 12/27/24.
//

#include "myWebsocket/MyWebsocketServerHandler.h"

int main(int argc,char **argv){
    Poco::Net::ServerSocket ss(10001);
    Poco::Net::HTTPServer svr(new MyWebsocketHandler,ss,new Poco::Net::HTTPServerParams());
    svr.start();
    while (1){
        sleep(5);
    }
    return 0;
}