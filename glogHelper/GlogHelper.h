//
// Created by lining on 2023/2/20.
//

#ifndef GLOGHELPER_H
#define GLOGHELPER_H

#include <string>
#include <thread>

class GlogHelper {
private:
    bool isRun = false;
    std::thread tClean;

    int scanPeriod = 5;
    std::string program;
    int keepDays = 1;
    std::string logDir;
    bool isSendSTDOUT;
public:
    GlogHelper(std::string _program, int _keep, std::string _logDir, bool _isSendSTDOUT);

    ~GlogHelper();

private:
    static int cleaner(const GlogHelper *local);

};


#endif //GLOGHELPER_H
