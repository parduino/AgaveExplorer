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

#include "filetreenode.h"

#include "fileoperator.h"
#include "filenoderef.h"

#include "../remoteModelViews/linkedstandarditem.h"

#include "../AgaveClientInterface/filemetadata.h"
#include "../AgaveClientInterface/remotedatainterface.h"
#include "../ae_globals.h"

FileTreeNode::FileTreeNode(FileMetaData contents, FileTreeNode * parent):QObject((QObject *)parent)
{
    fileData.copyDataFrom(contents);
    settimestamps();

    myParent = parent;
    parent->childList.append(this);

    recomputeNodeState();
}

FileTreeNode::FileTreeNode(QString rootFolderName, QObject * parent):QObject((QObject *)parent)
{
    QString fullPath = "/";
    fullPath = fullPath.append(rootFolderName);

    fileData.setFullFilePath(fullPath);
    fileData.setType(FileType::DIR);
    settimestamps();
    nodeVisible = true;

    recomputeNodeState();
}

FileTreeNode::~FileTreeNode()
{
    //Note: DO NOT call delete directly on a file tree node except when
    //shutting down or resetting the file tree
    //Use markForDelete to insure signaling of changes to the file tree
    while (this->childList.size() > 0)
    {
        FileTreeNode * toDelete = this->childList.takeLast();
        delete toDelete;
    }
    if (this->fileDataBuffer != NULL)
    {
        delete this->fileDataBuffer;
    }

    if (myParent != NULL)
    {
        if (myParent->childList.contains(this))
        {
            myParent->childList.removeAll(this);
        }
    }
}

bool FileTreeNode::isRootNode()
{
    return (myParent == NULL);
}

NodeState FileTreeNode::getNodeState()
{
    return myState;
}

FileNodeRef FileTreeNode::getFileData()
{
    return fileData;
}

QByteArray * FileTreeNode::getFileBuffer()
{
    return fileDataBuffer;
}

FileTreeNode * FileTreeNode::getNodeWithName(QString filename)
{
    return pathSearchHelper(filename,false);
}

FileTreeNode * FileTreeNode::getClosestNodeWithName(QString filename)
{
    return pathSearchHelper(filename,true);
}

FileTreeNode * FileTreeNode::getParentNode()
{
    return myParent;
}

FileTreeNode * FileTreeNode::getNodeReletiveToNodeWithName(QString searchPath)
{
    QStringList filePathParts = FileMetaData::getPathNameList(searchPath);
    return pathSearchHelperFromAnyNode(filePathParts, false);
}

void FileTreeNode::deleteFolderContentsData()
{
    folderContentsKnown = false;
    clearAllChildren();
    recomputeNodeState();
}

void FileTreeNode::setFileBuffer(const QByteArray * newFileBuffer)
{
    if (fileDataBuffer == NULL)
    {
        if (newFileBuffer != NULL)
        {
            fileDataBuffer = new QByteArray(*newFileBuffer);
        }
    }
    else if (newFileBuffer == NULL)
    {
        delete fileDataBuffer;
        fileDataBuffer = NULL;
    }
    else
    {
        delete fileDataBuffer;
        fileDataBuffer = new QByteArray(*newFileBuffer);
    }

    setNodeVisible();
    recomputeNodeState();
}

bool FileTreeNode::haveLStask()
{
    return (lsTask != NULL);
}

void FileTreeNode::setLStask(RemoteDataReply * newTask)
{
    if (!isFolder())
    {
        qCDebug(agaveAppLayer, "ERROR: LS called on file rather than folder.");
        return;
    }
    if (lsTask != NULL)
    {
        QObject::disconnect(lsTask, 0, this, 0);
    }
    lsTask = newTask;
    QObject::connect(lsTask, SIGNAL(haveLSReply(RequestState,QList<FileMetaData>)),
                     this, SLOT(deliverLSdata(RequestState,QList<FileMetaData>)));
    recomputeNodeState();
}

bool FileTreeNode::haveBuffTask()
{
    return (bufferTask != NULL);
}

void FileTreeNode::setBuffTask(RemoteDataReply * newTask)
{
    if (isFolder())
    {
        qCDebug(agaveAppLayer, "ERROR: Buffer download called on folder.");
        return;
    }
    if (bufferTask != NULL)
    {
        QObject::disconnect(bufferTask, 0, this, 0);
    }
    bufferTask = newTask;
    QObject::connect(bufferTask, SIGNAL(haveBufferDownloadReply(RequestState,QByteArray)),
                     this, SLOT(deliverBuffData(RequestState,QByteArray)));
    recomputeNodeState();
}

QList<FileTreeNode *> FileTreeNode::getChildList()
{
    return childList;
}

