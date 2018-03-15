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
#include "../ae_globals.h"

FileOperator::FileOperator(AgaveSetupDriver *parent) : QObject( (QObject *)parent)
{
    //Note: will be deconstructed with parent
    fileOpPending = new EasyBoolLock(this);
}

FileOperator::~FileOperator()
{
    delete rootFileNode;
}

void FileOperator::linkToFileTree(RemoteFileTree * newTreeLink)
{
    newTreeLink->setModel(&dataStore);
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
    rootFileNode = new FileTreeNode(&dataStore, ae_globals::get_connection()->getUserName(), this);

    QObject::connect(rootFileNode, SIGNAL(fileDataChanged()),
                     this, SLOT(fileNodesChange()));

    enactRootRefresh();
}

void FileOperator::totalResetErrorProcedure()
{
    //TODO: Try to recover once by resetting all data on remote files
    ae_globals::displayFatalPopup("Critical Remote file parseing error. Unable to Recover");
}

QString FileOperator::getStringFromInitParams(QString stringKey)
{
    QString ret;
    RemoteDataReply * theReply = qobject_cast<RemoteDataReply *> (QObject::sender());
    if (theReply == NULL)
    {
        ae_globals::displayFatalPopup("Unable to get sender object of reply signal");
        return ret;
    }
    ret = theReply->getTaskParamList()->value(stringKey);
    if (ret.isEmpty())
    {
        ae_globals::displayFatalPopup("Missing init param");
        return ret;
    }
    return ret;
}

void FileOperator::enactRootRefresh()
{
    qDebug("Enacting refresh of root.");
    RemoteDataReply * theReply = ae_globals::get_connection()->remoteLS("/");
    if (theReply == NULL)
    {
        //TODO: consider a more fatal error here
        totalResetErrorProcedure();
        return;
    }

    rootFileNode->setLStask(theReply);
}

void FileOperator::enactFolderRefresh(FileTreeNode * selectedNode, bool clearData)
{
    if (clearData)
    {
        selectedNode->deleteFolderContentsData();
    }

    if (selectedNode->haveLStask())
    {
        return;
    }
    QString fullFilePath = selectedNode->getFileData().getFullPath();

    qDebug("File Path Needs refresh: %s", qPrintable(fullFilePath));
    RemoteDataReply * theReply = ae_globals::get_connection()->remoteLS(fullFilePath);
    if (theReply == NULL)
    {
        //TODO: consider a more fatal error here
        totalResetErrorProcedure();
        return;
    }

    selectedNode->setLStask(theReply);
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
    RemoteDataReply * theReply = ae_globals::get_connection()->deleteFile(targetFile);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveDeleteReply(RequestState)),
                     this, SLOT(getDeleteReply(RequestState)));
}

void FileOperator::getDeleteReply(RequestState replyState)
{
    fileOpPending->release();

    lsClosestNodeToParent(getStringFromInitParams("toDelete"));
    emit fileOpDone(replyState);
}

