#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <QDebug>
#include <QDir>

#include <QThreadPool>
#include <QTimer>

#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>

#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qRegisterMetaType<FullInfo>("FullInfo");
    connect(&dbservice, &DBService::errorOccurred, this, [](const QString& msg){ qDebug() << msg; });

    if (dbservice.connect(Config::dbType, Config::ip, Config::port,
                           Config::dbName, Config::userName, Config::password)) {
        hostList = dbservice.loadHosts();
        setTable();
    }

    QThreadPool::globalInstance()->setMaxThreadCount(Config::threadsCount);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::startPool);
    timer->start(Config::delay);

    cleanupTimer = new QTimer(this);
    connect(cleanupTimer, &QTimer::timeout, &dbservice, &DBService::runCleanup);
    cleanupTimer->start(Config::cleanupTime);

    reconnectTimer = new QTimer(this);
    connect(reconnectTimer, &QTimer::timeout, this, &MainWindow::dbIsOpen);
    reconnectTimer->start(Config::reconnectTime);
}

void MainWindow::startPool() {
    for (int i = 0; i < hostList.size(); i++) {
        HostChecker *checker = new HostChecker(*hostList[i]);
        connect(checker, &HostChecker::finished, this, &MainWindow::onHostCheckerFinished);
        QThreadPool::globalInstance()->start(checker);
    }
}

void MainWindow::onHostCheckerFinished(const FullInfo& fullInfo) {
    int row = rowByHostId.value(fullInfo.id, -1);
    if (row == -1) return;

    for (int i = 0; i < ui->InfoTable->columnCount(); i++)
        ui->InfoTable->item(row, i)->setText(fullInfo[i]);

    dbservice.writeCheckResults(fullInfo);
}

void MainWindow::print(const FullInfo& fullInfo) const {
    qDebug() << QString(QString::number(fullInfo.id) + " | " + fullInfo.info + " | "+ fullInfo.ip.toString() +
                        " | " + QString::number(fullInfo.port) + " | " + QString(fullInfo.status ? "True" : "False") +
                        " | " + QString::number(fullInfo.latency) + " ms | " + fullInfo.last_checked.toString());
}

void MainWindow::setTable() {
    rowByHostId.clear();
    ui->InfoTable->setRowCount(hostList.size());
    ui->InfoTable->setColumnCount(5);

    ui->InfoTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->InfoTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->InfoTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->InfoTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->InfoTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    for (int row = 0; row < hostList.size(); row++) {
        for (int col = 0; col < 5; col++)
            ui->InfoTable->setItem(row, col, new QTableWidgetItem("..."));

        rowByHostId[hostList[row]->id] = row;
    }
}

bool MainWindow::dialogForm(BaseInfo& outInfo) {
    QDialog dialog(this);
    dialog.setWindowTitle("Параметры хоста");
    dialog.setMinimumSize(180, 130);

    QFormLayout form(&dialog);

    QLineEdit infoEdit(&dialog);
    form.addRow("Info:", &infoEdit);

    QLineEdit ipEdit(&dialog);
    form.addRow("Ip:", &ipEdit);

    QSpinBox portEdit(&dialog);
    portEdit.setRange(1, 65535);
    portEdit.setValue(1);
    form.addRow("Port:", &portEdit);

    QDialogButtonBox btnBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&btnBox);

    connect(&btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        outInfo = BaseInfo{
                           -1,
                           infoEdit.text(),
                           QHostAddress(ipEdit.text()),
                           static_cast<quint16>(portEdit.value())
                        };
        return true;
    } else return false;
}

void MainWindow::dbIsOpen() {
    if (dbservice.isOpen()) return;

    qDebug() << "Reconnect...";

    if (!dbservice.reconnect()) {
        qDebug() << "Fail in reconnect";
        return;
    }

    hostList = dbservice.loadHosts();
    setTable();
    qDebug() << "Successful reconnect!";
}

void MainWindow::on_addHost_clicked()
{
    BaseInfo tempInfo;
    if (!dialogForm(tempInfo))
        return;

    if (!dbservice.addHost(tempInfo.info, tempInfo.ip, tempInfo.port))
        return;

    hostList = dbservice.loadHosts();
    setTable();
}

void MainWindow::on_deleteHost_clicked()
{
    int row = ui->InfoTable->currentRow();
    if (row == -1) return;

    dbservice.removeHost(hostList[row]->id);

    hostList = dbservice.loadHosts();
    setTable();
}

void MainWindow::on_clearMetrics_clicked()
{
    if (dbservice.deleteMetrics())
        QMessageBox::information(this, "Очистка metrics", "Успешная очистка metrics!");
    else
        QMessageBox::critical(this, "Очистка metrics", "Ошибка при очистке metrics!");
}


void MainWindow::on_clearArchive_clicked()
{
    if (dbservice.deleteArchive())
        QMessageBox::information(this, "Очистка archive", "Успешная очистка archive!");
    else
        QMessageBox::critical(this, "Очистка archive", "Ошибка при очистке archive!");
}


MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    timer->stop();
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone();
    QMainWindow::closeEvent(event);
}
