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

#include "explorerdriver.h"

#include "explorerwindow.h"
#include "utilFuncs/authform.h"
#include "remoteFileOps/fileoperator.h"
#include "remoteFileOps/joboperator.h"

#include "../AgaveClientInterface/agaveInterfaces/agavehandler.h"
#include "../AgaveClientInterface/agaveInterfaces/agavetaskreply.h"

#include "../ae_globals.h"

ExplorerDriver::ExplorerDriver(QObject *parent, bool debug) : AgaveSetupDriver(parent, debug)
{
    AgaveHandler * tmpHandle = new AgaveHandler(this);
    tmpHandle->registerAgaveAppInfo("compress", "compress-0.1u1",{"directory", "compression_type"},{},"directory");
    tmpHandle->registerAgaveAppInfo("extract", "extract-0.1u1",{"inputFile"},{},"inputFile");

    tmpHandle->registerAgaveAppInfo("cwe-serial", "cwe-serial-0.1.0", {"stage"}, {"file_input", "directory"}, "directory");
    tmpHandle->registerAgaveAppInfo("cwe-parallel", "cwe-parallel-0.1.0", {"stage"}, {"file_input", "directory"}, "directory");

    theConnector = (RemoteDataInterface *) tmpHandle;
    QObject::connect(theConnector, SIGNAL(sendFatalErrorMessage(QString)), this, SLOT(fatalInterfaceError(QString)));
}

ExplorerDriver::~ExplorerDriver()
{
    if (mainWindow != NULL) mainWindow->deleteLater();
}

void ExplorerDriver::startup()
{
    myJobHandle = new JobOperator(this);
    myFileHandle = new FileOperator(this);
    authWindow = new AuthForm(this);

    authWindow->show();
    QObject::connect(authWindow->windowHandle(),SIGNAL(visibleChanged(bool)),this, SLOT(subWindowHidden(bool)));

    mainWindow = new ExplorerWindow(this);
}

void ExplorerDriver::closeAuthScreen()
{
    if (mainWindow == NULL)
    {
        fatalInterfaceError("Fatal Error in window system: No Main Window");
        return;
    }

    myJobHandle->demandJobDataRefresh();
    myFileHandle->resetFileData();
    mainWindow->startAndShow();

    //The dynamics of this may be different in windows. TODO: Find a more cross-platform solution
    QObject::connect(mainWindow->windowHandle(),SIGNAL(visibleChanged(bool)),this, SLOT(subWindowHidden(bool)));

    if (authWindow != NULL)
    {
        QObject::disconnect(authWindow->windowHandle(),SIGNAL(visibleChanged(bool)),this, SLOT(subWindowHidden(bool)));
        authWindow->hide();
        authWindow->deleteLater();
        authWindow = NULL;
    }

    AgaveHandler * tmpHandle = (AgaveHandler *) theConnector;
    AgaveTaskReply * agaveList = tmpHandle->getAgaveAppList();

    QObject::connect(agaveList, SIGNAL(haveAgaveAppList(RequestState,QJsonArray*)), this, SLOT(loadAppList(RequestState,QJsonArray*)));
}

void ExplorerDriver::startOffline()
{
    //Placeholder for virtual function which this program does not use
    return;
}

QString ExplorerDriver::getBanner()
{
    return "SimCenter Agave Explorer Program";
}

QString ExplorerDriver::getVersion()
{
    return "Version: 0.1";
}

void ExplorerDriver::loadAppList(RequestState replyState, QJsonArray * appList)
{
    if (replyState != RequestState::GOOD)
    {
        qDebug("App List not available.");
        return;
    }

    for (auto itr = appList->constBegin(); itr != appList->constEnd(); itr++)
    {
        QString appName = (*itr).toObject().value("name").toString();

        if (!appName.isEmpty())
        {
            mainWindow->addAppToList(appName);
        }
    }
}
