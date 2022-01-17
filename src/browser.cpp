#include "browser.h"
#include "ui_browser.h"
#include "urls.h"

#include "options.h"

#include <QThread>
#include <QTextCodec>

#include <QStandardPaths>
#include <QEventLoop>
#include "l2pitemmodel.h"
#include "qslog/QsLog.h"

Browser::Browser(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Browser)
{
    ui->setupUi(this);

#if defined(Q_OS_MACOS)
    // Remove minimum size of download button on macOS as it also changes the default font color
    ui->syncPushButton->setMinimumHeight(0);
#endif

    // Hinzufügen der Daten zur Baumansicht
    l2pItemModel = new L2pItemModel();
    proxy = l2pItemModel->getProxy();
    ui->dataTreeView->setModel(proxy);

    setupSignalsSlots();
}

Browser::~Browser()
{
    delete ui;
}

void Browser::init(Options *options)
{
    this->options = options;
    l2pItemModel->setOptions(options);
}

void Browser::loadSettings()
{
    QSettings settings;

    settings.beginGroup("sizeFilter");
    ui->sizeLimitCheckBox->setChecked(settings.value("sizeLimit", false).toBool());
    ui->sizeLimitSpinBox->setValue(settings.value("sizeLimitValue", 10).toInt());
    settings.endGroup();

    settings.beginGroup("dateFilter");
    ui->dateFilterCheckBox->setChecked(settings.value("dateFilter", false).toBool());
    ui->minDateEdit->setDate(settings.value("minDate", QDate(2000, 1, 1)).toDate());
    ui->maxDateEdit->setDate(settings.value("maxDate", QDate(2042, 1, 1)).toDate());
    settings.endGroup();

    l2pItemModel->loadDataFromFile();
}

void Browser::saveSettings()
{
    // Speichern aller Einstellungen
    QSettings settings;

    settings.beginGroup("sizeFilter");
    settings.setValue("sizeLimit",      ui->sizeLimitCheckBox->isChecked());
    settings.setValue("sizeLimitValue", ui->sizeLimitSpinBox->value());
    settings.endGroup();

    settings.beginGroup("dateFilter");
    settings.setValue("dateFilter",     ui->dateFilterCheckBox->isChecked());
    settings.setValue("minDate",        ui->minDateEdit->date());
    settings.setValue("maxDate",        ui->maxDateEdit->date());
    settings.endGroup();

    l2pItemModel->saveDataToFile();
}

void Browser::downloadDirectoryLineEditChangedSlot(QString downloadDirectory)
{
    if (downloadDirectory.isEmpty())
    {
        ui->openDownloadfolderPushButton->setEnabled(false);
    }
    else
    {
        ui->openDownloadfolderPushButton->setEnabled(true);
    }
}

/// Starten des Aktualisierungsvorgang durch das Abrufen der Veranstaltungen
void Browser::on_refreshPushButton_clicked()
{
    refreshCounter++;
    l2pItemModel->loadDataFromServer();

    // Zurücksetzen der freigeschalteten Schaltflächen
    updateButtons();

    // Einfrieren der Anzeige
    emit enableSignal(false);
}

