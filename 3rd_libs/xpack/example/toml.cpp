/*
* xpack toml 示例：与 example/yaml.cpp 对应
* 演示用 xpack::toml 从 .toml 文件 / 字符串解析到结构体，以及结构体转TOML（encode）
*/

#include <iostream>
#include "xpack/toml.h"
#include "xpack/json.h"

using namespace std;

struct Server {
    string host;
    int    port;
    bool   debug;
    XPACK(O(host, port, debug));
};

struct Product {
    string  name;
    int64_t sku;
    string  color;
    XPACK(O(name, sku, color));
};

struct Config {
    string               title;
    vector<string>       tags;
    map<string, string>  flags;
    Server               server;
    vector<Product>      products;
    XPACK(O(title, tags, flags, server, products));
};

int main() {
    // 从文件解析
    Config cfg;
    xpack::toml::decode_file("./test.toml", cfg);
    cout<<"decode_file => "<<xpack::json::encode(cfg)<<endl;

    // 从字符串解析
    string str = "title = \"inline\"\ntags = [\"a\", \"b\"]\n[server]\nhost = \"localhost\"\nport = 1234\ndebug = false\n";
    Config cfg2;
    xpack::toml::decode(str, cfg2);
    cout<<"decode string => "<<xpack::json::encode(cfg2)<<endl;

    // 结构体转 TOML
    string toml_text = xpack::toml::encode(cfg);
    cout<<"========== encode =========="<<endl<<toml_text<<"==============================="<<endl;

    // 往返：encode 的结果再 decode，应与原结构体一致
    Config cfg3;
    xpack::toml::decode(toml_text, cfg3);
    cout<<"round-trip equal => "<<(xpack::json::encode(cfg3)==xpack::json::encode(cfg) ? "true" : "false")<<endl;

    return 0;
}
