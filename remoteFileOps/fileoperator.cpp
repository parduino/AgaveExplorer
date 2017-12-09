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

#include "fileoperator.h"

#include "remotefiletree.h"
#include "filetreenode.h"
#include "easyboollock.h"

#include "../AgaveClientInterface/filemetadata.h"
#include "../AgaveClientInterface/remotedatainterface.h"

#include "../utilFuncs/agavesetupdriver.h"

FileOperator::FileOperator(RemoteDataInterface * newDataLink, AgaveSetupDriver *parent) : QObject( (QObject *)parent)
{
    dataLink = newDataLink;
    myParent = parent;

    //Note: will be deconstructed with parent
    fileOpPending = new EasyBoolLock(this);
}

void FileOperator::linkToFileTree(RemoteFileTree * newTreeLink)
{
    newTreeLink->setModel(&dataStore);
    QObject::connect(&dataStore, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
                     newTreeLink, SLOT(fileEntryTouched(QModelIndex)));
}

void FileOperator::resetFileData()
{
    dataStore.clear();
    dataStore.setColumnCount(tableNumCols);
    dataStore.setHorizontalHeaderLabels(shownHeaderLabelList);

    if (rootFileNode != NULL)
    {
        rootFileNode->deleteLater();
    }
    rootFileNode = new FileTreeNode(NULL, dataStore.invisibleRootItem());

    QObject::connect(rootFileNode, SIGNAL(fileSystemChanged()),
                     this, SLOT(fileNodesChange()));

    enactRootRefresh();
}

void FileOperator::totalResetErrorProcedure()
{
    //TODO: Try to recover once by resetting all data on remote files
    myParent->fatalInterfaceError("Critical Remote file parseing error. Unable to Recover");
}

QString FileOperator::getStringFromInitParams(QString stringKey)
{
    QString ret;
    RemoteDataReply * theReply = qobject_cast<RemoteDataReply *> (QObject::sender());
    if (theReply == NULL)
    {
        myParent->fatalInterfaceError("Unable to get sender object of reply signal");
        return ret;
    }
    ret = theReply->getTaskParamList()->value(stringKey);
    if (ret.isEmpty())
    {
        myParent->fatalInterfaceError("Missing init param");
        return ret;
    }
    return ret;
}

void FileOperator::enactRootRefresh()
{
    qDebug("Enacting refresh of root.");
    RemoteDataReply * theReply = dataLink->remoteLS("/");
    if (theReply == NULL)
    {
        //TODO: consider a more fatal error here
        totalResetErrorProcedure();
        return;
    }

    QObject::connect(theReply, SIGNAL(haveLSReply(RequestState,QList<FileMetaData>*)),
                     this, SLOT(getLSReply(RequestState,QList<FileMetaData>*)));
}

void FileOperator::enactFolderRefresh(FileTreeNode * selectedNode, bool clearData)
{
    if (selectedNode->haveLStask())
    {
        return;
    }
    QString fullFilePath = selectedNode->getFileData().getFullPath();

    qDebug("File Path Needs refresh: %s", qPrintable(fullFilePath));
    RemoteDataReply * theReply = dataLink->remoteLS(fullFilePath);
    if (theReply == NULL)
    {
        //TODO: consider a more fatal error here
        totalResetErrorProcedure();
        return;
    }

    selectedNode->setLStask(theReply, clearData);
}

bool FileOperator::operationIsPending()
{
    return fileOpPending->lockClosed();
}

void FileOperator::sendDeleteReq(FileTreeNode * selectedNode)
{
    if (!fileOpPending->checkAndClaim()) return;

    QString targetFile = selectedNode->getFileData().getFullPath();
    qDebug("Starting delete procedure: %s",qPrintable(targetFile));
    RemoteDataReply * theReply = dataLink->deleteFile(targetFile);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveDeleteReply(RequestState)),
                     this, SLOT(getDeleteReply(RequestState)));
}