FileTreeNode * FileTreeNode::getChildNodeWithName(QString filename)
{
    for (auto itr = this->childList.begin(); itr != this->childList.end(); itr++)
    {
        FileMetaData toSearch = (*itr)->getFileData();
        if ((toSearch.getFileName() == filename) && (toSearch.getFileType() != FileType::INVALID))
        {
            return (*itr);
        }
    }
    return NULL;
}

bool FileTreeNode::isFolder()
{
    return (fileData.getFileType() == FileType::DIR);
}

bool FileTreeNode::isFile()
{
    return (fileData.getFileType() == FileType::FILE);
}

bool FileTreeNode::isChildOf(FileTreeNode * possibleParent)
{
    //Note: In this method, a node is considered a child of itself
    FileTreeNode * nodeToCheck = this;

    if (possibleParent == NULL) return false;

    while (nodeToCheck != NULL)
    {
        if (nodeToCheck == possibleParent) return true;
        if (nodeToCheck->isRootNode()) return false;
        nodeToCheck = nodeToCheck->getParentNode();
    }
    return false;
}

void FileTreeNode::deliverLSdata(RequestState taskState, QList<FileMetaData> dataList)
{
    lsTask = NULL;
    if (taskState == RequestState::GOOD)
    {
        if (verifyControlNode(&dataList) == false)
        {
            qCDebug(agaveAppLayer, "ERROR: File tree data/node mismatch");
            recomputeNodeState();
            return;
        }
        this->updateFileNodeData(&dataList);
        return;
    }

    if (taskState == RequestState::FILE_NOT_FOUND)
    {
        if (!nodeVisible)
        {
            changeNodeState(NodeState::DELETING);
            return;
        }
    }
    else
    {
        ae_globals::displayPopup("Unable to connect to DesignSafe file server. If this problem persists, please contact DesignDafe.", "Connection Issue");
    }

    recomputeNodeState();
}

void FileTreeNode::deliverBuffData(RequestState taskState, QByteArray bufferData)
{
    bufferTask = NULL;
    if (taskState == RequestState::GOOD)
    {
        qCDebug(agaveAppLayer, "Download of buffer complete: %s", qPrintable(fileData.getFullPath()));
        if (bufferData.isNull())
        {
            setFileBuffer(NULL);
        }
        else
        {
            setFileBuffer(&bufferData);
        }
        return;
    }

    if (taskState == RequestState::FILE_NOT_FOUND)
    {
        if (!nodeVisible)
        {
            changeNodeState(NodeState::DELETING);
            return;
        }
    }
    else
    {
        ae_globals::displayPopup("Unable to connect to DesignSafe file server. If this problem persists, please contact DesignDafe.", "Connection Issue");
    }
    recomputeNodeState();
}

void FileTreeNode::slateNodeForDelete()
{
    if (myState == NodeState::DELETING) return;
    changeNodeState(NodeState::DELETING);
}

void FileTreeNode::setNodeVisible()
{
    if (nodeVisible) return;
    nodeVisible = true;

    FileTreeNode * searchNode = getParentNode();
    if (searchNode != NULL)
    {
        searchNode->setNodeVisible();
    }
    recomputeNodeState();
}

void FileTreeNode::recomputeNodeState()
{
    if (myState == NodeState::DELETING) return;
    if (isFolder())
    {
        if (!nodeVisible)
        {
            if (haveLStask())
            {
                changeNodeState(NodeState::FOLDER_SPECULATE_LOADING); return;
            }
            else
            {
                changeNodeState(NodeState::FOLDER_SPECULATE_IDLE); return;
            }
        }

        if (haveLStask())
        {
            if (!childList.isEmpty())
            {
                changeNodeState(NodeState::FOLDER_CONTENTS_RELOADING); return;
            }

            changeNodeState(NodeState::FOLDER_CONTENTS_LOADING); return;
        }
        else
        {
            if (!folderContentsKnown)
            {
                changeNodeState(NodeState::FOLDER_KNOWN_CONTENTS_NOT); return;
            }
            changeNodeState(NodeState::FOLDER_CONTENTS_LOADED); return;
        }
    }
    else if (isFile())
    {
        if (!nodeVisible)
        {
            if (haveBuffTask())
            {
                changeNodeState(NodeState::FILE_SPECULATE_LOADING); return;
            }
            else
            {
                changeNodeState(NodeState::FILE_SPECULATE_IDLE); return;
            }
        }

        if (haveBuffTask())
        {
            if (fileDataBuffer != NULL)
            {
                changeNodeState(NodeState::FILE_BUFF_RELOADING); return;
            }

            changeNodeState(NodeState::FILE_BUFF_LOADING); return;
        }
        else
        {
            if (fileDataBuffer == NULL)
            {
                changeNodeState(NodeState::FILE_KNOWN); return;
            }

            changeNodeState(NodeState::FILE_BUFF_LOADED); return;
        }
    }
    changeNodeState(NodeState::ERROR);
}

