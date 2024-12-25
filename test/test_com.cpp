//
// Created by lining on 11/27/24.
//

#include "common/common.h"
#include <iostream>

using namespace std;
using namespace common;

int main() {
    string content = R"({"comVersion": "0.0.1","guid": "309173ae-48d3-11ef-98b4-611827c88d84","code": "Heartbeat","state": 0,"content": "E:\\a\\b\\c"})";
    Com com;
    try {
        json::decode(content, com);
    } catch (std::exception &e) {
        cout << e.what() << endl;
    }

    return 0;
}