#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QProcess>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , process(new QProcess(this))
{
    ui->setupUi(this);

    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::handleScriptOutput);
    connect(process, &QProcess::readyReadStandardError, this, &MainWindow::handleScriptError);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::handleScriptFinished);

    // Connect the combo box editable text change to unsaved changes check
    connect(ui->ssidComboBox->lineEdit(), &QLineEdit::textChanged, this, &MainWindow::checkUnsavedChanges);
    connect(ui->passwordLineEdit, &QLineEdit::textChanged, this, &MainWindow::checkUnsavedChanges);
    connect(ui->scriptTextEdit, &QTextEdit::textChanged, this, &MainWindow::checkUnsavedChanges);
    connect(ui->updateConfigButton, &QPushButton::clicked, this, &MainWindow::on_updateConfigButton_clicked);

    QString exeDir = QCoreApplication::applicationDirPath();
    QString scriptsDir = exeDir + "/Scripts";
    ui->scriptsPathLineEdit->setText(scriptsDir);

    QString configPath = scriptsDir + "/config.txt";
    if (QFile::exists(configPath)) {
        appendOutput("Loading initial config from: " + configPath); // ⬅ Logging
        loadConfigFile(configPath);
    }

    loadScriptConfigIfExists();

    // Scan for wifi networks and populate combo box
    scanWifiNetworks();
}

void MainWindow::scanWifiNetworks() {
    // Clear current list but keep the current text
    QString currentText = ui->ssidComboBox->currentText();
    ui->ssidComboBox->clear();

    QProcess nmcliProcess;
    nmcliProcess.start("nmcli", QStringList() << "-t" << "-f" << "SSID" << "device" << "wifi" << "list");
    nmcliProcess.waitForFinished(3000);
    QString output = nmcliProcess.readAllStandardOutput();

    // Parse SSIDs (one per line, ignore empty lines)
    QStringList ssids;
    for (const QString &line : output.split('\n')) {
        QString ssid = line.trimmed();
        if (!ssid.isEmpty() && !ssids.contains(ssid)) {
            ssids.append(ssid);
        }
    }

    // Add SSIDs to combo box
    ui->ssidComboBox->addItems(ssids);

    // Restore previously typed/selected text if still available
    if (!currentText.isEmpty()) {
        int index = ui->ssidComboBox->findText(currentText);
        if (index >= 0) {
            ui->ssidComboBox->setCurrentIndex(index);
        } else {
            // Keep custom text
            ui->ssidComboBox->setEditText(currentText);
        }
    }
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
        } else {
            appendOutput("No config.txt found in selected folder.");
        }
    }
}

void MainWindow::on_saveConfigButton_clicked() {
    if (!QDir(scriptsFolderPath).exists()) {
        QMessageBox::warning(this, "Missing Folder", "The selected scripts folder does not exist.");
        appendOutput("Error: The selected scripts folder does not exist.");
        return;
    }

    QFile configFile(configTxtPath());
    if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&configFile);
        out << "WIFI_SSID=" << ui->ssidComboBox->currentText() << '\n';
        out << "WIFI_PASS=" << ui->passwordLineEdit->text() << '\n';
        configFile.close();
        appendOutput("Saved config.txt successfully.");
    } else {
        QMessageBox::critical(this, "Error", "Failed to write config.txt");
        appendOutput("Failed to write config.txt");
        return;
    }

    QFile scriptFile(scriptConfigTxtPath());
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << ui->scriptTextEdit->toPlainText();
        scriptFile.close();

        lastSavedSSID = ui->ssidComboBox->currentText();
        lastSavedPass = ui->passwordLineEdit->text();
        lastSavedScript = ui->scriptTextEdit->toPlainText();
        setSaveConfigHighlight(false);

        appendOutput("Saved script_config.txt successfully.");
    } else {
        QMessageBox::critical(this, "Error", "Failed to write script_config.txt");
        appendOutput("Failed to write script_config.txt");
        return;
    }

    QMessageBox::information(this, "Success", "Configuration saved.");
    appendOutput("Configuration saved successfully.");
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
        appendOutput("Script missing or not executable: " + script);
        return;
    }

    appendOutput("Running change_config.sh...");
    process->setWorkingDirectory(scriptsFolderPath);
    process->start("pkexec", QStringList() << script);
}

void MainWindow::on_buildButton_clicked() {
    QString scriptsPath = ui->scriptsPathLineEdit->text();

    QString configPath = scriptsPath + "/config.txt";
    if (!QFile::exists(configPath)) {
        QMessageBox::critical(this, "Error", "config.txt not found in scripts folder:\n" + configPath);
        appendOutput("Error: config.txt not found at: " + configPath);
        return;
    }

    QString scriptFile = scriptsPath + "/build_image.sh";
    QFileInfo fi(scriptFile);
    if (!fi.isExecutable()) {
        QMessageBox::critical(this, "Error", "Script is not executable:\n" + scriptFile);
        appendOutput("Error: build_image.sh is not executable: " + scriptFile);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(scriptsPath);

    ui->outputTextEdit->clear();
    appendOutput("Starting build...");

    connect(process, &QProcess::readyReadStandardOutput, [this, process]() {
        appendOutput(process->readAllStandardOutput());
    });

    connect(process, &QProcess::readyReadStandardError, [this, process]() {
        appendOutput(process->readAllStandardError());
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus){
                appendOutput(QString("\nScript finished with exit code %1.").arg(exitCode));
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
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendOutput("Failed to open config.txt");
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith("WIFI_SSID=")) {
            QString ssid = line.mid(QString("WIFI_SSID=").length());
            ui->ssidComboBox->setCurrentText(ssid);
        } else if (line.startsWith("WIFI_PASS=")) {
            QString pass = line.mid(QString("WIFI_PASS=").length());
            ui->passwordLineEdit->setText(pass);
        }
    }
    file.close();

    // Save loaded state for change tracking
    lastSavedSSID = ui->ssidComboBox->currentText();
    lastSavedPass = ui->passwordLineEdit->text();
}


void MainWindow::loadScriptConfigIfExists() {
    QString path = scriptConfigTxtPath();
    QFile file(path);
    if (!file.exists()) {
        appendOutput("No script_config.txt found.");
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendOutput("Failed to open script_config.txt");
        return;
    }

    QString content = file.readAll();
    file.close();

    ui->scriptTextEdit->setPlainText(content);
    lastSavedScript = content;
}

void MainWindow::checkUnsavedChanges() {
    bool changed = hasUnsavedChanges();
    setSaveConfigHighlight(changed);
}

bool MainWindow::hasUnsavedChanges() const {
    return (lastSavedSSID != ui->ssidComboBox->currentText()) ||
           (lastSavedPass != ui->passwordLineEdit->text()) ||
           (lastSavedScript != ui->scriptTextEdit->toPlainText());
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
    appendOutput("Launched SD flashing dialog.");
}

void MainWindow::on_cleanButton_clicked(){
    QString script = scriptsFolderPath + "/clean.sh";

    if (!QFileInfo::exists(script) || !QFileInfo(script).isExecutable()) {
        QMessageBox::critical(this, "Error", "Script not found or not executable:\n" + script);
        appendOutput("Script missing or not executable: " + script);
        return;
    }

    appendOutput("Cleaning scripts folder...");
    process->setWorkingDirectory(scriptsFolderPath);
    process->start("pkexec", QStringList() << script);
}

