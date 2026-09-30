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
#include <QLabel>

#include <QMessageBox>
#include <QInputDialog>
#include <QComboBox>
#include <QTableView>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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

bool MainWindow::addHostForm(BaseInfo& outInfo) {
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

bool MainWindow::clearRuleForm(Time& time) {
    QDialog dialog(this);
    dialog.setWindowTitle("Изменение времени удаления метрик");
    dialog.setMinimumSize(180, 130);

    QFormLayout form(&dialog);

    QLabel label("Введите новое время удаления", &dialog);
    form.addRow(&label);

    QSpinBox hoursEdit(&dialog);
    hoursEdit.setRange(0, 23);
    hoursEdit.setValue(0);
    form.addRow("Часы:", &hoursEdit);

    QSpinBox minutesEdit(&dialog);
    minutesEdit.setRange(0, 59);
    minutesEdit.setValue(0);
    form.addRow("Минуты:", &minutesEdit);

    QSpinBox secondsEdit(&dialog);
    secondsEdit.setRange(0, 59);
    secondsEdit.setValue(0);
    form.addRow("Секунды:", &secondsEdit);

    QDialogButtonBox btnBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&btnBox);

    connect(&btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        time.h = hoursEdit.value(),
        time.m = minutesEdit.value(),
        time.s = secondsEdit.value();
        // qDebug() << h << m << s;
        if (time.h == 0 and time.m == 0 and time.s == 0) {
            QMessageBox::critical(this, "Изменение времени удаления метрик", "Ошибка при изменении времени!\nВсе значения равны 0!");
            return false;
        }
        return true;
    } else return false;
}

bool MainWindow::selectTableForm(QVector<QString>& tables, int& selectIndex) {
    QDialog dialog(this);
    dialog.setWindowTitle("Выбор таблицы");
    dialog.setMinimumSize(180, 130);

    QFormLayout form(&dialog);

    QComboBox comboBox(&dialog);
    comboBox.addItems(tables);
    form.addRow(&comboBox);

    QDialogButtonBox btnBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&btnBox);

    connect(&btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        selectIndex = comboBox.currentIndex();
        return true;
    } else return false;
}

void MainWindow::showTable(QString& tableName) {
    QDialog dialog(this);
    dialog.setWindowTitle(tableName);
    dialog.setMinimumSize(180, 130);

    QFormLayout form(&dialog);

    QTableView tableView(&dialog);
    tableView.setModel(dbservice.getModel(&dialog, tableName));
    tableView.resizeColumnsToContents();
    tableView.setEditTriggers(QAbstractItemView::NoEditTriggers);
    // tableView.verticalHeader()->setVisible(false);
    form.addRow(&tableView);

    dialog.resize(tableView.horizontalHeader()->length() + 50, 180);
    dialog.exec();
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
    if (!addHostForm(tempInfo))
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

void MainWindow::on_changeClearRules_clicked()
{
    int row = ui->InfoTable->currentRow();
    if (row == -1) return;

    Time time;
    if (clearRuleForm(time)) {
        QString retain_for = time.Get();

        if (dbservice.updateClearRules(hostList[row]->id, retain_for))
            QMessageBox::information(this, "Изменение времени удаления метрик",
                                           "Успешное изменение времени удаления метрик!\nПолучено значение: " + retain_for);
        else
            QMessageBox::critical(this, "Изменение времени удаления метрик",
                                        "Ошибка при изменении времени удаления метрик!\nПолучено значение: " + retain_for);
    }
}

void MainWindow::on_selectTable_clicked()
{
    QVector<QString> namesTables = {"hosts", "host_status", "metrics", "clear_rules", "archive"};
    int selectIndex;
    if (selectTableForm(namesTables, selectIndex)) {
        showTable(namesTables[selectIndex]);
        return;
    }
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