void FileTreeNode::changeNodeState(NodeState newState)
{
    if (newState == myState) return;
    myState = newState;

    if (myState == NodeState::DELETING)
    {
        this->deleteLater();
    }

    ae_globals::get_file_handle()->fileNodesChange(fileData);
}

void FileTreeNode::settimestamps()
{
    nodeTimestamp = QDateTime::currentMSecsSinceEpoch();
    fileData.setTimestamp(nodeTimestamp);
}

FileTreeNode * FileTreeNode::pathSearchHelper(QString filename, bool stopEarly)
{//I am worried about how generic this function is.
    //Our current agave setup has a named root folder
    if (isRootNode() == false) return NULL;

    QStringList filePathParts = FileMetaData::getPathNameList(filename);
    if (filePathParts.isEmpty()) return NULL;
    FileTreeNode * searchNode = this;

    QString rootName = filePathParts.takeFirst();
    if (rootName != searchNode->getFileData().getFileName())
    {
        return NULL;
    }

    return searchNode->pathSearchHelperFromAnyNode(filePathParts, stopEarly);
}

FileTreeNode * FileTreeNode::pathSearchHelperFromAnyNode(QStringList filePathParts, bool stopEarly)
{
    FileTreeNode * searchNode = this;

    for (auto itr = filePathParts.cbegin(); itr != filePathParts.cend(); itr++)
    {
        FileTreeNode * nextNode = searchNode->getChildNodeWithName(*itr);
        if (nextNode == NULL)
        {
            if (stopEarly == true)
            {
                return searchNode;
            }
            return NULL;
        }
        searchNode = nextNode;
    }

    return searchNode;
}

bool FileTreeNode::verifyControlNode(QList<FileMetaData> * newDataList)
{
    QString controllerAddress = getControlAddress(newDataList);
    if (controllerAddress.isEmpty()) return false;

    FileTreeNode * myRootNode = this;
    while (myRootNode->myParent != NULL)
    {
        myRootNode = myRootNode->myParent;
    }

    FileTreeNode * controllerNode = myRootNode->getNodeWithName(controllerAddress);
    return (controllerNode == this);
}

QString FileTreeNode::getControlAddress(QList<FileMetaData> * newDataList)
{
    for (auto itr = newDataList->cbegin(); itr != newDataList->cend(); itr++)
    {
        if ((*itr).getFileName() == ".")
        {
            return (*itr).getContainingPath();
        }
    }

    return "";
}

void FileTreeNode::updateFileNodeData(QList<FileMetaData> * newDataList)
{
    folderContentsKnown = true;

    //If the incoming list is empty, ie. has one entry (.), place empty file item
    if (newDataList->size() <= 1)
    {
        clearAllChildren();
        recomputeNodeState();
        return;
    }

    purgeUnmatchedChildren(newDataList);

    for (auto itr = newDataList->begin(); itr != newDataList->end(); itr++)
    {
        insertFile(&(*itr));
    }

    for (auto itr = childList.begin(); itr != childList.end(); itr++)
    {
        (*itr)->setNodeVisible();
    }

    recomputeNodeState();
}

void FileTreeNode::clearAllChildren()
{
    while (!childList.isEmpty())
    {
        FileTreeNode * aChild = childList.takeLast();
        aChild->changeNodeState(NodeState::DELETING);
    }
}

void FileTreeNode::insertFile(FileMetaData * newData)
{
    if (newData->getFileName() == ".") return;

    for (auto itr = childList.begin(); itr != childList.end(); itr++)
    {
        if ((newData->getFullPath() == (*itr)->getFileData().getFullPath()) &&
                (newData->getFileType() == (*itr)->getFileData().getFileType()))
        {
            if (newData->getSize() != (*itr)->getFileData().getSize())
            {
                (*itr)->getFileData().setSize(newData->getSize());
            }
            return;
        }
    }

    new FileTreeNode(*newData,this);
}

void FileTreeNode::purgeUnmatchedChildren(QList<FileMetaData> * newChildList)
{
    if (childList.size() == 0) return;

    QList<FileTreeNode *> altList;

    while (!childList.isEmpty())
    {
        FileTreeNode * aNode = childList.takeLast();
        FileMetaData toCheck = aNode->getFileData();

        bool matchFound = false;
        FileMetaData matchedData;

        for (auto itr = newChildList->begin(); itr != newChildList->end() && (matchFound == false); ++itr)
        {
            if ((*itr).getFileName() == ".") continue;

            if ((toCheck.getFullPath() == (*itr).getFullPath()) &&
                    (toCheck.getFileType() == (*itr).getFileType()))
            {
                matchFound = true;
                matchedData = *itr;
            }
        }

        if (matchFound)
        {
            altList.append(aNode);
        }
        else
        {
            aNode->changeNodeState(NodeState::DELETING);
        }
    }

    while (!altList.isEmpty())
    {
        childList.append(altList.takeLast());
    }
}
