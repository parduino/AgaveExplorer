#include "selectedfilelabel.h"

#include "filetreenode.h"
#include "remotefiletree.h"
#include "../AgaveClientInterface/filemetadata.h"

SelectedFileLabel::SelectedFileLabel(QWidget *parent) : QLabel(parent)
{
    newSelectedItem(NULL);
}

void SelectedFileLabel::connectFileTreeWidget(RemoteFileTree * connectedTree)
{
    if (myFileTree != NULL)
    {
        QObject::disconnect(myFileTree, 0, this, 0);
    }
    myFileTree = connectedTree;
    if (myFileTree == NULL)
    {
        newSelectedItem(NULL);
        return;
    }
    QObject::connect(myFileTree, SIGNAL(newFileSelected(FileTreeNode*)),
                     this, SLOT(newSelectedItem(FileTreeNode*)));
    newSelectedItem(myFileTree->getSelectedNode());
}

void SelectedFileLabel::newSelectedItem(FileTreeNode * newFileData)
{
    if (newFileData == NULL)
    {
        this->setText("No File Selected.");
    }
    else
    {
        FileMetaData theFileData = newFileData->getFileData();

        QString fileString = "Filename: %1\nType: %2\nSize: %3";
        fileString = fileString.arg(theFileData.getFileName(),
                                    theFileData.getFileTypeString(),
                                    QString::number(theFileData.getSize()));
        this->setText(fileString);
    }
}