void FileOperator::sendMoveReq(FileTreeNode * moveFrom, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;
    ae_globals::get_connection()->setCurrentRemoteWorkingDirectory(moveFrom->getFileData().getContainingPath());

    qDebug("Starting move procedure: %s to %s",
           qPrintable(moveFrom->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->moveFile(moveFrom->getFileData().getFullPath(), newName);
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

    if (replyState == RequestState::GOOD)
    {
        lsClosestNodeToParent(getStringFromInitParams("from"));
        lsClosestNode(revisedFileData->getFullPath());
    }

    emit fileOpDone(replyState);
}

void FileOperator::sendCopyReq(FileTreeNode * copyFrom, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;
    ae_globals::get_connection()->setCurrentRemoteWorkingDirectory(copyFrom->getFileData().getContainingPath());

    qDebug("Starting copy procedure: %s to %s",
           qPrintable(copyFrom->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->copyFile(copyFrom->getFileData().getFullPath(), newName);
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

    if (replyState == RequestState::GOOD)
    {
        lsClosestNode(newFileData->getFullPath());
    }

    emit fileOpDone(replyState);
}

void FileOperator::sendRenameReq(FileTreeNode * selectedNode, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;

    qDebug("Starting rename procedure: %s to %s",
           qPrintable(selectedNode->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->renameFile(selectedNode->getFileData().getFullPath(), newName);
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

    if (replyState == RequestState::GOOD)
    {
        lsClosestNodeToParent(getStringFromInitParams("fullName"));
        lsClosestNodeToParent(newFileData->getFullPath());
    }

    emit fileOpDone(replyState);
}

void FileOperator::sendCreateFolderReq(FileTreeNode * selectedNode, QString newName)
{
    if (!fileOpPending->checkAndClaim()) return;

    qDebug("Starting create folder procedure: %s at %s",
           qPrintable(selectedNode->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->mkRemoteDir(selectedNode->getFileData().getFullPath(), newName);
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

    if (replyState == RequestState::GOOD)
    {
        lsClosestNode(newFolderData->getContainingPath());
    }

    emit fileOpDone(replyState);
}

void FileOperator::sendUploadReq(FileTreeNode * uploadTarget, QString localFile)
{
    if (!fileOpPending->checkAndClaim()) return;
    qDebug("Starting upload procedure: %s to %s", qPrintable(localFile),
           qPrintable(uploadTarget->getFileData().getFullPath()));
    RemoteDataReply * theReply = ae_globals::get_connection()->uploadFile(uploadTarget->getFileData().getFullPath(), localFile);
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
    RemoteDataReply * theReply = ae_globals::get_connection()->uploadBuffer(uploadTarget->getFileData().getFullPath(), fileBuff, newName);
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

    if (replyState == RequestState::GOOD)
    {
        lsClosestNodeToParent(newFileData->getFullPath());
    }

    emit fileOpDone(replyState);
}

void FileOperator::sendDownloadReq(FileTreeNode * targetFile, QString localDest)
{   
    if (!fileOpPending->checkAndClaim()) return;
    qDebug("Starting download procedure: %s to %s", qPrintable(targetFile->getFileData().getFullPath()),
           qPrintable(localDest));
    RemoteDataReply * theReply = ae_globals::get_connection()->downloadFile(localDest, targetFile->getFileData().getFullPath());
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
    RemoteDataReply * theReply = ae_globals::get_connection()->downloadBuffer(targetFile->getFileData().getFullPath());
    if (theReply == NULL)
    {
        return;
    }
    targetFile->setBuffTask(theReply);
}

bool FileOperator::performingRecursiveDownload()
{
    return (currentRecursiveTask == FileOp_RecursiveTask::DOWNLOAD);
}

void FileOperator::enactRecursiveDownload(FileTreeNode * targetFolder, QString containingDestFolder)
{
    if (!fileOpPending->checkAndClaim()) return;
    if (currentRecursiveTask != FileOp_RecursiveTask::NONE) return;

    if (!targetFolder->isFolder())
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Only folders can be downloaded recursively.");
        return;
    }

    QDir downloadParent(containingDestFolder);

    if (!downloadParent.exists())
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Download destination does not exist.");
        return;
    }

    if (downloadParent.exists(targetFolder->getFileData().getFileName()))
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Download destination already occupied.");
        return;
    }

    if (!downloadParent.mkdir(targetFolder->getFileData().getFileName()))
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Unable to create local destination for download, please check that you have permissions to write to the specified folder.");
        return;
    }
    recursiveLocalHead = downloadParent;
    if (!recursiveLocalHead.cd(targetFolder->getFileData().getFileName()))
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Unable to create local destination for download, please check that you have permissions to write to the specified folder.");
        return;
    }

    recursiveRemoteHead = targetFolder;
    currentRecursiveTask = FileOp_RecursiveTask::DOWNLOAD;
    recursiveDownloadProcessRetry();
}

bool FileOperator::performingRecursiveUpload()
{
    return (currentRecursiveTask == FileOp_RecursiveTask::UPLOAD);
}

void FileOperator::enactRecursiveUpload(FileTreeNode * containingDestFolder, QString localFolderToCopy)
{
    if (!fileOpPending->checkAndClaim()) return;
    if (currentRecursiveTask != FileOp_RecursiveTask::NONE) return;

    if (recursivefileOpPending->lockClosed())
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Still cleaning up tasks from last upload attempt. Please Wait.");
        return;
    }

    recursiveLocalHead = QDir(localFolderToCopy);
    if (!recursiveLocalHead.exists())
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: The folder to upload does not exist.");
        return;
    }

    if (!recursiveLocalHead.isReadable())
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Unable to read from local folder to upload, please check that you have permissions to read the specified folder.");
        return;
    }

    if (recursiveLocalHead.dirName().isEmpty())
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Cannot upload unnamed or root folders.");
        return;
    }

    if (!containingDestFolder->isFolder())
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: The destination for an upload must be a folder.");
        return;
    }

    if (containingDestFolder->getNodeState() != NodeState::FOLDER_CONTENTS_LOADED)
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: The destination for an upload must be fully loaded.");
        return;
    }

    if (containingDestFolder->getChildNodeWithName(recursiveLocalHead.dirName()) != NULL)
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: The destination for the upload is already occupied.");
        return;
    }

    speculateNodeWithName(recursiveRemoteHead, recursiveLocalHead.dirName(), true);

    recursiveRemoteHead = containingDestFolder->getChildNodeWithName(recursiveLocalHead.dirName());
    if (recursiveRemoteHead == NULL)
    {
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Internal Error, please consult developers to fix this.");
        return;
    }

    currentRecursiveTask = FileOp_RecursiveTask::UPLOAD;
    recursiveUploadProcessRetry();
}

