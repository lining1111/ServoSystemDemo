//
// Created by lining on 11/27/24.
//

#include "common/common.h"
#include <iostream>

using namespace std;
using namespace common;

int main() {
    // string content = R"({"comVersion": "0.0.1","guid": "309173ae-48d3-11ef-98b4-611827c88d84","code": "Heartbeat","state": 0,"content": "E:\\a\\b\\c"})";
    // Com com;
    // try {
    //     json::decode(content, com);
    // } catch (std::exception &e) {
    //     cout << e.what() << endl;
    // }

    std::map<std::vector<string>, int> myMap = {
        {{"a", "b"}, 1},
        {{"b", "a"}, 2},
        {{"b", "c"}, 3}
    };

    std::vector<std::string> key = {"a", "b"};
    auto iter = myMap.find(key);
    if (iter != myMap.end()) {
        std::cout << "Found key: " << key[0] << ", " << key[1] << ", val:" << iter->second << std::endl;
    } else {
        std::cout << "Key not found" << std::endl;
    }

    return 0;
}
