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

#include "../AgaveClientInterface/filemetadata.h"
#include "../AgaveClientInterface/remotedatainterface.h"

FileTreeNode::FileTreeNode(FileMetaData contents, FileTreeNode * parent):QObject((QObject *)parent)
{
    fileData = new FileMetaData(contents);

    if (parent == NULL)
    {
        rootNode = true;
    }
    else
    {
        rootNode = false;
        myParent = parent;
        parent->childList.append(this);

        constructModelNodes(parent->getModelNode());
    }
}

FileTreeNode::FileTreeNode(FileTreeNode * parent, QStandardItem * parentModelNode):QObject((QObject *)parent)
{
    fileData = new FileMetaData();
    if (parent == NULL)
    {
        rootNode = true;

        fileData->setFullFilePath("/");
        fileData->setType(FileType::DIR);

        myModelNode = parentModelNode;

        new FileTreeNode(this);
    }
    else
    {
        rootNode = false;
        myParent = parent;
        parent->childList.append(this);

        //Create loading placeholder
        QString fullPath = parent->fileData->getFullPath();
        fullPath.append("/Loading");

        fileData->setFullFilePath(fullPath);
        fileData->setType(FileType::UNLOADED);

        constructModelNodes(parent->getModelNode());
    }
}

FileTreeNode::~FileTreeNode()
{
    while (this->childList.size() > 0)
    {
        FileTreeNode * toDelete = this->childList.takeLast();
        delete toDelete;
    }
    if (fileData != NULL)
    {
        delete fileData;
    }
    if (this->fileDataBuffer != NULL)
    {
        delete this->fileDataBuffer;
    }
    if (this->parent() != NULL)
    {
        FileTreeNode * parentNode = (FileTreeNode *)this->parent();
        if (parentNode->childList.contains(this))
        {
            parentNode->childList.removeAll(this);
        }
        if (myModelNode != NULL)
        {
            myParent->getModelNode()->removeRow(myModelNode->row());
        }
    }

}

void FileTreeNode::constructModelNodes(QStandardItem * parentNode)
{
    QList<QStandardItem *> newDataList;

    myModelNode = new QStandardItem(fileData->getFileName());
    newDataList.append(myModelNode);
    for (int i = 1; i < parentNode->model()->columnCount(); i++)
    {
        newDataList.append(new QStandardItem(getRawColumnData(i, parentNode->model())));
    }
    parentNode->appendRow(newDataList);
}

