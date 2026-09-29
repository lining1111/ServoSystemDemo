/*
 * xpack toml 解码器（新增格式，与 json/xml/yaml 并列）
 *
 * 基于内置 toml++（tomlplusplus，见 toml++/ 目录，MIT）解析 TOML，
 * 把解析出的 toml::node 树包装成 xpack::XDecoder 要求的"节点"接口
 * （Find/Size/At/Next/Get），从而让 XPACK 宏标记的结构体字段自动从
 * TOML 文件/字符串中取值。实现方式模仿自带的 yaml_decoder.h。
 *
 * 要求 C++17（toml++ 的最低要求）。
 *
 * License: Apache-2.0
 */
#ifndef __X_PACK_TOML_DECODER_H
#define __X_PACK_TOML_DECODER_H

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

#include "toml++/toml.hpp"
#include "xdecoder.h"

namespace xpack {

/*
 * TomlNode 包装一个 toml::node：
 *   - 生命周期：节点树归 parse_result 所有，这里用 shared_ptr<void> 保活句柄，
 *     所有子节点都携带同一份句柄，保证 XDecoder 解码期间树不会被释放。
 *   - Find / Size / At / Next 按节点实际类型（table / array）访问，
 *     Get 按 node_type 分派到整数/浮点/布尔/字符串/日期时间。
 */
class TomlNode {
public:
    typedef XDecoder<TomlNode> decoder;
    typedef toml::table::const_iterator Iterator;

    TomlNode() : n_(NULL) {}
    TomlNode(const toml::node *n, const std::shared_ptr<void> &keep)
        : n_(n), keep_(keep) {}

    static const char *Name() {
        return "toml";
    }
    operator bool() const {
        return n_ != NULL;
    }
    bool IsNull() const {
        return false;
    }

    // 按 key 找子节点；key 不存在返回无效节点（XDecoder 会跳过或按 Mandatory 报错）
    TomlNode Find(decoder &de, const char *key, const Extend *ext) const {
        (void)ext;
        if (!*this) {
            return TomlNode();
        }
        const toml::table *t = n_->as_table();
        if (t == NULL) {
            de.decode_exception("not map", key);
            return TomlNode();
        }
        if (t->contains(key)) {
            return TomlNode(&t->at(key), keep_);
        }
        return TomlNode();
    }

    size_t Size(decoder &de) const {
        const toml::array *a = n_->as_array();
        if (a == NULL) {
            de.decode_exception("not array", NULL);
            return 0;
        }
        return a->size();
    }

    TomlNode At(size_t index) const {
        const toml::array *a = n_->as_array();
        if (a == NULL) {
            return TomlNode();
        }
        return TomlNode(&(*a)[index], keep_);
    }

    // 遍历 map：首次调用（this == parent）初始化迭代器，之后每次递增。
    // 逻辑与 yaml_decoder 的 YamlNode::Next 一致。
    TomlNode Next(decoder &de, const TomlNode &parent, Iterator &iter, std::string &key) const {
        const toml::table *t = parent.n_->as_table();
        if (t == NULL) {
            de.decode_exception("not map", NULL);
            return TomlNode();
        }
        if ((void *)this != (void *)(&parent)) {
            ++iter;
        } else {
            iter = t->begin();
        }
        if (iter != t->end()) {
            key.assign(iter->first.str());
            return TomlNode(&iter->second, keep_);
        }
        return TomlNode();
    }

    // ---- Get：把 TOML 标量取到 C++ 成员里 ----
    template <class T>
    typename x_enable_if<numeric<T>::is_integer, bool>::type
    Get(decoder &de, T &val, const Extend *ext) {
        (void)ext;
        if (n_ != NULL && n_->type() == toml::node_type::integer) {
            val = (T)n_->as_integer()->get();
            return true;
        }
        de.decode_exception("type unmatch, expect integer", NULL);
        return false;
    }

    template <class T>
    typename x_enable_if<numeric<T>::is_float, bool>::type
    Get(decoder &de, T &val, const Extend *ext) {
        (void)ext;
        if (n_ != NULL && n_->type() == toml::node_type::floating_point) {
            val = (T)n_->as_floating_point()->get();
            return true;
        }
        de.decode_exception("type unmatch, expect float", NULL);
        return false;
    }

    bool Get(decoder &de, bool &val, const Extend *ext) {
        (void)ext;
        if (n_ != NULL && n_->type() == toml::node_type::boolean) {
            val = n_->as_boolean()->get();
            return true;
        }
        de.decode_exception("type unmatch, expect bool", NULL);
        return false;
    }

