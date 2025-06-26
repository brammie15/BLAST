#include "sdwriterwindow.h"
#include "ui_sdwriterwindow.h"  // matches sdwriterwindow.ui

#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>

FlashSDWindow::FlashSDWindow(QWidget *parent)
    : QDialog(parent),
    ui(new Ui::FlashSDWindow),
    process(new QProcess(this))
{
    ui->setupUi(this);
    this->setWindowTitle("Flash SD Card");
    this->adjustSize();  // Make window fit its content
    setWindowFlags(windowFlags() | Qt::Window);  // Ensure it behaves like a window

    // Populate device dropdown on startup
    populateDevices();

    // Connect process signals
    connect(process, &QProcess::readyReadStandardOutput, this, &FlashSDWindow::processReadyRead);
    connect(process, &QProcess::readyReadStandardError, this, &FlashSDWindow::processReadyRead);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FlashSDWindow::processFinished);
}

void FlashSDWindow::setScriptsFolderPath(const QString &path) {
    scriptsFolderPath = path;
}

FlashSDWindow::~FlashSDWindow()
{
    delete ui;
}

void FlashSDWindow::populateDevices()
{
    // Clear existing items
    ui->deviceComboBox->clear();

    // Run "lsblk -dpno NAME,SIZE,MODEL" and parse output to list block devices
    QProcess lsblkProcess;
    lsblkProcess.start("lsblk", {"-dpno", "NAME,SIZE,MODEL"});
    lsblkProcess.waitForFinished();

    QString output = lsblkProcess.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    // Filter out loop devices
    for (const QString &line : lines) {
        if (line.contains("loop"))
            continue;

        // Format line example: "/dev/sda 465.8G Samsung SSD 860"
        ui->deviceComboBox->addItem(line.trimmed());
    }

    if (ui->deviceComboBox->count() == 0)
        ui->deviceComboBox->addItem("No devices found");
}

void FlashSDWindow::on_flashButton_clicked()
{
    if (process->state() != QProcess::NotRunning) {
        QMessageBox::warning(this, "Busy", "Flashing is already in progress.");
        return;
    }

    QString selectedDevice = ui->deviceComboBox->currentText().section(' ', 0, 0).trimmed();
    if (selectedDevice.isEmpty() || selectedDevice == "No devices found") {
        QMessageBox::warning(this, "Error", "Please select a valid device.");
        return;
    }

    QMessageBox::StandardButton ret = QMessageBox::question(this, "Confirm Flash",
                                                            QString("Are you sure you want to flash %1?\nThis will ERASE ALL DATA on the device!").arg(selectedDevice),
                                                            QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes) {
        ui->outputTextEdit->append("Flashing cancelled by user.");
        return;
    }

    ui->outputTextEdit->clear();

    QString scriptPath = scriptsFolderPath + "/write_sd.sh";

    QFileInfo scriptFile(scriptPath);
    if (!scriptFile.exists() || !scriptFile.isExecutable()) {
        QMessageBox::critical(this, "Error", QString("Script not found or not executable:\n%1").arg(scriptPath));
        return;
    }

    QStringList arguments;
    arguments << "--device" << selectedDevice << "--yes";

    process->start("pkexec", QStringList() << scriptPath << arguments);

    if (!process->waitForStarted()) {
        QMessageBox::critical(this, "Error", "Failed to start flashing process.");
        return;
    }
}

void FlashSDWindow::processReadyRead()
{
    // Append output or error output to the QTextEdit
    QByteArray stdOut = process->readAllStandardOutput();
    QByteArray stdErr = process->readAllStandardError();

    if (!stdOut.isEmpty())
        ui->outputTextEdit->append(QString::fromUtf8(stdOut));

    if (!stdErr.isEmpty())
        ui->outputTextEdit->append(QString::fromUtf8(stdErr));
}

void FlashSDWindow::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    if (exitStatus == QProcess::CrashExit) {
        ui->outputTextEdit->append("Flashing process crashed.");
    } else {
        ui->outputTextEdit->append("Flashing process finished.");
    }
}

void FlashSDWindow::on_refreshButton_clicked()
{
    populateDevices();
    ui->outputTextEdit->append("Device list refreshed.");
}
