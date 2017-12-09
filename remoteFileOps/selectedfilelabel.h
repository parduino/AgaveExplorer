#ifndef SELECTEDFILELABEL_H
#define SELECTEDFILELABEL_H

#include <QObject>
#include <QLabel>

class FileTreeNode;
class RemoteFileTree;

class SelectedFileLabel : public QLabel
{
    Q_OBJECT

public:
    SelectedFileLabel(QWidget *parent=Q_NULLPTR);

    void connectFileTreeWidget(RemoteFileTree * connectedTree);

private slots:
    void newSelectedItem(FileTreeNode * newFileData);

private:
    RemoteFileTree * myFileTree = NULL;
};

#endif // SELECTEDFILELABEL_H