void Browser::on_syncPushButton_clicked()
{
    emit enableSignal(false);

    // Falls noch kein Downloadverzeichnis angegeben wurde, abbrechen
    QString downloadPath = options->downloadFolderLineEditText();
    if (downloadPath.isEmpty())
    {
        Utils::errorMessageBox(tr("Downloadverzeichnis fehlt!"),
                               tr("Download unmöglich, da kein Zielverzeichnis angegeben wurde."));
        emit switchTab(1);
        emit enableSignal(true);
        return;
    }

    // Überprüfung, ob das angegebene Verzeichnis existiert oder
    // erstellt werden kann
    QDir dir(downloadPath);
    if (!dir.exists() && !dir.mkpath(dir.path()))
    {
        QLOG_ERROR() << tr("Kann Verzeichnis nicht erzeugen. Download abgebrochen.");
        emit enableSignal(true);
        return;
    }

    auto *data = l2pItemModel->getData();

    // Hinzufügen aller eingebundenen Elemente
    QList<Structureelement *> elementList;
    for (int i = 0; i < data->rowCount(); i++)
    {
        getStructureelementsList((Structureelement *) data->item(i, 0), elementList, true);
    }

    // Abbruch bei fehlenden Elementen
    if (elementList.isEmpty())
    {
        Utils::errorMessageBox(tr("Kann nicht synchronisieren"),
                               tr("Keine Dateien zur Synchronisation verfügbar."));
        emit enableSignal(true);
        return;
    }

    int counter = getFileCount(elementList);
    FileDownloader *loader = new FileDownloader(
            counter,
            this);

    // Iterieren über alle Elemente
    int changedCounter = 0;
    QString courseName;

    QItemSelection newSelection;
    ui->dataTreeView->collapseAll();

    const auto overrideFilesSetting = this->options->getOverrideFilesSetting();
    bool showOverrideFilesDialog = overrideFilesSetting == Options::overrideFiles::ask;
    bool overrideFiles = overrideFilesSetting == Options::overrideFiles::download;

#ifdef _WIN32
    const bool oldWindowsVersion = !Utils::longPathsSupported();
    const auto longPathsSetting = this->options->getLongPathsSetting();
    bool showLongPathDialog = longPathsSetting == Options::longPaths::ask;
    bool downloadLongPathFiles = longPathsSetting == Options::longPaths::download;
#endif

    foreach(Structureelement *currentElement, elementList)
    {
        if(currentElement->type() != fileItem)
        {
            continue;
        }

        Structureelement* course = Utils::getParentCourse(currentElement);
        if(course)
        {
            courseName = course->text();
        }

        QString directoryPath = Utils::getElementLocalPath(currentElement, downloadPath, false, false);
        QDir directory(directoryPath);

#ifdef _WIN32
        // handle too long path name on older Windows versions
        const int pathLength = directoryPath.length() + currentElement->text().length();
        if (oldWindowsVersion && pathLength > 260)
        {
            if(showLongPathDialog)
            {
                // ask user if 
                const int res = Browser::pathTooLongDialog(currentElement->text());
                bool cancel = false;
                switch (res)
                {
                case QMessageBox::Yes:
                    downloadLongPathFiles = true;
                    break;
                case QMessageBox::YesToAll:
                    downloadLongPathFiles = true; 
                    showLongPathDialog = false;
                    break;
                case QMessageBox::No:
                default:
                    downloadLongPathFiles = false;
                    break;
                case QMessageBox::NoToAll:
                    downloadLongPathFiles = false; 
                    showLongPathDialog = false;
                    break;
                case QMessageBox::Cancel:
                    cancel = true;
                    break;
                }
                if (cancel)
                {
                    break;
                }
            }
            if (!downloadLongPathFiles)
            {
                continue;
            }
        }
#endif

        // Ordner ggf. erstellen
        if(!directory.mkpath(directoryPath))
        {
            Utils::errorMessageBox(tr("Verzeichnis nicht erstellbar!"), tr("Kann folgendes Verzeichnis nicht erstellen: ") + directoryPath);
            QLOG_ERROR() << tr("Verzeichnis nicht erstellbar: ") << directoryPath;
            break;
        }

        QString filename = currentElement->text();

        if(filename.contains(".aspx"))
            continue;

        bool fileExists = directory.exists(filename);
        bool downloadFile = !fileExists;

        if (fileExists && (currentElement->data(synchronisedRole) == NOT_SYNCHRONISED)) // File is out of sync but already exists: Implies either local changes, corrupt file or updated remote file
        {
            if(showOverrideFilesDialog)
            {
                // ask user if
                const int res = Browser::overrideFilesDialog(currentElement->text());
                bool cancel = false;
                switch (res)
                {
                case QMessageBox::Yes:
                default:
                    overrideFiles = true;
                    break;
                case QMessageBox::YesToAll: // Apply on all and make standard
                    overrideFiles = true;
                    showOverrideFilesDialog = false;
                    break;
                case QMessageBox::No:
                    overrideFiles = false;
                    break;
                case QMessageBox::NoToAll:
                    overrideFiles = false;
                    showOverrideFilesDialog = false;
                    break;
                case QMessageBox::Cancel:
                    cancel = true;
                    break;
                }
                if (cancel)
                {
                    break;
                }

            }
            if (!overrideFiles)
            {
                continue;
            }
            downloadFile = overrideFiles;
        }
        // Datei existiert noch nicht
        if(downloadFile)
        {


            QString downloadurl = currentElement->data(urlRole).toString();
            QString token = options->getAccessToken();

            QString url;
            url = moodleDownloadFileUrl % "/" % filename % "?downloadurl=" % downloadurl % "&token=" % token;


            if (!loader->startNextDownload(filename,
                                           courseName,
                                           directory.absoluteFilePath(filename),
                                           QUrl(url),
                                           changedCounter++,
                                           currentElement->data(sizeRole).toInt(),
                                           currentElement->data(dateRole).toDateTime().toMSecsSinceEpoch()/1000))
            {
                break;
            }

            currentElement->setData(NOW_SYNCHRONISED, synchronisedRole);
            ui->dataTreeView->scrollTo(proxy->mapFromSource(currentElement->index()));
            newSelection.select(currentElement->index(), currentElement->index());
        }
    }

#ifdef __linux__
    // Kurzes warten, da es sonst zu Fehldarstellungen unter Linux kommen kann,
    // wenn das Fenster zu schnell wieder geschlossen wird
    QThread::msleep(20);
#endif

    loader->hide();
    loader->close();

    // Automatisches Beenden nach der Synchronisation
    if (options->isAutoCloseAfterSyncCheckBoxChecked())
    {
        AutoCloseDialog autoClose;
        if(!autoClose.exec())
        {
            QCoreApplication::quit();
        }
    }

    // Information über abgeschlossene Synchronisation anzeigen
    QMessageBox messageBox(this);
    QTimer::singleShot(10000, &messageBox, SLOT(accept()));
    messageBox.setText
    (tr("Synchronisation mit RWTHmoodle abgeschlossen."));
    messageBox.setIcon(QMessageBox::NoIcon);
    messageBox.setInformativeText(QString
                                  (tr("Es wurden %1 von %2 eingebundenen Dateien synchronisiert.\n(Dieses Fenster schließt nach 10 Sek. automatisch.)")).arg
                                  (QString::number(changedCounter),
                                   QString::number(counter)));
    messageBox.setStandardButtons(QMessageBox::Ok);
    messageBox.exec();

    // Alle synchronisierten Elemente auswählen
    ui->dataTreeView->selectionModel()->
    select(proxy->mapSelectionFromSource(newSelection),
           QItemSelectionModel::ClearAndSelect);


    emit enableSignal(true);
}

