//
// Created by lining on 7/19/24.
//

#ifndef LOCALBUSINESS_H
#define LOCALBUSINESS_H

#include "utils/nonCopyable.hpp"
#include <map>
#include <string>
#include "Poco/Timer.h"
#include <memory>
#include "myTcp/MyTcpClient.h"
#include "myTcp/MyTcpServer.h"
#include "myWebsocket/MyWebsocketServer.h"
#include "myWebsocket/MyWebsocketClient.h"

using namespace std;

class LocalBusiness : public NonCopyable {
public:
    static LocalBusiness *m_pInstance;

    bool isRun = false;
    std::map<string, shared_ptr<MyTcpServer>> serverList;
    std::map<string, shared_ptr<MyTcpClient>> clientList;
    std::map<string, shared_ptr<MyWebsocketServer>> wsServerList;
    std::map<string, shared_ptr<MyWebsocketClient>> wsClientList;
private:
    mutex mtx;
    vector<shared_ptr<MyTcpServerHandler>> _conns;//接入的客户端数组

    mutex mtx_ws;
    vector<MyWebSocketRequestHandler *> _conns_ws;//接入的客户端数组,由于Poco的WS client部分采用了智能指针，这里就不要使用智能指针了，容易出现重复释放引起的SIGSEGV

public:
    static LocalBusiness *instance();

    ~LocalBusiness() {
        if (isRun) {
            Stop();
        }
    };

    void AddServer(const string &name, int port, bool isTcp = true);

    void AddClient(const string &name, const string &cloudIp, int cloudPort, bool isTcp = true);

    void Run();

    void Stop();

    void addConn(shared_ptr<MyTcpServerHandler> p);

    void addConn_ws(MyWebSocketRequestHandler* p);

    void delConn(const string &peerAddress, bool isTcp = true);

    void stopAllConns(bool isTcp = true);

    void Broadcast(const string &msg);

    int SendToClient(const string &peerAddress, const string &msg);

    /*
     * clientType
     * 0 local tcp client
     * 1 remote tcp client
     * 2 local ws client
     * 3 remote ws client
     */

    enum CLIType {
        CT_LOCALTCP = 0,
        CT_REMOTETCP = 1,
        CT_LOCALWS = 2,
        CT_REMOTEWS = 3,
    };

    void *FindClient(const string &peerAddress, CLIType &clientType);

    void kickoff(uint64_t timeout, uint64_t now, CLIType clientType = CT_LOCALTCP);


public:
    void StartTimerTask();

    void StopTimerTaskAll();

    void ShowInfo();

public:
    Poco::Timer timerKeep;

private:
    void Task_Keep(Poco::Timer &timer);
};


#endif //LOCALBUSINESS_H
