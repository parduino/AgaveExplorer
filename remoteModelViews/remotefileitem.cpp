/*********************************************************************************
**
** Copyright (c) 2018 The University of Notre Dame
** Copyright (c) 2018 The Regents of the University of California
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

#include "remotefileitem.h"

#include "../ae_globals.h"

RemoteFileItem::RemoteFileItem(bool isLoading) : QStandardItem()
{
    myFile = FileNodeRef::nil();
    if (isLoading)
    {
        setText("Loading . . . ");
    }
    else
    {
        setText("Empty Folder");
    }
}

RemoteFileItem::RemoteFileItem(FileNodeRef fileInfo) : QStandardItem()
{
    myFile = fileInfo;
    appendToRowList(this);
}

RemoteFileItem::RemoteFileItem(RemoteFileItem * rowLeader) : QStandardItem()
{
    myFile = FileNodeRef::nil();
    if (rowLeader == NULL)
    {
        qCDebug(agaveAppLayer, "Warning: Invalid Remote File Item Construction");
        return;
    }
    myRowLeader = rowLeader;
    myRowLeader->appendToRowList(this);
}

RemoteFileItem * RemoteFileItem::getRowHeader()
{
    if (myRowLeader == NULL) return this;
    return myRowLeader->getRowHeader();
}

QList<RemoteFileItem*> RemoteFileItem::getRowList()
{
    if (myRowLeader == NULL) return rowList;
    return myRowLeader->getRowList();
}

FileNodeRef RemoteFileItem::getFile()
{
    if (myRowLeader == NULL) return myFile;
    return myRowLeader->getFile();
}

bool RemoteFileItem::parentOfPlaceholder()
{
    if (!getRowHeader()->hasChildren()) return false;
    RemoteFileItem * nodeToCheck = (RemoteFileItem *)(getRowHeader()->child(0,0));
    if (nodeToCheck == NULL) return false;
    if (nodeToCheck->myFile.isNil()) return true;
    return false;
}

void RemoteFileItem::appendToRowList(RemoteFileItem * toAdd)
{
    rowList.append(toAdd);
}