// Diese Funktion nimmt die Eingabe im Suchfeld und zeigt alle Treffer
// an und markiert sie
void Browser::on_searchPushButton_clicked()
{
    // Kein Suchwort
    if (ui->searchLineEdit->text().isEmpty())
    {
        return;
    }

    auto *data = l2pItemModel->getData();

    // Alle Elemente mit dem Suchwort finden
    QList<QStandardItem *> found = data->findItems("*" % ui->searchLineEdit->text() % "*",
                                                   Qt::MatchWildcard | Qt::MatchRecursive);

    // Gefundene Elemente auswählen und anzeigen
    QItemSelection newSelection;
    ui->dataTreeView->collapseAll();

    foreach(QStandardItem * item, found)
    {
        newSelection.select(item->index(), item->index());
        ui->dataTreeView->scrollTo(proxy->mapFromSource(item->index()));
    }

    ui->dataTreeView->selectionModel()->
    select(proxy->mapSelectionFromSource(newSelection),
           QItemSelectionModel::ClearAndSelect);
}

// Ausschließen der ausgewählten Elemente, ggf. deren Vaterelemente und alle ihre Kindelemente
void Browser::on_removeSelectionPushButton_clicked()
{
    auto *data = l2pItemModel->getData();

    // Holen der ausgewählten Dateien
    QModelIndexList selectedElementsIndexList = proxy->mapSelectionToSource(ui->dataTreeView->selectionModel()->selection()).indexes();
    // Iteration über alle Elemente
    Structureelement *parentElement = nullptr;
    QModelIndexList::Iterator iteratorEnd = selectedElementsIndexList.end();

    for (QModelIndexList::Iterator iterator = selectedElementsIndexList.begin();
         iterator != iteratorEnd; iterator++)
    {
        // Holen des Vaterelements des ausgewählten Strukturelements
        parentElement = (Structureelement *) iterator->internalPointer();

        // Ausschließen des ausgewählten Elements
        Structureelement *selectedElement = (Structureelement *) parentElement->child(iterator->row(), 0);

        // Ausschließen aller Kindelemente
        removeSelection(selectedElement);

        // Prüfen, ob alle Geschwisterelemente ausgeschlossen sind
        // Falls ja, ausschließen des Vaterlements und rekursiv wiederholen

        // Hinweis: Nur wenn selectedElement ein Toplevel-Item ist, kann parentElement invisibleRootItem sein
        // ist selectedElement ein tieferes Item, ist der oberste Parent 0x00
        while ((parentElement != nullptr) && (parentElement != l2pItemModel->getData()->invisibleRootItem()))
        {
            bool siblingsExcluded = true;

            // Prüfung aller Zeilen, ob alle ausgeschlossen
            for (int i = 0; i < parentElement->rowCount(); i++)
            {
                if (((Structureelement *) parentElement->
                     child(i, 0))->data(includeRole).toBool())
                    siblingsExcluded = false;
            }

            // Falls ja, Vaterelement auch ausschließen
            if (siblingsExcluded)
                parentElement->setData(false, includeRole);

            parentElement = (Structureelement*) parentElement->parent();

        }
    }

    // Aktualisieren der kompletten Ansicht
    ui->dataTreeView->dataChanged(data->index(0, 0),
                                  data->index(data->rowCount(), 0));
}

