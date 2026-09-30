#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSharedPointer>
#include <QSqlQuery>
#include <QSqlDatabase>

#include "HostChecker.h"
#include "DBService.h"

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

    void print(const FullInfo& fullInfo) const;

    void setTable();

    bool addHostForm(BaseInfo& outInfo);
    bool clearRuleForm(Time& time);
    bool selectTableForm(QVector<QString>& tables, int& selectIndex);
    void showTable(QString& tableName);

    void dbIsOpen();

public slots:
    void onHostCheckerFinished(const FullInfo& fullInfo);

    void startPool();

private slots:
    void on_addHost_clicked();

    void on_deleteHost_clicked();

    void on_clearMetrics_clicked();

    void on_clearArchive_clicked();

    void on_changeClearRules_clicked();

    void on_selectTable_clicked();

private:
    Ui::MainWindow *ui;

    QVector<QSharedPointer<BaseInfo>> hostList;
    QHash<int, int> rowByHostId;

    QTimer* timer,
        *cleanupTimer,
        *reconnectTimer;

    DBService dbservice;

protected:
    void closeEvent(QCloseEvent *event) override;
};
#endif // MAINWINDOW_H
