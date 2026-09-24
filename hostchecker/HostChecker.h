#ifndef HOSTCHECKER_H
#define HOSTCHECKER_H

#include "Common.h"

#include <QObject>
#include <QRunnable>
#include <QTcpSocket>

class HostChecker: public QObject, public QRunnable
{
    Q_OBJECT
public:
    HostChecker() = default;

    HostChecker(const BaseInfo& baseInfo);

    void run() override;

signals:
    void finished(const FullInfo& fullInfo);
    // void testing(QString time);

private:
    BaseInfo baseInfo;
};

#endif // HOSTCHECKER_H
