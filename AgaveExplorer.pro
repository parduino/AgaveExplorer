##################################################################################
#
# Copyright (c) 2017 The University of Notre Dame
# Copyright (c) 2017 The Regents of the University of California
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this
# list of conditions and the following disclaimer in the documentation and/or other
# materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may
# be used to endorse or promote products derived from this software without specific
# prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
# SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
####################################################################################

# Contributors:
# Written by Peter Sempolinski, for the Natural Hazard Modeling Laboratory, director: Ahsan Kareem, at Notre Dame

QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = AgaveExplorer
TEMPLATE = app

SOURCES += main.cpp \
    ../AgaveClientInterface/agaveInterfaces/agavehandler.cpp \
    ../AgaveClientInterface/agaveInterfaces/agavetaskguide.cpp \
    ../AgaveClientInterface/agaveInterfaces/agavetaskreply.cpp \
    ../AgaveClientInterface/agaveInterfaces/agavepipebuffer.cpp \
    ../AgaveClientInterface/remotedatainterface.cpp \
    ../AgaveClientInterface/filemetadata.cpp \
    ../AgaveClientInterface/remotejobdata.cpp \
    remoteFileOps/easyboollock.cpp \
    remoteFileOps/fileoperator.cpp \
    remoteFileOps/filetreenode.cpp \
    remoteFileOps/joboperator.cpp \
    remoteFileOps/remotefiletree.cpp \
    remoteFileOps/remotejoblister.cpp \
    instances/explorerdriver.cpp \
    instances/explorerwindow.cpp \
    SimCenterCommon/FooterWidget.cpp \
    SimCenterCommon/HeaderWidget.cpp \
    utilFuncs/agavesetupdriver.cpp \
    utilFuncs/authform.cpp \
    utilFuncs/copyrightdialog.cpp \
    utilFuncs/singlelinedialog.cpp \
    remoteFileOps/selectedfilelabel.cpp \
    utilFuncs/linkedstandarditem.cpp \
    remoteFileOps/joblistnode.cpp \
    ae_globals.cpp

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    ../AgaveClientInterface/agaveInterfaces/agavehandler.h \
    ../AgaveClientInterface/agaveInterfaces/agavetaskguide.h \
    ../AgaveClientInterface/agaveInterfaces/agavetaskreply.h \
    ../AgaveClientInterface/remotedatainterface.h \
    ../AgaveClientInterface/filemetadata.h \
    ../AgaveClientInterface/remotejobdata.h \
    remoteFileOps/easyboollock.h \
    remoteFileOps/fileoperator.h \
    remoteFileOps/filetreenode.h \
    remoteFileOps/joboperator.h \
    remoteFileOps/remotefiletree.h \
    remoteFileOps/remotejoblister.h \
    instances/explorerdriver.h \
    instances/explorerwindow.h \
    SimCenterCommon/FooterWidget.h \
    SimCenterCommon/HeaderWidget.h \
    utilFuncs/agavesetupdriver.h \
    utilFuncs/authform.h \
    utilFuncs/copyrightdialog.h \
    utilFuncs/singlelinedialog.h \
    remoteFileOps/selectedfilelabel.h \
    ../AgaveClientInterface/agaveInterfaces/agavepipebuffer.h \
    utilFuncs/linkedstandarditem.h \
    remoteFileOps/joblistnode.h \
    ae_globals.h

FORMS += \
    instances/explorerwindow.ui \
    utilFuncs/authform.ui \
    utilFuncs/copyrightdialog.ui \
    utilFuncs/singlelinedialog.ui

RESOURCES += \
    SimCenterCommon/commonResources.qrc \
    instances/explorerfiles.qrc
