/*********************************************************************************
**
** Copyright (c) 2017 The University of Notre Dame
** Copyright (c) 2017 The Regents of the University of California
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice, this
** list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice, this
** list of conditions and the following disclaimer in the documentation and/or other
** materials provided with the distribution.
**
** 3. Neither the name of the copyright holder nor the names of its contributors may
** be used to endorse or promote products derived from this software without specific
** prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
** EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
** SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
** IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
***********************************************************************************/

// Contributors:
// Written by Peter Sempolinski, for the Natural Hazard Modeling Laboratory, director: Ahsan Kareem, at Notre Dame

#include "explorerwindow.h"
#include "ui_explorerwindow.h"

#include "../AgaveClientInterface/remotedatainterface.h"
#include "../AgaveClientInterface/filemetadata.h"

#include "remoteFileOps/filetreenode.h"
#include "remoteFileOps/fileoperator.h"
#include "remoteFileOps/remotefiletree.h"
#include "remoteFileOps/joboperator.h"

#include "../utilFuncs/singlelinedialog.h"

#include "explorerdriver.h"

ExplorerWindow::ExplorerWindow(ExplorerDriver *theDriver, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ExplorerWindow)
{
    ui->setupUi(this);

    programDriver = theDriver;
    dataLink = programDriver->getDataConnection();

    agaveParamLists.insert("compress",{"compression_type"});
    agaveParamLists.insert("extract", {"inputFile"});
    agaveParamLists.insert("openfoam", {"solver","inputDirectory"});

    for (auto itr = agaveParamLists.keyBegin(); itr != agaveParamLists.keyEnd(); itr++)
    {
        taskListModel.appendRow(new QStandardItem(*itr));
    }
    ui->agaveAppList->setModel(&taskListModel);

    ui->selectedFileLabel->connectFileTreeWidget(ui->remoteFileView);
    ui->selectedFileInfo->connectFileTreeWidget(ui->remoteFileView);
}

ExplorerWindow::~ExplorerWindow()
{
    delete ui;
}

void ExplorerWindow::startAndShow()
{
    theFileOperator = programDriver->getFileHandler();
    ui->remoteFileView->setFileOperator(theFileOperator);
    ui->remoteFileView->setupFileView();
    QObject::connect(ui->remoteFileView, SIGNAL(customContextMenuRequested(QPoint)),
                     this, SLOT(customFileMenu(QPoint)));

    ui->agaveAppList->setModel(&taskListModel);

    ui->jobTable->setJobHandle(programDriver->getJobHandler());
    QObject::connect(ui->jobTable, SIGNAL(customContextMenuRequested(QPoint)),
                     this, SLOT(jobRightClickMenu(QPoint)));

    //Note: Adding widget to header will re-parent them
    QLabel * username = new QLabel(programDriver->getDataConnection()->getUserName());
    ui->header->appendWidget(username);

    QPushButton * logoutButton = new QPushButton("Logout");
    QObject::connect(logoutButton, SIGNAL(clicked(bool)), programDriver, SLOT(shutdown()));
    ui->header->appendWidget(logoutButton);
    this->show();
}

void ExplorerWindow::addAppToList(QString appName)
{
    if (appName == "cwe-mesh")
    {
        agaveParamLists.insert("cwe-mesh", {});
        taskListModel.appendRow(new QStandardItem("cwe-mesh"));
    }
    else if (appName == "cwe-sim")
    {
        agaveParamLists.insert("cwe-sim", {});
        taskListModel.appendRow(new QStandardItem("cwe-sim"));
    }
    else if (appName == "cwe-post")
    {
        agaveParamLists.insert("cwe-post", {});
        taskListModel.appendRow(new QStandardItem("cwe-post"));
    }
}