// Rekursives ausschließen aller untergeordneten Elemente des übergebenen Elements
void Browser::removeSelection(Structureelement *element)
{
    if (element->data(includeRole).toBool())
    {
        element->setData(false, includeRole);

        for (int i = 0; i < element->rowCount(); i++)
        {
            removeSelection((Structureelement *) element->child(i, 0));
        }
    }
}

// Einbinden der ausgewählten Elemente, deren Vaterelemente und alle ihre Kindelemente
void Browser::on_addSelectionPushButton_clicked()
{
    // Bestimmen der ausgewählten Items
    QModelIndexList selectedElementsIndexList = proxy->mapSelectionToSource(ui->dataTreeView->selectionModel()->selection())
            .indexes();

    // Variablen zur Speicherung von Pointern aus Performancegründen vor
    // der Schleife
    Structureelement *element = nullptr;
    QModelIndexList::Iterator iteratorEnd = selectedElementsIndexList.end();

    for (QModelIndexList::Iterator iterator = selectedElementsIndexList.begin();
         iterator != iteratorEnd; iterator++)
    {
        // Holen des Pointers auf das ausgewählte Item
        // Hinweis: internalPointer liefert einen Pointer auf das Vaterelement
        element = (Structureelement *) ((Structureelement *) iterator->internalPointer())->child(iterator->row(), 0);

        // Einbinden aller übergeordneter Ordner
        Structureelement *parent = element;

        while ((parent = (Structureelement *) parent->parent()) != nullptr)
        {
            // Annahme: Wenn ein übergeordnetes Element eingebunden ist, dann sind es auch alle darüber
            if (parent->data(includeRole).toBool())
                break;

            parent->setData(true, includeRole);
        }

        // Einbinden aller untergeordneter Ordner und Dateien
        addSelection(element);
    }

    auto *data = l2pItemModel->getData();

    // Aktualisierung der kompletten Ansicht
    ui->dataTreeView->dataChanged(data->index(0, 0), data->index(data->rowCount(), 0));
}

