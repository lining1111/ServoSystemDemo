//
// Created by lining on 2023/4/23.
//

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include "proc.h"
#include "common.h"
#include "config/config.h"
#include "localBusiness/localBusiness.h"
#include <glog/logging.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include "utils/utils.h"
#include <bitset>
#include <utility>


void CacheTimestamp::update(int index, uint64_t timestamp, int caches) {
    std::unique_lock<std::mutex> lock(mtx);
    //If it has already been updated, do not proceed with the following operations.
    if (!isSetInterval) {
        //Check if index is set; if not, the default value is -1.
        if (dataIndex == -1) {
            dataIndex = index;
            dataTimestamps.clear();
            dataTimestamps.push_back(timestamp);
        } else {
            //Check if it is the corresponding index.
            if (dataIndex == index) {
                //Check if the timestamp is increasing; if not, clear the previous content and start over.
                if (dataTimestamps.empty()) {
                    //if queue is empty push it
                    dataTimestamps.push_back(timestamp);
                } else {
                    if (timestamp <= dataTimestamps.back()) {
                        LOG(ERROR) << "当前插入时间戳 " << timestamp << " 小于已插入的最新时间戳 "
                                << dataTimestamps.back() << "，将恢复到最初状态";
                        dataTimestamps.clear();
                        dataIndex = -1;
                    } else {
                        //If it is the corresponding index, store the timestamp in the queue.
                        dataTimestamps.push_back(timestamp);
                    }
                }
            }
        }
        //After the cache frame is full, calculate the frame rate and set the flag.
        if (dataTimestamps.size() == caches) {
            LOG(WARNING) << "动态帧率原始时间戳队列:" << fmt::format("{}", dataTimestamps);
            std::vector<uint64_t> intervals;
            for (int i = 1; i < dataTimestamps.size(); i++) {
                auto cha = dataTimestamps.at(i) - dataTimestamps.at(i - 1);
                intervals.push_back(cha);
            }
            //calculate the average frame rate
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

#include <Poco/StringTokenizer.h>

namespace common {

    std::vector<std::string> split_path(const std::string& path) {
        Poco::StringTokenizer tokenizer(path, "/", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
        return std::vector<std::string>(tokenizer.begin(), tokenizer.end());
    }

    /*
     * Base class for business processing interfaces
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
        Handler() = default;

        virtual ~Handler() = default;

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

        //pre-processing, parse request
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

        //pre-processing fail, actual business
        void decErr() {
            rsp->state = State_UnmarshalFail;
            rsp->param = "json decode err:" + err;
        }

        //pre-processing success, actual business
        virtual void proc() = 0;

        //post-processing, return result
        void enc() {
            if (rsp != nullptr && rsp->state != State_CmdExeNoRsp) {
                rsp->guid = req->guid;
                //Code starting with Req, Req start with Rsp
                if (req->code.find("Req") == 0) {
                    rsp->code = "Rsp" + req->code.substr(3);
                } else {
                    rsp->code = req->code;
                }

                string pkg = json::encode(*rsp);
                auto localBusiness = LocalBusiness::instance();
                if (localBusiness->SendToClient(_handler, pkg) != 0) {
                    VLOG(3) << _handler << " send failed:" << pkg;
                } else {
                    VLOG(3) << _handler << " send success:" << pkg;
                }
            }
        }

    public:
        int exe(string h, string c) {
            init(std::move(h), std::move(c));
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

#define Handle(xxx) \
int Handle##xxx(const string &h, const string &content) { \
    Handler##xxx handler;                   \
    return handler.exe(h, content);          \
}

    static void *LocalFindClient(const string &peerAddress) {
        auto localBusiness = LocalBusiness::instance();
        LocalBusiness::CLIType clientType;
        auto client = localBusiness->FindClient(peerAddress, clientType);

        switch (clientType) {
            case LocalBusiness::CT_LOCALTCP:
            case LocalBusiness::CT_REMOTETCP: {
                if (client != nullptr) {
                    static_cast<MyTcpHandler *>(client)->timeRecv = getTimestampMs();
                }
            }
            break;
            case LocalBusiness::CT_LOCALWS: {
                if (client != nullptr) {
                    static_cast<MyWebsocketClient *>(client)->timeRecv = getTimestampMs();
                }
            }
            break;
            case LocalBusiness::CT_REMOTEWS: {
                if (client != nullptr) {
                    static_cast<MyWebSocketRequestHandler *>(client)->timeRecv = getTimestampMs();
                }
            }
            break;
        }
        return client;
    }

    class HandlerHeartbeat : public Handler<Com, Com> {
    public:
        void proc() final {
            auto client = LocalFindClient(_handler);
            rsp->guid = req->guid;
            rsp->param = "rsp";
        }
    };

    Handle(Heartbeat)


    map<vector<string>, Handle> HandleRouter = {
        {{"Heartbeat"}, HandleHeartbeat},

    };
}
