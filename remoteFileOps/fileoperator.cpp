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

#include "../AgaveExplorer/remoteModelViews/remotefiletree.h"
#include "filetreenode.h"
#include "easyboollock.h"
#include "filenoderef.h"

#include "../AgaveClientInterface/filemetadata.h"
#include "../AgaveClientInterface/remotedatainterface.h"

#include "../utilFuncs/agavesetupdriver.h"
#include "../ae_globals.h"

FileOperator::FileOperator(AgaveSetupDriver *parent) : QObject( (QObject *)parent)
{
    //Note: will be deconstructed with parent
    //TODO: Replace these with mutexes when implementing threading
    fileOpPending = new EasyBoolLock(this);
    recursivefileOpPending = new EasyBoolLock(this);
}

FileOperator::~FileOperator()
{
    delete rootFileNode;
}

void FileOperator::resetFileData()
{
    if (rootFileNode != NULL)
    {
        rootFileNode->deleteLater();
    }
    rootFileNode = new FileTreeNode(ae_globals::get_connection()->getUserName(), this);

    enactRootRefresh();
}

void FileOperator::totalResetErrorProcedure()
{
    //TODO: Try to recover once by resetting all data on remote files
    ae_globals::displayFatalPopup("Critical Remote file parseing error. Unable to Recover");
}

FileTreeNode * FileOperator::getFileNodeFromNodeRef(const FileNodeRef &thedata, bool verifyTimestamp)
{
    if (thedata.isNil()) return NULL;
    FileTreeNode * ret = rootFileNode->getNodeWithName(thedata.getFullPath());
    if (ret == NULL) return NULL;

    if (!verifyTimestamp) return ret;

    if (ret->getFileData().getTimestamp() != thedata.getTimestamp()) return NULL;
    return ret;
}

void FileOperator::enactRootRefresh()
{
    qCDebug(agaveAppLayer, "Enacting refresh of root.");
    RemoteDataReply * theReply = ae_globals::get_connection()->remoteLS("/");
    if (theReply == NULL)
    {
        //TODO: consider a more fatal error here
        totalResetErrorProcedure();
        return;
    }

    rootFileNode->setLStask(theReply);
}

void FileOperator::enactFolderRefresh(const FileNodeRef &selectedNode, bool clearData)
{
    FileTreeNode * trueNode = getFileNodeFromNodeRef(selectedNode);
    if (trueNode == NULL) return;
    if (clearData)
    {
        trueNode->deleteFolderContentsData();
    }

    if (trueNode->haveLStask())
    {
        return;
    }
    QString fullFilePath = trueNode->getFileData().getFullPath();

    qCDebug(agaveAppLayer, "File Path Needs refresh: %s", qPrintable(fullFilePath));
    RemoteDataReply * theReply = ae_globals::get_connection()->remoteLS(fullFilePath);
    if (theReply == NULL)
    {
        //TODO: consider a more fatal error here
        totalResetErrorProcedure();
        return;
    }

    trueNode->setLStask(theReply);
}

bool FileOperator::operationIsPending()
{
    return fileOpPending->lockClosed();
}

void FileOperator::sendDeleteReq(const FileNodeRef &selectedNode)
{
    if (!selectedNode.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;

    QString targetFile = selectedNode.getFullPath();
    qCDebug(agaveAppLayer, "Starting delete procedure: %s",qPrintable(targetFile));
    RemoteDataReply * theReply = ae_globals::get_connection()->deleteFile(targetFile);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveDeleteReply(RequestState, QString)),
                     this, SLOT(getDeleteReply(RequestState, QString)));
    emit fileOpStarted();
}

void FileOperator::getDeleteReply(RequestState replyState, QString toDelete)
{
    fileOpPending->release();

    if (replyState == RequestState::GOOD)
    {
        lsClosestNodeToParent(toDelete);
        emit fileOpDone(replyState, QString("File successfully deleted: %1").arg(toDelete));
    }
    else
    {
        emit fileOpDone(replyState, QString("Unable to delete file: %1").arg(RemoteDataInterface::interpretRequestState(replyState)));
    }
}

