
NEEDED_REPO=AgaveClientInterface

NEEDED_PRI=$$PWD/../$$NEEDED_REPO/"$$NEEDED_REPO".pri
!exists( $$NEEDED_PRI ) {
    message("Needed Git repo $$NEEDED_REPO not found. This project requires $$NEEDED_REPO from https://github.com/NHERI-SimCenter.")
}

include($$NEEDED_PRI)

INCLUDEPATH += "$$PWD/"

SOURCES += \
    $$PWD/utilFuncs/agavesetupdriver.cpp \
    $$PWD/utilFuncs/authform.cpp \
    $$PWD/utilFuncs/copyrightdialog.cpp \
    $$PWD/utilFuncs/singlelinedialog.cpp \
    $$PWD/ae_globals.cpp \   
    $$PWD/commonUI/FooterWidget.cpp \
    $$PWD/commonUI/HeaderWidget.cpp

HEADERS += \
    $$PWD/utilFuncs/agavesetupdriver.h \
    $$PWD/utilFuncs/authform.h \
    $$PWD/utilFuncs/copyrightdialog.h \
    $$PWD/utilFuncs/singlelinedialog.h \
    $$PWD/ae_globals.h \
    $$PWD/commonUI/FooterWidget.h \
    $$PWD/commonUI/HeaderWidget.h

FORMS += \
    $$PWD/utilFuncs/authform.ui \
    $$PWD/utilFuncs/copyrightdialog.ui \
    $$PWD/utilFuncs/singlelinedialog.ui

RESOURCES += \
    $$PWD/commonUI/commonResources.qrc \
