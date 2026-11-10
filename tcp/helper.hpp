//
// Created by lining on 2026/11/10.
//
// TCP 分包/处理辅助类：Server 与 Client 共用
//   第一步 SplitPkg：按包尾'*'分包，兼容断包（半包留缓存续接）、粘包（一次拆多个完整包入队）
//   第二步 ProcessPkgQueue：轮询待处理队列，逐包交给报文处理回调，非空应答经发送函数回发

#ifndef TCPHELPER_H
#define TCPHELPER_H

#include <deque>
#include <functional>
#include <mutex>
#include <string>


namespace tcp {
class helper {
public:
    //报文处理回调：入参为去掉包尾'*'的报文，返回应答报文(空表示不应答)
    using PkgFun = std::function<std::string(const std::string &pkg)>;
    //应答发送函数：入参为应答报文，返回发送结果
    using SendFun = std::function<int(const std::string &rsp)>;

    /**
     * 设置报文处理回调（对应 server/client 的 OnPkg）
     */
    void SetPkgFun(PkgFun fun) {
        std::lock_guard<std::mutex> lock(funMtx_);
        pkgFun_ = std::move(fun);
    }

    /**
     * 设置应答发送函数（对应 server/client 的 SendBase）
     */
    void SetSendFun(SendFun fun) {
        std::lock_guard<std::mutex> lock(funMtx_);
        sendFun_ = std::move(fun);
    }

    /**
     * 第一步：分包处理（'*'为包尾），完整包进入待处理队列
     * 粘包：一次收包中含多个'*'，可连续拆出多个完整包入队
     * 断包：未遇'*'的数据留在分包缓存，下次收包继续拼接
     */
    void SplitPkg(const char *data, size_t len) {
        std::lock_guard<std::mutex> lock(pkgMtx_);
        for (size_t i = 0; i < len; i++) {
            if (data[i] == '*') {
                pkgQueue_.push_back(pkgCache_);
                pkgCache_.clear();
            } else {
                pkgCache_.push_back(data[i]);
            }
        }
    }

    /**
     * 第二步：轮询待处理包队列，逐包处理
     * 取包在锁内，回调与应答在锁外执行，慢处理不阻塞下一轮分包
     */
    void ProcessPkgQueue() {
        PkgFun onPkg;
        SendFun send;
        {
            std::lock_guard<std::mutex> lock(funMtx_);
            onPkg = pkgFun_;
            send = sendFun_;
        }
        while (true) {
            std::string pkg;
            {
                std::lock_guard<std::mutex> lock(pkgMtx_);
                if (pkgQueue_.empty()) {
                    break;
                }
                pkg = std::move(pkgQueue_.front());
                pkgQueue_.pop_front();
            }
            //锁外处理：回调与应答耗时不应阻塞下一轮分包
            std::string rsp = onPkg ? onPkg(pkg) : std::string();
            if (!rsp.empty() && send) {
                send(rsp);
            }
        }
    }

private:
    //分包缓存：未凑齐包尾'*'的断包数据
    std::string pkgCache_;
    //待处理完整包队列：SplitPkg 入队，ProcessPkgQueue 轮询出队处理
    std::deque<std::string> pkgQueue_;
    std::mutex pkgMtx_; //分包缓存与待处理队列互斥锁
    std::mutex funMtx_; //回调互斥锁
    PkgFun pkgFun_;
    SendFun sendFun_;
};

} // namespace tcp

#endif //TCPHELPER_H