// Rekursives Durchlaufen alle Kindelemente eines Elements und Einbindung dieser
void Browser::addSelection(Structureelement *aktuelleStruktur)
{
    // Einbinden des aktuellen Items
    aktuelleStruktur->setData(true, includeRole);
    // Einbinden aller untergeordnete Ordner und Dateien
    int rowCount = aktuelleStruktur->rowCount();

    for (int i = 0; i < rowCount; i++)
        addSelection((Structureelement *) aktuelleStruktur->child(i, 0));
}

void Browser::getStructureelementsList(Structureelement *currentElement, QList <Structureelement *> &liste, bool onlyIncluded)
{
    if (!onlyIncluded
        || (onlyIncluded && currentElement->data(includeRole).toBool()))
    {
        if (((ui->sizeLimitCheckBox->isChecked()
             && currentElement->data(sizeRole).toInt() <
             (ui->sizeLimitSpinBox->value() * 1024 * 1024))
            || !ui->sizeLimitCheckBox->isChecked())
                && ((ui->dateFilterCheckBox->isChecked() && ((ui->minDateEdit->date() <= currentElement->data(dateRole).toDate() && ui->maxDateEdit->date() >= currentElement->data(dateRole).toDate())|| !currentElement->data(dateRole).toDate().isValid())) || !ui->dateFilterCheckBox->isChecked()))
        liste.append(currentElement);


        if (currentElement->hasChildren())
            for (int i = 0; i < currentElement->rowCount(); i++)
                getStructureelementsList((Structureelement *)
                                         currentElement->child(i, 0),
                                         liste, onlyIncluded);
    }
}

void Browser::getStructureelementsList(QStandardItem *topElement, QList <Structureelement *> &list)
{
    Structureelement *item = dynamic_cast<Structureelement*>(topElement);
    if(item)
    {
        list.append(item);
    }
    for (int i = 0; i < topElement->rowCount(); i++)
        getStructureelementsList(topElement->child(i), list);
}

/// Bestimmung der Zahl der Dateien in einer Liste
int Browser::getFileCount(QList < Structureelement * > &items)
{
    int fileCounter = 0;

    foreach(Structureelement* item, items)
    {
        if(item->type() == fileItem)
        {
            fileCounter++;
        }
    }

    return fileCounter;
}

// Aktivierung oder Deaktivierung der Buttons in Abhängigkeit des Status
void Browser::updateButtons()
{
    if(l2pItemModel->getData()->rowCount() == 0 || options->getAccessToken().isEmpty())
    {
        ui->showNewDataPushButton->setEnabled(false);
        ui->expandPushButton->setEnabled(false);
        ui->contractPushButton->setEnabled(false);

        ui->removeSelectionPushButton->setEnabled(false);
        ui->addSelectionPushButton->setEnabled(false);
        ui->syncPushButton->setEnabled(false);
    }
    else
    {
        ui->showNewDataPushButton->setEnabled(true);
        ui->expandPushButton->setEnabled(true);
        ui->contractPushButton->setEnabled(true);

        ui->removeSelectionPushButton->setEnabled(true);
        ui->addSelectionPushButton->setEnabled(true);
        ui->syncPushButton->setEnabled(true);
    }

    ui->refreshPushButton->setEnabled(!options->getAccessToken().isEmpty());
}