    bool Get(decoder &de, std::string &val, const Extend *ext) {
        (void)ext;
        if (n_ != NULL) {
            switch (n_->type()) {
                case toml::node_type::string:
                    val = n_->as_string()->get();
                    return true;
                // TOML 的 date/time/date_time 没有 C++ 原生类型，转成字符串
                case toml::node_type::date:
                    val = formatDate(n_->as_date()->get());
                    return true;
                case toml::node_type::time:
                    val = formatTime(n_->as_time()->get());
                    return true;
                case toml::node_type::date_time:
                    val = formatDateTime(n_->as_date_time()->get());
                    return true;
                // 宽松处理：数字转字符串
                case toml::node_type::integer:
                    val = std::to_string(n_->as_integer()->get());
                    return true;
                case toml::node_type::floating_point:
                    val = doubleToString(n_->as_floating_point()->get());
                    return true;
                default:
                    break;
            }
        }
        de.decode_exception("type unmatch, expect string", NULL);
        return false;
    }

private:
    static std::string formatDate(const toml::date &d) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u",
                 (unsigned)d.year, (unsigned)d.month, (unsigned)d.day);
        return std::string(buf);
    }
    static std::string formatTime(const toml::time &t) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                 (unsigned)t.hour, (unsigned)t.minute, (unsigned)t.second);
        std::string r(buf);
        if (t.nanosecond != 0) {
            snprintf(buf, sizeof(buf), ".%09u", (unsigned)t.nanosecond);
            r += buf;
        }
        return r;
    }
    static std::string formatOffset(const toml::time_offset &off) {
        if (off.minutes == 0) {
            return "Z";
        }
        char buf[16];
        int m = off.minutes;
        if (m < 0) {
            m = -m;
            snprintf(buf, sizeof(buf), "-%02d:%02d", m / 60, m % 60);
        } else {
            snprintf(buf, sizeof(buf), "+%02d:%02d", m / 60, m % 60);
        }
        return std::string(buf);
    }
    static std::string formatDateTime(const toml::date_time &dt) {
        std::string r = formatDate(dt.date) + "T" + formatTime(dt.time);
        if (dt.offset) {
            r += formatOffset(*dt.offset);
        }
        return r;
    }
    // 保证 double -> 文本 -> double 可往返
    static std::string doubleToString(double d) {
        char buf[64];
        for (int prec = 15; prec <= 17; ++prec) {
            snprintf(buf, sizeof(buf), "%.*g", prec, d);
            char *end = NULL;
            double back = strtod(buf, &end);
            if (back == d) break;
        }
        return std::string(buf);
    }

private:
    const toml::node *n_;
    std::shared_ptr<void> keep_;
};

/*
 * TOML 解码入口。与 YamlDecoder 对齐：
 *  parse 失败抛出 runtime_error（携带 toml++ 的报错信息），
 *  字段/类型不匹配通过 XDecoder::decode_exception 抛出。
 *
 * 注意：默认 TOML_EXCEPTIONS=1，toml::parse 返回 toml::table 并在失败时抛
 * toml::parse_error；本实现基于该默认配置（不要定义 TOML_EXCEPTIONS=0）。
 */
class TomlDecoder {
public:
    template <class T>
    bool decode(const std::string &str, T &val) {
        std::shared_ptr<toml::table> tbl;
        try {
            tbl = std::make_shared<toml::table>(toml::parse(str));
        } catch (const toml::parse_error &e) {
            throw std::runtime_error(std::string("toml parse error: ") + std::string(e.description()));
        }
        TomlNode node(tbl.get(), tbl);
        return XDecoder<TomlNode>(NULL, (const char *)NULL, node).decode(val, NULL);
    }

    template <class T>
    bool decode_file(const std::string &fname, T &val) {
        std::shared_ptr<toml::table> tbl;
        try {
            tbl = std::make_shared<toml::table>(toml::parse_file(fname));
        } catch (const toml::parse_error &e) {
            throw std::runtime_error(std::string("toml parse error(file=") + fname + "): " + std::string(e.description()));
        }
        TomlNode node(tbl.get(), tbl);
        return XDecoder<TomlNode>(NULL, (const char *)NULL, node).decode(val, NULL);
    }
};

} // namespace xpack

#endif /* __X_PACK_TOML_DECODER_H */
