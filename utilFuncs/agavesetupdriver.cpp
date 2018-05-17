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

#include "agavesetupdriver.h"

#include "../AgaveExplorer/ae_globals.h"
#include "../AgaveExplorer/utilFuncs/authform.h"
#include "../AgaveExplorer/remoteFileOps/fileoperator.h"
#include "../AgaveExplorer/remoteFileOps/joboperator.h"

#include "../AgaveClientInterface/agaveInterfaces/agavehandler.h"

Q_LOGGING_CATEGORY(agaveAppLayer, "Agave App Layer")

AgaveSetupDriver::AgaveSetupDriver(QObject *parent, bool debug) : QObject(parent)
{
    ae_globals::set_Driver(this);

    if (debug)
    {
        QLoggingCategory::installFilter(debugCategoryFilterOn);
    }
    else
    {
        QLoggingCategory::installFilter(debugCategoryFilterOff);
    }

    qRegisterMetaType<RequestState>("RequestState");
    qRegisterMetaType<FileNodeRef>("FileNodeRef");
    qRegisterMetaType<FileMetaData>("FileMetaData");
    qRegisterMetaType<RemoteJobData>("RemoteJobData");
    qRegisterMetaType<QList<FileMetaData>>("QList<FileMetaData>");
    qRegisterMetaType<QList<RemoteJobData>>("QList<RemoteJobData>");

    qApp->setQuitOnLastWindowClosed(false);
    //Note: Window closing must link to the shutdown sequence, otherwise the app will not close
    //Note: Might consider a better way of implementing this.
}

AgaveSetupDriver::~AgaveSetupDriver()
{
    if (authWindow != NULL) delete authWindow;

    if (myJobHandle != NULL) delete myJobHandle;
    if (myFileHandle != NULL) delete myFileHandle;

    if (theConnectThread != NULL)
    {
        theConnectThread->quit();
        theConnectThread->wait();
    }
}

void AgaveSetupDriver::debugCategoryFilterOn(QLoggingCategory *category)
{
    performDebugFiltering(category, true);
}

void AgaveSetupDriver::debugCategoryFilterOff(QLoggingCategory *category)
{
    performDebugFiltering(category, false);
}

void AgaveSetupDriver::performDebugFiltering(QLoggingCategory *category, bool debugEnabled)
{
    if (qstrcmp(category->categoryName(), "Raw HTTP") == 0)
    {
        category->setEnabled(QtDebugMsg, false);
        return;
    }
    if (qstrcmp(category->categoryName(), "Remote Interface") == 0)
    {
        category->setEnabled(QtDebugMsg, debugEnabled);
        return;
    }
    if (qstrcmp(category->categoryName(), "Agave App Layer") == 0)
    {
        category->setEnabled(QtDebugMsg, debugEnabled);
        return;
    }
    if (qstrcmp(category->categoryName(), "default") == 0)
    {
        category->setEnabled(QtDebugMsg, debugEnabled);
        return;
    }
    category->setEnabled(QtDebugMsg, false);
}

RemoteDataThread * AgaveSetupDriver::getDataConnection()
{
    return theConnectThread;
}

JobOperator * AgaveSetupDriver::getJobHandler()
{
    return myJobHandle;
}

FileOperator * AgaveSetupDriver::getFileHandler()
{
    return myFileHandle;
}

void AgaveSetupDriver::getAuthReply(RequestState authReply)
{
    if ((authReply == RequestState::GOOD) && (authWindow != NULL) && (authWindow->isVisible()))
    {
        closeAuthScreen();
    }
}

void AgaveSetupDriver::subWindowHidden(bool nowVisible)
{
    if (nowVisible == false)
    {
        shutdown();
    }
}

void AgaveSetupDriver::shutdown()
{
    if (doingShutdown) //If shutdown already in progress
    {
        return;
    }
    doingShutdown = true;
    qCDebug(agaveAppLayer, "Beginning graceful shutdown.");
    if (theConnectThread != NULL)
    {
        RemoteDataReply * revokeTask = theConnectThread->closeAllConnections();
        if (revokeTask != NULL)
        {
            QObject::connect(revokeTask, SIGNAL(connectionsClosed(RequestState)), this, SLOT(shutdownCallback()));
            qCDebug(agaveAppLayer, "Waiting on outstanding tasks");
            QMessageBox * waitBox = new QMessageBox(); //Note: deliberate memory leak, as program closes right after
            waitBox->setText("Waiting for network shutdown. Click OK to force quit.");
            waitBox->setStandardButtons(QMessageBox::Close);
            waitBox->setDefaultButton(QMessageBox::Close);
            QObject::connect(waitBox, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(shutdownCallback()));
            waitBox->show();
            return;
        }
    }
    shutdownCallback();
}

void AgaveSetupDriver::shutdownCallback()
{
    qCDebug(agaveAppLayer, "Invoking final exit");
    QCoreApplication::instance()->exit(0);
}
