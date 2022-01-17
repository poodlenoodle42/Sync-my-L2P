#include <QApplication>
#include <QObject>

#include "mymainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MyMainWindow w;
    w.show();
    
    QObject::connect(&a, SIGNAL(aboutToQuit()), &w, SLOT(closeTask()));
    return a.exec();
}
