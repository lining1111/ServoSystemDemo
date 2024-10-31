#include <iostream>
#include <csignal>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <Poco/Path.h>
#include "glogHelper/GlogHelper.h"
#include "utils/utils.h"
#include "version.h"
#include "common/config.h"
#include "localBusiness/localBusiness.h"


bool isExit = false;

void handleSignal(int sig) {
    if (sig == SIGPIPE) {
        cout << "sig pipe" << endl;
    } else {
        cout << "exit" << endl;
        isExit = true;
    }
}


int signalIgnPipe() {
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = handleSignal;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGTERM, &act, nullptr);
    sigaction(SIGINT, &act, nullptr);
    sigaction(SIGSTOP, &act, nullptr);
    if (sigaction(SIGPIPE, &act, nullptr) < 0) {
        printf("call sigaction fail, %s\n", strerror(errno));
        return errno;
    }

    return 0;
}

DEFINE_int32(port, 10001, "本地服务端端口号，默认10001");
DEFINE_int32(keep, 5, "日志清理周期 单位day，默认5");
DEFINE_bool(isSendSTDOUT, false, "输出到控制台，默认false");
DEFINE_string(logDir, "log", "日志的输出目录,默认log");
DEFINE_string(configFile, "./Config.ini", "配置文件路径，默认./Config.ini");

int main(int argc, char **argv) {
    gflags::SetVersionString(VERSION_BUILD_TIME);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    std::string proFul = std::string(argv[0]);
    std::string pro = getFileName(proFul);
    //日志系统类
    GlogHelper glogHelper(pro, FLAGS_keep, FLAGS_logDir, FLAGS_isSendSTDOUT);

    LOG(WARNING) << "程序工作目录:" << Poco::Path::current() << ",版本号:" << VERSION_BUILD_TIME;
    LOG(WARNING) << "获取程序参数:" << FLAGS_configFile;
    if (localConfig.getDeviceConfigFromINI(FLAGS_configFile) != 0) {
        LOG(ERROR) << "获取配置文件失败";
        return -1;
    }

    device.Init();

    signalIgnPipe();
    LOG(WARNING) << "开启本地tcp通信";
    auto localBusiness = LocalBusiness::instance();
    localBusiness->AddServer("server1", FLAGS_port);
    localBusiness->Run();

    while (true) {
        if (isExit) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
        //获取现在的毫秒
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        if (localConfig._config.isCheckClient) {
            //轮询接入的客户端接收时间，如果超过10秒没有新信息，则踢掉
            localBusiness->kickoff(30 * 1000, now_ms);
        }
        //往fpm写入一些数据
//        uint8_t mesg[12] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 0x03, 0x00, 0x00, 0x0A};
//        ((FPM*) device.g_fpm)->TriggerAction((char *)mesg, 12);
        device.Keep();

    }

    //释放
    localBusiness->Stop();
    localBusiness->stopAllConns();
    delete localBusiness;

    device.Exit();

    return 0;
}
