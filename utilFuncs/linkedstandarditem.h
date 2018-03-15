#ifndef LINKEDSTANDARDITEM_H
#define LINKEDSTANDARDITEM_H

#include <QStandardItem>

class LinkedStandardItem : public QStandardItem
{
public:
    LinkedStandardItem(QObject * linkedObject);
    LinkedStandardItem(QObject * linkedObject, QString text);

    QObject * getLinkedObject();

private:
    QObject * myLinkedObject;
};

#endif // LINKEDSTANDARDITEM_H