void ExplorerWindow::agaveAppSelected(QModelIndex clickedItem)
{
    QString newSelection = taskListModel.itemFromIndex(clickedItem)->text();
    if (selectedAgaveApp == newSelection)
    {
        return;
    }
    selectedAgaveApp = newSelection;

    QGridLayout * panelLayout = new QGridLayout();
    QObjectList childList = ui->AgaveParamWidget->children();

    while (childList.size() > 0)
    {
        QObject * aChild = childList.takeLast();
        delete aChild;
    }

    ui->AgaveParamWidget->setLayout(panelLayout);

    QStringList inputList = agaveParamLists.value(selectedAgaveApp);
    int rowNum = 0;

    for (auto itr = inputList.cbegin(); itr != inputList.cend(); itr++)
    {
        QString paramName = "debugAgave_";
        paramName = paramName.append(*itr);

        QLabel * tmpLabel = new QLabel(*itr);
        QLineEdit * tmpInput = new QLineEdit();
        tmpInput->setObjectName(paramName);

        panelLayout->addWidget(tmpLabel,rowNum,0);
        panelLayout->addWidget(tmpInput,rowNum,1);
        rowNum++;
    }
}

void ExplorerWindow::agaveCommandInvoked()
{
    if (waitingOnCommand)
    {
        return;
    }
    qDebug("Selected App: %s", qPrintable(selectedAgaveApp));

    QString workingDir = ui->remoteFileView->getSelectedNode()->getFileData().getFullPath();
    qDebug("Working Dir: %s", qPrintable(workingDir));

    QStringList inputList = agaveParamLists.value(selectedAgaveApp);
    QMultiMap<QString, QString> allInputs;

    qDebug("Input List:");
    for (auto itr = inputList.cbegin(); itr != inputList.cend(); itr++)
    {
        QString paramName = "debugAgave_";
        paramName = paramName.append(*itr);

        QLineEdit * theInput = ui->AgaveParamWidget->findChild<QLineEdit *>(paramName);
        if (theInput != NULL)
        {
            allInputs.insert((*itr),theInput->text());
            qDebug("%s : %s", qPrintable(*itr), qPrintable(theInput->text()));
        }
    }

    RemoteDataReply * theTask = dataLink->runRemoteJob(selectedAgaveApp,allInputs,workingDir);
    if (theTask == NULL)
    {
        qDebug("Unable to invoke task");
        return;
    }
    waitingOnCommand = true;
    QObject::connect(theTask, SIGNAL(haveJobReply(RequestState,QJsonDocument*)),
                     this, SLOT(finishedAppInvoke(RequestState,QJsonDocument*)));
}

void ExplorerWindow::finishedAppInvoke(RequestState, QJsonDocument *)
{
    waitingOnCommand = false;
    programDriver->getJobHandler()->demandJobDataRefresh();
}

void ExplorerWindow::customFileMenu(QPoint pos)
{
    QMenu fileMenu;
    if (ui->remoteFileView->getFileOperator()->operationIsPending())
    {
        fileMenu.addAction("File Operation In Progress . . .");
        fileMenu.exec(QCursor::pos());
        return;
    }

    QModelIndex targetIndex = ui->remoteFileView->indexAt(pos);
    ui->remoteFileView->fileEntryTouched(targetIndex);

    targetNode = ui->remoteFileView->getSelectedNode();

    //If we did not click anything, we should return
    if (targetNode == NULL) return;
    if (targetNode->isRootNode()) return;
    FileMetaData theFileData = targetNode->getFileData();

    if (theFileData.getFileType() == FileType::INVALID) return;
    if (theFileData.getFileType() == FileType::UNLOADED) return;
    if (theFileData.getFileType() == FileType::EMPTY_FOLDER) return;

    fileMenu.addAction("Copy To . . .",this, SLOT(copyMenuItem()));
    fileMenu.addAction("Move To . . .",this, SLOT(moveMenuItem()));
    fileMenu.addAction("Rename",this, SLOT(renameMenuItem()));
    //We don't let the user delete the username folder
    if (!(targetNode->getParentNode()->isRootNode()))
    {
        fileMenu.addSeparator();
        fileMenu.addAction("Delete",this, SLOT(deleteMenuItem()));
        fileMenu.addSeparator();
    }
    if (theFileData.getFileType() == FileType::DIR)
    {
        fileMenu.addAction("Upload File Here",this, SLOT(uploadMenuItem()));
        fileMenu.addAction("Create New Folder",this, SLOT(createFolderMenuItem()));
    }
    if (theFileData.getFileType() == FileType::FILE)
    {
        fileMenu.addAction("Download File",this, SLOT(downloadMenuItem()));
        if (targetNode->getFileBuffer() != NULL)
        {
            fileMenu.addAction("Read File",this, SLOT(readMenuItem()));
        }
        else
        {
            fileMenu.addAction("Retrive File",this, SLOT(retriveMenuItem()));
        }
    }
    if (theFileData.getFileType() == FileType::DIR)
    {
        fileMenu.addAction("Compress Folder",this, SLOT(compressMenuItem()));
    }
    else if (theFileData.getFileType() == FileType::FILE)
    {
        fileMenu.addAction("De-Compress File",this, SLOT(decompressMenuItem()));
    }

    if ((theFileData.getFileType() == FileType::DIR) || (theFileData.getFileType() == FileType::FILE))
    {
        fileMenu.addSeparator();
        fileMenu.addAction("Refresh Data",this, SLOT(refreshMenuItem()));
        fileMenu.addSeparator();
    }

    fileMenu.exec(QCursor::pos());
}