void FileOperator::abortRecursiveProcess()
{
    QString toDisplay = "Internal ERROR";
    if (currentRecursiveTask == FileOp_RecursiveTask::NONE) return;

    if (currentRecursiveTask == FileOp_RecursiveTask::DOWNLOAD)
    {
        toDisplay = "Folder download stopped by user.";
    }
    else if (currentRecursiveTask == FileOp_RecursiveTask::UPLOAD)
    {
        toDisplay = "Folder upload stopped by user.";
    }

    currentRecursiveTask = FileOp_RecursiveTask::NONE;
    fileOpPending->release();
    emit recursiveProcessFinished(false, toDisplay);
    return;
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
    RemoteDataReply * compressTask = ae_globals::get_connection()->runRemoteJob("compress",oneInput,fileData.getFullPath());
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

    //TODO: ask for refresh of relevant containing folder, after finishing job
    emit fileOpDone(finalState);

    if (finalState != RequestState::GOOD)
    {
        //TODO: give reasonable error
    }
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

    RemoteDataReply * decompressTask = ae_globals::get_connection()->runRemoteJob("extract",oneInput,"");
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

    //TODO: ask for refresh of relevant containing folder, after finishing job
    emit fileOpDone(finalState);

    if (finalState != RequestState::GOOD)
    {
        //TODO: give reasonable error
    }
}

void FileOperator::fileNodesChange()
{
    emit fileSystemChange();

    if (performingRecursiveDownload())
    {
        recursiveDownloadProcessRetry();
    }
    else if (performingRecursiveUpload())
    {
        recursiveUploadProcessRetry();
    }
}

void FileOperator::getRecursiveUploadReply(RequestState replyState, FileMetaData * newFileData)
{
    recursivefileOpPending->release();

    if (replyState == RequestState::NO_CONNECT)
    {
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Network connection lost during folder upload.");
        return;
    }

    if (replyState != RequestState::GOOD)
    {
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Folder upload failed to upload file.");
        return;
    }
    lsClosestNodeToParent(newFileData->getFullPath());
}

void FileOperator::getRecursiveMkdirReply(RequestState replyState, FileMetaData * newFolderData)
{
    recursivefileOpPending->release();

    if (replyState == RequestState::NO_CONNECT)
    {
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Network connection lost during folder upload.");
        return;
    }

    if (replyState != RequestState::GOOD)
    {
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emit recursiveProcessFinished(false, "ERROR: Folder upload failed to create new remote folder.");
        return;
    }
    lsClosestNode(newFolderData->getContainingPath());
}

