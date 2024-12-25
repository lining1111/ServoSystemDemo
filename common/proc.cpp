//
// Created by lining on 2023/4/23.
//

#include "proc.h"
#include "common.h"
#include "config.h"
#include "localBusiness/localBusiness.h"
#include <glog/logging.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include "utils/utils.h"
#include <bitset>
#include <utility>

using namespace common;

void CacheTimestamp::update(int index, uint64_t timestamp, int caches) {
    std::unique_lock<std::mutex> lock(mtx);
    //如果已经更新了就不再进行下面的操作
    if (!isSetInterval) {
        //判断是否设置了index，index默认为-1
        if (dataIndex == -1) {
            dataIndex = index;
            dataTimestamps.clear();
            dataTimestamps.push_back(timestamp);
        } else {
            //判断是否是对应的index
            if (dataIndex == index) {
                //判断时间戳是否是递增的，如果不是的话，清除之前的内容，重新开始
                if (dataTimestamps.empty()) {
                    //是对应的index的话，将时间戳存进队列
                    dataTimestamps.push_back(timestamp);
                } else {
                    if (timestamp <= dataTimestamps.back()) {
                        //恢复到最初状态
                        LOG(ERROR) << "当前插入时间戳 " << timestamp << " 小于已插入的最新时间戳 "
                                   << dataTimestamps.back() << "，将恢复到最初状态";
                        dataTimestamps.clear();
                        dataIndex = -1;
                    } else {
                        //是对应的index的话，将时间戳存进队列，正常插入
                        dataTimestamps.push_back(timestamp);
                    }
                }
            }
        }
        //如果存满缓存帧后，计算帧率，并设置标志位
        if (dataTimestamps.size() == caches) {
            //打印下原始时间戳队列
            LOG(WARNING) << "动态帧率原始时间戳队列:" << fmt::format("{}", dataTimestamps);
            std::vector<uint64_t> intervals;
            for (int i = 1; i < dataTimestamps.size(); i++) {
                auto cha = dataTimestamps.at(i) - dataTimestamps.at(i - 1);
                intervals.push_back(cha);
            }
            //计算差值的平均数
            uint64_t sum = 0;
            for (auto iter: intervals) {
                sum += iter;
            }
            this->interval = sum / intervals.size();
//            printf("帧率的差和为%d\n", sum);
//            printf("计算的帧率为%d\n", this->interval);
            this->isSetInterval = true;
        }
    }
}


/*
 * 业务处理的接口基类
 */
template<class Req, class Rsp>
class Handler {
public:
    Req *req = nullptr;
    Rsp *rsp = nullptr;
    string _handler;
    string _content;
    string err;
public:

    void init(string h, string c) {
        _handler = std::move(h);
        _content = std::move(c);
        if (req != nullptr) {
            delete req;
        }
        req = new Req();
        if (rsp != nullptr) {
            delete rsp;
        }
        rsp = new Rsp();
    }

    //前处理，解析请求
    int dec() {
        int ret = 0;
        try {
            json::decode(_content, *req);
        } catch (std::exception &e) {
            err = e.what();
            LOG(ERROR) << "json decode err:" << err;
            ret = -1;
        }
        return ret;
    }

    //前处理失败后
    void decErr() {
        rsp->state = State_UnmarshalFail;
        rsp->param = "json decode err:" + err;
    }

    //前处理成功后，实际业务
    virtual void proc() = 0;

    //后处理，返回结果
    void enc() {
        if (rsp != nullptr && rsp->state != State_CmdExeNoRsp) {
            rsp->guid = req->guid;
            //code 以Req开头的，Req为Rsp
            if (req->code.find("Req") == 0) {
                rsp->code = "Rsp" + req->code.substr(3);
            } else {
                rsp->code = req->code;
            }

            string pkg = json::encode(*rsp);
            auto localBusiness = LocalBusiness::instance()
            if (localBusiness->SendToClient(_handler, pkg) != 0) {
                VLOG(3) << _handler << " send failed:" << pkg;
            } else {
                VLOG(3) << _handler << " send success:" << pkg;
            }
        }
    }

public:
    int exe(string h, string c) {
        init(h, c);
        if (dec() != 0) {
            decErr();
            enc();
            return -1;
        } else {
            proc();
            enc();
            return 0;
        }

    }
};
/**
 * 业务处理的具体类
 */

#define Handle(xxx) \
int Handle##xxx(const string &h, const string &content) { \
    Handler##xxx handler;                   \
    return handler.exe(h, content);          \
}                                           \


class HandlerHeartbeat : public Handler<Com, Com> {
public:
    void proc() final {
        auto localBusiness = LocalBusiness::instance();
        auto client = localBusiness->FindClient(_handler);
        if (client != nullptr) {
            ((MyTcpHandler *) client)->timeRecv = getTimestampMs();
        }
        rsp->param = "rsp";
    }
};

Handle(Heartbeat)


map<string, Handle> HandleRouter = {
        make_pair("Heartbeat", HandleHeartbeat),

};