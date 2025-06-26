#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>
#include <QRegularExpression>
#include <QSignalBlocker>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , process(new QProcess(this))
{
    ui->setupUi(this);

    // Setup process connections
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::handleScriptOutput);
    connect(process, &QProcess::readyReadStandardError, this, &MainWindow::handleScriptError);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::handleScriptFinished);

    connect(ui->ssidLineEdit, &QLineEdit::textChanged, this, &MainWindow::checkUnsavedChanges);
    connect(ui->passwordLineEdit, &QLineEdit::textChanged, this, &MainWindow::checkUnsavedChanges);
    connect(ui->scriptTextEdit, &QTextEdit::textChanged, this, &MainWindow::checkUnsavedChanges);
    connect(ui->updateConfigButton, &QPushButton::clicked, this, &MainWindow::on_updateConfigButton_clicked);

    QString exeDir = QCoreApplication::applicationDirPath();
    QString scriptsDir = exeDir + "/Scripts";
    ui->scriptsPathLineEdit->setText(scriptsDir);

    QString configPath = scriptsDir + "/config.txt";
    if (QFile::exists(configPath)) {
        loadConfigFile(configPath);
    }

    loadScriptConfigIfExists();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_browseScriptsButton_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Scripts Folder", QDir::currentPath());
    if (!dir.isEmpty()) {
        appendOutput("New scripts folder targeted!");
        scriptsFolderPath = dir;
        ui->scriptsPathLineEdit->setText(scriptsFolderPath);
        QString configPath = dir + "/config.txt";
        if (QFile::exists(configPath)) {
            appendOutput("Existing config.txt found, trying to load values.");
            loadConfigFile(configPath);
            loadScriptConfigIfExists();
        }
    }
}

void MainWindow::on_saveConfigButton_clicked() {
    if (scriptsFolderPath.isEmpty()) {
        QMessageBox::warning(this, "Missing Folder", "Please select a scripts folder first.");
        return;
    }

    QFile configFile(configTxtPath());
    if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&configFile);
        out << "WIFI_SSID=" << ui->ssidLineEdit->text() << '\n';
        out << "WIFI_PASS=" << ui->passwordLineEdit->text() << '\n';
        configFile.close();
    } else {
        QMessageBox::critical(this, "Error", "Failed to write config.txt");
        return;
    }

    QFile scriptFile(scriptConfigTxtPath());
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << ui->scriptTextEdit->toPlainText();
        scriptFile.close();

        lastSavedSSID = ui->ssidLineEdit->text();
        lastSavedPass = ui->passwordLineEdit->text();
        lastSavedScript = ui->scriptTextEdit->toPlainText();
        setSaveConfigHighlight(false);
    } else {
        QMessageBox::critical(this, "Error", "Failed to write script_config.txt");
        return;
    }

    QMessageBox::information(this, "Success", "Configuration saved.");
}

