#include "fixforssl.h"

#ifdef Q_OS_WIN
#include "ui_fixforssl.h"
#endif

#include "../AgaveExplorer/ae_globals.h"

bool FixForSSL::performSSLcheck()
{
    if (QSslSocket::supportsSsl() == false)
    {
        //TODO: Lines commented out for debug purposes
        #ifdef Q_OS_WIN
            FixForSSL * sslWindow = new FixForSSL();
            sslWindow->show();
        #else
            ae_globals::displayFatalPopup("SSL support was not detected on this computer.\nPlease insure that some version of SSL is installed,\nNormally, this comes with all Linux and Mac computers, so your OS install may be broken. However, due to bugs in the most recent Ubuntu, (18.04), CWE cannot connect with SSL in that version. We are working on fixing this.");
        #endif
        return false;
    }
    return true;
}

#ifdef Q_OS_WIN

FixForSSL::FixForSSL(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::FixForSSL)
{
    ui->setupUi(this);
}

FixForSSL::~FixForSSL()
{
    delete ui;
}

void FixForSSL::exitProgram()
{
    qApp->exit(0);
}

void FixForSSL::showLicense()
{
    QDesktopServices::openUrl(QUrl("https://www.openssl.org/source/license.html"));
}

void FixForSSL::getSSL()
{
    QNetworkReply * clientReply = NULL;

    QString activeURL = "http://indy.fulgan.com/SSL/openssl-1.0.2k-x64_86-win64.zip";

    QNetworkRequest * clientRequest = new QNetworkRequest();
    clientRequest->setUrl(QUrl(activeURL));

    clientReply = networkManger.get(*clientRequest);

    if (clientReply == NULL)
    {
        ae_globals::displayFatalPopup("CWE program unable to open http conenction. Please check your internet connection and try again.");
        return;
    }

    QObject::connect(clientReply, SIGNAL(finished()), this, SLOT(getDownloadReply()));
    ui->head_label->setText("Downloading files. Please wait.");
    ui->label_2->hide();
    ui->label_3->hide();
    ui->btn_install->setEnabled(false);
}


void FixForSSL::getDownloadReply()
{
    QNetworkReply * downloadReply = (QNetworkReply *)QObject::sender();
    QString zipFileName = "openssl-1.0.2k-x64_86-win64.zip";

    if (downloadReply->error() != QNetworkReply::NoError)
    {
        ae_globals::displayFatalPopup("CWE program unable complete download. Please check your internet connection and try again.");
        return;
    }

    QByteArray theData = downloadReply->readAll();
    QDir myProgramFolder = QCoreApplication::applicationDirPath();
    QFile downloadedFile(myProgramFolder.filePath(zipFileName));

    if (!downloadedFile.open(QIODevice::WriteOnly))
    {
        ae_globals::displayFatalPopup("CWE program unable to write to local system. Please check your file permissions and try again.");
        return;
    }
    downloadedFile.write(theData);
    downloadedFile.close();

    QString program = "powershell.exe -nologo -noprofile -command \"& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('%1', 'openssl'); }\"";
    program = program.arg(zipFileName);
    QProcess unzipProgram;
    unzipProgram.setWorkingDirectory(myProgramFolder.absolutePath());
    unzipProgram.start(program);

    if (!unzipProgram.waitForFinished(15000))
    {
        ae_globals::displayFatalPopup("CWE program unable extract SSL libraries from download. Please install a version openssl manually. 1");
        return;
    }

    QDir sslFolder = myProgramFolder;
    if (!sslFolder.cd("openssl"))
    {
        ae_globals::displayFatalPopup("CWE program unable extract SSL libraries from download. Please install a version openssl manually. 3");
        return;
    }

    if (!QFile::copy(sslFolder.absoluteFilePath("libeay32.dll"), myProgramFolder.absoluteFilePath("libeay32.dll")))
    {
        ae_globals::displayFatalPopup("CWE program unable extract SSL libraries from download. Please install a version openssl manually. 4");
        return;
    }

    if (!QFile::copy(sslFolder.absoluteFilePath("ssleay32.dll"), myProgramFolder.absoluteFilePath("ssleay32.dll")))
    {
        ae_globals::displayFatalPopup("CWE program unable extract SSL libraries from download. Please install a version openssl manually. 5");
        return;
    }

    ae_globals::displayPopup("SSL setup complete. Please restart CWE program to use.","Setup Somplete");
    qApp->exit(0);
}
#else
FixForSSL::FixForSSL() {}
#endif