void Browser::setupSignalsSlots()
{
    connect(ui->searchLineEdit, SIGNAL(returnPressed()), ui->searchPushButton, SLOT(click()));
    connect(ui->expandPushButton, &QPushButton::clicked, ui->dataTreeView, &QTreeView::expandAll);
    connect(ui->contractPushButton, &QPushButton::clicked, ui->dataTreeView, &QTreeView::collapseAll);

    connect(ui->sizeLimitSpinBox, SIGNAL(valueChanged(int)), proxy, SLOT(setMaximumSize(int)));
    connect(ui->sizeLimitCheckBox, &QCheckBox::toggled, proxy, &MySortFilterProxyModel::setMaximumSizeFilter);

    connect(ui->dateFilterCheckBox, &QCheckBox::toggled, proxy, &MySortFilterProxyModel::setInRangeDateFilter);
    connect(ui->minDateEdit, &QDateEdit::dateChanged, proxy, &MySortFilterProxyModel::setFilterMinimumDate);
    connect(ui->maxDateEdit, &QDateEdit::dateChanged, proxy, &MySortFilterProxyModel::setFilterMaximumDate);

    connect(l2pItemModel, &L2pItemModel::loadingFinished, this, &Browser::itemModelReloadedSlot);
    connect(l2pItemModel, &L2pItemModel::showStatusMessage, this, &Browser::showStatusMessage);
}

#ifdef _WIN32
int Browser::pathTooLongDialog(QString filename)
{
    if (filename.length() > 75)
    {
        filename = filename.left(75) + "...";
    }
    QMessageBox msgBox;
    msgBox.setText(tr("Datei herunterladen?"));
    msgBox.setInformativeText(QString(tr("Pfad zur Datei \"%1\" enthält mehr als 260 Zeichen. Wenn die Datei heruntergeladen wird, kann sie von den meisten "
                                 "Programmen nicht geöffnet und von Windows nicht so einfach gelöscht werden. Wenn möglich, ändere dein "
                                 "Downloadverzeichnis auf einen Pfad mit weniger Zeichen! Datei trotzdem herunterladen? (Du kannst in den Optionen das "
                                 "Standardverhalten ändern.)")).arg(filename));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::No);
    return msgBox.exec();
}
#endif

int Browser::overrideFilesDialog(QString filename)
{
    QMessageBox msgBox;
    msgBox.setText(tr("Datei überschreiben?"));
    msgBox.setInformativeText(QString(tr("Für die Datei \"%1\" liegt eine aktualisierte Version zum Download bereit. "
                                 "Soll die lokale Datei überschrieben werden? "
                                 "Hierbei gehen bisherige Änderungen verloren! (Du kannst in den Einstellungen das "
                                 "Standardverhalten ändern.)")).arg(filename));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    return msgBox.exec();
}

void Browser::on_openDownloadfolderPushButton_clicked()
{
    // Betriebssystem soll mit dem Standardprogramm den Pfad öffnen
    QDesktopServices::openUrl(QUrl
                              ("file:///" %
                               options->downloadFolderLineEditText(),
                               QUrl::TolerantMode));
}

void Browser::on_dataTreeView_doubleClicked(const QModelIndex &index)
{
    Structureelement *item =
        (Structureelement *) l2pItemModel->getData()->
        itemFromIndex(proxy->mapToSource(index));

    if (item->type() == fileItem)
    {
        lastRightClickItem = item;
        openFile();
    }
}

void Browser::on_dataTreeView_customContextMenuRequested(const QPoint &pos)
{
    // Bestimmung des Elements, auf das geklickt wurde
    Structureelement *RightClickedItem =
        (Structureelement *) l2pItemModel->getData()->
        itemFromIndex(proxy->mapToSource(ui->dataTreeView->indexAt(pos)));

    // Überprüfung, ob überhaupt auf ein Element geklickt wurde (oder
    // ins Leere)
    if (RightClickedItem == nullptr)
    {
        return;
    }

    // Speichern des geklickten Items
    lastRightClickItem = RightClickedItem;
    // Erstellen eines neuen Menus
    QMenu newCustomContextMenu(this);

    // Öffnen der Veranstaltungsseite in RWTHmoodle
    if (RightClickedItem->type() == courseItem)
    {
        newCustomContextMenu.addAction(tr("Veranstaltungsseite öffnen"), this, SLOT(openCourse()));
    }

    // Öffnen des Elements lokal oder in RWTHmoodle
    if (RightClickedItem->type() == fileItem)
    {
        newCustomContextMenu.addAction(tr("Öffnen"), this, SLOT(openFile()));
    }
    // Kopieren der URL
    if(RightClickedItem->type() == courseItem || RightClickedItem->type() == fileItem)
    {
        newCustomContextMenu.addAction(tr("Link kopieren"), this, SLOT(copyUrlToClipboardSlot()));
    }

    // Anzeigen des Menus an der Mausposition
    newCustomContextMenu.exec(ui->dataTreeView->mapToGlobal(pos));

}

