#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>

#include "sdwriterwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_browseScriptsButton_clicked();
    void on_saveConfigButton_clicked();
    void on_buildButton_clicked();
    void on_updateConfigButton_clicked();
    void on_flashSDButton_clicked();

    void handleScriptOutput();
    void handleScriptError();
    void handleScriptFinished(int exitCode, QProcess::ExitStatus status);

private:
    Ui::MainWindow *ui;
    QProcess *process;
    QString scriptsFolderPath;

    void appendOutput(const QString &text);
    QString configTxtPath() const;
    QString scriptConfigTxtPath() const;
    QString scriptPath() const;

    void loadConfigFile(const QString& configPath);

    void checkUnsavedChanges();
    void setSaveConfigHighlight(bool highlight);
    void loadScriptConfigIfExists();

    QString lastSavedSSID;
    QString lastSavedPass;
    QString lastSavedScript;

    bool hasUnsavedChanges() const;
};

#endif // MAINWINDOW_H
