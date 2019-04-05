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
// Written: fmckenna
// Modified by Peter Sempolinski, for the Natural Hazard Modeling Laboratory, director: Ahsan Kareem, at Notre Dame

#include "HeaderWidget.h"

//
// headers for HeaderWidgetDistribution subclasses that user can select
//

HeaderWidget::HeaderWidget(QWidget *parent)
    :QFrame(parent)
{
    QHBoxLayout * layout = new QHBoxLayout();
    QHBoxLayout * leftLayout = new QHBoxLayout();
    subLayout = new QHBoxLayout();

    titleText = new QLabel();
    titleText->setObjectName(QString::fromUtf8("titleText"));
    titleText->setText(tr("A SimCenter Application"));

    leftLayout->setAlignment(Qt::AlignLeft); //can this be done in CSS???
    leftLayout->addWidget(titleText);

//    commented this out so that in CFDClientProgram/mainWindow/cwe_mainwindow.cpp
//    we could add the status line to the center by specifying on the new widget
//    setAlignment(Qt::AlignHCenter) and setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred)
//    see lines 86-89 of that file
//    subLayout->setAlignment(Qt::AlignRight);

    layout->addLayout(leftLayout);
    layout->addLayout(subLayout);
    this->setLayout(layout);
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

HeaderWidget::~HeaderWidget()
{

}

void 
HeaderWidget::setHeadingText(const QString &newText)
{
  titleText->setText(newText);
}

void HeaderWidget::appendWidget(QWidget * newWidget)
{
    newWidget->setParent(this);
    subLayout->addWidget(newWidget);
}
