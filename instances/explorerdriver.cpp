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
#include "remoteFiles/fileoperator.h"
#include "remoteJobs/joboperator.h"

#include "agaveInterfaces/agavetaskreply.h"
#include "agaveInterfaces/agavehandler.h"

#include "ae_globals.h"

ExplorerDriver::ExplorerDriver(QObject *parent) : AgaveSetupDriver(parent) {}

ExplorerDriver::~ExplorerDriver()
{
    if (mainWindow != nullptr) delete mainWindow;
}

void ExplorerDriver::startup()
{
    createAndStartAgaveThread();

    myDataInterface->registerAgaveAppInfo("compress", "compress-0.1u1",{"directory", "compression_type"},{},"directory");
    myDataInterface->registerAgaveAppInfo("extract", "extract-0.1u1",{"inputFile"},{},"inputFile");

    myDataInterface->registerAgaveAppInfo("cwe-serial", "cwe-serial-0.2.0", {"stage"}, {"file_input", "directory"}, "directory");
    myDataInterface->registerAgaveAppInfo("cwe-parallel", "cwe-parallel-0.2.0", {"stage"}, {"file_input", "directory"}, "directory");

    authWindow = new AuthForm();
    authWindow->show();
    QObject::connect(authWindow->windowHandle(),SIGNAL(visibleChanged(bool)),this, SLOT(subWindowHidden(bool)));
}

void ExplorerDriver::closeAuthScreen()
{
    mainWindow = new ExplorerWindow();

    myJobHandle->demandJobDataRefresh();
    myFileHandle->resetFileData(myDataInterface, myDataInterface->getUserName());

    mainWindow->startAndShow();

    //The dynamics of this may be different in windows. TODO: Find a more cross-platform solution
    QObject::connect(mainWindow->windowHandle(),SIGNAL(visibleChanged(bool)),this, SLOT(subWindowHidden(bool)));

    if (authWindow != nullptr)
    {
        QObject::disconnect(authWindow->windowHandle(),SIGNAL(visibleChanged(bool)),this, SLOT(subWindowHidden(bool)));
        authWindow->hide();
        authWindow->deleteLater();
        authWindow = nullptr;
    }

    AgaveTaskReply * agaveList = myDataInterface->getAgaveAppList();

    QObject::connect(agaveList, SIGNAL(haveAgaveAppList(RequestState,QVariantList)), this, SLOT(loadAppList(RequestState,QVariantList)));
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

void ExplorerDriver::loadAppList(RequestState replyState, QVariantList appList)
{
    if (replyState != RequestState::GOOD)
    {
        qCDebug(agaveAppLayer, "App List not available.");
        return;
    }

    for (auto itr = appList.constBegin(); itr != appList.constEnd(); itr++)
    {
        QString appName = (*itr).toJsonObject().value("name").toString();

        if (!appName.isEmpty())
        {
            mainWindow->addAppToList(appName);
        }
    }
}

void ExplorerDriver::loadStyleFiles()
{
    QFile simCenterStyle(":/styleCommon/style.qss");
    if (!simCenterStyle.open(QFile::ReadOnly))
    {
        ae_globals::displayFatalPopup("Missing file for graphics style. Your install is probably corrupted.");
    }
    QString commonStyle = QLatin1String(simCenterStyle.readAll());

    qApp->setStyleSheet(commonStyle);
}