void FileOperator::lsClosestNode(QString fullPath, bool clearData)
{
    FileTreeNode * nodeToRefresh = rootFileNode->getClosestNodeWithName(fullPath);
    enactFolderRefresh(nodeToRefresh, clearData);
}

void FileOperator::lsClosestNodeToParent(QString fullPath, bool clearData)
{
    FileTreeNode * nodeToRefresh = rootFileNode->getNodeWithName(fullPath);
    if (nodeToRefresh != NULL)
    {
        if (!nodeToRefresh->isRootNode())
        {
            nodeToRefresh = nodeToRefresh->getParentNode();
        }
        enactFolderRefresh(nodeToRefresh, clearData);
        return;
    }

    nodeToRefresh = rootFileNode->getClosestNodeWithName(fullPath);
    enactFolderRefresh(nodeToRefresh);
}

FileTreeNode * FileOperator::getNodeFromName(QString fullPath)
{
    return rootFileNode->getNodeWithName(fullPath);
}

FileTreeNode * FileOperator::getClosestNodeFromName(QString fullPath)
{
    return rootFileNode->getClosestNodeWithName(fullPath);
}

FileTreeNode * FileOperator::speculateNodeWithName(QString fullPath, bool folder)
{
    FileTreeNode * scanNode = rootFileNode->getNodeWithName(fullPath);
    if (scanNode != NULL)
    {
        return scanNode;
    }
    scanNode = rootFileNode->getClosestNodeWithName(fullPath);
    if (scanNode == NULL)
    {
        return NULL;
    }
    QStringList fullPathParts = FileMetaData::getPathNameList(fullPath);
    QStringList scanPathParts = FileMetaData::getPathNameList(scanNode->getFileData().getFullPath());

    int accountedParts = scanPathParts.size();
    QString pathSoFar = "";

    for (auto itr = fullPathParts.cbegin(); itr != fullPathParts.cend(); itr++)
    {
        if (accountedParts <= 0)
        {
            pathSoFar = pathSoFar.append("/");
            pathSoFar = pathSoFar.append(*itr);
        }
        else
        {
            accountedParts--;
        }
    }
    return speculateNodeWithName(scanNode, pathSoFar, folder);
}

