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

#include "remotejobentry.h"

#include "../AgaveClientInterface/remotejobdata.h"

RemoteJobEntry::RemoteJobEntry(RemoteJobData newData, QStandardItem * modelParent, QObject *parent) : QObject(parent)
{
    modelParentNode = modelParent;
    setData(newData);
}

void RemoteJobEntry::setData(RemoteJobData newData)
{
    bool signalChange = false;
    if ((newData.getState() != myData.getState()) && (myData.getState() != "APP_INIT"))
    {
        signalChange = true;
    }
    myData = newData;

    if (myModelNode == NULL)
    {
        QList<QStandardItem *> newRow;
        myModelNode = new QStandardItem(myData.getName());
        newRow.append(myModelNode);
        newRow.append(new QStandardItem(myData.getState()));
        newRow.append(new QStandardItem(myData.getApp()));
        newRow.append(new QStandardItem(myData.getTimeCreated().toString()));
        newRow.append(new QStandardItem(myData.getID()));
        modelParentNode->insertRow(0,newRow);
    }
    else
    {
        int rowNum = myModelNode->row();

        modelParentNode->child(rowNum,0)->setText(myData.getName());
        modelParentNode->child(rowNum,1)->setText(myData.getState());
        modelParentNode->child(rowNum,2)->setText(myData.getApp());
        modelParentNode->child(rowNum,3)->setText(myData.getTimeCreated().toString());
        modelParentNode->child(rowNum,4)->setText(myData.getID());
    }

    if (signalChange)
    {
        emit jobStateChanged(&myData);
    }
}

RemoteJobData RemoteJobEntry::getData()
{
    return myData;
}

void RemoteJobEntry::setDetails(QMap<QString, QString> inputs, QMap<QString, QString> params)
{
    myData.setDetails(inputs, params);
}
