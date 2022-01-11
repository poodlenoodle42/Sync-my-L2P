#include <QSystemTrayIcon>

#include "options.h"
#include "ui_options.h"
#include "browser.h"
#include "logindialog.h"
#include "info.h"

Options::Options(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Options),
    login(this)
{
    ui->setupUi(this);
    loginCounter = 0;

    QObject::connect(&login, SIGNAL(newAccessToken(QString)), this, SLOT(accessTokenChanged(QString)));

#ifdef _WIN32
    if (QSystemTrayIcon::isSystemTrayAvailable())
        ui->minimizeInTrayCheckBox->setEnabled(true);
#endif

    // Verfügbare Sprachen; Falls neue verfügbar, bitte hier, in options.h und in der mymainwindow.cpp ergänzen!
    ui->langCB->insertItem(ETOI(language::sys), tr("Systemsprache"));
    ui->langCB->insertItem(ETOI(language::de), "Deutsch");
    ui->langCB->insertItem(ETOI(language::en), "English");
    ui->langCB->insertItem(ETOI(language::lb), "Lëtzebuergesch");
    ui->langCB->insertItem(ETOI(language::sq), "Shqip");

    ui->longPathsCB->insertItem(ETOI(longPaths::ask), tr("Nachfragen"));
    ui->longPathsCB->insertItem(ETOI(longPaths::download), tr("Ja"));
    ui->longPathsCB->insertItem(ETOI(longPaths::skip), tr("Nein"));
}

Options::~Options()
{
    delete ui;
}

// Laden der lokal gespeicherten Einstellungen für den Tab Einstellungen
void Options::loadSettings()
{
    QSettings settings;

    settings.beginGroup("loginData");
    if (settings.value("saveLoginData", false).toBool())
    {
        ui->userDataSaveCheckBox->setChecked(true);
        ui->userDataSaveCheckBox->setEnabled(true);
    }
    else
    {
        ui->userDataSaveCheckBox->setChecked(false);
    }
    settings.endGroup();

    ui->downloadFolderlineEdit->setText(            settings.value("downloadFolder", "").toString());

    settings.beginGroup("automation");
    ui->autoLoginOnStartCheckBox->setChecked(       settings.value("autoLoginOnStart", false).toBool());
    ui->autoSyncOnStartCheckBox->setChecked(        settings.value("autoSyncOnStart", false).toBool());
    ui->autoCloseAfterSyncCheckBox->setChecked(     settings.value("autoCloseAfterSync", false).toBool());
    settings.endGroup();

    settings.beginGroup("misc");
    ui->minimizeInTrayCheckBox->setChecked(         settings.value("minimizeInTray", false).toBool());
    ui->overrideFilesCheckBox->setChecked(          settings.value("overrideFiles", false).toBool());
    ui->checkForUpdateCheckBox->setChecked(         settings.value("checkForUpdates", true).toBool());
    ui->currentSemesterCheckBox->setChecked(        settings.value("currentSemester", true).toBool());
    ui->longPathsCB->setCurrentIndex(               settings.value("longPaths", ETOI(longPaths::ask)).toInt());
    if (Utils::longPathsSupported()) {
        ui->longPathsLabel->setEnabled(false);
        ui->longPathsCB->setEnabled(false);
    }
    settings.endGroup();

    settings.beginGroup("language");
    ui->langCB->setCurrentIndex(                    settings.value("language", ETOI(language::sys)).toInt());
    settings.endGroup();

    login.init();
}

// Speichert lokal alle Einstellungen ab, die in unter Einstellungen getroffen wurden
void Options::saveSettings()
{
    QSettings settings;

    if (ui->userDataSaveCheckBox->isChecked())
    {
        settings.beginGroup("loginData");
        settings.setValue("saveLoginData", true);
        settings.endGroup();
    }
    else
        settings.remove("loginData");

    settings.setValue("downloadFolder",     ui->downloadFolderlineEdit->text());

    settings.beginGroup("automation");
    settings.setValue("autoLoginOnStart",   ui->autoLoginOnStartCheckBox->isChecked());
    settings.setValue("autoSyncOnStart",    ui->autoSyncOnStartCheckBox->isChecked());
    settings.setValue("autoCloseAfterSync", ui->autoCloseAfterSyncCheckBox->isChecked());
    settings.endGroup();

    settings.beginGroup("misc");
    settings.setValue("minimizeInTray",     ui->minimizeInTrayCheckBox->isChecked());
    settings.setValue("overrideFiles",      ui->overrideFilesCheckBox->isChecked());
    settings.setValue("checkForUpdates",    ui->checkForUpdateCheckBox->isChecked());
    settings.setValue("currentSemester",    ui->currentSemesterCheckBox->isChecked());
    settings.setValue("longPaths",          ui->longPathsCB->currentIndex());
    settings.endGroup();

    settings.beginGroup("language");
    settings.setValue("language",           ui->langCB->currentIndex());
    settings.endGroup();

    if(ui->userDataSaveCheckBox->isChecked())
    {
        login.saveAccessToken();
    }
}

