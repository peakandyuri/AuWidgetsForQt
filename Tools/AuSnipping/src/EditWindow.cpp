#include "EditWindow.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QCloseEvent>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "Notifier.h"
#include "Settings.h"

QToolButton *EditWindow::makeToolButton(const QString &text, const QString &tip,
                                        AnnotationCanvas::Tool tool, QWidget *parent)
{
    auto *btn = new QToolButton(parent);
    btn->setText(text);
    btn->setToolTip(tip);
    btn->setCheckable(true);
    btn->setAutoExclusive(true);
    btn->setMinimumWidth(52);
    btn->setFocusPolicy(Qt::NoFocus);
    connect(btn, &QToolButton::clicked, this, [this, tool] { m_canvas->setTool(tool); });
    return btn;
}

EditWindow::EditWindow(const QPixmap &image, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(tr("截图编辑"));
    setAttribute(Qt::WA_DeleteOnClose);
    // 主窗口隐藏期间本窗口是唯一可见顶层窗口，必须关掉“关窗即退出程序”
    setAttribute(Qt::WA_QuitOnClose, false);

    m_canvas = new AnnotationCanvas(image, this);

    // ---- 第一行：撤销 / 恢复 + 绘制工具 + 保存 / 复制 / 关闭 ----
    m_undoBtn = new QToolButton(this);
    m_undoBtn->setText(tr("撤销"));
    m_undoBtn->setToolTip(tr("撤销 (Ctrl+Z)"));
    m_undoBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_undoBtn, &QToolButton::clicked, m_canvas, &AnnotationCanvas::undo);

    m_redoBtn = new QToolButton(this);
    m_redoBtn->setText(tr("恢复"));
    m_redoBtn->setToolTip(tr("恢复 (Ctrl+Shift+Z / Ctrl+Y)"));
    m_redoBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_redoBtn, &QToolButton::clicked, m_canvas, &AnnotationCanvas::redo);

    m_toolButtons[0] = makeToolButton(tr("画笔"), tr("自由画笔"), AnnotationCanvas::PenTool, this);
    m_toolButtons[1] = makeToolButton(tr("直线"), tr("直线"), AnnotationCanvas::LineTool, this);
    m_toolButtons[2] = makeToolButton(tr("矩形"), tr("矩形框"), AnnotationCanvas::RectTool, this);
    m_toolButtons[3] = makeToolButton(tr("椭圆"), tr("椭圆"), AnnotationCanvas::EllipseTool, this);
    m_toolButtons[4] = makeToolButton(tr("文字"), tr("文字标注"), AnnotationCanvas::TextTool, this);
    m_toolButtons[0]->setChecked(true);

    auto *saveBtn = new QPushButton(tr("保存"), this);
    connect(saveBtn, &QPushButton::clicked, this, &EditWindow::saveAs);

    m_copyBtn = new QPushButton(tr("复制"), this);
    m_copyBtn->setToolTip(tr("把当前画面复制到剪贴板"));
    connect(m_copyBtn, &QPushButton::clicked, this, &EditWindow::copyToClipboard);

    auto *closeBtn = new QPushButton(tr("关闭"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

    auto *row1 = new QHBoxLayout;
    row1->setSpacing(6);
    row1->addWidget(m_undoBtn);
    row1->addWidget(m_redoBtn);
    row1->addSpacing(12);
    for (QToolButton *btn : m_toolButtons)
        row1->addWidget(btn);
    row1->addStretch();
    row1->addWidget(saveBtn);
    row1->addWidget(m_copyBtn);
    row1->addWidget(closeBtn);

    // ---- 第二行：颜色 + 粗细 + 字号 ----
    auto *row2 = new QHBoxLayout;
    row2->setSpacing(6);
    row2->addWidget(new QLabel(tr("颜色:"), this));

    const QColor swatches[] = {
        QColor(0xe5, 0x39, 0x35), QColor(0xfb, 0x8c, 0x00), QColor(0xfd, 0xd8, 0x35),
        QColor(0x43, 0xa0, 0x47), QColor(0x1e, 0x88, 0xe5), QColor(0x8e, 0x24, 0xaa),
        QColor(0x21, 0x21, 0x21), QColor(0xff, 0xff, 0xff)
    };
    bool firstSwatch = true;
    for (const QColor &c : swatches) {
        auto *btn = new QToolButton(this);
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setFixedSize(24, 24);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setToolTip(c.name().toUpper());
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { background: %1; border: 2px solid #9e9e9e; border-radius: 12px; }"
            "QToolButton:checked { border: 2px solid #1e88e5; }").arg(c.name()));
        connect(btn, &QToolButton::clicked, this, [this, c] { m_canvas->setColor(c); });
        if (firstSwatch) {
            btn->setChecked(true);
            firstSwatch = false;
        }
        row2->addWidget(btn);
    }

    auto *customBtn = new QToolButton(this);
    customBtn->setText(tr("更多…"));
    customBtn->setToolTip(tr("自定义颜色"));
    customBtn->setFocusPolicy(Qt::NoFocus);
    connect(customBtn, &QToolButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_canvas->color(), this, tr("选择颜色"));
        if (c.isValid())
            m_canvas->setColor(c);
    });
    row2->addWidget(customBtn);

    row2->addSpacing(12);
    row2->addWidget(new QLabel(tr("粗细:"), this));
    auto *widthSpin = new QSpinBox(this);
    widthSpin->setRange(1, 20);
    widthSpin->setValue(3);
    widthSpin->setToolTip(tr("画笔 / 图形线条粗细"));
    connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_canvas, &AnnotationCanvas::setPenWidth);
    row2->addWidget(widthSpin);

    row2->addSpacing(12);
    row2->addWidget(new QLabel(tr("字号:"), this));
    auto *fontSpin = new QSpinBox(this);
    fontSpin->setRange(8, 96);
    fontSpin->setValue(16);
    fontSpin->setToolTip(tr("文字标注的字体大小"));
    connect(fontSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_canvas, &AnnotationCanvas::setFontSize);
    row2->addWidget(fontSpin);
    row2->addStretch();

    // ---- 布局 ----
    auto *scroll = new QScrollArea(this);
    scroll->setWidget(m_canvas);
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);
    mainLayout->addLayout(row1);
    mainLayout->addLayout(row2);
    mainLayout->addWidget(scroll);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(central);

    // 画板 1:1 显示，超出屏幕时用滚动条
    QRect avail;
    if (QScreen *s = QGuiApplication::primaryScreen())
        avail = s->availableGeometry();
    QSize want = m_canvas->size() + QSize(44, 132);
    if (avail.isValid())
        want = want.boundedTo(avail.size() - QSize(40, 40));
    resize(want.expandedTo(QSize(460, 320)));
    if (avail.isValid())
        move(avail.center() - QRect(QPoint(0, 0), size()).center());

    connect(m_canvas, &AnnotationCanvas::stateChanged, this, &EditWindow::syncUndoRedo);
    connect(m_canvas, &AnnotationCanvas::contentsChanged, this, [this] {
        QGuiApplication::clipboard()->setPixmap(m_canvas->renderedImage());
    });

    // 撤销 / 恢复快捷键：Linux(UKUI) 习惯 Ctrl+Z / Ctrl+Shift+Z，
    // 同时兼容 Windows 习惯的 Ctrl+Y
    auto *undoShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Z")), this);
    connect(undoShortcut, &QShortcut::activated, m_canvas, &AnnotationCanvas::undo);
    auto *redoShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Z")), this);
    connect(redoShortcut, &QShortcut::activated, m_canvas, &AnnotationCanvas::redo);
    auto *redoAltShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Y")), this);
    connect(redoAltShortcut, &QShortcut::activated, m_canvas, &AnnotationCanvas::redo);

    syncUndoRedo();
    updateTitle();
}

