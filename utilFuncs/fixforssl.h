#ifndef FIXFORSSL_H
#define FIXFORSSL_H

#include <QMainWindow>
#include <QSslSocket>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDir>
#include <QFile>
#include <QProcess>

#ifdef Q_OS_WIN
namespace Ui {
class FixForSSL;
}

class FixForSSL : public QMainWindow
{
    Q_OBJECT

public:
    explicit FixForSSL(QWidget *parent = nullptr);
    ~FixForSSL();

    static bool performSSLcheck();

private slots:
    void exitProgram();
    void showLicense();
    void getSSL();

    void getDownloadReply();

private:
    Ui::FixForSSL *ui;

    QNetworkAccessManager networkManger;
};
#else
class FixForSSL : public QObject
{
    Q_OBJECT
public:
    FixForSSL();
    static bool performSSLcheck();
};
#endif

#endif // FIXFORSSL_H
