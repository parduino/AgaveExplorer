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

#include "ae_globals.h"
#include "utilFuncs/authform.h"
#include "remoteFiles/fileoperator.h"
#include "remoteJobs/joboperator.h"

#include "agaveInterfaces/agavehandler.h"

Q_LOGGING_CATEGORY(agaveAppLayer, "Agave App Layer")

QStringList AgaveSetupDriver::enabledDebugs;

AgaveSetupDriver::AgaveSetupDriver(QObject *parent) : QObject(parent)
{
    ae_globals::set_Driver(this);

    qRegisterMetaType<RequestState>("RequestState");
    qRegisterMetaType<FileNodeRef>("FileNodeRef");
    qRegisterMetaType<FileMetaData>("FileMetaData");
    qRegisterMetaType<RemoteJobData>("RemoteJobData");
    qRegisterMetaType<RemoteDataInterfaceState>("RemoteDataInterfaceState");
    qRegisterMetaType<QList<FileMetaData>>("QList<FileMetaData>");
    qRegisterMetaType<QList<RemoteJobData>>("QList<RemoteJobData>");

    qApp->setQuitOnLastWindowClosed(false);
    //Note: Window closing must link to the shutdown sequence, otherwise the app will not close
    //Note: Might consider a better way of implementing this.
}

AgaveSetupDriver::~AgaveSetupDriver()
{
    if (authWindow != nullptr) delete authWindow;

    if (theNetManager != nullptr) delete theNetManager;
    if (myDataInterface != nullptr) delete myDataInterface;

    if (remoteInterfacesThread != nullptr)
    {
        remoteInterfacesThread->quit();
        remoteInterfacesThread->wait();
    }
}

void AgaveSetupDriver::createAndStartAgaveThread()
{
    remoteInterfacesThread = new QThread(this);

    remoteInterfacesThread->start();

    theNetManager = new QNetworkAccessManager();
    theNetManager->moveToThread(remoteInterfacesThread);

    myDataInterface = new AgaveHandler(theNetManager);
    myDataInterface->moveToThread(remoteInterfacesThread);
    myDataInterface->setAgaveConnectionParams("https://agave.designsafe-ci.org", "SimCenter_CWE_GUI", "designsafe.storage.default");
    QObject::connect(myDataInterface, SIGNAL(connectionStateChanged(RemoteDataInterfaceState)),
                     this, SLOT(newConnectionState(RemoteDataInterfaceState)));

    myJobHandle = new JobOperator(myDataInterface, this);
    myFileHandle = new FileOperator(myDataInterface, this);
}

void AgaveSetupDriver::setDebugLogging(bool loggingEnabled)
{
    if (loggingEnabled)
    {
        enabledDebugs.append("Remote Interface");
        enabledDebugs.append("Agave App Layer");
        enabledDebugs.append("File Manager");
        enabledDebugs.append("default");
        enabledDebugs.append("Job Manager");
    }
    QLoggingCategory::installFilter(debugCategoryFilter);
}

void AgaveSetupDriver::debugCategoryFilter(QLoggingCategory *category)
{
    if (enabledDebugs.contains(category->categoryName()))
    {
        category->setEnabled(QtDebugMsg, true);
        return;
    }
    category->setEnabled(QtDebugMsg, false);
}

RemoteDataInterface * AgaveSetupDriver::getDataConnection()
{
    return myDataInterface;
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
    if ((authReply == RequestState::GOOD) && (authWindow != nullptr) && (authWindow->isVisible()))
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

void AgaveSetupDriver::newConnectionState(RemoteDataInterfaceState newState)
{
    if (shutdownStarted == false) return;

    if (newState == RemoteDataInterfaceState::DISCONNECTED)
    {
        shutdownCallback();
    }
}

void AgaveSetupDriver::shutdown()
{
    if (shutdownStarted) return;
    shutdownStarted = true;

    if ((myDataInterface == nullptr) || (myDataInterface->getInterfaceState() == RemoteDataInterfaceState::INIT) ||
            (myDataInterface->getInterfaceState() == RemoteDataInterfaceState::READY_TO_AUTH) ||
            (myDataInterface->getInterfaceState() == RemoteDataInterfaceState::DISCONNECTED))
    {
        shutdownCallback();
        return;
    }

    qCDebug(agaveAppLayer, "Beginning graceful shutdown.");
    RemoteDataReply * shutdownInvoke = myDataInterface->closeAllConnections();
    shutdownInvoke->setAsUnconnectedReply();

    qCDebug(agaveAppLayer, "Waiting on outstanding tasks");
    QMessageBox * waitBox = new QMessageBox(); //Note: deliberate memory leak, as program closes right after
    waitBox->setText("Waiting for network shutdown. Click OK to force quit.");
    waitBox->setStandardButtons(QMessageBox::Close);
    waitBox->setDefaultButton(QMessageBox::Close);
    QObject::connect(waitBox, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(shutdownCallback()));
    waitBox->show();
    return;
}

void AgaveSetupDriver::shutdownCallback()
{
    qCDebug(agaveAppLayer, "Invoking final exit");
    QCoreApplication::instance()->exit(0);
}