void ExplorerWindow::copyMenuItem()
{
    SingleLineDialog newNamePopup("Please type a file name to copy to:", "newname");
    if (newNamePopup.exec() != QDialog::Accepted)
    {
        return;
    }

    ui->remoteFileView->getFileOperator()->sendCopyReq(targetNode, newNamePopup.getInputText());
}

void ExplorerWindow::moveMenuItem()
{
    SingleLineDialog newNamePopup("Please type a file name to move to:", "newname");

    if (newNamePopup.exec() != QDialog::Accepted)
    {
        return;
    }

    ui->remoteFileView->getFileOperator()->sendMoveReq(targetNode,newNamePopup.getInputText());
}

void ExplorerWindow::renameMenuItem()
{
    SingleLineDialog newNamePopup("Please type a new file name:", "newname");

    if (newNamePopup.exec() != QDialog::Accepted)
    {
        return;
    }

    ui->remoteFileView->getFileOperator()->sendRenameReq(targetNode, newNamePopup.getInputText());
}

void ExplorerWindow::deleteMenuItem()
{
    if (ui->remoteFileView->getFileOperator()->deletePopup(targetNode))
    {
        ui->remoteFileView->getFileOperator()->sendDeleteReq(targetNode);
    }
}

void ExplorerWindow::uploadMenuItem()
{
    SingleLineDialog uploadNamePopup("Please input full path of file to upload:", "");

    if (uploadNamePopup.exec() != QDialog::Accepted)
    {
        return;
    }
    ui->remoteFileView->getFileOperator()->sendUploadReq(targetNode, uploadNamePopup.getInputText());
}

void ExplorerWindow::createFolderMenuItem()
{
    SingleLineDialog newFolderNamePopup("Please input a name for the new folder:", "newFolder1");

    if (newFolderNamePopup.exec() != QDialog::Accepted)
    {
        return;
    }
    ui->remoteFileView->getFileOperator()->sendCreateFolderReq(targetNode, newFolderNamePopup.getInputText());
}

void ExplorerWindow::downloadMenuItem()
{
    SingleLineDialog downloadNamePopup("Please input full path download destination:", "");

    if (downloadNamePopup.exec() != QDialog::Accepted)
    {
        return;
    }
    ui->remoteFileView->getFileOperator()->sendDownloadReq(targetNode, downloadNamePopup.getInputText());
}

void ExplorerWindow::readMenuItem()
{
    QMessageBox dataPopup;
    dataPopup.setText(QString(*(targetNode->getFileBuffer())));
    dataPopup.exec();
}

void ExplorerWindow::retriveMenuItem()
{
    ui->remoteFileView->getFileOperator()->sendDownloadBuffReq(targetNode);
}

void ExplorerWindow::compressMenuItem()
{
    ui->remoteFileView->getFileOperator()->sendCompressReq(targetNode);
}

void ExplorerWindow::decompressMenuItem()
{
    ui->remoteFileView->getFileOperator()->sendDecompressReq(targetNode);
}

void ExplorerWindow::refreshMenuItem()
{
    ui->remoteFileView->getFileOperator()->enactFolderRefresh(targetNode);
}

void ExplorerWindow::jobRightClickMenu(QPoint)
{
    if (programDriver->getJobHandler() == NULL)
    {
        return;
    }
    QMenu jobMenu;

    jobMenu.addAction("Refresh Job Info", programDriver->getJobHandler(), SLOT(demandJobDataRefresh()));
    jobMenu.exec(QCursor::pos());
}
