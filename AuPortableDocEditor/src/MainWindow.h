#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QIcon>
#include <QListWidget>
#include <QMainWindow>
#include <QSizeF>
#include <QVector>

class QLabel;
class QAction;
class QTimer;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class PdfPageEditor;
class PdfPreviewRenderer;

// 页面列表：平铺布局，页面顺序用自定义的鼠标拖动调整——
// 按下后超过拖动阈值进入拖动，跟随光标的是半透明快照
// （原位置条目保持不动），指示线标示有效插入位置
// （自身页前/后为无效槽位），松手后才真正调整顺序。
class PageListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit PageListWidget(QWidget *parent = nullptr);

    // 计算视口坐标 pos 处的插入位置（0..count；count 表示插到末尾）
    int insertionRowAt(const QPoint &viewportPos) const;
    // 插入位置 row 的指示线几何（视口坐标；无内容时为空矩形）
    QRect indicatorRect(int row) const;
    // 该插入位置对当前拖动是否为无效槽位（落在自身页前/后，顺序不会改变）
    bool isNoOpSlot(int row) const;
    bool isDragging() const { return m_dragging; }

signals:
    void pagesReordered();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void paintEvent(QPaintEvent *event);

private:
    void startDrag();
    void buildGhost();
    void finishDrag(bool commit);

    QPoint m_pressPos;
    int m_pressedRow;
    bool m_dragging;
    QVector<int> m_dragRows; // 被拖动的行（升序）
    QPixmap m_ghost;         // 拖动时跟随光标的半透明快照
    QPoint m_ghostOffset;
    QPoint m_ghostPos;
    int m_indicatorRow;
};

// 主窗口：工具栏 + 页面列表 + 状态栏。
// 页面列表平铺排布（左到右换行）：窗口宽时每行显示更多页，
// 窄时自动换行。每项显示真实缩略图（无可用渲染后端时为占位图）
// 与页号、页面尺寸。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void openPath(const QString &path); // 命令行 / 拖拽入口

protected:
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
private slots:
    void openFile();
    void saveFile();
    void saveFileAs();
    void insertBlankBefore();
    void insertBlankAfter();
    void deleteSelected();
    void replaceSelected();
    void rotateSelected(int deltaDegrees);
    void renderNextThumbnail();
    void onSelectionChanged();
    void syncPagesFromList();

private:
    bool loadDocument(const QString &path);
    bool doSave(const QString &path);
    void rebuildPageList();
    void applyItemAppearance(int index);
    void updateActions();
    void updateStatus();
    void setModified(bool modified);
    void updateWindowTitle();
    bool maybeSave();
    QSizeF chooseBlankPageSize(int newIndex) const;
    void doInsertBlank(int newIndex);
    QList<int> selectedPageRows() const;
    QIcon pagePlaceholderIcon(int pageNumber) const;
    void scheduleThumbnails();
    void remapPreviewAfterInsert(int newIndex);
    void remapPreviewAfterDelete(const QVector<int> &deletedIndexes);
    void remapPreviewAfterReplace(int index, const QImage &pageImage);
    void refreshPreviewFromDisk();

    PdfPageEditor *m_doc;
    PdfPreviewRenderer *m_preview;  // 页面渲染后端，探测失败时为空
    bool m_previewAvailable;        // 当前文件能否用 m_preview 打开
    QString m_filePath;
    bool m_modified;
    PageListWidget *m_pageList;
    QLabel *m_statusLabel;
    QAction *m_saveAction;
    QAction *m_saveAsAction;
    QAction *m_insertBeforeAction;
    QAction *m_insertAfterAction;
    QAction *m_deleteAction;
    QAction *m_replaceAction;
    QTimer *m_thumbTimer;
    QVector<int> m_previewSource;   // 各页对应预览文件中的页号（-1 表示占位）
    QVector<QIcon> m_previewIcons;  // 已渲染的缩略图缓存（增删页时随页面平移）
    QVector<bool> m_previewRendered;
};

#endif // MAINWINDOW_H
