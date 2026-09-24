#include "HostChecker.h"

#include <QElapsedTimer>

HostChecker::HostChecker(const BaseInfo& baseInfo):
    baseInfo(baseInfo) {}


void HostChecker::run() {
    QTcpSocket socket;

    bool status = false;
    QDateTime last_checked = QDateTime::currentDateTime();

    QElapsedTimer timer;
    timer.start();

    socket.connectToHost(baseInfo.ip, baseInfo.port);
    if (socket.waitForConnected(1000))
        status = true;

    quint64 latency = timer.elapsed();

    if (!status)
        latency = NULL;

    emit finished(FullInfo(baseInfo, status, latency, last_checked));
}
