#pragma once

#include <QWidget>

#include "AnnotationCanvas.h"

class QPushButton;
class QSpinBox;
class QToolButton;

// 截图编辑窗口：简易画板 + 保存 / 复制。
// 保存目录默认为上一次选择的目录，文件名默认为截图时间；
// 标注内容每次变化都会同步到剪贴板。
class EditWindow : public QWidget
{
    Q_OBJECT

public:
    explicit EditWindow(const QPixmap &image, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void saveAs();
    void copyToClipboard();
    void syncUndoRedo();

private:
    QToolButton *makeToolButton(const QString &text, const QString &tip,
                                AnnotationCanvas::Tool tool, QWidget *parent);
    void updateTitle();

    AnnotationCanvas *m_canvas = nullptr;
    QToolButton *m_undoBtn = nullptr;
    QToolButton *m_redoBtn = nullptr;
    QToolButton *m_toolButtons[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    QPushButton *m_copyBtn = nullptr;
    bool m_saved = false;
    QString m_savedPath;
};
