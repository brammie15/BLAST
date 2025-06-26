#ifndef FLASHSDWINDOW_H
#define FLASHSDWINDOW_H

#include <QWidget>
#include <QProcess>
#include <QMainWindow>
#include <QDialog>


QT_BEGIN_NAMESPACE
namespace Ui {
class FlashSDWindow;
}
QT_END_NAMESPACE

class FlashSDWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FlashSDWindow(QWidget *parent = nullptr);
    ~FlashSDWindow();

    void setScriptsFolderPath(const QString &path);

private slots:
    void on_flashButton_clicked();
    void on_refreshButton_clicked();
    void processReadyRead();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void populateDevices();

    Ui::FlashSDWindow *ui;
    QProcess *process;

    QString scriptsFolderPath;
};

#endif // FLASHSDWINDOW_H
