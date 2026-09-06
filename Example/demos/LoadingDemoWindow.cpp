#include "LoadingDemoWindow.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridWidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

LoadingDemoWindow::LoadingDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"加载遮罩"),
                     QStringLiteral(u"setLoading(true / false) 由调用方控制显隐时机，"
                                     u"半透明蒙层 + 旋转圆环 + 文案，带淡入淡出，适用于数据请求等异步场景。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    m_model = new AgGridModel(this);
    m_model->setColumnDefs(makeStandardColumns());
    m_model->setRowData(makeFlatRows(50));

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setRowNumberColumnVisible(true);
    m_grid->setCheckboxColumnVisible(true);

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *toggleBtn = new QPushButton(QStringLiteral(u"Loading 开关"), this);
    toggleBtn->setCheckable(true);
    QPushButton *requestBtn = new QPushButton(QStringLiteral(u"模拟请求数据"), this);
    buttons->addWidget(toggleBtn);
    buttons->addWidget(requestBtn);
    buttons->addStretch(1);

    connect(toggleBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_grid->setLoading(checked);
    });
    connect(requestBtn, &QPushButton::clicked, this, [this]() {
        // 模拟异步请求：发起前显示遮罩，数据"返回"后写入新数据并关闭遮罩
        m_grid->setLoading(true);
        m_grid->setLoadingText(QStringLiteral(u"正在请求数据…"));
        QTimer::singleShot(2000, this, [this]() {
            m_model->setRowData(makeFlatRows(80));   // 数据返回后刷新模型
            m_grid->setLoading(false);
        });
    });

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("遮罩显示期间：鼠标点击 / 双击 / 滚轮 / 键盘 / 右键菜单均被屏蔽，"
            "悬停高亮与手型光标一并清除；滚动条隐藏，淡出结束后按内容恢复。");
    addNote("打开中的编辑器自动关闭（不提交）；进行中的行 / 列拖拽自动取消。");
    addNote("蒙层 / 圆环 / 文字颜色与默认文字由样式键 Loading* 控制（见文档样式键章节）。");
}
