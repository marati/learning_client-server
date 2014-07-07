#include <QtGui/QApplication>
#include "client.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Client udpClient;
    udpClient.show();

    return app.exec();
}