void EditWindow::saveAs()
{
    QString dir = AppSettings::lastSaveDir();
    if (dir.isEmpty())
        dir = AppSettings::defaultSaveDir();
    QDir().mkpath(dir);

    // 默认文件名为截图时间
    const QString path = QFileDialog::getSaveFileName(
        this, tr("保存截图"),
        dir + QLatin1Char('/') + AppSettings::defaultFileName(),
        tr("PNG 图片 (*.png);;JPEG 图片 (*.jpg);;所有文件 (*)"));
    if (path.isEmpty())
        return;

    QString out = path;
    if (!out.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)
            && !out.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive)
            && !out.endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive))
        out += QStringLiteral(".png");

    QImage image = m_canvas->renderedImage().toImage();
    if (out.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive)
            || out.endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive)) {
        QImage flat(image.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, image);
        p.end();
        image = flat;
    }

    if (!image.save(out)) {
        QMessageBox::warning(this, tr("截图编辑"), tr("保存失败：%1").arg(out));
        return;
    }

    m_saved = true;
    m_savedPath = out;
    AppSettings::setLastSaveDir(QFileInfo(out).absolutePath());
    updateTitle();
    // 点击通知可用系统默认看图工具打开保存的文件
    Notifier::notify(tr("截图已保存"), out,
                     [out] { QDesktopServices::openUrl(QUrl::fromLocalFile(out)); });
}

void EditWindow::copyToClipboard()
{
    QGuiApplication::clipboard()->setPixmap(m_canvas->renderedImage());
    m_copyBtn->setText(tr("已复制 ✓"));
    QTimer::singleShot(1500, this, [this] { m_copyBtn->setText(tr("复制")); });
}

void EditWindow::syncUndoRedo()
{
    m_undoBtn->setEnabled(m_canvas->canUndo());
    m_redoBtn->setEnabled(m_canvas->canRedo());
}

void EditWindow::closeEvent(QCloseEvent *event)
{
    if (!m_saved && m_canvas->isModified()) {
        QMessageBox box(this);
        box.setWindowTitle(tr("截图编辑"));
        box.setText(tr("标注尚未保存，确定退出？\n（当前画面仍在剪贴板中）"));
        QPushButton *saveBtn = box.addButton(tr("保存"), QMessageBox::AcceptRole);
        QPushButton *discardBtn = box.addButton(tr("放弃修改"), QMessageBox::DestructiveRole);
        box.addButton(tr("取消"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == saveBtn) {
            saveAs();
            if (!m_saved) {
                event->ignore();
                return;
            }
        } else if (box.clickedButton() != discardBtn) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void EditWindow::updateTitle()
{
    setWindowTitle(m_saved ? tr("截图编辑 - %1").arg(m_savedPath) : tr("截图编辑"));
}
