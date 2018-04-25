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

#include "authform.h"
#include "ui_authform.h"

#include "../AgaveClientInterface/remotedatainterface.h"
#include "copyrightdialog.h"

#include "agavesetupdriver.h"

AuthForm::AuthForm(AgaveSetupDriver * theDriver, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AuthForm)
{
    ui->setupUi(this);

    myDriver = theDriver;
    theConnection = myDriver->getDataConnection();

    QLabel * versionLabel = new QLabel(myDriver->getVersion());

    ui->headerBox->setHeadingText(myDriver->getBanner());
    ui->headerBox->appendWidget(versionLabel);

    QPushButton * copyButton = new QPushButton("Click Here for Copyright and License");
    QObject::connect(copyButton, SIGNAL(clicked(bool)), this, SLOT(getCopyingInfo()));
    ui->headerBox->appendWidget(copyButton);

    ui->footerBox->condense();

    this->setTabOrder(ui->unameInput, ui->passwordInput);
    this->setTabOrder(ui->passwordInput, ui->loginButton);
    this->setTabOrder(ui->loginButton, ui->quitButton);
}

AuthForm::~AuthForm()
{
    delete ui;
}

void AuthForm::getCopyingInfo()
{
    CopyrightDialog copyrightPopup;
    copyrightPopup.exec();
}

void AuthForm::exitAuth()
{
    myDriver->shutdown();
}

void AuthForm::performAuth()
{
    if (authInProgress) return;

    QString unameText = ui->unameInput->text();
    QString passText = ui->passwordInput->text();

    RemoteDataReply * authReply = theConnection->performAuth(unameText, passText);

    if (authReply == NULL)
    {
        myDriver->fatalInterfaceError("Unable to connect to DesignSafe. Please check internet connection.");
        return;
    }
    this->setCursor(QCursor(Qt::WaitCursor));

    ui->instructText->setText("Connecting to DesignSafe");
    QObject::connect(authReply,SIGNAL(haveAuthReply(RequestState)),this,SLOT(getAuthReply(RequestState)));
    QObject::connect(authReply,SIGNAL(haveAuthReply(RequestState)),myDriver, SLOT(getAuthReply(RequestState)));
    ui->loginButton->setEnabled(false);
}

void AuthForm::getAuthReply(RequestState authReply)
{
    if (authReply == RequestState::GOOD)
    {
        ui->instructText->setText("Loading . . .");
    }
    else if (authReply == RequestState::FAIL)
    {
        ui->instructText->setText("Username/Password combination incorrect, verify your credentials and try again.");
        ui->loginButton->setEnabled(true);
    }
    else if (authReply == RequestState::NO_CONNECT)
    {
        ui->instructText->setText("Unable to contact DesignSafe, verify your connection and try again.");
        ui->loginButton->setEnabled(true);
    }
    else
    {
        myDriver->fatalInterfaceError("Authentication Problems Detected");
    }
    this->unsetCursor();
}
