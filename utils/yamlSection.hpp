//
// yaml 分段解码工具：param.yaml 是多段共享配置（cameraList / grpcServerList / db），
// xpack::yaml::decode_file 只能把整个根节点解到一个类型上，按 key 取某一段要包一层
// 临时结构体。这里用 XDecoder 直接按 key 解码，省掉这些包装结构体。
//

#ifndef UTILS_YAML_SECTION_HPP
#define UTILS_YAML_SECTION_HPP

#include <string>

#include "xpack/yaml.h"

/**
 * @brief 从 yaml 文件中按 key 解码某一段配置（其余 key 全部忽略）
 * @tparam T 目标类型，需要带 XPACK 宏（如 CameraList::Item、Db::Config 等）
 * @param file yaml 文件路径
 * @param key  段名，如 "cameraList" / "grpcServerList" / "db"
 * @param val  输出：解码结果，缺字段时保留 T 的默认值
 * @return true 成功；false 该 key 不存在
 * @throws YAML::BadFile 文件不存在或无法打开
 * @throws YAML::ParserException yaml 格式错误
 * @throws std::runtime_error 字段类型不匹配（xpack 抛出的 decode 异常）
 */
template<class T>
bool decodeYamlSection(const std::string &file, const std::string &key, T &val) {
    YAML::Node root = YAML::LoadFile(file);
    if (key.empty() || !root[key]) {
        return false;
    }
    xpack::YamlNode node(root);
    xpack::XDecoder<xpack::YamlNode> dec(nullptr, nullptr, node);
    // XDecoder::decode 的 key 形参是 const char*，std::string 需要显式 c_str()
    return dec.decode(key.c_str(), val, nullptr);
}

#endif //UTILS_YAML_SECTION_HPP
