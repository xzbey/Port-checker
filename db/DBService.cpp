#include "DBService.h"

#include <QSqlError>

DBService::DBService(QObject* parent): QObject(parent) {}

bool DBService::connect(const QString& dbType, const QString& ip, const quint16& port,
                        const QString& dbName, const QString& userName, const QString& password) {
    db = QSqlDatabase::addDatabase(dbType);
    db.setHostName(ip);
    db.setPort(port);
    db.setDatabaseName(dbName);
    db.setUserName(userName);
    db.setPassword(password);

    if (!db.open()) {
        emit errorOccurred(db.lastError().text());
        return false;
    }

    return createQueries();
}

QVector<QSharedPointer<BaseInfo>> DBService::loadHosts() {
    QSqlQuery query = QSqlQuery(db);
    QVector<QSharedPointer<BaseInfo>> result;

    if (!query.exec("SELECT * FROM hosts ORDER BY hosts_id")) {
        emit errorOccurred(query.lastError().text());
        return result;
    }

    while (query.next()) {
        result.append(QSharedPointer<BaseInfo>::create(
            query.value("hosts_id").toInt(),
            query.value("info").toString(),
            QHostAddress(query.value("ip").toString()),
            static_cast<quint16>(query.value("port").toInt())
        ));
    }

    return result;
}

bool DBService::createQueries() {
    updateHostStatusQuery = QSqlQuery(db);
    if (!updateHostStatusQuery.prepare("UPDATE host_status "
                                       "SET down_since = CASE "
                                       "WHEN :status THEN NULL "
                                       "ELSE COALESCE(down_since, NOW()) "
                                       "END "
                                       "WHERE hosts_id = :hosts_id")) {
        emit errorOccurred(updateHostStatusQuery.lastError().text());
        return false;
    }

    insertHostStatusQuery = QSqlQuery(db);
    if (!insertHostStatusQuery.prepare("INSERT INTO host_status (hosts_id) "
                                       "VALUES (:hosts_id)")) {
        emit errorOccurred(insertHostStatusQuery.lastError().text());
        return false;
    }


    insertHostsQuery = QSqlQuery(db);
    if (!insertHostsQuery.prepare("INSERT INTO hosts (info, ip, port) "
                                  "VALUES (:info, :ip, :port) "
                                  "RETURNING hosts_id")) {
        emit errorOccurred(insertHostsQuery.lastError().text());
        return false;
    }

    deleteHostsQuery = QSqlQuery(db);
    if (!deleteHostsQuery.prepare("WITH moved AS ("
                                  "   DELETE FROM metrics m "
                                  "   WHERE m.hosts_id = :hosts_id "
                                  "   RETURNING m.hosts_id, m.status, m.latency, m.ts"
                                  "), archived AS ("
                                  "   INSERT INTO archive (info, ip, port, status, latency, ts) "
                                  "   SELECT h.info, h.ip, h.port, moved.status, moved.latency, moved.ts "
                                  "   FROM hosts h JOIN moved USING(hosts_id)"
                                  ") "
                                  "DELETE FROM hosts "
                                  "WHERE hosts_id = :hosts_id")) {
        emit errorOccurred(deleteHostsQuery.lastError().text());
        return false;
    }


    insertClearRulesQuery = QSqlQuery(db);
    if (!insertClearRulesQuery.prepare("INSERT INTO clear_rules (hosts_id) "
                                       "VALUES (:hosts_id)")) {
        emit errorOccurred(insertClearRulesQuery.lastError().text());
        return false;
    }


    deleteMetricsForTimerQuery = QSqlQuery(db);
    if (!deleteMetricsForTimerQuery.prepare("WITH moved AS ("
                                            "   DELETE FROM metrics m "
                                            "   USING clear_rules r "
                                            "   WHERE m.hosts_id = r.hosts_id AND (now() - m.ts) > r.retain_for "
                                            "   RETURNING m.hosts_id, m.status, m.latency, m.ts"
                                            ") "
                                            "INSERT INTO archive (info, ip, port, status, latency, ts) "
                                            "SELECT h.info, h.ip, h.port, moved.status, moved.latency, moved.ts "
                                            "FROM hosts h JOIN moved USING(hosts_id)")) {
        emit errorOccurred(deleteMetricsForTimerQuery.lastError().text());
        return false;
    }


    insertMetricsQuery = QSqlQuery(db);
    if (!insertMetricsQuery.prepare("INSERT INTO metrics (hosts_id, status, latency, ts) "
                                    "VALUES (:hosts_id, :status, :latency, :ts)")) {
        emit errorOccurred(insertMetricsQuery.lastError().text());
        return false;
    }

    deleteAllMetricsQuery = QSqlQuery(db);
    if (!deleteAllMetricsQuery.prepare("WITH moved AS ("
                                       "   DELETE FROM metrics m "
                                       "   RETURNING m.hosts_id, m.status, m.latency, m.ts"
                                       ") "
                                       "INSERT INTO archive (info, ip, port, status, latency, ts) "
                                       "SELECT h.info, h.ip, h.port, moved.status, moved.latency, moved.ts "
                                       "FROM hosts h JOIN moved USING(hosts_id)")) {
        emit errorOccurred(deleteAllMetricsQuery.lastError().text());
        return false;
    }


    deleteAllArchiveQuery = QSqlQuery(db);
    if (!deleteAllArchiveQuery.prepare("DELETE FROM archive")) {
        emit errorOccurred(deleteAllArchiveQuery.lastError().text());
        return false;
    }

    updateClearRulesQuery = QSqlQuery(db);
    if (!updateClearRulesQuery.prepare("UPDATE clear_rules "
                                       "SET retain_for = :retain_for "
                                       "WHERE hosts_id = :hosts_id")) {
        emit errorOccurred(updateClearRulesQuery.lastError().text());
        return false;
    }

    return true;
}

