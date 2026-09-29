/*
 * xpack toml 编码器（新增格式，与 json/xml/yaml 并列）
 *
 * 把 XPACK 结构体序列化为 TOML 文本：
 *   - table（结构体 / std::map）      -> [表头] 小节（含点分路径），父子顺序保证合法
 *   - 标量数组 / 内联结构体数组       -> 行内数组 [ ... ]
 *   - vector<结构体>                 -> [[表头]] 数组表
 *   - map 中的表                      -> 行内表 { ... }（出现在行内数组里时）
 *
 * TOML 与 JSON/YAML 的嵌套语法不同：它要求"标量先于子表头"、父表头先于子表头，
 * 因此这里先通过 XEncoder 回调把结构体收集成一棵中间树（TomlValue），
 * 再按 TOML 规则做两遍/三遍序列化，保证输出总是合法 TOML。
 *
 * License: Apache-2.0
 */
#ifndef __X_PACK_TOML_ENCODER_H
#define __X_PACK_TOML_ENCODER_H

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "xencoder.h"

namespace xpack {

/*
 * 中间表示：一棵 TOML 值树。
 * table（TAB）用有序 kv 保存条目（保持成员声明顺序），
 * array（ARR）用 arr 保存元素；元素为 TAB 时表示数组表。
 */
struct TomlValue {
    enum Kind { NUL, BOOL, INT, DBL, STR, ARR, TAB };
    Kind kind = NUL;
    bool b = false;
    long long i64 = 0;
    unsigned long long u64 = 0;
    bool isU = false;
    double d = 0.0;
    std::string s;
    std::vector<TomlValue> arr;
    std::vector<std::pair<std::string, TomlValue> > kv;

    bool isTable() const { return kind == TAB; }
    bool isArrayOfTables() const {
        if (kind != ARR || arr.empty()) return false;
        for (size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].isTable()) return false;
        }
        return true;
    }
};

class TomlWriter {
    friend class XEncoder<TomlWriter>;
    friend class TomlEncoder;

    const static bool support_null = false;
public:
    TomlWriter() {
        root_.kind = TomlValue::TAB;
        Ctx c = { &root_, false };
        stack_.push_back(c);
    }

private:
    static const char *Name() {
        return "toml";
    }
    inline const char *IndexKey(size_t index) {
        (void)index;
        return NULL;
    }
    std::string String() {
        std::vector<std::string> path;
        std::string out;
        serializeTable(root_, path, out);
        return out;
    }

    // ---- 收集回调（由 XEncoder 调用）----
    void ArrayBegin(const char *key, const Extend *ext) {
        (void)ext;
        TomlValue v;
        v.kind = TomlValue::ARR;
        TomlValue *node = addValue(key, std::move(v));
        Ctx c = { node, true };
        stack_.push_back(c);
    }
    void ArrayEnd(const char *key, const Extend *ext) {
        (void)key;
        (void)ext;
        stack_.pop_back();
    }
    void ObjectBegin(const char *key, const Extend *ext) {
        (void)ext;
        // 根对象：key 为空且已在根表 -> 直接进入根表，不新建条目
        if (stack_.size() == 1 && (key == NULL || key[0] == '\0')) {
            Ctx c = { &root_, false };
            stack_.push_back(c);
            return;
        }
        TomlValue v;
        v.kind = TomlValue::TAB;
        TomlValue *node = addValue(key, std::move(v));
        Ctx c = { node, false };
        stack_.push_back(c);
    }
    void ObjectEnd(const char *key, const Extend *ext) {
        (void)key;
        (void)ext;
        stack_.pop_back();
    }
    bool WriteNull(const char *key, const Extend *ext) {
        (void)key;
        (void)ext;
        return false; // TOML 没有 null，跳过该键
    }
    bool encode_bool(const char *key, const bool &val, const Extend *ext) {
        (void)ext;
        TomlValue v;
        v.kind = TomlValue::BOOL;
        v.b = val;
        addValue(key, std::move(v));
        return true;
    }
    bool encode_string(const char *key, const std::string &val, const Extend *ext) {
        (void)ext;
        TomlValue v;
        v.kind = TomlValue::STR;
        v.s = val;
        addValue(key, std::move(v));
        return true;
    }
    template <typename T>
    typename x_enable_if<numeric<T>::is_integer, bool>::type
    encode_number(const char *key, const T &val, const Extend *ext) {
        (void)ext;
        TomlValue v;
        v.kind = TomlValue::INT;
        if (numeric<T>::is_signed) {
            v.i64 = (long long)val;
            v.isU = false;
        } else {
            v.u64 = (unsigned long long)val;
            v.isU = true;
        }
        addValue(key, std::move(v));
        return true;
    }
    template <typename T>
    typename x_enable_if<numeric<T>::is_float, bool>::type
    encode_number(const char *key, const T &val, const Extend *ext) {
        (void)ext;
        TomlValue v;
        v.kind = TomlValue::DBL;
        v.d = (double)val;
        addValue(key, std::move(v));
        return true;
    }

    // ---- 中间树构建 ----
    TomlValue *addValue(const char *key, TomlValue &&v) {
        Ctx &c = stack_.back();
        if (c.isArray) {
            c.node->arr.push_back(std::move(v));
            return &c.node->arr.back();
        }
        if (key == NULL || key[0] == '\0') {
            if (stack_.size() == 1) {
                throw std::runtime_error("toml encode: top level must be a table(struct/map)");
            }
            c.node->kv.push_back(std::make_pair(std::string(), std::move(v)));
            return &c.node->kv.back().second;
        }
        c.node->kv.push_back(std::make_pair(std::string(key), std::move(v)));
        return &c.node->kv.back().second;
    }

