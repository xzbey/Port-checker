#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <QDebug>
#include <QDir>

#include <QThreadPool>
#include <QTimer>

#include <QSqlError>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qRegisterMetaType<FullInfo>("FullInfo");

    loadJson();

    setTable();

    QThreadPool::globalInstance()->setMaxThreadCount(8);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::startPool);
    timer->start(1000);

    QSqlDatabase db = createDbConnection("QPSQL", "127.0.0.1", 5432,
                                        "postgres", "postgres", "mysecretpassword");
    if (!db.open())
        qDebug() << "Data base connection error:" << db.lastError().text();
    else {
        qDebug() << "Successful connect to PostgreSQL";
        insertQuery = QSqlQuery(db);
        dbReady = insertQuery.prepare("INSERT INTO metrics (info, ip, port, status, latency, ts)"
                                 "VALUES (:info, :ip, :port, :status, :latency, :ts)");
        if (!dbReady)
            qDebug() << "Prepare error:" << insertQuery.lastError().text();
    }
}


bool MainWindow::downloadHostList(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Open file error:" << file.errorString();
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());

    file.close();
    if (doc.isNull() or !doc.isArray()) {
        qDebug() << "File is null or isnt array";
        return false;
    }

    QJsonArray array = doc.array();

    QJsonObject item;
    quint16 id = 0;
    for (const QJsonValue& value: array) {
        item = value.toObject();

        QSharedPointer<BaseInfo> info = QSharedPointer<BaseInfo>::create(
            id++,
            item["info"].toString(),
            QHostAddress(item["ip"].toString()),
            static_cast<quint16>(item["port"].toString().toInt())
        );

        hostList.append(info);
    }

    return true;
}

void MainWindow::loadJson() {
    QString path = ":/list.json";
    // qDebug() << QDir(":/").entryList();
    if (!downloadHostList(path)) {
        qDebug() << "Failed to parse";
        close();
    }
}

void MainWindow::startPool() {
    for (int i = 0; i < hostList.size(); i++) {
        HostChecker *checker = new HostChecker(*hostList[i]);
        connect(checker, &HostChecker::finished, this, &MainWindow::onHostCheckerFinished);
        QThreadPool::globalInstance()->start(checker);
    }
}

QVector<QSharedPointer<BaseInfo>> MainWindow::getHostList() const{
    return hostList;
}

void MainWindow::onHostCheckerFinished(const FullInfo& fullInfo) {
    // print(fullInfo);

    for (int i = 0; i < ui->InfoTable->columnCount(); i++)
        ui->InfoTable->item(fullInfo.id, i)->setText(fullInfo[i]);

    if (dbReady) {
        insertQuery.bindValue(":info", fullInfo.info);
        insertQuery.bindValue(":ip", fullInfo.ip.toString());
        insertQuery.bindValue(":port", fullInfo.port);
        insertQuery.bindValue(":status", fullInfo.status);
        insertQuery.bindValue(":latency", fullInfo.latency);
        insertQuery.bindValue(":ts", fullInfo.last_checked);

        if (!insertQuery.exec())
            qDebug() << "Insert error:" << insertQuery.lastError().text();
        // else
        //     qDebug() << "Successful insert in db";
    }
}

void MainWindow::print(const FullInfo& fullInfo) const {
    qDebug() << QString(QString::number(fullInfo.id) + " | " + fullInfo.info + " | "+ fullInfo.ip.toString() +
                        " | " + QString::number(fullInfo.port) + " | " + QString(fullInfo.status ? "True" : "False") +
                        " | " + QString::number(fullInfo.latency) + " ms | " + fullInfo.last_checked.toString());
}

void MainWindow::setTable() {
    ui->InfoTable->setRowCount(hostList.size());
    ui->InfoTable->setColumnCount(5);

    ui->InfoTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->InfoTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->InfoTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->InfoTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->InfoTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    for (int row = 0; row < hostList.size(); row++)
        for (int col = 0; col < 5; col++)
            ui->InfoTable->setItem(row, col, new QTableWidgetItem("..."));
}

QSqlDatabase MainWindow::createDbConnection(const QString& dbType, const QString& ip, const quint16& port,
                                    const QString& dbName, const QString& userName, const QString& password) const {
    QSqlDatabase db = QSqlDatabase::addDatabase(dbType);
    db.setHostName(ip);
    db.setPort(port);
    db.setDatabaseName(dbName);
    db.setUserName(userName);
    db.setPassword(password);

    return db;
}

MainWindow::~MainWindow()
{
    delete ui;
}