FileTreeNode * FileOperator::speculateNodeWithName(FileTreeNode * baseNode, QString addedPath, bool folder)
{
    FileTreeNode * searchNode = baseNode;
    QStringList pathParts = FileMetaData::getPathNameList(addedPath);
    for (auto itr = pathParts.cbegin(); itr != pathParts.cend(); itr++)
    {
        FileTreeNode * nextNode = searchNode->getChildNodeWithName(*itr);
        if (nextNode != NULL)
        {
            searchNode = nextNode;
            continue;
        }
        if (!searchNode->isFolder())
        {
            qDebug("Invalid file speculation path.");
            return NULL;
        }
        if (searchNode->getNodeState() == NodeState::FOLDER_CONTENTS_LOADED)
        {
            //Speculation failed, file known not to exist
            return NULL;
        }

        FileMetaData newFolderData;
        QString newPath = searchNode->getFileData().getFullPath();
        newPath = newPath.append("/");
        newPath = newPath.append(*itr);
        newFolderData.setFullFilePath(newPath);
        newFolderData.setType(FileType::DIR);
        if ((itr + 1 == pathParts.cend()) && (folder == false))
        {
            newFolderData.setType(FileType::FILE);
        }
        newFolderData.setSize(0);
        nextNode = new FileTreeNode(newFolderData, searchNode);
        if ((searchNode->getNodeState() == NodeState::FOLDER_KNOWN_CONTENTS_NOT) ||
               (searchNode->getNodeState() == NodeState::FOLDER_SPECULATE_IDLE))
        {
            enactFolderRefresh(searchNode);
        }

        searchNode = nextNode;
    }

    if (folder)
    {
        enactFolderRefresh(searchNode);
    }
    else
    {
        sendDownloadBuffReq(searchNode);
    }

    return searchNode;
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

void FileOperator::recursiveDownloadProcessRetry()
{
    if (recursiveDownloadRetrivalHelper(recursiveRemoteHead))
    {
        QString outText = "INTERNAL ERROR";
        RecursiveErrorCodes errNum = RecursiveErrorCodes::NONE;
        bool success = recursiveDownloadFolderEmitHelper(recursiveLocalHead, recursiveRemoteHead, errNum);
        if (success)
        {
            outText = "Remote folder downloaded.";
        }
        else
        {
            if (errNum == RecursiveErrorCodes::LOST_FILE)
            {
                outText = "Internal Error: File entry missing in downloaded data. Files may have changed outside of program.";
            }
            else if (errNum == RecursiveErrorCodes::TYPE_MISSMATCH)
            {
                outText = "Internal Error: Type Mismatch in downloaded data. Files may have changed outside of program.";
            }
            else
            {
                outText = "ERROR: Unable to write local files for download, please check that you have permissions to write to the specified folder.";
            }
        }
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emit recursiveProcessFinished(success, outText);
        return;
    }
}

bool FileOperator::recursiveDownloadRetrivalHelper(FileTreeNode * nodeToCheck)
{
    if (nodeToCheck->isFile())
    {
        if (nodeToCheck->getFileBuffer() == NULL)
        {
            sendDownloadBuffReq(nodeToCheck);
            return false;
        }
        return true;
    }

    if (!nodeToCheck->isFolder()) return true; //For now, we only copy files and folders

    bool foundAll = true;

    if (nodeToCheck->getNodeState() != NodeState::FOLDER_CONTENTS_LOADED)
    {
        foundAll = false;
        enactFolderRefresh(nodeToCheck);
    }

    for (FileTreeNode * aChild : nodeToCheck->getChildList())
    {
        if (!recursiveDownloadRetrivalHelper(aChild))
        {
            foundAll = false;
        }
    }

    return foundAll;
}

bool FileOperator::recursiveDownloadFolderEmitHelper(QDir currentLocalDir, FileTreeNode *nodeToGet, RecursiveErrorCodes &errNum)
{
    if (!nodeToGet->isFolder())
    {
        errNum = RecursiveErrorCodes::TYPE_MISSMATCH;
        return false;
    }
    if (!currentLocalDir.exists())
    {
        errNum = RecursiveErrorCodes::LOST_FILE;
        return false;
    }
    for (FileTreeNode * aChild : nodeToGet->getChildList())
    {
        if (aChild->getFileData().getFileType() == FileType::DIR)
        {
            if (!currentLocalDir.mkdir(aChild->getFileData().getFileName())) return false;
            QDir newFolder = currentLocalDir;
            newFolder.cd(aChild->getFileData().getFileName());
            if (!newFolder.exists()) return false;
            if (!recursiveDownloadFolderEmitHelper(newFolder, aChild, errNum)) return false;
        }
        else if (aChild->getFileData().getFileType() == FileType::FILE)
        {
            if (!emitBufferToFile(currentLocalDir, aChild, errNum)) return false;
        }
    }

    return true;
}

bool FileOperator::emitBufferToFile(QDir containingDir, FileTreeNode * nodeToGet, RecursiveErrorCodes &errNum)
{
    if (!nodeToGet->isFile())
    {
        errNum = RecursiveErrorCodes::TYPE_MISSMATCH;
        return false;
    }

    if (!containingDir.exists())
    {
        errNum = RecursiveErrorCodes::LOST_FILE;
        return false;
    }

    if (containingDir.exists(nodeToGet->getFileData().getFileName())) return false;

    QFile newFile(containingDir.absoluteFilePath(nodeToGet->getFileData().getFileName()));
    if (nodeToGet->getFileBuffer() == NULL) return false;
    if (!newFile.open(QFile::WriteOnly)) return false;
    if (newFile.write(*(nodeToGet->getFileBuffer())) < 0) return false;
    newFile.close();

    return true;
}

void FileOperator::recursiveUploadProcessRetry()
{
    if (recursivefileOpPending->lockClosed()) return;

    RecursiveErrorCodes theError = RecursiveErrorCodes::NONE;

    if (recursiveUploadHelper(recursiveRemoteHead, recursiveLocalHead, theError))
    {
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emit recursiveProcessFinished(true, "Folder uploaded.");
        return;
    }

    if (theError == RecursiveErrorCodes::NONE) return;

    currentRecursiveTask = FileOp_RecursiveTask::NONE;
    fileOpPending->release();
    if (theError == RecursiveErrorCodes::MKDIR_FAIL)
    {
        emit recursiveProcessFinished(true, "Create folder operation failed during recursive upload. Check your network connection and try again.");
        return;
    }

    if (theError == RecursiveErrorCodes::UPLOAD_FAIL)
    {
        emit recursiveProcessFinished(true, "File upload operation failed during recursive upload. Check your network connection and try again.");
        return;
    }

    if (theError == RecursiveErrorCodes::TYPE_MISSMATCH)
    {
        emit recursiveProcessFinished(true, "Internal error. File type mismatch. Remote files may be being accessed outside of this program.");
        return;
    }
}

bool FileOperator::recursiveUploadHelper(FileTreeNode * nodeToSend, QDir localPath, RecursiveErrorCodes &errNum)
{
    errNum = RecursiveErrorCodes::NONE;
    if (recursivefileOpPending->lockClosed()) return false;

    if (!nodeToSend->isFolder())
    {
        errNum = RecursiveErrorCodes::TYPE_MISSMATCH;
        return false;
    }

    if (nodeToSend->getNodeState() != NodeState::FOLDER_CONTENTS_LOADED)
    {
        enactFolderRefresh(nodeToSend);
        return false;
    }

    for (QFileInfo anEntry : localPath.entryInfoList())
    {
        if (anEntry.isDir())
        {
            QDir childDir = anEntry.dir();
            FileTreeNode * childNode = nodeToSend->getChildNodeWithName(childDir.dirName());
            if (childNode == NULL)
            {
                if (!sendRecursiveCreateFolderReq(nodeToSend, childDir.dirName()))
                {
                    errNum = RecursiveErrorCodes::MKDIR_FAIL;
                }
                return false;
            }
            if (!recursiveUploadHelper(childNode, childDir, errNum)) return false;
        }
        else if (anEntry.isFile())
        {
            FileTreeNode * childNode = nodeToSend->getChildNodeWithName(anEntry.fileName());
            if (childNode == NULL)
            {
                if (!sendRecursiveUploadReq(nodeToSend, anEntry.absoluteFilePath()))
                {
                    errNum = RecursiveErrorCodes::UPLOAD_FAIL;
                }
                return false;
            }
            if (!childNode->isFile())
            {
                errNum = RecursiveErrorCodes::TYPE_MISSMATCH;
                return false;
            }
        }
    }

    return true;
}

bool FileOperator::sendRecursiveCreateFolderReq(FileTreeNode * selectedNode, QString newName)
{
    if (!recursivefileOpPending->checkAndClaim()) return false;

    qDebug("Starting Recursive mkdir procedure: %s at %s",
           qPrintable(selectedNode->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->mkRemoteDir(selectedNode->getFileData().getFullPath(), newName);
    if (theReply == NULL)
    {
        recursivefileOpPending->release();
        return false;
    }
    QObject::connect(theReply, SIGNAL(haveMkdirReply(RequestState,FileMetaData*)),
                     this, SLOT(getRecursiveMkdirReply(RequestState,FileMetaData*)));
    return true;
}

bool FileOperator::sendRecursiveUploadReq(FileTreeNode * uploadTarget, QString localFile)
{
    if (!recursivefileOpPending->checkAndClaim()) return false;
    qDebug("Starting recursively enacted upload procedure: %s to %s", qPrintable(localFile),
           qPrintable(uploadTarget->getFileData().getFullPath()));
    RemoteDataReply * theReply = ae_globals::get_connection()->uploadFile(uploadTarget->getFileData().getFullPath(), localFile);
    if (theReply == NULL)
    {
        recursivefileOpPending->release();
        return false;
    }
    QObject::connect(theReply, SIGNAL(haveUploadReply(RequestState,FileMetaData*)),
                     this, SLOT(getRecursiveUploadReply(RequestState,FileMetaData*)));
    return true;
}