    // ---- TOML 序列化 ----
    static bool isBareKey(const std::string &k) {
        if (k.empty()) return false;
        for (size_t i = 0; i < k.size(); ++i) {
            unsigned char c = (unsigned char)k[i];
            if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
        }
        return true;
    }
    static std::string quoteString(const std::string &s) {
        std::string out;
        out.reserve(s.size() + 8);
        out.push_back('"');
        for (size_t i = 0; i < s.size(); ++i) {
            unsigned char c = (unsigned char)s[i];
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\t': out += "\\t";  break;
                case '\r': out += "\\r";  break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                default:
                    if (c < 0x20 || c == 0x7f) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04X", c);
                        out += buf;
                    } else {
                        out += (char)c;
                    }
            }
        }
        out.push_back('"');
        return out;
    }
    static std::string quoteKey(const std::string &k) {
        return isBareKey(k) ? k : quoteString(k);
    }
    static std::string joinPath(const std::vector<std::string> &path) {
        std::string r;
        for (size_t i = 0; i < path.size(); ++i) {
            if (i > 0) r += ".";
            r += quoteKey(path[i]);
        }
        return r;
    }
    static std::string intToString(const TomlValue &v) {
        return v.isU ? std::to_string(v.u64) : std::to_string(v.i64);
    }
    // 保证 double -> 文本 -> double 可往返，且尽量短
    static std::string doubleToString(double d) {
        if (std::isnan(d)) return "nan";
        if (std::isinf(d)) return d > 0 ? "inf" : "-inf";
        char buf[64];
        for (int prec = 15; prec <= 17; ++prec) {
            snprintf(buf, sizeof(buf), "%.*g", prec, d);
            char *end = NULL;
            double back = strtod(buf, &end);
            if (back == d) break;
        }
        std::string r(buf);
        // 整数样子的浮点必须带小数点/指数，否则会被 TOML 当作整数
        bool isIntLike = true;
        for (size_t i = 0; i < r.size(); ++i) {
            char c = r[i];
            if (c != '-' && c != '+' && !(c >= '0' && c <= '9')) { isIntLike = false; break; }
        }
        if (isIntLike) r += ".0";
        return r;
    }
    static std::string inlineTable(const TomlValue &t) {
        std::string out = "{";
        for (size_t i = 0; i < t.kv.size(); ++i) {
            if (i > 0) out += ", ";
            out += quoteKey(t.kv[i].first) + " = " + serializeValue(t.kv[i].second);
        }
        out += "}";
        return out;
    }
    static std::string serializeValue(const TomlValue &v) {
        switch (v.kind) {
            case TomlValue::BOOL: return v.b ? "true" : "false";
            case TomlValue::INT:  return intToString(v);
            case TomlValue::DBL:  return doubleToString(v.d);
            case TomlValue::STR:  return quoteString(v.s);
            case TomlValue::ARR: {
                std::string out = "[";
                for (size_t i = 0; i < v.arr.size(); ++i) {
                    if (i > 0) out += ", ";
                    out += serializeValue(v.arr[i]);
                }
                out += "]";
                return out;
            }
            case TomlValue::TAB: return inlineTable(v);
            default: return "";
        }
    }
    void serializeTable(const TomlValue &t, const std::vector<std::string> &path, std::string &out) {
        // 第一遍：标量 / 行内数组 / 行内表（所有非表条目必须先于子表头）
        for (size_t i = 0; i < t.kv.size(); ++i) {
            const TomlValue &v = t.kv[i].second;
            if (v.isTable() || v.isArrayOfTables()) continue;
            out += quoteKey(t.kv[i].first) + " = " + serializeValue(v) + "\n";
        }
        // 第二遍：表条目 -> [path.key]
        for (size_t i = 0; i < t.kv.size(); ++i) {
            const TomlValue &v = t.kv[i].second;
            if (!v.isTable()) continue;
            if (!out.empty()) out += "\n";
            std::vector<std::string> sub = path;
            sub.push_back(t.kv[i].first);
            out += "[" + joinPath(sub) + "]\n";
            serializeTable(v, sub, out);
        }
        // 第三遍：数组表条目 -> [[path.key]]
        for (size_t i = 0; i < t.kv.size(); ++i) {
            const TomlValue &v = t.kv[i].second;
            if (!v.isArrayOfTables()) continue;
            std::vector<std::string> sub = path;
            sub.push_back(t.kv[i].first);
            for (size_t j = 0; j < v.arr.size(); ++j) {
                if (!out.empty()) out += "\n";
                out += "[[" + joinPath(sub) + "]]\n";
                serializeTable(v.arr[j], sub, out);
            }
        }
    }

private:
    struct Ctx {
        TomlValue *node;
        bool isArray;
    };
    TomlValue root_;
    std::vector<Ctx> stack_;
};

class TomlEncoder {
public:
    template <class T>
    std::string encode(const T &val) {
        TomlWriter wr;
        XEncoder<TomlWriter> en(wr);
        en.encode(NULL, val, NULL);
        return wr.String();
    }
};

} // namespace xpack

#endif /* __X_PACK_TOML_ENCODER_H */
