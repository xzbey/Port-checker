#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSharedPointer>
#include <QSqlQuery>
#include <QSqlDatabase>

#include "HostChecker.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    bool downloadHostList(const QString& path);

    void loadJson();

    QVector<QSharedPointer<BaseInfo>> getHostList() const;

    void print(const FullInfo& fullInfo) const;

    void setTable();

    QSqlDatabase createDbConnection(const QString& dbType, const QString& ip, const quint16& port,
                            const QString& dbName, const QString& userName, const QString& password) const;

public slots:
    void onHostCheckerFinished(const FullInfo& fullInfo);

    void startPool();

private:
    Ui::MainWindow *ui;

    QVector<QSharedPointer<BaseInfo>> hostList;
    QTimer* timer;

    QSqlQuery insertQuery;
    bool dbReady = false;

};
#endif // MAINWINDOW_H