void Browser::openCourse()
{
    // Öffnen der URL des mit der rechten Maustaste geklickten Items
    QDesktopServices::openUrl(lastRightClickItem->data(urlRole).toUrl());
}

void Browser::openFile()
{
    Structureelement* item = lastRightClickItem;
    QFileInfo fileInfo(Utils::getElementLocalPath(item,
                                                  options->downloadFolderLineEditText(),
                                                  true,
                                                  false));

    // Überprüfung, ob Datei lokal oder in RWTHmoodle geöffnet werden soll
    QUrl url;
    if(fileInfo.exists())
    {
        QString fileUrl = Utils::getElementLocalPath(item, options->downloadFolderLineEditText(), true, true);
        QLOG_DEBUG() << tr("Opening local file: ") << fileUrl;
        url = QUrl(fileUrl);
    }
    else
    {
        QString token = options->getAccessToken();
        QString fileUrl = Utils::getElementRemotePath(item) % "&token=" % token;
        url = QUrl(fileUrl);
    }

    QDesktopServices::openUrl(url);
}

void Browser::on_showNewDataPushButton_clicked()
{
    QList<Structureelement*> list;

    auto *data = l2pItemModel->getData();

    for (int i = 0; i < data->rowCount(); i++)
        getStructureelementsList((Structureelement*)(data->invisibleRootItem()->child(i)), list);

    QItemSelection newSelection;
    ui->dataTreeView->collapseAll();

    foreach(Structureelement * item, list)
    {
        if ((item->type() == fileItem) && (item->data(synchronisedRole) == NOT_SYNCHRONISED) && (item->data(includeRole) == true))
        {
            newSelection.select(item->index(), item->index());
            ui->dataTreeView->scrollTo(proxy->mapFromSource(item->index()));
        }
    }

    ui->dataTreeView->selectionModel()->
            select(proxy->mapSelectionFromSource(newSelection),
                   QItemSelectionModel::ClearAndSelect);
}

void Browser::copyUrlToClipboardSlot()
{
    QString url;
    if(lastRightClickItem->type() == fileItem)
    {
        QString token = options->getAccessToken();
        url = Utils::getElementRemotePath(lastRightClickItem) % "&token=" % token;
    }
    else if(lastRightClickItem->type() == courseItem)
    {
        url = lastRightClickItem->data(urlRole).toString();
    }

    Utils::copyTextToClipboard(url);
}

void Browser::successfulLoginSlot()
{
    ui->refreshPushButton->setEnabled(true);
}

void Browser::itemModelReloadedSlot()
{
    QLOG_DEBUG() << tr("Finalized pulling data. Checking local state...");
    updateButtons();

    QList<Structureelement*> items;
    getStructureelementsList(l2pItemModel->getData()->invisibleRootItem(), items);
    Utils::checkAllFilesIfSynchronised(items, options->downloadFolderLineEditText());

    // Anzeigen aller neuen, unsynchronisierten Dateien
    if (refreshCounter == 1)
    {
        on_showNewDataPushButton_clicked();
    }

    emit enableSignal(true);

    // Automatische Synchronisation beim Programmstart
    if (options->isAutoSyncOnStartCheckBoxChecked() && refreshCounter==1 && options->getLoginCounter()==1)
    {
        on_syncPushButton_clicked();
    }
}

void Browser::clearItemModel()
{
//    itemModel->clear();
    updateButtons();
}

void Browser::retranslate()
{
    ui->retranslateUi(this);
}