void Options::on_loginPushButton_clicked()
{
    LoginDialog ld;
    QObject::connect(&ld, SIGNAL(finished(int)), this, SLOT(loginResultSlot(int)));

    ld.run(&login);
    ld.exec();

    loginCounter++;
}

void Options::on_userDataSaveCheckBox_stateChanged(int checked)
{
    // (De-)Aktivierung der autoLoginOnStartCheckBox CB
    if (checked)
        ui->autoLoginOnStartCheckBox->setEnabled(true);
    else
    {
        ui->autoLoginOnStartCheckBox->setEnabled(false);
        ui->autoLoginOnStartCheckBox->setChecked(false);
    }
}

void Options::on_autoLoginOnStartCheckBox_stateChanged(int checked)
{
    // (De-)Aktivierung der autoLoginOnStartCheckBox CB
    if (checked)
    {
        ui->autoSyncOnStartCheckBox->setEnabled(true);
    }
    else
    {
        ui->autoSyncOnStartCheckBox->setEnabled(false);
        ui->autoSyncOnStartCheckBox->setChecked(false);
    }
}

void Options::on_downloadFolderlineEdit_textChanged(const QString downloadFolder)
{
    emit downloadFolderLineEditTextChanged(downloadFolder);
}

void Options::on_downloadFolderPushButton_clicked()
{
    // Aufruf des Ordnerdialogs
    QString newDirectory = QFileDialog::getExistingDirectory(this,
                           tr("Downloadverzeichnis auswählen"),
                           QDir::rootPath(),
                           QFileDialog::ShowDirsOnly |
                           QFileDialog::DontResolveSymlinks);

    if (!newDirectory.isEmpty())
        ui->downloadFolderlineEdit->setText(newDirectory);
}

QString Options::downloadFolderLineEditText()
{
    return ui->downloadFolderlineEdit->text();
}

void Options::init(Browser *browser)
{
    this->browser = browser;
}

bool Options::isAutoSyncOnStartCheckBoxChecked()
{
    return ui->autoSyncOnStartCheckBox->isChecked();
}

bool Options::isMinimizeInTrayCheckBoxChecked()
{
    return ui->minimizeInTrayCheckBox->isChecked();
}

bool Options::isAutoCloseAfterSyncCheckBoxChecked()
{
    return ui->autoCloseAfterSyncCheckBox->isChecked();
}

bool Options::isOverrideFilesCheckBoxChecked()
{
    return ui->overrideFilesCheckBox->isChecked();
}

bool Options::isCheckForUpdateCheckBoxChecked()
{
    return ui->checkForUpdateCheckBox->isChecked();
}

QString Options::getAccessToken() const
{
    return accessToken;
}

bool Options::isAutoLoginOnStartCheckBoxChecked()
{
    return ui->autoLoginOnStartCheckBox->isChecked();
}

bool Options::isCurrentSemesterCheckBoxChecked()
{
    return ui->currentSemesterCheckBox->isChecked();
}

Options::longPaths Options::getLongPathsSetting()
{
    return static_cast<longPaths>(ui->longPathsCB->currentIndex());
}

int Options::getLoginCounter()
{
    return loginCounter;
}

void Options::on_loginErasePushButton_clicked()
{
    ui->loginStatusLabel->setText(tr("Status: ausgeloggt"));

    login.deleteAccess();
    ui->loginErasePushButton->setEnabled(false);
    ui->loginPushButton->setEnabled(true);
    ui->autoLoginOnStartCheckBox->setEnabled(false);
    ui->autoLoginOnStartCheckBox->setChecked(false);
    ui->userDataSaveCheckBox->setEnabled(false);
    ui->userDataSaveCheckBox->setChecked(false);

    browser->clearItemModel();
}

void Options::loginResultSlot(int result)
{
    if(result == QDialog::Accepted)
    {
        ui->loginStatusLabel->setText(tr("Status: Login erfolgreich"));

        // Aktualisieren der Buttons
        ui->loginErasePushButton->setEnabled(true);
        ui->loginPushButton->setEnabled(false);
        ui->autoLoginOnStartCheckBox->setEnabled(true);
        ui->userDataSaveCheckBox->setEnabled(true);

        emit switchTab(0);
        // Sofortiges Aktualisiern der Daten
        browser->on_refreshPushButton_clicked();
    }
    else
    {
        ui->loginStatusLabel->setText(tr("Status: Login fehlgeschlagen"));
    }
}

void Options::accessTokenChanged(QString newAccessToken)
{
    accessToken = newAccessToken;
}

void Options::retranslate()
{
    ui->retranslateUi(this);

    // translate ComboBox entries
    ui->longPathsCB->setItemText(ETOI(longPaths::ask), tr("Nachfragen"));
    ui->longPathsCB->setItemText(ETOI(longPaths::download), tr("Ja"));
    ui->longPathsCB->setItemText(ETOI(longPaths::skip), tr("Nein"));
    ui->langCB->setItemText(ETOI(language::sys), tr("Systemsprache"));
}

void Options::on_aboutButton_clicked()
{
    Info info;
    info.exec();
}
