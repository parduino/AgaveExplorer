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

#ifndef FILETREENODE_H
#define FILETREENODE_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QStandardItem>

enum class RequestState;
class FileMetaData;
class RemoteDataInterface;
class RemoteDataReply;

class FileTreeNode : public QObject
{
    Q_OBJECT
public:
    FileTreeNode(FileMetaData contents, FileTreeNode * parent = NULL);
    FileTreeNode(FileTreeNode * parent = NULL, QStandardItem * parentModelNode = NULL); //This creates either the default root folder, or default load pending,
                                                //depending if the parent is NULL
    ~FileTreeNode();

    void updateFileFolder(QList<FileMetaData> * newDataList); //DEPRECATED

    bool isRootNode();
    FileMetaData getFileData();
    QByteArray * getFileBuffer();
    QStandardItem * getModelNode();
    void setFileBuffer(QByteArray * newFileBuffer);

    bool nodeWithNameIsLoading(QString filename);
    FileTreeNode * getNodeWithName(QString filename, bool unrestricted = false);
    FileTreeNode * getClosestNodeWithName(QString filename, bool unrestricted = false);
    FileTreeNode * getParentNode();

    bool childIsUnloaded();
    bool childIsEmpty();
    void clearAllChildren();

    bool haveLStask();
    void setLStask(RemoteDataReply * newTask, bool clearData = true);
    bool haveBuffTask();
    void setBuffTask(RemoteDataReply * newTask);

    QList<FileTreeNode *> * getChildList();
    FileTreeNode * getChildNodeWithName(QString filename, bool unrestricted = false);

    bool fileNameMatches(QString folderToMatch);

    //TODO: Clean up the code to make the algorithms using marks cleaner
    bool marked = false;

signals:
    void fileSystemChanged();

public slots:
    void deliverLSdata(RequestState taskState, QList<FileMetaData>* dataList);
    void deliverBuffData(RequestState taskState, QByteArray * bufferData);

private:
    FileTreeNode * getPertinantNode(QList<FileMetaData> * newDataList);
    bool verifyControlNode(QList<FileMetaData> * newDataList);
    QString getControlAddress(QList<FileMetaData> * newDataList);
    void updateFileNodeData(QList<FileMetaData> * newDataList);

    void insertFile(FileMetaData *newData);
    void purgeUnmatchedChildren(QList<FileMetaData> * newChildList);
    QString getRawColumnData(int i, QStandardItemModel * fullModel);
    void constructModelNodes(QStandardItem * parentNode);

    FileTreeNode * pathSearchHelper(QString filename, bool stopEarly, bool unrestricted = false);

    void underlyingChildChanged();

    FileTreeNode * myParent = NULL;
    QStandardItem * myModelNode = NULL;

    FileMetaData * fileData = NULL;
    QList<FileTreeNode *> childList;
    bool rootNode = false;

    QByteArray * fileDataBuffer = NULL;

    RemoteDataReply * lsTask = NULL;
    RemoteDataReply * bufferTask = NULL;
};

#endif // FILETREENODE_H
