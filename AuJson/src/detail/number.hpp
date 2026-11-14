#ifndef AUJSON_DETAIL_NUMBER_HPP
#define AUJSON_DETAIL_NUMBER_HPP

// 内部头（不对外承诺稳定）：十进制 ↔ double 快速路径。
//   - fastDouble：≤15 位有效数字且 |指数| ≤ 22 时与 strtod 位精确一致；其余退回 strtod。
//   - pow10：10^k（k ≤ 22，double 内精确）。
// 见 docs/AuJson-设计文档.md §5 / §8。

#include <cstddef>
#include <cstdint>

namespace aujson {
namespace detail {

inline double pow10(int k) {
    static const double POW10[] = {
        1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
        1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
        1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
    };
    return POW10[k];
}

// s 指向数字 token（含符号 / 小数点 / 指数，语法已由扫描器校验）。
// 成功时 out 为位精确结果；false 表示超出快速路径（调用者退回 strtod）。
inline bool fastDouble(const char* s, size_t len, double& out) {
    size_t i = 0;
    bool neg = false;
    if (i < len && (s[i] == '-' || s[i] == '+')) { neg = (s[i] == '-'); ++i; }

    while (i < len && s[i] == '0') ++i;  // 前导零不计有效位

    uint64_t mant = 0;
    int digits = 0;      // 有效数字个数（≤15）
    int pointPos = 0;    // 小数点相对有效数字起点的位置
    bool dot = false;
    bool anyDigit = false;
    for (; i < len; ++i) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            anyDigit = true;
            if (digits >= 15) return false;          // >15 位有效数字 → strtod
            mant = mant * 10 + static_cast<uint64_t>(c - '0');
            ++digits;
            if (!dot) ++pointPos;
        } else if (c == '.' && !dot) {
            dot = true;
        } else {
            break;
        }
    }
    if (!anyDigit) return false;

    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        bool eneg = false;
        if (i < len && (s[i] == '-' || s[i] == '+')) { eneg = (s[i] == '-'); ++i; }
        int eVal = 0;
        int eDigits = 0;
        for (; i < len; ++i) {
            if (s[i] < '0' || s[i] > '9') return false;
            if (++eDigits <= 6) eVal = eVal * 10 + (s[i] - '0');
            else return false;                       // 指数过大 → strtod
        }
        if (eDigits == 0) return false;
        pointPos += eneg ? -eVal : eVal;
    }
    if (i != len) return false;

    int exp10 = pointPos - digits;
    if (exp10 < -22 || exp10 > 22) return false;

    double v = exp10 >= 0
        ? static_cast<double>(mant) * pow10(exp10)
        : static_cast<double>(mant) / pow10(-exp10);
    out = neg ? -v : v;
    return true;
}

} // namespace detail
} // namespace aujson

#endif // AUJSON_DETAIL_NUMBER_HPP
