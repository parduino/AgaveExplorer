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

#ifndef COPYRIGHTDIALOG_H
#define COPYRIGHTDIALOG_H

#include <QDialog>
#include <QFile>

namespace Ui {
class CopyrightDialog;
}

/*! \brief The CopyrightDialog is a window displaying copyright and license info.
 *
 *  This simple class contains a text panel to display the information, and a button to close the window. It is a subclass of QDialog and will get its text from ":/copyText.txt". Once created, the object should be shown with exec() or show().
 */
class CopyrightDialog : public QDialog
{
    Q_OBJECT

public:
    /*! \brief The CopyrightDialog will retrive and load the copyright text.
     *
     *  \param parent As a window, this object usually will not have a parent.
     *
     *  After construction, use show() or exec() to display the window.
     */
    explicit CopyrightDialog(QWidget *parent = nullptr);
    ~CopyrightDialog();

private:
    Ui::CopyrightDialog *ui;
};

#endif // COPYRIGHTDIALOG_H
