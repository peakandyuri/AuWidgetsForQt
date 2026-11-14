// AuJson 基准：同机对比 QJsonDocument（解析 / 序列化 / 遍历 / 建 DOM）。
// 用法：aujson_benchmark [runs]
// 输出 Markdown 表；取多次运行最小值（最稳定）。
// 说明：内存目标由后续专门工具验证（Qt 内部走 malloc，全局 new 计数无法公平覆盖）。

#include "aujson.hpp"
#include "aujson_qt.hpp"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

// ---------------------------------------------------------------- 负载构造

static std::string makeConfig() {
    aujson::Value o = aujson::Value::makeObject();
    o["window"]["title"] = "AuWidgets";
    o["window"]["size"][0] = 1280;
    o["window"]["size"][1] = 720;
    o["window"]["fullscreen"] = false;
    o["theme"]["name"] = "dark";
    o["theme"]["accent"] = "#4A90D9";
    o["theme"]["fonts"][0] = "Microsoft YaHei";
    o["theme"]["fonts"][1] = "Consolas";
    o["recentFiles"].push_back("C:/docs/a.pdf");
    o["recentFiles"].push_back("D:/docs/b.pdf");
    o["recentFiles"].push_back("E:/docs/中文.pdf");
    o["flags"]["aaa"] = true;
    o["flags"]["bbb"] = 42;
    o["flags"]["ccc"] = 3.14159;
    return aujson::stringify(o);
}

static std::string makeList(int n) {
    aujson::Value arr = aujson::Value::makeArray();
    for (int i = 0; i < n; ++i) {
        aujson::Value o = aujson::Value::makeObject();
        o["id"] = i;
        o["name"] = "item" + std::to_string(i);
        o["price"] = i * 1.5;
        o["active"] = (i % 2) == 0;
        o["tags"].push_back("a");       // 注意：push_back 返回新元素引用，链式会嵌套
        o["tags"].push_back("b");
        arr.push_back(std::move(o));
    }
    return aujson::stringify(arr);
}

static std::string makeNumbers(int n) {
    aujson::Value arr = aujson::Value::makeArray();
    for (int i = 0; i < n; ++i) arr.push_back(i * 0.25);
    return aujson::stringify(arr);
}

// ---------------------------------------------------------------- 计时

static double benchMs(int runs, const std::function<void()>& f) {
    QElapsedTimer t;
    double best = 1e300;
    for (int i = 0; i < runs; ++i) {
        t.restart();
        f();
        double ms = static_cast<double>(t.nsecsElapsed()) / 1e6;
        if (ms < best) best = ms;
    }
    return best;
}

// ---------------------------------------------------------------- 对照操作

static void qWalk(const QJsonValue& v, size_t& sum) {
    if (v.isArray()) {
        for (const QJsonValue& e : v.toArray()) qWalk(e, sum);
    } else if (v.isObject()) {
        const QJsonObject obj = v.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) qWalk(it.value(), sum);
    } else if (v.isDouble()) {
        sum += static_cast<size_t>(v.toDouble());
    } else if (v.isString()) {
        sum += static_cast<size_t>(v.toString().size());
    }
}

static void aWalk(const aujson::Value& v, size_t& sum) {
    for (auto item : v) {
        if (item.key == nullptr) aWalk(*item.value, sum);
        else { sum += item.key->size(); aWalk(*item.value, sum); }
    }
    if (v.isNumber()) sum += static_cast<size_t>(v.asDouble());
    else if (v.isString()) sum += v.asString().size();
}

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::atoi(argv[1]) : 30;

    const std::string config = makeConfig();
    const std::string list = makeList(5000);
    const std::string numbers = makeNumbers(100000);

    std::printf("| 负载 | 操作 | QJsonDocument (μs) | AuJson (μs) | 倍率 |\n");
    std::printf("| --- | --- | --- | --- | --- |\n");

    volatile size_t guard = 0;

    auto report = [&](const char* payload, const char* op, double qMs, double aMs) {
        std::printf("| %s | %s | %.1f | %.1f | %.2fx |\n",
                    payload, op, qMs * 1000.0, aMs * 1000.0, qMs / aMs);
    };

    const char* names[] = {"config ~1KB", "list 5k 对象", "numbers 10w"};
    const std::string payloads[] = {config, list, numbers};

    for (int i = 0; i < 3; ++i) {
        const std::string& text = payloads[i];
        const char* name = names[i];
        const QByteArray qba(text.data(), static_cast<int>(text.size()));

        // 解析（DOM 构建）
        double qMs = benchMs(runs, [&] {
            QJsonDocument d = QJsonDocument::fromJson(qba);
            guard += d.isNull() ? 0u : 1u;
        });
        double aMs = benchMs(runs, [&] {
            aujson::Value v;
            aujson::parse(text.data(), text.size(), v);
            guard += v.isNull() ? 0u : 1u;
        });
        report(name, "解析", qMs, aMs);

        // 预建 DOM：后续操作只测操作本身
        const QJsonDocument qd = QJsonDocument::fromJson(qba);
        aujson::Value av;
        aujson::parse(text.data(), text.size(), av);

        // 序列化（紧凑）
        qMs = benchMs(runs, [&] {
            QByteArray out = qd.toJson(QJsonDocument::Compact);
            guard += static_cast<size_t>(out.size());
        });
        aMs = benchMs(runs, [&] {
            std::string out = aujson::stringify(av);
            guard += out.size();
        });
        report(name, "序列化（紧凑）", qMs, aMs);

        // 序列化（pretty）
        qMs = benchMs(runs, [&] {
            QByteArray out = qd.toJson(QJsonDocument::Indented);
            guard += static_cast<size_t>(out.size());
        });
        aMs = benchMs(runs, [&] {
            aujson::WriteOptions wo; wo.pretty = true;
            std::string out = aujson::stringify(av, wo);
            guard += out.size();
        });
        report(name, "序列化（pretty）", qMs, aMs);

        // 遍历求和
        qMs = benchMs(runs, [&] {
            size_t s = 0;
            qWalk(qd.isArray() ? QJsonValue(qd.array()) : QJsonValue(qd.object()), s);
            guard += s;
        });
        aMs = benchMs(runs, [&] {
            size_t s = 0;
            aWalk(av, s);
            guard += s;
        });
        report(name, "遍历", qMs, aMs);
    }

    // DOM 构建：从零构造 10w 元素
    {
        const int n = 100000;
        double qMs = benchMs(runs, [&] {
            QJsonArray arr;
            for (int i = 0; i < n; ++i) {
                QJsonObject o;
                o["id"] = i;
                o["name"] = "item";
                arr.append(o);
            }
            guard += static_cast<size_t>(arr.size());
        });
        double aMs = benchMs(runs, [&] {
            aujson::Value arr = aujson::Value::makeArray();
            for (int i = 0; i < n; ++i) {
                aujson::Value o = aujson::Value::makeObject();
                o["id"] = i;
                o["name"] = "item";
                arr.push_back(std::move(o));
            }
            guard += arr.size();
        });
        report("build 10w", "建 DOM", qMs, aMs);
    }

    std::printf("sizeof(aujson::Value) = %zu 字节\n", sizeof(aujson::Value));
    return 0;
}
