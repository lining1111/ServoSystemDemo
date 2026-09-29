/*
 * xpack toml 对外入口（新增格式，与 json/xml/yaml 并列）
 *
 * 用法：
 *   struct Config {
 *       std::string title;
 *       int port;
 *       XPACK(O(title, port));
 *   };
 *   Config cfg;
 *   xpack::toml::decode_file("config.toml", cfg);
 *   xpack::toml::decode(toml_string, cfg);
 *   std::string toml_text = xpack::toml::encode(cfg);  // 结构体转TOML
 *
 * 依赖：内置 toml++（tomlplusplus，见 toml++/ 目录，MIT），纯头文件，
 * 无需编译额外源文件；要求 C++17。
 *
 * License: Apache-2.0
 */
#ifndef __X_PACK_TOML_H
#define __X_PACK_TOML_H

#include "toml_decoder.h"
#include "toml_encoder.h"
#include "xpack.h"

namespace xpack {

class toml {
public:
    template <class T>
    static void decode(const std::string &data, T &val) {
        TomlDecoder de;
        de.decode(data, val);
    }

    template <class T>
    static void decode_file(const std::string &file_name, T &val) {
        TomlDecoder de;
        de.decode_file(file_name, val);
    }

    template <class T>
    static std::string encode(const T &val) {
        TomlEncoder en;
        return en.encode(val);
    }
};

} // namespace xpack

#endif /* __X_PACK_TOML_H */
