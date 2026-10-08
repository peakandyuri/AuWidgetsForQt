#include "MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStringList>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // 高分屏缩放（Qt6 默认启用）
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("AuPortableDocEditor"));
    QApplication::setOrganizationName(QStringLiteral("AuWidgets"));
    // 窗口 / 任务栏图标（exe 文件本身的图标由 .rc / RC_ICONS 内嵌）
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/logo.png")));

    MainWindow window;
    window.show();

    // 命令行参数中的 PDF 文件直接打开
    const QStringList args = QApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i).endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)
                && QFile::exists(args.at(i))) {
            window.openPath(args.at(i));
            break;
        }
    }
    return app.exec();
}