void FileOperator::sendMoveReq(const FileNodeRef &moveFrom, QString newName)
{
    if (!moveFrom.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;

    qCDebug(agaveAppLayer, "Starting move procedure: %s to %s",
            qPrintable(moveFrom.getFullPath()),
            qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->moveFile(moveFrom.getFullPath(), newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveMoveReply(RequestState,FileMetaData, QString)),
                     this, SLOT(getMoveReply(RequestState,FileMetaData, QString)));
    emit fileOpStarted();
}

void FileOperator::getMoveReply(RequestState replyState, FileMetaData revisedFileData, QString from)
{
    fileOpPending->release();

    if (replyState == RequestState::GOOD)
    {
        lsClosestNodeToParent(from);
        lsClosestNode(revisedFileData.getFullPath());
        emit fileOpDone(replyState, QString("File successfully moved from: %1 to: %2")
                        .arg(from)
                        .arg(revisedFileData.getFullPath()));
    }
    else
    {
        emitStdFileOpErr("Unable to move file", replyState);
    }
}

void FileOperator::sendCopyReq(const FileNodeRef &copyFrom, QString newName)
{
    if (!copyFrom.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;

    qCDebug(agaveAppLayer, "Starting copy procedure: %s to %s",
           qPrintable(copyFrom.getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->copyFile(copyFrom.getFullPath(), newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO: Better error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveCopyReply(RequestState,FileMetaData)),
                     this, SLOT(getCopyReply(RequestState,FileMetaData)));
    emit fileOpStarted();
}

void FileOperator::getCopyReply(RequestState replyState, FileMetaData newFileData)
{
    fileOpPending->release();

    if (replyState == RequestState::GOOD)
    {
        lsClosestNode(newFileData.getFullPath());
        emit fileOpDone(replyState, QString("File successfully copied: %1").arg(newFileData.getFullPath()));
    }
    else
    {
        emitStdFileOpErr("Unable to copy file", replyState);
    }
}

void FileOperator::sendRenameReq(const FileNodeRef &selectedNode, QString newName)
{
    if (!selectedNode.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;

    qCDebug(agaveAppLayer, "Starting rename procedure: %s to %s",
           qPrintable(selectedNode.getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->renameFile(selectedNode.getFullPath(), newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveRenameReply(RequestState,FileMetaData, QString)),
                     this, SLOT(getRenameReply(RequestState,FileMetaData, QString)));
    emit fileOpStarted();
}

void FileOperator::getRenameReply(RequestState replyState, FileMetaData newFileData, QString oldName)
{
    fileOpPending->release();

    if (replyState == RequestState::GOOD)
    {
        lsClosestNodeToParent(oldName);
        lsClosestNodeToParent(newFileData.getFullPath());
        emit fileOpDone(replyState, QString("File successfully renamed from %1 to %2")
                        .arg(oldName)
                        .arg(newFileData.getFullPath()));
    }
    else
    {
        emitStdFileOpErr("Unable to rename file", replyState);
    }
}

void FileOperator::sendCreateFolderReq(const FileNodeRef &selectedNode, QString newName)
{
    if (!selectedNode.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return; 

    qCDebug(agaveAppLayer,"Starting create folder procedure: %s at %s",
           qPrintable(selectedNode.getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->mkRemoteDir(selectedNode.getFullPath(), newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        //TODO, should have more meaningful error here
        return;
    }
    QObject::connect(theReply, SIGNAL(haveMkdirReply(RequestState,FileMetaData)),
                     this, SLOT(getMkdirReply(RequestState,FileMetaData)));
    emit fileOpStarted();
}

void FileOperator::getMkdirReply(RequestState replyState, FileMetaData newFolderData)
{
    fileOpPending->release();

    if (replyState == RequestState::GOOD)
    {
        lsClosestNode(newFolderData.getContainingPath());
        emit fileOpDone(replyState, QString("New Folder Created at %1")
                        .arg(newFolderData.getFullPath()));
    }
    else
    {
        emitStdFileOpErr("Unable to create remote folder", replyState);
    }
}

void FileOperator::sendUploadReq(const FileNodeRef &uploadTarget, QString localFile)
{
    if (!uploadTarget.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;

    qCDebug(agaveAppLayer, "Starting upload procedure: %s to %s", qPrintable(localFile),
           qPrintable(uploadTarget.getFullPath()));
    RemoteDataReply * theReply = ae_globals::get_connection()->uploadFile(uploadTarget.getFullPath(), localFile);
    if (theReply == NULL)
    {
        fileOpPending->release();
        return;
    }
    QObject::connect(theReply, SIGNAL(haveUploadReply(RequestState,FileMetaData)),
                     this, SLOT(getUploadReply(RequestState,FileMetaData)));
    emit fileOpStarted();
}

void FileOperator::sendUploadBuffReq(const FileNodeRef &uploadTarget, QByteArray fileBuff, QString newName)
{
    if (!uploadTarget.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;
    qCDebug(agaveAppLayer, "Starting upload procedure: to %s", qPrintable(uploadTarget.getFullPath()));
    RemoteDataReply * theReply = ae_globals::get_connection()->uploadBuffer(uploadTarget.getFullPath(), fileBuff, newName);
    if (theReply == NULL)
    {
        fileOpPending->release();
        return;
    }
    QObject::connect(theReply, SIGNAL(haveUploadReply(RequestState,FileMetaData)),
                     this, SLOT(getUploadReply(RequestState,FileMetaData)));
    emit fileOpStarted();
}

void FileOperator::getUploadReply(RequestState replyState, FileMetaData newFileData)
{
    fileOpPending->release();

    if (replyState == RequestState::GOOD)
    {
        lsClosestNodeToParent(newFileData.getFullPath());
        emit fileOpDone(replyState, QString("File successfully uploaded to %1")
                        .arg(newFileData.getFullPath()));
    }
    else
    {
        emitStdFileOpErr("Unable to upload file", replyState);
    }
}

void FileOperator::sendDownloadReq(const FileNodeRef &targetFile, QString localDest)
{   
    if (!targetFile.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;
    qCDebug(agaveAppLayer, "Starting download procedure: %s to %s", qPrintable(targetFile.getFullPath()),
           qPrintable(localDest));
    RemoteDataReply * theReply = ae_globals::get_connection()->downloadFile(localDest, targetFile.getFullPath());
    if (theReply == NULL)
    {
        fileOpPending->release();
        return;
    }
    QObject::connect(theReply, SIGNAL(haveDownloadReply(RequestState, QString)),
                     this, SLOT(getDownloadReply(RequestState, QString)));
    emit fileOpStarted();
}

void FileOperator::getDownloadReply(RequestState replyState, QString localDest)
{
    fileOpPending->release();

    if (replyState == RequestState::GOOD)
    {
        emit fileOpDone(replyState, QString("Download complete to %1")
                        .arg(localDest));
    }
    else
    {
        emitStdFileOpErr("Unable to download requested file", replyState);
    }
}

void FileOperator::sendDownloadBuffReq(const FileNodeRef &targetFile)
{
    if (!targetFile.fileNodeExtant()) return;
    FileTreeNode * trueNode = getFileNodeFromNodeRef(targetFile);
    if (trueNode->haveBuffTask())
    {
        return;
    }
    qCDebug(agaveAppLayer, "Starting download buffer procedure: %s", qPrintable(targetFile.getFullPath()));
    RemoteDataReply * theReply = ae_globals::get_connection()->downloadBuffer(targetFile.getFullPath());
    if (theReply == NULL)
    {
        return;
    }
    trueNode->setBuffTask(theReply);
}

bool FileOperator::performingRecursiveDownload()
{
    return (currentRecursiveTask == FileOp_RecursiveTask::DOWNLOAD);
}

void FileOperator::enactRecursiveDownload(const FileNodeRef &targetFolder, QString containingDestFolder)
{
    if (!targetFolder.fileNodeExtant()) return;
    if (currentRecursiveTask != FileOp_RecursiveTask::NONE) return;
    if (!fileOpPending->checkAndClaim()) return;

    if (targetFolder.getFileType() != FileType::DIR)
    {
        fileOpPending->release();
        emit fileOpDone(RequestState::INVALID_PARAM, "ERROR: Only folders can be downloaded recursively.");
        return;
    }

    QDir downloadParent(containingDestFolder);

    if (!downloadParent.exists())
    {
        fileOpPending->release();
        emit fileOpDone(RequestState::LOCAL_FILE_ERROR, "ERROR: Download destination does not exist.");
        return;
    }

    if (downloadParent.exists(targetFolder.getFileName()))
    {
        fileOpPending->release();
        emit fileOpDone(RequestState::LOCAL_FILE_ERROR, "ERROR: Download destination already occupied.");
        return;
    }

    if (!downloadParent.mkdir(targetFolder.getFileName()))
    {
        fileOpPending->release();
        emit fileOpDone(RequestState::LOCAL_FILE_ERROR, "ERROR: Unable to create local destination for download, please check that you have permissions to write to the specified folder.");
        return;
    }
    recursiveLocalHead = downloadParent;
    if (!recursiveLocalHead.cd(targetFolder.getFileName()))
    {
        fileOpPending->release();
        emit fileOpDone(RequestState::LOCAL_FILE_ERROR, "ERROR: Unable to create local destination for download, please check that you have permissions to write to the specified folder.");
        return;
    }

    recursiveRemoteHead = getFileNodeFromNodeRef(targetFolder);
    currentRecursiveTask = FileOp_RecursiveTask::DOWNLOAD;
    emit fileOpStarted();
    recursiveDownloadProcessRetry();
}

bool FileOperator::performingRecursiveUpload()
{
    return (currentRecursiveTask == FileOp_RecursiveTask::UPLOAD);
}

void FileOperator::enactRecursiveUpload(const FileNodeRef &containingDestFolder, QString localFolderToCopy)
{
    if (!containingDestFolder.fileNodeExtant()) return;
    if (currentRecursiveTask != FileOp_RecursiveTask::NONE) return;
    if (!fileOpPending->checkAndClaim()) return;

    if (recursivefileOpPending->lockClosed())
    {
        fileOpPending->release();
        fileOpDone(RequestState::NOT_READY, "ERROR: Still cleaning up tasks from last upload attempt. Please Wait.");
        return;
    }

    recursiveLocalHead = QDir(localFolderToCopy);
    if (!recursiveLocalHead.exists())
    {
        fileOpPending->release();
        fileOpDone(RequestState::INVALID_PARAM, "ERROR: The folder to upload does not exist.");
        return;
    }

    if (!recursiveLocalHead.isReadable())
    {
        fileOpPending->release();
        fileOpDone(RequestState::LOCAL_FILE_ERROR, "ERROR: Unable to read from local folder to upload, please check that you have permissions to read the specified folder.");
        return;
    }

    if (recursiveLocalHead.dirName().isEmpty())
    {
        fileOpPending->release();
        fileOpDone(RequestState::INVALID_PARAM, "ERROR: Cannot upload unnamed or root folders.");
        return;
    }

    if (containingDestFolder.getFileType() != FileType::DIR)
    {
        fileOpPending->release();
        fileOpDone(RequestState::INVALID_PARAM, "ERROR: The destination for an upload must be a folder.");
        return;
    }

    if (!containingDestFolder.folderContentsLoaded())
    {
        fileOpPending->release();
        fileOpDone(RequestState::INVALID_PARAM, "ERROR: The destination for an upload must be fully loaded.");
        return;
    }

    if (!containingDestFolder.getChildWithName(recursiveLocalHead.dirName()).isNil())
    {
        fileOpPending->release();
        fileOpDone(RequestState::INVALID_PARAM, "ERROR: The destination for the upload is already occupied.");
        return;
    }

    recursiveRemoteHead = getFileNodeFromNodeRef(containingDestFolder);

    currentRecursiveTask = FileOp_RecursiveTask::UPLOAD;
    emit fileOpStarted();
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
    fileOpDone(RequestState::STOPPED_BY_USER, toDisplay);
    return;
}

void FileOperator::sendCompressReq(const FileNodeRef &selectedFolder)
{
    if (!selectedFolder.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;
    qCDebug(agaveAppLayer, "Folder compress specified");
    QMultiMap<QString, QString> oneInput;
    oneInput.insert("compression_type","tgz");

    if (selectedFolder.getFileType() != FileType::DIR)
    {
        fileOpPending->release();
        //TODO: give reasonable error
        return;
    }
    RemoteDataReply * compressTask = ae_globals::get_connection()->runRemoteJob("compress",oneInput,selectedFolder.getFullPath());
    if (compressTask == NULL)
    {
        fileOpPending->release();
        //TODO: give reasonable error
        return;
    }
    QObject::connect(compressTask, SIGNAL(haveJobReply(RequestState,QJsonDocument)),
                     this, SLOT(getCompressReply(RequestState,QJsonDocument)));
    emit fileOpStarted();
}

void FileOperator::getCompressReply(RequestState finalState, QJsonDocument)
{
    fileOpPending->release();

    //TODO: ask for refresh of relevant containing folder, after finishing job
    emit fileOpDone(finalState, "compress enacted");

    if (finalState != RequestState::GOOD)
    {
        //TODO: give reasonable error
    }
}

void FileOperator::sendDecompressReq(const FileNodeRef &selectedFolder)
{
    if (!selectedFolder.fileNodeExtant()) return;
    if (!fileOpPending->checkAndClaim()) return;
    qCDebug(agaveAppLayer, "Folder de-compress specified");
    QMultiMap<QString, QString> oneInput;

    if (selectedFolder.getFileType() == FileType::DIR)
    {
        fileOpPending->release();
        //TODO: give reasonable error
        return;
    }
    oneInput.insert("inputFile",selectedFolder.getFullPath());

    RemoteDataReply * decompressTask = ae_globals::get_connection()->runRemoteJob("extract",oneInput,"");
    if (decompressTask == NULL)
    {
        fileOpPending->release();
        //TODO: give reasonable error
        return;
    }
    QObject::connect(decompressTask, SIGNAL(haveJobReply(RequestState,QJsonDocument)),
                     this, SLOT(getDecompressReply(RequestState,QJsonDocument)));
    emit fileOpStarted();
}

void FileOperator::getDecompressReply(RequestState finalState, QJsonDocument)
{
    fileOpPending->release();

    //TODO: ask for refresh of relevant containing folder, after finishing job
    emit fileOpDone(finalState, "deconpress enacted");

    if (finalState != RequestState::GOOD)
    {
        //TODO: give reasonable error
    }
}

void FileOperator::fileNodesChange(FileNodeRef changedFile)
{
    emit fileSystemChange(changedFile);

    if (performingRecursiveDownload())
    {
        recursiveDownloadProcessRetry();
    }
    else if (performingRecursiveUpload())
    {
        recursiveUploadProcessRetry();
    }
}

void FileOperator::getRecursiveUploadReply(RequestState replyState, FileMetaData newFileData)
{
    recursivefileOpPending->release();

    if (replyState != RequestState::GOOD)
    {
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emitStdFileOpErr("Folder upload failed to upload file", replyState);
        return;
    }

    lsClosestNodeToParent(newFileData.getFullPath());
}

void FileOperator::getRecursiveMkdirReply(RequestState replyState, FileMetaData newFolderData)
{
    recursivefileOpPending->release();

    if (replyState != RequestState::GOOD)
    {
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emitStdFileOpErr("Folder upload failed to create new remote folder", replyState);
        return;
    }
    lsClosestNode(newFolderData.getContainingPath());
}

void FileOperator::lsClosestNode(QString fullPath, bool clearData)
{
    FileTreeNode * nodeToRefresh = rootFileNode->getClosestNodeWithName(fullPath);
    enactFolderRefresh(nodeToRefresh->getFileData(), clearData);
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
        enactFolderRefresh(nodeToRefresh->getFileData(), clearData);
        return;
    }

    nodeToRefresh = rootFileNode->getClosestNodeWithName(fullPath);
    enactFolderRefresh(nodeToRefresh->getFileData());
}

bool FileOperator::fileStillExtant(const FileNodeRef &theFile)
{
    FileTreeNode * scanNode = getFileNodeFromNodeRef(theFile);
    return (scanNode != NULL);
}

NodeState FileOperator::getFileNodeState(const FileNodeRef &theFile)
{
    FileTreeNode * scanNode = getFileNodeFromNodeRef(theFile);
    if (scanNode == NULL) return NodeState::NON_EXTANT;
    return scanNode->getNodeState();
}

bool FileOperator::isAncestorOf(const FileNodeRef &parent, const FileNodeRef &child)
{
    //Also returns false if one or the other is not extant
    FileTreeNode *  parentNode = getFileNodeFromNodeRef(parent);
    if (parentNode == NULL) return false;
    FileTreeNode *  childNode = getFileNodeFromNodeRef(child);
    if (childNode == NULL) return false;

    return childNode->isChildOf(parentNode);
}

const FileNodeRef FileOperator::speculateFileWithName(QString fullPath, bool folder)
{
    FileTreeNode * scanNode = rootFileNode->getNodeWithName(fullPath);
    if (scanNode != NULL)
    {
        return scanNode->getFileData();
    }
    scanNode = rootFileNode->getClosestNodeWithName(fullPath);
    if (scanNode == NULL)
    {
        FileNodeRef ret;
        return ret;
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
    return speculateFileWithName(scanNode->getFileData(), pathSoFar, folder);
}

const FileNodeRef FileOperator::speculateFileWithName(const FileNodeRef &baseNode, QString addedPath, bool folder)
{
    FileNodeRef fail;
    FileTreeNode * searchNode = getFileNodeFromNodeRef(baseNode);
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
            qCDebug(agaveAppLayer, "Invalid file speculation path.");
            return fail;
        }
        if (searchNode->getNodeState() == NodeState::FOLDER_CONTENTS_LOADED)
        {
            //Speculation failed, file known to not exist
            return fail;
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
        enactFolderRefresh(searchNode->getFileData());

        searchNode = nextNode;
    }

    if (folder)
    {
        if (searchNode->getNodeState() != NodeState::FOLDER_CONTENTS_LOADED)
        {
            enactFolderRefresh(searchNode->getFileData());
        }
    }
    else if (searchNode->getFileBuffer() == NULL)
    {
        sendDownloadBuffReq(searchNode->getFileData());
    }

    return searchNode->getFileData();
}

const FileNodeRef FileOperator::getChildWithName(const FileNodeRef &baseFile, QString childName)
{
    FileNodeRef fail;
    FileTreeNode * baseNode = getFileNodeFromNodeRef(baseFile);
    if (baseNode == NULL) return fail; //TODO: Consider exception handling here
    FileTreeNode * childNode = baseNode->getChildNodeWithName(childName);
    if (childNode == NULL) return fail;
    return childNode->getFileData();
}

const QByteArray FileOperator::getFileBuffer(const FileNodeRef &baseFile)
{
    FileTreeNode * baseNode = getFileNodeFromNodeRef(baseFile);
    if (baseNode == NULL) return NULL; //TODO: Consider exception handling here
    QByteArray ret;
    QByteArray * storedArray = baseNode->getFileBuffer();
    if (storedArray == NULL) return ret;
    return *storedArray;
}

void FileOperator::setFileBuffer(const FileNodeRef &theFile, const QByteArray * toSet)
{
    FileTreeNode * baseNode = getFileNodeFromNodeRef(theFile);
    if (baseNode == NULL) return; //TODO: Consider exception handling here
    baseNode->setFileBuffer(toSet);
}

FileNodeRef FileOperator::getParent(const FileNodeRef &theFile)
{
    FileNodeRef fail;
    FileTreeNode * baseNode = getFileNodeFromNodeRef(theFile);
    if (baseNode == NULL) return fail;
    FileTreeNode * parentNode = baseNode->getParentNode();
    if (parentNode == NULL) return fail;
    return parentNode->getFileData();
}

QList<FileNodeRef> FileOperator::getChildList(const FileNodeRef &theFile)
{
    QList<FileNodeRef> ret;
    FileTreeNode * baseNode = getFileNodeFromNodeRef(theFile);
    if (baseNode == NULL) return ret; //TODO: Consider exception handling here
    for (FileTreeNode * aNode : baseNode->getChildList())
    {
        ret.append(aNode->getFileData());
    }
    return ret;
}

bool FileOperator::nodeIsRoot(const FileNodeRef &theFile)
{
    FileTreeNode * theNode = getFileNodeFromNodeRef(theFile);
    if (theNode == NULL) return false;
    return theNode->isRootNode();
}

void FileOperator::quickInfoPopup(QString infoText)
{
    //TODO: Slated for deletion, errors to be passed in status object
    QMessageBox infoMessage;
    infoMessage.setText(infoText);
    infoMessage.setIcon(QMessageBox::Information);
    infoMessage.exec();
}

bool FileOperator::deletePopup(const FileNodeRef &toDelete)
{
    QMessageBox deleteQuery;
    deleteQuery.setText(QString("Are you sure you wish to delete the file:\n\n%1").arg(toDelete.getFullPath()));
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
            currentRecursiveTask = FileOp_RecursiveTask::NONE;
            fileOpPending->release();
            fileOpDone(RequestState::GOOD,"Remote folder downloaded");
            return;
        }

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
            outText = "Unable to write local files for download, please check that you have permissions to write to the specified folder.";
        }

        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emit fileOpDone(RequestState::UNCLASSIFIED, outText);
        return;
    }
}

bool FileOperator::recursiveDownloadRetrivalHelper(FileTreeNode * nodeToCheck)
{
    if (nodeToCheck->isFile())
    {
        if (nodeToCheck->getFileBuffer() == NULL)
        {
            sendDownloadBuffReq(nodeToCheck->getFileData());
            return false;
        }
        return true;
    }

    if (!nodeToCheck->isFolder()) return true; //For now, we only copy files and folders

    bool foundAll = true;

    if (nodeToCheck->getNodeState() != NodeState::FOLDER_CONTENTS_LOADED)
    {
        foundAll = false;
        enactFolderRefresh(nodeToCheck->getFileData());
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
    FileTreeNode * trueRemoteHead = recursiveRemoteHead->getChildNodeWithName(recursiveLocalHead.dirName());
    if (trueRemoteHead == NULL)
    {
        sendRecursiveCreateFolderReq(recursiveRemoteHead, recursiveLocalHead.dirName());
        return;
    }

    if (recursiveUploadHelper(trueRemoteHead, recursiveLocalHead, theError))
    {
        currentRecursiveTask = FileOp_RecursiveTask::NONE;
        fileOpPending->release();
        emit fileOpDone(RequestState::GOOD, "Folder uploaded.");
        return;
    }

    if (theError == RecursiveErrorCodes::NONE) return;

    currentRecursiveTask = FileOp_RecursiveTask::NONE;
    fileOpPending->release();
    if (theError == RecursiveErrorCodes::MKDIR_FAIL)
    {
        emit fileOpDone(RequestState::UNCLASSIFIED, "Create folder operation failed during recursive upload. Check your network connection and try again.");
        return;
    }

    if (theError == RecursiveErrorCodes::UPLOAD_FAIL)
    {
        emit fileOpDone(RequestState::UNCLASSIFIED, "File upload operation failed during recursive upload. Check your network connection and try again.");
        return;
    }

    if (theError == RecursiveErrorCodes::TYPE_MISSMATCH)
    {
        emit fileOpDone(RequestState::UNCLASSIFIED, "Internal error. File type mismatch. Remote files may be being accessed outside of this program.");
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
        enactFolderRefresh(nodeToSend->getFileData());
        return false;
    }

    for (QFileInfo anEntry : localPath.entryInfoList(QDir::Dirs	| QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot))
    {
        if (anEntry.isDir())
        {
            QDir childDir = anEntry.dir();
            childDir.cd(anEntry.fileName());
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

    qCDebug(agaveAppLayer, "Starting Recursive mkdir procedure: %s at %s",
           qPrintable(selectedNode->getFileData().getFullPath()),
           qPrintable(newName));
    RemoteDataReply * theReply = ae_globals::get_connection()->mkRemoteDir(selectedNode->getFileData().getFullPath(), newName);
    if (theReply == NULL)
    {
        recursivefileOpPending->release();
        return false;
    }
    QObject::connect(theReply, SIGNAL(haveMkdirReply(RequestState,FileMetaData)),
                     this, SLOT(getRecursiveMkdirReply(RequestState,FileMetaData)));
    return true;
}

bool FileOperator::sendRecursiveUploadReq(FileTreeNode * uploadTarget, QString localFile)
{
    if (!recursivefileOpPending->checkAndClaim()) return false;
    qCDebug(agaveAppLayer, "Starting recursively enacted upload procedure: %s to %s", qPrintable(localFile),
           qPrintable(uploadTarget->getFileData().getFullPath()));
    RemoteDataReply * theReply = ae_globals::get_connection()->uploadFile(uploadTarget->getFileData().getFullPath(), localFile);
    if (theReply == NULL)
    {
        recursivefileOpPending->release();
        return false;
    }
    QObject::connect(theReply, SIGNAL(haveUploadReply(RequestState,FileMetaData)),
                     this, SLOT(getRecursiveUploadReply(RequestState,FileMetaData)));
    return true;
}

void FileOperator::emitStdFileOpErr(QString errString, RequestState errState)
{
    emit fileOpDone(errState, QString("%1: %2")
                    .arg(errString)
                    .arg(RemoteDataInterface::interpretRequestState(errState)));
}
