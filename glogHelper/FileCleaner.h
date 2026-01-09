//
// Created by lining on 2026/1/9.
//

#ifndef FILECLEANER_H
#define FILECLEANER_H

#include <string>
#include <vector>

using namespace std;

class FileCleaner {
public:
    typedef struct Config {
        string path;
        vector<string> suffix;
        uint64_t keepSeconds;
    } Config;

    explicit
    FileCleaner(Config config);

    ~FileCleaner();

    void run();

private:
    Config localConfig;
};


#endif //FILECLEANER_H
