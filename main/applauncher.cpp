#include "applauncher.h"
#include "microwriter.h"
#include "ncurses.h"

#include <QApplication>
#include <cstring>

static bool hasTerminalFlag(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-c") == 0 || std::strcmp(argv[i], "--console") == 0 ||
            std::strcmp(argv[i], "-C") == 0 || std::strcmp(argv[i], "--code") == 0) {
            return true;
        }
    }
    return false;
}

int launchApplication(int argc, char *argv[]) {
    if (hasTerminalFlag(argc, argv)) {
        return runCursesApp(argc, argv);
    }

    // Unmodified Qt GUI initialization path
    QApplication app(argc, argv);
    MicroWriter window;

    QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        window.openFilePath(args.at(i));
    }

    window.show();
    return app.exec();
}