bool DBService::writeCheckResults(const FullInfo& fullInfo) {
    db.transaction();

    insertMetricsQuery.bindValue(":hosts_id", fullInfo.id);
    insertMetricsQuery.bindValue(":status", fullInfo.status);
    insertMetricsQuery.bindValue(":latency", fullInfo.latency);
    insertMetricsQuery.bindValue(":ts", fullInfo.last_checked);
    if (!insertMetricsQuery.exec()) {
        emit errorOccurred(insertMetricsQuery.lastError().text());
        db.rollback();
        if (insertMetricsQuery.lastError().type() == QSqlError::ConnectionError)
            db.close();
        return false;
    }


    updateHostStatusQuery.bindValue(":status", fullInfo.status);
    updateHostStatusQuery.bindValue(":hosts_id", fullInfo.id);
    if (!updateHostStatusQuery.exec()) {
        emit errorOccurred(updateHostStatusQuery.lastError().text());
        db.rollback();
        return false;
    }

    return db.commit();
}

bool DBService::addHost(const QString& info, const QHostAddress& ip, const quint16& port) {
    db.transaction();

    insertHostsQuery.bindValue(":info", info);
    insertHostsQuery.bindValue(":ip", ip.toString());
    insertHostsQuery.bindValue(":port", port);
    if (!insertHostsQuery.exec() || !insertHostsQuery.next()) {
        emit errorOccurred(insertHostsQuery.lastError().text());
        db.rollback();
        return false;
    }

    int hosts_id = insertHostsQuery.value(0).toInt();

    insertHostStatusQuery.bindValue(":hosts_id", hosts_id);
    if (!insertHostStatusQuery.exec()) {
        emit errorOccurred(insertHostStatusQuery.lastError().text());
        db.rollback();
        return false;
    }

    insertClearRulesQuery.bindValue(":hosts_id", hosts_id);
    if (!insertClearRulesQuery.exec()) {
        emit errorOccurred(insertClearRulesQuery.lastError().text());
        db.rollback();
        return false;
    }

    return db.commit();
}

bool DBService::removeHost(const int& hosts_id) {
    db.transaction();

    deleteHostsQuery.bindValue(":hosts_id", hosts_id);
    if (!deleteHostsQuery.exec()) {
        emit errorOccurred(deleteHostsQuery.lastError().text());
        db.rollback();
        return false;
    }

    return db.commit();
}

bool DBService::runCleanup() {
    db.transaction();

    if (!deleteMetricsForTimerQuery.exec()) {
        emit errorOccurred(deleteMetricsForTimerQuery.lastError().text());
        db.rollback();
        return false;
    }
    return db.commit();
}

bool DBService::deleteMetrics() {
    db.transaction();

    if (!deleteAllMetricsQuery.exec()) {
        emit errorOccurred(deleteAllMetricsQuery.lastError().text());
        db.rollback();
        return false;
    }
    return db.commit();
}

bool DBService::deleteArchive() {
    db.transaction();

    if (!deleteAllArchiveQuery.exec()) {
        emit errorOccurred(deleteAllArchiveQuery.lastError().text());
        db.rollback();
        return false;
    }
    return db.commit();
}

bool DBService::updateClearRules(const int& hosts_id, const QString& retain_for) {
    db.transaction();

    updateClearRulesQuery.bindValue(":hosts_id", hosts_id);
    updateClearRulesQuery.bindValue(":retain_for", retain_for);
    if(!updateClearRulesQuery.exec()) {
        emit errorOccurred(updateClearRulesQuery.lastError().text());
        db.rollback();
        return false;
    }
    return db.commit();
}

bool DBService::isOpen() const {
    return db.isOpen();
}

bool DBService::reconnect() {
    if (!db.open()) {
        emit errorOccurred(db.lastError().text());
        return false;
    }

    return createQueries();
}

QSqlTableModel* DBService::getModel(QObject* parent, QString& tableName) {
    QSqlTableModel* model = new QSqlTableModel(parent, db);
    model->setTable(tableName);
    model->select();

    return model;
}
