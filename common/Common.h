#ifndef COMMON_H
#define COMMON_H

#include <QHostAddress>
#include <QDateTime>

namespace Config {
    inline const QString dbType = "QPSQL";
    inline const QString ip = "127.0.0.1";
    inline constexpr quint16 port = 5432;
    inline const QString dbName = "postgres";
    inline const QString userName = "postgres";
    inline const QString password = "mysecretpassword";

    inline constexpr int threadsCount = 8;
    inline constexpr int delay = 1000;
    inline constexpr int cleanupTime = 3000;
    inline constexpr int reconnectTime = 5000;
}

struct BaseInfo {
    BaseInfo() = default;

    BaseInfo(const int& id, const QString& info, const QHostAddress& ip, const quint16& port):
        id(id), info(info), ip(ip), port(port) {}

    int id;
    QString info;
    QHostAddress ip;
    quint16 port;
};

struct FullInfo: public BaseInfo {
    FullInfo() = default;

    FullInfo(const BaseInfo& baseInfo, const bool& status, const quint64& latency, const QDateTime& last_checked):
        BaseInfo(baseInfo), status(status), latency(latency), last_checked(last_checked) {}

    FullInfo(const int& id, const QString& info, const QHostAddress& ip, const quint16& port,
                const bool& status, const quint64& latency, const QDateTime& last_checked):
        BaseInfo(id, info, ip, port), status(status), latency(latency), last_checked(last_checked) {}

    /*
     * 0 - info
     * 1 - QString(ip + port)
     * 2 - QString(status)
     * 3 - latency
     * 4 - last_checked
     */
    QString operator[](int index) const {
        switch(index) {
            case 0:
                return info;
            case 1:
                return ip.toString() + ":" + QString::number(port);
            case 2:
                return status ? "True" : "False";
            case 3:
                return QString::number(latency) + " ms";
            case 4:
                return last_checked.toString();
            default:
                return "-";
        }
    }

    bool status;
    quint64 latency;
    QDateTime last_checked;
};

Q_DECLARE_METATYPE(FullInfo)

#endif // COMMON_H
