#ifndef DBSERVICE_H
#define DBSERVICE_H

#include <QObject>

#include <QSqlDatabase>
#include <QSqlQuery>

#include <QSharedPointer>

#include <QSqlTableModel>

#include "Common.h"

class DBService: public QObject
{
    Q_OBJECT
public:
    DBService(QObject* parent = nullptr);

    bool connect(const QString& dbType, const QString& ip, const quint16& port,
                 const QString& dbName, const QString& userName, const QString& password);
    QVector<QSharedPointer<BaseInfo>> loadHosts();
    bool createQueries();
    bool writeCheckResults(const FullInfo& fullinfo);

    bool addHost(const QString& info, const QHostAddress& ip, const quint16& port);
    bool removeHost(const int& hosts_id);

    bool runCleanup();

    bool deleteMetrics();
    bool deleteArchive();

    bool updateClearRules(const int& hosts_id, const QString& retain_for);

    bool isOpen() const;
    bool reconnect();

    QSqlTableModel* getModel(QObject* parent, QString& tableName);

signals:
    void errorOccurred(const QString& msg);

private:
    QSqlDatabase db;
    QSqlQuery insertMetricsQuery,
        updateHostStatusQuery,
        updateClearRulesQuery,
        insertHostsQuery,
        insertHostStatusQuery,
        insertClearRulesQuery,
        deleteHostsQuery,
        deleteMetricsForTimerQuery,
        deleteAllMetricsQuery,
        deleteAllArchiveQuery;
};

#endif // DBSERVICE_H
