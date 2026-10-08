#include <QApplication>
#include <QClipboard>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>

#include "AnnotationCanvas.h"
#include "Capturer.h"
#include "EditWindow.h"
#include "Launcher.h"
#include "SnipOverlay.h"

namespace {

// 无头自检：标注渲染、撤销/恢复、保存与剪贴板
int runSelfTest()
{
    QImage base(320, 240, QImage::Format_ARGB32_Premultiplied);
    base.fill(Qt::white);
    {
        QPainter pt(&base);
        pt.setPen(Qt::gray);
        pt.drawRect(10, 10, 299, 219);
    }

    AnnotationCanvas canvas(QPixmap::fromImage(base));

    AnnotationItem pen;
    pen.type = AnnotationItem::Pen;
    pen.color = Qt::red;
    pen.penWidth = 3;
    for (int i = 0; i <= 10; ++i)
        pen.points << QPointF(20 + i * 8, 30 + i * 4);
    canvas.commitItem(pen);

    AnnotationItem rectItem;
    rectItem.type = AnnotationItem::Rect;
    rectItem.color = Qt::blue;
    rectItem.points << QPointF(40, 60) << QPointF(200, 150);
    canvas.commitItem(rectItem);

    AnnotationItem textItem;
    textItem.type = AnnotationItem::Text;
    textItem.color = Qt::black;
    textItem.fontSize = 18;
    textItem.text = QStringLiteral("自检文字");
    textItem.points << QPointF(60, 40);
    canvas.commitItem(textItem);

    const QPixmap rendered = canvas.renderedImage();
    if (rendered.isNull()) {
        qWarning("SELFTEST FAIL: rendered image is null");
        return 1;
    }
    if (!canvas.canUndo() || canvas.canRedo()) {
        qWarning("SELFTEST FAIL: undo/redo state");
        return 1;
    }
    canvas.undo();
    if (!canvas.canRedo()) {
        qWarning("SELFTEST FAIL: redo after undo");
        return 1;
    }
    canvas.redo();

    // 会话期间主窗口隐藏，编辑窗 / 遮罩层是仅有的可见顶层窗口，
    // 必须关闭 WA_QuitOnClose，否则“关窗”会变成“退出整个程序”
    {
        EditWindow editorProbe(QPixmap::fromImage(base));
        SnipOverlay overlayProbe(ScreenGrab(), SnipOverlay::RegionMode, false);
        if (editorProbe.testAttribute(Qt::WA_QuitOnClose)
                || overlayProbe.testAttribute(Qt::WA_QuitOnClose)) {
            qWarning("SELFTEST FAIL: transient windows must not quit on close");
            return 1;
        }
    }

    const QString path = QDir(QDir::tempPath())
            .absoluteFilePath(QStringLiteral("ausnipping_selftest.png"));
    if (!rendered.toImage().save(path)) {
        qWarning("SELFTEST FAIL: save to %s", qPrintable(path));
        return 1;
    }
    QGuiApplication::clipboard()->setPixmap(rendered);

    // 抓屏在 offscreen 平台内容可为空，但不应崩溃
    const ScreenGrab grab = Capturer::grab();

    qInfo("SELFTEST OK %s (grab valid: %d)", qPrintable(path), int(grab.valid));
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5 启用高分屏缩放（Qt6 默认开启）
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("AuSnipping"));
    QApplication::setApplicationName(QStringLiteral("AuSnipping"));
    QApplication::setApplicationDisplayName(QStringLiteral("AuSnipping"));
    // 窗口 / 任务栏图标（exe 文件本身的图标由 .rc / RC_ICONS 内嵌）
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/logo.png")));

    // 全局字体统一为微软雅黑；Linux（银河麒麟）通常没有该字体，
    // 按序回退到系统中常见的中文字体
    QFont::insertSubstitutions(QStringLiteral("Microsoft YaHei"), {
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Source Han Sans CN"),
        QStringLiteral("WenQuanYi Micro Hei"),
        QStringLiteral("WenQuanYi Zen Hei"),
    });
    QFont appFont(QStringLiteral("Microsoft YaHei"), 9);
    app.setFont(appFont);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "AuSnipping 截图工具：全屏 / 区域 / 窗口截图，支持标注编辑。\n"
        "不带参数启动时显示截图主界面。"));
    parser.addHelpOption();

    QCommandLineOption fullscreenOpt(QStringLiteral("fullscreen"),
                                     QStringLiteral("启动后立即进行全屏截图"));
    QCommandLineOption regionOpt(QStringLiteral("region"),
                                 QStringLiteral("启动后立即进行区域截图"));
    QCommandLineOption windowOpt(QStringLiteral("window"),
                                 QStringLiteral("启动后立即进行窗口截图"));
    QCommandLineOption selftestOpt(QStringLiteral("selftest"),
                                   QStringLiteral("运行无头自检后退出"));
    parser.addOptions({ fullscreenOpt, regionOpt, windowOpt, selftestOpt });
    parser.process(app);

    if (parser.isSet(selftestOpt))
        return runSelfTest();

    const bool quitAfter = parser.isSet(fullscreenOpt) || parser.isSet(regionOpt)
            || parser.isSet(windowOpt);

    Launcher launcher(quitAfter);
    if (parser.isSet(fullscreenOpt))
        launcher.startMode(Launcher::Fullscreen);
    else if (parser.isSet(regionOpt))
        launcher.startMode(Launcher::Region);
    else if (parser.isSet(windowOpt))
        launcher.startMode(Launcher::Window);
    else
        launcher.show();

    return app.exec();
}
