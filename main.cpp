#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

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
    cout << "exit" << endl;
    isExit = true;
}

//获取接入客户端列表
void handleSignalUSR1(int sig) {
    LOG(WARNING)<<"USR1:show local info";
    auto localBusiness = LocalBusiness::instance();
    if (!localBusiness) {
        LOG(WARNING) << "localBusiness is null";
    } else {
        localBusiness->ShowInfo();
    }

}


DEFINE_int32(port, 10001, "本地服务端端口号，默认10001");
DEFINE_int32(keep, 5, "日志清理周期 单位day，默认5");
DEFINE_bool(isSendSTDOUT, false, "输出到控制台，默认false");
DEFINE_string(logDir, "log", "日志的输出目录,默认log");
DEFINE_string(configFile, "./config.yaml", "配置文件路径，默认./config.yaml");

int main(int argc, char **argv) {
    gflags::SetVersionString(VERSION_BUILD_TIME);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    std::string proFul = std::string(argv[0]);
    std::string pro = getFileName(proFul);

    GlogHelper glogHelper(pro, FLAGS_keep, FLAGS_logDir, FLAGS_isSendSTDOUT);

    LOG(WARNING) << "程序工作目录:" << Poco::Path::current() << ",版本号:" << VERSION_BUILD_TIME;
    LOG(WARNING) << "获取程序参数:" << FLAGS_configFile;
    if (localConfig.getDeviceConfigFromYAML(FLAGS_configFile) != 0) {
        LOG(ERROR) << "获取配置文件失败";
        return -1;
    }

    device.Init();
#if defined(__linux__)
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1,handleSignalUSR1);//USR1 show local info
#endif
    signal(SIGINT, handleSignal);//Ctrl+C
    signal(SIGTERM, handleSignal);//kill

    LOG(WARNING) << "开启本地tcp通信";
    auto localBusiness = LocalBusiness::instance();
    localBusiness->AddServer("server1", FLAGS_port);
    localBusiness->Run();

    while (true) {
        if (isExit) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        if (localConfig._config.isCheckClient) {
            localBusiness->kickoff(30 * 1000, now_ms);
        }

//        uint8_t mesg[12] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 0x03, 0x00, 0x00, 0x0A};
//        ((FPM*) device.g_fpm)->TriggerAction((char *)mesg, 12);
        device.Keep();

    }

    localBusiness->Stop();
    localBusiness->stopAllConns();
    delete localBusiness;

    device.Exit();

    return 0;
}