void FileTreeNode::updateFileFolder(QList<FileMetaData> *newDataList)
{
    if (rootNode == false) return;

    FileTreeNode * controllerNode = getPertinantNode(newDataList);
    if (controllerNode == NULL) return;
    controllerNode->updateFileNodeData(newDataList);
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

FileTreeNode * FileTreeNode::getPertinantNode(QList<FileMetaData> * newDataList)
{
    if (rootNode == false) return NULL;

    QString controllerAddress = getControlAddress(newDataList);
    if (controllerAddress.isEmpty()) return NULL;

    FileTreeNode * controllerNode = this->getNodeWithName(controllerAddress);

    if (controllerNode != NULL)
    {
        return controllerNode;
    }

    //If we can't find the node, we need to make some folders
    QStringList filePathParts = FileMetaData::getPathNameList(controllerAddress);
    controllerNode = this;

    bool folderSearch = true;

    for (auto itr = filePathParts.cbegin(); itr != filePathParts.cend(); itr++)
    {
        FileTreeNode * nextNode = NULL;
        if (folderSearch)
        {
            nextNode = controllerNode->getChildNodeWithName(*itr,false);
            if (nextNode == NULL)
            {
                folderSearch = false;
            }
        }
        if (folderSearch == false)
        {
            if (controllerNode->childIsUnloaded())
            {
                controllerNode->clearAllChildren();
            }
            if (controllerNode->childIsEmpty())
            {
                controllerNode->clearAllChildren();
            }
            FileMetaData newFolder;
            newFolder.setType(FileType::DIR);
            QString pathSoFar = controllerNode->getFileData().getFullPath();
            pathSoFar = pathSoFar.append("/");
            pathSoFar = pathSoFar.append(*itr);
            newFolder.setFullFilePath(pathSoFar);
            nextNode = new FileTreeNode(newFolder, controllerNode);
            new FileTreeNode(nextNode);
        }
        controllerNode = nextNode;
        if (controllerNode == NULL)
        {
            //Note: this should never happen
            qDebug("ERROR: Cannot parse new remote file data.");
            return NULL;
        }
    }

    return controllerNode;
}

void FileTreeNode::updateFileNodeData(QList<FileMetaData> * newDataList)
{
    //If the incoming list is empty, ie. has one entry (.), place empty file placeholder
    if (newDataList->size() <= 1)
    {
        clearAllChildren();

        FileMetaData emptyFolder;
        QString emptyName = fileData->getFullPath();
        emptyName.append("/Empty Folder");
        emptyFolder.setFullFilePath(emptyName);
        emptyFolder.setType(FileType::EMPTY_FOLDER);
        new FileTreeNode(emptyFolder,this);
        underlyingChildChanged();
        return;
    }

    //If the target node has a loading placeholder, clear it
    if (childIsUnloaded())
    {
        clearAllChildren();
    }
    else if (childIsEmpty())
    {
        clearAllChildren();
    }
    else
    {
        purgeUnmatchedChildren(newDataList);
    }

    for (auto itr = newDataList->begin(); itr != newDataList->end(); itr++)
    {
        insertFile(&(*itr));
    }
    underlyingChildChanged();
}

QList<FileTreeNode *> * FileTreeNode::getChildList()
{
    return &childList;
}

void FileTreeNode::deliverLSdata(RequestState taskState, QList<FileMetaData>* dataList)
{
    if (lsTask == QObject::sender())
    {
        lsTask = NULL;
    }
    if (taskState != RequestState::GOOD)
    {
        return;
    }

    if (verifyControlNode(dataList) == false)
    {
        qDebug("ERROR: File tree data/node mismatch");
        return;
    }
    this->updateFileNodeData(dataList);
}

void FileTreeNode::deliverBuffData(RequestState taskState, QByteArray * bufferData)
{
    if (bufferTask == QObject::sender())
    {
        bufferTask = NULL;
    }
    if (taskState != RequestState::GOOD)
    {
        return;
    }

    setFileBuffer(bufferData);
    underlyingChildChanged();
}

void FileTreeNode::insertFile(FileMetaData * newData)
{
    if (newData->getFileName() == ".") return;

    for (auto itr = childList.begin(); itr != childList.end(); itr++)
    {
        if ((*newData) == (*itr)->getFileData())
        {
            return;
        }
    }

    FileTreeNode * newFile = new FileTreeNode(*newData,this);
    if (newData->getFileType() == FileType::DIR)
    {   //If its a new folder, put the loading placeholder in it
         new FileTreeNode(newFile);
    }
}

void FileTreeNode::purgeUnmatchedChildren(QList<FileMetaData> * newChildList)
{
    if (childList.size() == 0) return;

    //Unmark all files in the old list
    for (auto itr = childList.begin(); itr != childList.end(); itr++)
    {
        (*itr)->marked = false;
    }

    int markCount = 0;
    //For each file in the new file list, check for it
    //Mark if it is there and IDENTICAL
    for (auto itr = newChildList->begin(); itr != newChildList->end(); ++itr)
    {
        if ((*itr).getFileName() == ".") continue;

        for (auto itr2 = childList.begin(); itr2 != childList.end(); itr2++)
        {
            if ((*itr2)->marked) continue;

            if ((*itr) == (*itr2)->getFileData())
            {
                (*itr2)->marked = true;
                markCount++;
                break;
            }
        }
    }

    if (markCount == 0) return;

    //Remove all unmarked files from old list
    //Note: Not sure about the interaction between destructors and the iterator
    //So, this is less efficient then I might have liked.
    //If this proves a time bottleneck (unlikely) the underlying container or
    //algorithm can be revised
    while (markCount > 0)
    {
        FileTreeNode * toRemove = NULL;
        for (auto itr = childList.begin(); itr != childList.end(); itr++)
        {
            if ((*itr)->marked) continue;

            toRemove = (*itr);
        }
        if (toRemove != NULL)
        {
            delete toRemove;
            markCount--;
        }
        else
        {
            return;
        }
    }
}

bool FileTreeNode::isRootNode()
{
    return rootNode;
}

FileMetaData FileTreeNode::getFileData()
{
    return *fileData;
}

QByteArray * FileTreeNode::getFileBuffer()
{
    return fileDataBuffer;
}

QStandardItem * FileTreeNode::getModelNode()
{
    return myModelNode;
}

void FileTreeNode::setFileBuffer(QByteArray * newFileBuffer)
{
    if (fileDataBuffer != NULL)
    {
        delete fileDataBuffer;
    }

    if (newFileBuffer == NULL)
    {
        fileDataBuffer = NULL;
    }
    else
    {
        fileDataBuffer = new QByteArray(*newFileBuffer);
    }
}

bool FileTreeNode::nodeWithNameIsLoading(QString filename)
{
    //TODO: Revise this so it is faster
    FileTreeNode * possibleNode = pathSearchHelper(filename,false,false);
    if (possibleNode != NULL) return false;
    possibleNode = pathSearchHelper(filename,true,false);
    if (possibleNode == NULL) return false; // Should never happen
    if (possibleNode->childIsUnloaded()) return true;
    return false;
}

FileTreeNode * FileTreeNode::getNodeWithName(QString filename, bool unrestricted)
{
    return pathSearchHelper(filename,false,unrestricted);
}

FileTreeNode * FileTreeNode::getClosestNodeWithName(QString filename, bool unrestricted)
{
    return pathSearchHelper(filename,true,unrestricted);
}

FileTreeNode * FileTreeNode::getParentNode()
{
    if (rootNode == true) return NULL;
    return (FileTreeNode *)this->parent();
}

bool FileTreeNode::childIsUnloaded()
{
    for (auto itr = childList.cbegin(); itr != childList.cend(); itr++)
    {
        if ((*itr)->getFileData().getFileType() == FileType::UNLOADED)
        {
            return true;
        }
    }
    return false;
}

bool FileTreeNode::childIsEmpty()
{
    for (auto itr = childList.cbegin(); itr != childList.cend(); itr++)
    {
        if ((*itr)->getFileData().getFileType() == FileType::EMPTY_FOLDER)
        {
            return true;
        }
    }
    return false;
}

void FileTreeNode::clearAllChildren()
{
    while (childList.size() > 0)
    {
        FileTreeNode *toDestroy = childList.takeLast();
        toDestroy->deleteLater();
    }
}

bool FileTreeNode::haveLStask()
{
    return (lsTask != NULL);
}

void FileTreeNode::setLStask(RemoteDataReply * newTask, bool clearData)
{
    if (lsTask != NULL)
    {
        QObject::disconnect(lsTask, 0, this, 0);
    }
    lsTask = newTask;
    QObject::connect(lsTask, SIGNAL(haveLSReply(RequestState,QList<FileMetaData>*)),
                     this, SLOT(deliverLSdata(RequestState,QList<FileMetaData>*)));

    if (clearData)
    {
        clearAllChildren();
        new FileTreeNode(this);
    }
}

bool FileTreeNode::haveBuffTask()
{
    return (bufferTask != NULL);
}

void FileTreeNode::setBuffTask(RemoteDataReply * newTask)
{
    if (bufferTask != NULL)
    {
        QObject::disconnect(bufferTask, 0, this, 0);
    }
    bufferTask = newTask;
    QObject::connect(bufferTask, SIGNAL(haveBufferDownloadReply(RequestState,QByteArray*)),
                     this, SLOT(deliverBuffData(RequestState,QByteArray*)));
}

FileTreeNode * FileTreeNode::pathSearchHelper(QString filename, bool stopEarly, bool unrestricted)
{
    if (rootNode == false) return NULL;

    QStringList filePathParts = FileMetaData::getPathNameList(filename);
    FileTreeNode * searchNode = this;

    for (auto itr = filePathParts.cbegin(); itr != filePathParts.cend(); itr++)
    {
        FileTreeNode * nextNode = searchNode->getChildNodeWithName(*itr,unrestricted);
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

FileTreeNode * FileTreeNode::getChildNodeWithName(QString filename, bool unrestricted)
{
    for (auto itr = this->childList.begin(); itr != this->childList.end(); itr++)
    {
        FileMetaData toSearch = (*itr)->getFileData();
        if ((unrestricted) || (toSearch.getFileType() == FileType::DIR) || (toSearch.getFileType() == FileType::FILE))
        {
            if (toSearch.getFileName() == filename)
            {
                return (*itr);
            }
        }
    }
    return NULL;
}

bool FileTreeNode::fileNameMatches(QString fileToMatch)
{
    FileTreeNode * rootNode = this;
    while (rootNode->isRootNode() == false)
    {
        rootNode = rootNode->getParentNode();
    }

    FileTreeNode * checkNode = rootNode->getNodeWithName(fileToMatch);
    if (checkNode == NULL)
    {
        return false;
    }
    return (checkNode == this);
}

QString FileTreeNode::getRawColumnData(int i, QStandardItemModel * fullModel)
{
    QString headerText = fullModel->horizontalHeaderItem(i)->text();
    if (headerText == "File Name")
    {
        return fileData->getFileName();
    }
    if (headerText == "Type")
    {
        return fileData->getFileTypeString();
    }
    if (headerText == "Size")
    {
        return QString::number(fileData->getSize());
    }
    return "";
}

void FileTreeNode::underlyingChildChanged()
{
    if (rootNode == false)
    {
        myParent->underlyingChildChanged();
    }
    else
    {
        emit fileSystemChanged();
    }
}
