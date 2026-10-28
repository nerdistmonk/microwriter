#include <QApplication>
#include "microwriter.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    MicroWriter window;

    QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        window.openFilePath(args.at(i));
    }

    window.show();

    return app.exec();
}