void MainWindow::on_updateConfigButton_clicked() {
    if (hasUnsavedChanges()) {
        auto result = QMessageBox::question(this, "Unsaved Changes", "You have unsaved changes. Do you want to save them?",
                                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (result == QMessageBox::Cancel) return;
        if (result == QMessageBox::Yes) on_saveConfigButton_clicked();
    }

    QString script = scriptsFolderPath + "/change_config.sh";
    if (!QFileInfo::exists(script) || !QFileInfo(script).isExecutable()) {
        QMessageBox::critical(this, "Error", "Script not found or not executable:\n" + script);
        return;
    }

    ui->outputTextEdit->append("Running change_config.sh...\n");
    process->setWorkingDirectory(scriptsFolderPath);
    process->start("pkexec", QStringList() << script);
}

void MainWindow::on_buildButton_clicked() {
    QString scriptsPath = ui->scriptsPathLineEdit->text();

    QString configPath = scriptsPath + "/config.txt";
    if (!QFile::exists(configPath)) {
        QMessageBox::critical(this, "Error", "config.txt not found in scripts folder:\n" + configPath);
        return;
    }

    QString scriptFile = scriptsPath + "/build_image.sh";
    QFileInfo fi(scriptFile);
    if (!fi.isExecutable()) {
        QMessageBox::critical(this, "Error", "Script is not executable:\n" + scriptFile);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(scriptsPath);

    ui->outputTextEdit->clear();
    ui->outputTextEdit->append("Starting build...\n");

    connect(process, &QProcess::readyReadStandardOutput, [this, process]() {
        ui->outputTextEdit->append(QString::fromUtf8(process->readAllStandardOutput()));
    });

    connect(process, &QProcess::readyReadStandardError, [this, process]() {
        ui->outputTextEdit->append(QString::fromUtf8(process->readAllStandardError()));
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus){
                ui->outputTextEdit->append(QString("\nScript finished with exit code %1.").arg(exitCode));
            });

    process->start("pkexec", QStringList() << scriptFile);
}

void MainWindow::handleScriptOutput() {
    appendOutput(process->readAllStandardOutput());
}

void MainWindow::handleScriptError() {
    appendOutput(process->readAllStandardError());
}

void MainWindow::handleScriptFinished(int exitCode, QProcess::ExitStatus status) {
    QString msg = (status == QProcess::NormalExit)
    ? QString("Script finished with code %1.").arg(exitCode)
    : "Script crashed.";
    appendOutput(msg);
}

void MainWindow::appendOutput(const QString &text) {
    ui->outputTextEdit->append(text.trimmed());
}

QString MainWindow::configTxtPath() const {
    return scriptsFolderPath + "/config.txt";
}

QString MainWindow::scriptConfigTxtPath() const {
    return scriptsFolderPath + "/script_config.txt";
}

QString MainWindow::scriptPath() const {
    return scriptsFolderPath + "/build_image.sh";
}

void MainWindow::loadConfigFile(const QString& configPath) {
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        appendOutput("Config file not found :(");
        return;
    }
    QTextStream in(&file);
    QString content = in.readAll();

    QRegularExpression ssidRegex(R"(WIFI_SSID\s*=\s*(.+)$)", QRegularExpression::MultilineOption);
    QRegularExpression passRegex(R"(WIFI_PASS\s*=\s*(.+)$)", QRegularExpression::MultilineOption);

    const QSignalBlocker blocker1(ui->ssidLineEdit);
    const QSignalBlocker blocker2(ui->passwordLineEdit);

    auto ssidMatch = ssidRegex.match(content);
    if (ssidMatch.hasMatch()) {
        QString ssid = ssidMatch.captured(1);
        ui->ssidLineEdit->setText(ssid);
        lastSavedSSID = ssid;
    }

    auto passMatch = passRegex.match(content);
    if (passMatch.hasMatch()) {
        QString pass = passMatch.captured(1);
        ui->passwordLineEdit->setText(pass);
        lastSavedPass = pass;
    }
}

void MainWindow::loadScriptConfigIfExists() {
    QString path = scriptConfigTxtPath();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QSignalBlocker blocker(ui->scriptTextEdit);
        ui->scriptTextEdit->setPlainText(file.readAll());
        file.close();
        lastSavedScript = ui->scriptTextEdit->toPlainText();
    }
}

void MainWindow::checkUnsavedChanges() {
    setSaveConfigHighlight(hasUnsavedChanges());
}

bool MainWindow::hasUnsavedChanges() const {
    return ui->ssidLineEdit->text() != lastSavedSSID ||
           ui->passwordLineEdit->text() != lastSavedPass ||
           ui->scriptTextEdit->toPlainText() != lastSavedScript;
}

void MainWindow::setSaveConfigHighlight(bool highlight) {
    QString style = highlight ? "background-color: yellow" : "";
    ui->saveConfigButton->setStyleSheet(style);
}

void MainWindow::on_flashSDButton_clicked() {
    FlashSDWindow *flashWindow = new FlashSDWindow(this);
    flashWindow->setAttribute(Qt::WA_DeleteOnClose);
    flashWindow->setScriptsFolderPath(scriptsFolderPath);
    flashWindow->exec();
}