void FileOperator::getLSReply(RequestState replyState,QList<FileMetaData> * newFileData)
{
    if (replyState != RequestState::GOOD)
    {
        return;
    }

    rootFileNode->updateFileFolder(newFileData);
}

void FileOperator::getDeleteReply(RequestState replyState)
{
    fileOpPending->release();

    emit fileOpDone(replyState);

    if (replyState != RequestState::GOOD)
    {
        return;
    }

    lsClosestNodeToParent(getStringFromInitParams("toDelete"));
}

void FileOperator::sendMoveReq(FileTreeNode * moveFrom, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;
    dataLink->setCurrentRemoteWorkingDirectory(moveFrom->getFileData().getContainingPath());

    qDebug("Starting move procedure: %s to %s",
           qPrintable(moveFrom->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = dataLink->moveFile(moveFrom->getFileData().getFullPath(), newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveMoveReply(RequestState,FileMetaData*)), this, SLOT(getMoveReply(RequestState,FileMetaData*)));
}

void FileOperator::getMoveReply(RequestState replyState, FileMetaData * revisedFileData)
{
    fileOpPending->release();

    emit fileOpDone(replyState);

    if (replyState != RequestState::GOOD)
    {
        return;
    }

    lsClosestNodeToParent(getStringFromInitParams("from"));
    lsClosestNode(revisedFileData->getFullPath());
}

void FileOperator::sendCopyReq(FileTreeNode * copyFrom, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;
    dataLink->setCurrentRemoteWorkingDirectory(copyFrom->getFileData().getContainingPath());

    qDebug("Starting copy procedure: %s to %s",
           qPrintable(copyFrom->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = dataLink->copyFile(copyFrom->getFileData().getFullPath(), newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO: Better error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveCopyReply(RequestState,FileMetaData*)), this, SLOT(getCopyReply(RequestState,FileMetaData*)));
}

void FileOperator::getCopyReply(RequestState replyState, FileMetaData * newFileData)
{
    fileOpPending->release();

    emit fileOpDone(replyState);

    if (replyState != RequestState::GOOD)
    {
        return;
    }

    lsClosestNode(newFileData->getFullPath());
}

void FileOperator::sendRenameReq(FileTreeNode * selectedNode, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;

    qDebug("Starting rename procedure: %s to %s",
           qPrintable(selectedNode->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = dataLink->renameFile(selectedNode->getFileData().getFullPath(), newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveRenameReply(RequestState,FileMetaData*)), this, SLOT(getRenameReply(RequestState,FileMetaData*)));
}

void FileOperator::getRenameReply(RequestState replyState, FileMetaData * newFileData)
{
    fileOpPending->release();

    emit fileOpDone(replyState);

    if (replyState != RequestState::GOOD)
    {
        return;
    }

    lsClosestNodeToParent(getStringFromInitParams("fullName"));
    lsClosestNodeToParent(newFileData->getFullPath());
}

void FileOperator::sendCreateFolderReq(FileTreeNode * selectedNode, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;

    qDebug("Starting create folder procedure: %s at %s",
           qPrintable(selectedNode->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = dataLink->mkRemoteDir(selectedNode->getFileData().getFullPath(), newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveMkdirReply(RequestState,FileMetaData*)),
                     this, SLOT(getMkdirReply(RequestState,FileMetaData*)));
}

void FileOperator::getMkdirReply(RequestState replyState, FileMetaData * newFolderData)
{
    fileOpPending->release();

    emit fileOpDone(replyState);

    if (replyState != RequestState::GOOD)
    {
        return;
    }

    lsClosestNode(newFolderData->getContainingPath());
}

void FileOperator::sendUploadReq(FileTreeNode * uploadTarget, QString localFile)
{
    if (!fileOpPending->checkAndClaim()) return;
    qDebug("Starting upload procedure: %s to %s", qPrintable(localFile),
           qPrintable(uploadTarget->getFileData().getFullPath()));
    RemoteDataReply * theReply = dataLink->uploadFile(uploadTarget->getFileData().getFullPath(), localFile);
    if (theReply == NULL)
    {
        fileOpPending->release();
        return;
    }
    QObject::connect(theReply, SIGNAL(haveUploadReply(RequestState,FileMetaData*)),
                     this, SLOT(getUploadReply(RequestState,FileMetaData*)));
}

void FileOperator::sendUploadBuffReq(FileTreeNode * uploadTarget, QByteArray fileBuff, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;
    qDebug("Starting upload procedure: to %s", qPrintable(uploadTarget->getFileData().getFullPath()));
    RemoteDataReply * theReply = dataLink->uploadBuffer(uploadTarget->getFileData().getFullPath(), fileBuff, newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        return;
    }
    QObject::connect(theReply, SIGNAL(haveUploadReply(RequestState,FileMetaData*)),
                     this, SLOT(getUploadReply(RequestState,FileMetaData*)));
}

void FileOperator::getUploadReply(RequestState replyState, FileMetaData * newFileData)
{
    fileOpPending->release();

    emit fileOpDone(replyState);

    if (replyState != RequestState::GOOD)
    {
        return;
    }

    lsClosestNode(newFileData->getFullPath());
}

void FileOperator::sendDownloadReq(FileTreeNode * targetFile, QString localDest)
{   
    if (!fileOpPending->checkAndClaim()) return;
    qDebug("Starting download procedure: %s to %s", qPrintable(targetFile->getFileData().getFullPath()),
           qPrintable(localDest));
    RemoteDataReply * theReply = dataLink->downloadFile(localDest, targetFile->getFileData().getFullPath());
    if (theReply == NULL)
    {
        fileOpPending->release();
        return;
    }
    QObject::connect(theReply, SIGNAL(haveDownloadReply(RequestState)),
                     this, SLOT(getDownloadReply(RequestState)));
}

void FileOperator::getDownloadReply(RequestState replyState)
{
    fileOpPending->release();

    emit fileOpDone(replyState);

    if (replyState != RequestState::GOOD)
    {
        quickInfoPopup("Error: Unable to download requested file.");
    }
    else
    {
        quickInfoPopup(QString("Download complete to: %1").arg(getStringFromInitParams("localDest")));
    }
}

void FileOperator::sendDownloadBuffReq(FileTreeNode * targetFile)
{
    if (targetFile->haveBuffTask())
    {
        return;
    }
    qDebug("Starting download buffer procedure: %s", qPrintable(targetFile->getFileData().getFullPath()));
    RemoteDataReply * theReply = dataLink->downloadBuffer(targetFile->getFileData().getFullPath());
    if (theReply == NULL)
    {
        fileOpPending->release();
        return;
    }
    targetFile->setBuffTask(theReply);
}

void FileOperator::sendCompressReq(FileTreeNode * selectedFolder)
{
    if (!fileOpPending->checkAndClaim()) return;
    qDebug("Folder compress specified");
    QMultiMap<QString, QString> oneInput;
    oneInput.insert("compression_type","tgz");
    FileMetaData fileData = selectedFolder->getFileData();
    if (fileData.getFileType() != FileType::DIR)
    {
        fileOpPending->release();
        //TODO: give reasonable error
        return;
    }
    RemoteDataReply * compressTask = dataLink->runRemoteJob("compress",oneInput,fileData.getFullPath());
    if (compressTask == NULL)
    {
        fileOpPending->release();
        //TODO: give reasonable error
        return;
    }
    QObject::connect(compressTask, SIGNAL(haveJobReply(RequestState,QJsonDocument*)),
                     this, SLOT(getCompressReply(RequestState,QJsonDocument*)));
}

void FileOperator::getCompressReply(RequestState finalState, QJsonDocument *)
{
    fileOpPending->release();

    emit fileOpDone(finalState);

    if (finalState != RequestState::GOOD)
    {
        //TODO: give reasonable error
        return;
    }

    //TODO: ask for refresh of relevant containing folder
}

void FileOperator::sendDecompressReq(FileTreeNode * selectedFolder)
{
    if (!fileOpPending->checkAndClaim()) return;
    qDebug("Folder de-compress specified");
    QMultiMap<QString, QString> oneInput;
    FileMetaData fileData = selectedFolder->getFileData();
    if (fileData.getFileType() == FileType::DIR)
    {
        fileOpPending->release();
        //TODO: give reasonable error
        return;
    }
    oneInput.insert("inputFile",fileData.getFullPath());

    RemoteDataReply * decompressTask = dataLink->runRemoteJob("extract",oneInput,"");
    if (decompressTask == NULL)
    {
        fileOpPending->release();
        //TODO: give reasonable error
        return;
    }
    QObject::connect(decompressTask, SIGNAL(haveJobReply(RequestState,QJsonDocument*)),
                     this, SLOT(getDecompressReply(RequestState,QJsonDocument*)));
}

void FileOperator::getDecompressReply(RequestState finalState, QJsonDocument *)
{
    fileOpPending->release();

    emit fileOpDone(finalState);

    if (finalState != RequestState::GOOD)
    {
        //TODO: give reasonable error
        return;
    }

    //TODO: ask for refresh of relevant containing folder
}

void FileOperator::fileNodesChange()
{
    emit fileSystemChange();
}

void FileOperator::lsClosestNode(QString fullPath)
{
    FileTreeNode * nodeToRefresh = rootFileNode->getClosestNodeWithName(fullPath);
    enactFolderRefresh(nodeToRefresh);
}

void FileOperator::lsClosestNodeToParent(QString fullPath)
{
    FileTreeNode * nodeToRefresh = rootFileNode->getNodeWithName(fullPath);
    if (nodeToRefresh != NULL)
    {
        if (!nodeToRefresh->isRootNode())
        {
            nodeToRefresh = nodeToRefresh->getParentNode();
        }
        enactFolderRefresh(nodeToRefresh);
        return;
    }

    nodeToRefresh = rootFileNode->getClosestNodeWithName(fullPath);
    enactFolderRefresh(nodeToRefresh);
}

FileTreeNode * FileOperator::getNodeFromModel(QStandardItem * toFind)
{
    QStandardItem * findParent = toFind->parent();
    if (findParent == NULL)
    {
        findParent = dataStore.invisibleRootItem();
    }

    int colNum = toFind->column();
    if (colNum != 0)
    {
        toFind = findParent->child(toFind->row(), 0);
    }
    QString realPath;
    while (toFind != NULL)
    {
        realPath = realPath.prepend(toFind->text());
        realPath = realPath.prepend("/");

        toFind = toFind->parent();
    }
    return rootFileNode->getNodeWithName(realPath);
}

FileTreeNode * FileOperator::getNodeFromIndex(QModelIndex fileIndex)
{
    QStandardItem * theModelItem = dataStore.itemFromIndex(fileIndex);
    return getNodeFromModel(theModelItem);
}

FileTreeNode * FileOperator::getNodeFromName(QString fullPath)
{
    return rootFileNode->getNodeWithName(fullPath);
}

FileTreeNode * FileOperator::getClosestNodeFromName(QString fullPath)
{
    return rootFileNode->getClosestNodeWithName(fullPath);
}

void FileOperator::quickInfoPopup(QString infoText)
{
    QMessageBox infoMessage;
    infoMessage.setText(infoText);
    infoMessage.setIcon(QMessageBox::Information);
    infoMessage.exec();
}

bool FileOperator::deletePopup(FileTreeNode * toDelete)
{
    QMessageBox deleteQuery;
    deleteQuery.setText(QString("Are you sure you wish to delete the file:\n\n%1").arg(toDelete->getFileData().getFullPath()));
    deleteQuery.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes);
    deleteQuery.setDefaultButton(QMessageBox::Cancel);
    int button = deleteQuery.exec();
    switch (button)
    {
      case QMessageBox::Yes:
          return true;
      default:
          return false;
    }
    return false;
}
