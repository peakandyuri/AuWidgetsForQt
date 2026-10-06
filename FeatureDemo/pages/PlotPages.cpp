#include "PlotPages.h"

#include "PageHelpers.h"

#include "AuPlot.h"
#include "AuPlotAxis.h"
#include "AuPlotCurve.h"
#include "AuPlotPrimitive.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSharedPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>
#include <chrono>

// MSVC 的 M_PI 依赖 _USE_MATH_DEFINES 且受头文件包含顺序影响，直接兜底定义
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

/// 演示页布局：顶部控制条（可空）+ 曲线本体 + 可选底部日志。
QWidget *plotPanel(AuPlot *plot, QHBoxLayout *controls = nullptr, QWidget *footer = nullptr)
{
    QWidget *body = new QWidget;
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    v->addWidget(plot, 1);
    return PageHelpers::composeDemo(controls, body, footer);
}

/// 生成 0~10 的正弦叠加曲线数据。
QVector<QPointF> sineWave(int count = 200, double amplitude = 3.0, double slope = 0.5)
{
    QVector<QPointF> points;
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        const double x = 10.0 * i / (count - 1);
        points.append(QPointF(x, qSin(x) * amplitude + x * slope));
    }
    return points;
}

} // namespace

QVector<FeaturePage> makePlotPages()
{
    QVector<FeaturePage> pages;

    // ============ 1. 基础曲线与背景网格 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"基础曲线与网格");
        page.description = QStringLiteral(
            u"默认单轴（底部 X1 + 左侧 Y1），一条正弦曲线；背景网格默认虚线（横向 + 纵向），"
            u"可用下拉框切换线型与方向——线型、颜色、开关都逐轴独立配置。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;
            AuPlotCurve *curve = new AuPlotCurve;
            curve->setPoints(sineWave());
            plot->addItem(curve);

            auto *styleBox = new QComboBox;
            styleBox->addItems({ QStringLiteral(u"虚线(默认)"), QStringLiteral(u"实线"),
                                 QStringLiteral(u"点线"), QStringLiteral(u"无网格") });
            QObject::connect(styleBox, QOverload<int>::of(&QComboBox::currentIndexChanged), plot,
                             [plot](int index) {
                                 if (index == 3) {
                                     plot->xAxis()->setGridVisible(false);
                                     plot->yAxis()->setGridVisible(false);
                                 } else {
                                     plot->xAxis()->setGridVisible(true);
                                     plot->yAxis()->setGridVisible(true);
                                     plot->xAxis()->setGridStyle(index == 0 ? Qt::DashLine
                                                                : index == 1 ? Qt::SolidLine
                                                                             : Qt::DotLine);
                                     plot->yAxis()->setGridStyle(index == 0 ? Qt::DashLine
                                                                : index == 1 ? Qt::SolidLine
                                                                             : Qt::DotLine);
                                 }
                             });

            auto *dirBox = new QComboBox;
            dirBox->addItems({ QStringLiteral(u"横向+纵向(默认)"), QStringLiteral(u"仅横向"),
                               QStringLiteral(u"仅纵向") });
            QObject::connect(dirBox, QOverload<int>::of(&QComboBox::currentIndexChanged), plot, [plot](int index) {
                plot->xAxis()->setGridVisible(index != 2);
                plot->yAxis()->setGridVisible(index != 1);
            });

            auto *layout = new QHBoxLayout;
            layout->addWidget(new QLabel(QStringLiteral(u"网格线型:")));
            layout->addWidget(styleBox);
            layout->addSpacing(12);
            layout->addWidget(new QLabel(QStringLiteral(u"网格方向:")));
            layout->addWidget(dirBox);
            layout->addStretch(1);
            return plotPanel(plot, layout);
        };
        page.code = QStringLiteral(
            "AuPlot *plot = new AuPlot;\n"
            "\n"
            "// 曲线：坐标按 X1/Y1 轴的数据值\n"
            "AuPlotCurve *curve = new AuPlotCurve;\n"
            "for (int i = 0; i < 200; ++i) {\n"
            "    double x = 10.0 * i / 199;\n"
            "    curve->addPoint(x, qSin(x) * 3 + x * 0.5);\n"
            "}\n"
            "plot->addItem(curve);          // 控件接管所有权，zLevel 默认 0\n"
            "\n"
            "// 背景网格：默认横向+纵向、虚线，逐轴独立配置\n"
            "plot->xAxis()->setGridStyle(Qt::DashLine);  // 纵向网格（X 轴）\n"
            "plot->yAxis()->setGridStyle(Qt::SolidLine); // 横向网格（Y 轴）\n"
            "plot->yAxis()->setGridColor(QColor(\"#c0d8f0\"));\n"
            "plot->xAxis()->setGridVisible(false);       // 只留横向\n");
        pages << page;
    }

    // ============ 2. 双轴 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"双轴");
        page.description = QStringLiteral(
            u"setDualAxisEnabled(true) 同时开启顶部 X2 与右侧 Y2（也可用 "
            u"xAxis2()->setVisible() 单独开）。两条曲线量纲不同：一条绑 Y1，一条绑 Y2，"
            u"各自按绑定轴自适应量程；Y2 轴设置了独立轴色便于区分。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;
            plot->setDualAxisEnabled(true);

            AuPlotCurve *temperature = new AuPlotCurve;   // 绑默认 Y1
            AuPlotCurve *humidity = new AuPlotCurve;      // 绑右侧 Y2
            for (int i = 0; i <= 48; ++i) {
                const double x = i / 2.0;
                temperature->addPoint(x, 15 + qSin(x / 3.0) * 8 + i * 0.1);
                humidity->addPoint(x, 60 - qSin(x / 4.0) * 25 - i * 0.2);
            }
            plot->addItem(temperature);
            plot->addItem(humidity);
            humidity->setYAxis(AuPlotAxisPos::Y2);
            humidity->setLineColor(QColor(0x27, 0xae, 0x60));

            // Y2 轴独立颜色（无效时用全局样式的轴色）
            plot->yAxis2()->setAxisColor(QColor(0x27, 0xae, 0x60));
            return plotPanel(plot);
        };
        page.code = QStringLiteral(
            "AuPlot *plot = new AuPlot;\n"
            "plot->setDualAxisEnabled(true);   // 显示顶部 X2 + 右侧 Y2\n"
            "\n"
            "// 曲线一：默认绑定 X1 + Y1\n"
            "AuPlotCurve *temperature = new AuPlotCurve;\n"
            "// ... addPoint(时间, 温度)\n"
            "plot->addItem(temperature);\n"
            "\n"
            "// 曲线二：绑定右侧 Y2（X 轴仍可用 X1）\n"
            "AuPlotCurve *humidity = new AuPlotCurve;\n"
            "humidity->setYAxis(AuPlotAxisPos::Y2);\n"
            "// ... addPoint(时间, 湿度)\n"
            "plot->addItem(humidity);\n"
            "\n"
            "// Y2 轴独立配置：颜色、量程、刻度、网格（默认关）\n"
            "plot->yAxis2()->setAxisColor(QColor(0x27ae60));\n"
            "plot->yAxis2()->setRange(0, 100);          // 也可固定量程\n"
            "plot->xAxis2()->setVisible(false);          // 只开 Y2 不开 X2 也可以\n");
        pages << page;
    }

    // ============ 3. 自定义刻度 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"自定义刻度");
        page.description = QStringLiteral(
            u"X 轴数据值 1~7 通过 setLabelFormatter() 显示为\"周一~周日\"；Y 轴 "
            u"setTickCount(5) 固定 5 个大刻度（首尾各一个均分范围），大刻度之间默认平分 "
            u"4 个小刻度（setMinorTickCount 可改）。setTickLabelsVisible(false) 可整体"
            u"隐藏刻度数值。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;

            plot->xAxis()->setRange(1, 7);
            plot->xAxis()->setTickCount(7);
            plot->xAxis()->setLabelFormatter([](double value) {
                static const QStringList days = {
                    QString(), QStringLiteral(u"周一"), QStringLiteral(u"周二"),
                    QStringLiteral(u"周三"), QStringLiteral(u"周四"), QStringLiteral(u"周五"),
                    QStringLiteral(u"周六"), QStringLiteral(u"周日")
                };
                const int index = qRound(value);
                return (index >= 1 && index <= 7) ? days.at(index) : QString::number(value);
            });

            plot->yAxis()->setTickCount(5);      // 固定 5 个大刻度
            plot->yAxis()->setMinorTickCount(4); // 大刻度间 4 个小刻度

            AuPlotCurve *curve = new AuPlotCurve;
            const QVector<double> week = { 42, 68, 55, 90, 74, 30, 48 };
            for (int i = 0; i < week.size(); ++i)
                curve->addPoint(i + 1, week.at(i));
            curve->setPointStyle(AuPlotPointStyle::Circle);
            curve->setPointSize(7);
            plot->addItem(curve);
            return plotPanel(plot);
        };
        page.code = QStringLiteral(
            "// X 轴：数据值 1~7 显示为星期\n"
            "plot->xAxis()->setRange(1, 7);\n"
            "plot->xAxis()->setTickCount(7);\n"
            "plot->xAxis()->setLabelFormatter([](double value) {\n"
            "    static const QStringList days = {\"\", \"周一\", \"周二\", \"周三\",\n"
            "                                     \"周四\", \"周五\", \"周六\", \"周日\"};\n"
            "    int i = qRound(value);\n"
            "    return (i >= 1 && i <= 7) ? days.at(i) : QString::number(value);\n"
            "});\n"
            "\n"
            "// Y 轴：固定大刻度数量 + 小刻度个数\n"
            "plot->yAxis()->setTickCount(5);        // <=0 则按像素宽度自动\n"
            "plot->yAxis()->setMinorTickCount(4);   // 大刻度间平分 4 个小刻度\n"
            "\n"
            "// 隐藏刻度数值（只保留刻度线与网格）\n"
            "plot->yAxis()->setTickLabelsVisible(false);\n"
            "\n"
            "// 默认数值显示的精度：-1 自动，也可固定小数位\n"
            "plot->xAxis()->setLabelPrecision(1);\n");
        pages << page;
    }

    // ============ 4. 点线面与层级 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"点线面与层级");
        page.description = QStringLiteral(
            u"独立图元：面（绿色半透明区域带，z=-1）在曲线下方；曲线（z=0，默认层级）；"
            u"参考线（红色虚线，z=1）、垂直全幅标线（青绿点线，z=1）和散点（空心圆，"
            u"z=2）在曲线上方。绘制按 zLevel 从小到大，负级在默认曲线底部。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;
            plot->xAxis()->setRange(0, 10);
            plot->yAxis()->setRange(-1, 8);

            // 面：z=-1，画在默认曲线底部（多边形自动闭合）
            AuPlotArea *area = new AuPlotArea;
            area->setZLevel(-1);
            area->setPoints({ QPointF(2.5, 0), QPointF(5.5, 0), QPointF(6.5, 4.5),
                              QPointF(4.5, 7.0), QPointF(2.0, 4.0) });
            area->setFillColor(QColor(0x27, 0xae, 0x60, 50));
            area->setBorderVisible(true);
            area->setBorderStyle(Qt::SolidLine);
            plot->addItem(area);

            // 曲线：z=0（默认层级）
            AuPlotCurve *curve = new AuPlotCurve;
            curve->setPoints(sineWave(100, 2.0, 0.3));
            plot->addItem(curve);

            // 线：z=1，阈值参考线（虚线可用 setDashPattern 精细控制间隔）
            AuPlotLine *reference = new AuPlotLine;
            reference->setZLevel(1);
            reference->setPoints({ QPointF(0, 4.6), QPointF(10, 4.6) });
            reference->setLineColor(QColor(0xe7, 0x4c, 0x3c));
            reference->setLineStyle(Qt::DashLine);
            plot->addItem(reference);

            // 参考线：z=1，垂直全幅标线（AuPlotRefLine，只需一个数据值）
            AuPlotRefLine *refLine = new AuPlotRefLine(AuPlotRefLine::Orientation::Vertical);
            refLine->setZLevel(1);
            refLine->setValue(8.0);
            refLine->setLineColor(QColor(0x16, 0xa0, 0x85));
            refLine->setLineStyle(Qt::DotLine);
            plot->addItem(refLine);

            // 点：z=2，散点标注（X 样式）
            AuPlotPoints *markers = new AuPlotPoints;
            markers->setZLevel(2);
            markers->setPointStyle(AuPlotPointStyle::HollowCircle);
            markers->setPointSize(9);
            markers->setPointColor(QColor(0x8e, 0x44, 0xad));
            markers->setPoints({ QPointF(2.0, 2.0), QPointF(5.0, 4.9), QPointF(8.0, 3.9) });
            plot->addItem(markers);
            return plotPanel(plot);
        };
        page.code = QStringLiteral(
            "// 层级约定：zLevel 越小越先画（越靠底层），默认曲线为 0\n"
            "\n"
            "// 面：z=-1 → 默认曲线下方；多边形自动闭合，填充色支持透明度\n"
            "AuPlotArea *area = new AuPlotArea;\n"
            "area->setZLevel(-1);\n"
            "area->setPoints({ {2.5, 0}, {5.5, 0}, {6.5, 4.5}, {4.5, 7.0}, {2.0, 4.0} });\n"
            "area->setFillColor(QColor(0x27ae60, 50));  // 内部背景 20% 透明度\n"
            "area->setBorderStyle(Qt::SolidLine);       // 边框实线\n"
            "plot->addItem(area);\n"
            "\n"
            "// 曲线：z=0（默认）\n"
            "AuPlotCurve *curve = new AuPlotCurve;\n"
            "// ... addPoint(x, y)\n"
            "plot->addItem(curve);\n"
            "\n"
            "// 线：z=1 → 曲线上方；虚线间隔可自定义（单位为线宽倍数）\n"
            "AuPlotLine *reference = new AuPlotLine;\n"
            "reference->setZLevel(1);\n"
            "reference->setPoints({ {0, 4.6}, {10, 4.6} });\n"
            "reference->setLineStyle(Qt::DashLine);\n"
            "reference->setDashPattern({4, 2});   // 画4空2（可选）\n"
            "plot->addItem(reference);\n"
            "\n"
            "// 水平 / 垂直参考线：全幅标线只需一个数据值（阈值/报警线）\n"
            "AuPlotRefLine *refLine = new AuPlotRefLine(AuPlotRefLine::Vertical);\n"
            "refLine->setValue(8.0);              // 垂直线在 x=8；Horizontal 则为 y 值\n"
            "refLine->setLineStyle(Qt::DotLine);\n"
            "plot->addItem(refLine);              // 注意：不参与自适应量程\n"
            "\n"
            "// 点：z=2；内置样式：实心/空心圆、方块、菱形、三角、\n"
            "// 十字架(Cross)、X(XCross)、带圈的十字架(RingedCross) 等\n"
            "AuPlotPoints *markers = new AuPlotPoints;\n"
            "markers->setZLevel(2);\n"
            "markers->setPointStyle(AuPlotPointStyle::HollowCircle);\n"
            "markers->addPoint(2.0, 2.0);\n"
            "plot->addItem(markers);\n");
        pages << page;
    }

    // ============ 5. 追踪器与十字线 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"追踪器与十字线");
        page.description = QStringLiteral(
            u"setTrackerEnabled(true)：鼠标在绘图区内移动时，以鼠标 x 同时吸附所有可见"
            u"曲线的最近数据点（每条曲线一个空心圆标记），所有曲线的 x/y 值合并在同一个 "
            u"tooltip 中逐行显示（行首色点标识曲线），tooltip 跟随鼠标、可视区域放不下时"
            u"自动翻转到左侧 / 下方；追踪线离开某条曲线的数据范围后，该曲线的标记与数值行"
            u"自动隐藏（setTrackerSnapOutOfRange(true) 可改为继续吸附端点），移出绘图区"
            u"自动隐藏。setCrosshairEnabled(true)：横贯绘图区的十字点线跟随鼠标"
            u"（最多横竖两条，setCrosshairDirections 可单独开启横线或纵线）。"
            u"trackerChanged() 信号实时输出各曲线吸附值（见日志）。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;
            plot->setTrackerEnabled(true);
            plot->setCrosshairEnabled(true);

            AuPlotCurve *sine = new AuPlotCurve;
            AuPlotCurve *damped = new AuPlotCurve;
            for (int i = 0; i <= 100; ++i) {
                const double x = 10.0 * i / 100.0;
                sine->addPoint(x, qSin(x) * 3.0);
                damped->addPoint(x, qCos(x) * 3.0 * qExp(-x / 8.0));
            }
            plot->addItem(sine);
            plot->addItem(damped);
            damped->setLineColor(QColor(0xe7, 0x4c, 0x3c));

            QPlainTextEdit *log = PageHelpers::makeLog(110);
            QObject::connect(plot, &AuPlot::trackerChanged, plot,
                             [log](AuPlotCurve *curve, const QPointF &point) {
                                 if (curve)
                                     PageHelpers::appendLog(log, QStringLiteral(u"吸附: x=%1, y=%2")
                                                                            .arg(point.x(), 0, 'f', 2)
                                                                            .arg(point.y(), 0, 'f', 3));
                                 else
                                     PageHelpers::appendLog(log, QStringLiteral(u"离开曲线"));
                             });
            return plotPanel(plot, nullptr, log);
        };
        page.code = QStringLiteral(
            "AuPlot *plot = new AuPlot;\n"
            "\n"
            "// 追踪器：以鼠标 x 同时吸附所有可见曲线，\n"
            "// 每条曲线一个标记，数值合并在同一个 tooltip 中\n"
            "plot->setTrackerEnabled(true);\n"
            "// 追踪线离开曲线数据范围后隐藏其标记与数值行（默认），\n"
            "// 设 true 则离开后继续吸附最近端点：\n"
            "// plot->setTrackerSnapOutOfRange(true);\n"
            "// 十字线：横贯绘图区跟随鼠标（横+竖，最多两条）\n"
            "plot->setCrosshairEnabled(true);\n"
            "// 只显示纵线 / 横线（追踪参考线按需开启）：\n"
            "// plot->setCrosshairDirections(Qt::Vertical);\n"
            "\n"
            "// 吸附值实时回调（某条曲线吸附点变化时发出；\n"
            "// curve 为 nullptr 表示已离开所有曲线）\n"
            "connect(plot, &AuPlot::trackerChanged, this,\n"
            "        [log](AuPlotCurve *curve, const QPointF &p) {\n"
            "    if (curve)\n"
            "        log->appendPlainText(QString(\"x=%1 y=%2\").arg(p.x()).arg(p.y()));\n"
            "});\n"
            "\n"
            "// tooltip 值文本使用绑定轴的刻度格式化：\n"
            "// 自定义过 setLabelFormatter 的轴（如星期）同样生效；\n"
            "// 背景色/边框/文字颜色/字体在 AuPlotStyle 的 tracker* 字段配置。\n");
        pages << page;
    }

    // ============ 6. 拖动与缩放 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"拖动与缩放");
        page.description = QStringLiteral(
            u"交互默认关闭：setDragEnabled / setZoomEnabled 开启。拖动可精细化控制方向"
            u"（此处横向+纵向），滚轮以光标为中心缩放。两轴都用 setRange 固定了最小值/"
            u"最大值，因此拖动和缩放被钳制在该范围内——滚轮缩小到全范围后不会再小。"
            u"提示：拖动/缩放过的轴会自动脱离数据自适应量程，setAutoRange(true) 可恢复。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;

            // 交互开关 + 方向精细化控制（默认全关）
            plot->setDragEnabled(true);
            plot->setDragDirections(Qt::Horizontal | Qt::Vertical);
            plot->setZoomEnabled(true);
            plot->setZoomDirections(Qt::Horizontal | Qt::Vertical);

            // 固定量程：同时成为拖动 / 缩放的边界
            plot->xAxis()->setRange(0, 100);
            plot->yAxis()->setRange(-120, 120);

            AuPlotCurve *curve = new AuPlotCurve;
            for (int i = 0; i <= 1000; ++i) {
                const double x = i / 10.0;
                curve->addPoint(x, qSin(x / 5.0) * 80 + qSin(x / 1.7) * 30);
            }
            plot->addItem(curve);
            return plotPanel(plot);
        };
        page.code = QStringLiteral(
            "AuPlot *plot = new AuPlot;\n"
            "\n"
            "// 交互默认关闭，按需开启\n"
            "plot->setDragEnabled(true);   // 鼠标左键拖动平移\n"
            "plot->setZoomEnabled(true);   // 鼠标滚轮缩放（以光标为中心）\n"
            "\n"
            "// 缩放灵敏度：每格滚轮放大/缩小的比例，默认 1.2\n"
            "plot->setZoomFactor(1.2);     // 调大一次缩放更多，调小更细腻\n"
            "\n"
            "// 精细化控制：只允许横向拖动 / 纵向缩放等\n"
            "plot->setDragDirections(Qt::Horizontal | Qt::Vertical);\n"
            "plot->setZoomDirections(Qt::Horizontal | Qt::Vertical);\n"
            "\n"
            "// 轴设置了最小值/最大值后，拖动与缩放只能在此范围内：\n"
            "plot->xAxis()->setRange(0, 100);       // 固定量程 + 交互边界\n"
            "plot->yAxis()->setRange(-120, 120);\n"
            "// 若想自适应量程但保留交互边界，用 setLimits：\n"
            "// plot->xAxis()->setLimits(0, 100);\n"
            "//\n"
            "// 注意：拖动/缩放过的轴自动脱离数据自适应量程（否则交互结果\n"
            "// 会被量程自适应覆盖），需要恢复跟随数据时：\n"
            "// plot->xAxis()->setAutoRange(true);\n");
        pages << page;
    }

    // ============ 7. 大数据量 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"大数据量");
        page.description = QStringLiteral(
            u"100 万个点：x 升序数据二分定位可见区间 + 按像素列 min/max 抽稀（每列保留"
            u"首/最小/最大/末，峰谷不丢），重绘流畅。已开启拖动、滚轮缩放与追踪器"
            u"（升序曲线二分吸附，移动无卡顿）。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;
            plot->setDragEnabled(true);
            plot->setZoomEnabled(true);
            plot->setTrackerEnabled(true);
            plot->setCrosshairEnabled(true);

            AuPlotCurve *curve = new AuPlotCurve;
            const int count = 1000 * 1000;
            QVector<QPointF> points;
            points.reserve(count);
            for (int i = 0; i < count; ++i) {
                const double x = i;
                const double y = qSin(x * 0.001) * 80.0
                                 + qSin(x * 0.0137) * 20.0
                                 + qSin(x * 0.103) * 6.0;
                points.append(QPointF(x, y));
            }
            curve->setPoints(points);
            plot->addItem(curve);

            auto *info = new QLabel(QStringLiteral(u"100 万点"));
            auto *button = new QPushButton(QStringLiteral(u"重新生成"));
            QObject::connect(button, &QPushButton::clicked, plot, [plot, curve, info]() {
                const auto start = std::chrono::steady_clock::now();
                const int count = 1000 * 1000;
                QVector<QPointF> points;
                points.reserve(count);
                for (int i = 0; i < count; ++i) {
                    const double x = i;
                    const double y = qSin(x * 0.001) * 80.0 + qSin(x * 0.0137) * 20.0
                                     + qSin(x * 0.103) * 6.0;
                    points.append(QPointF(x, y));
                }
                curve->setPoints(points);
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - start)
                                         .count();
                info->setText(QStringLiteral(u"100 万点，生成+装载 %1 ms").arg(elapsed));
            });

            auto *layout = new QHBoxLayout;
            layout->addWidget(info);
            layout->addWidget(button);
            layout->addStretch(1);
            return plotPanel(plot, layout);
        };
        page.code = QStringLiteral(
            "// 百万级数据：正常 setPoints 即可，内部自动优化\n"
            "AuPlotCurve *curve = new AuPlotCurve;\n"
            "QVector<QPointF> points;\n"
            "points.reserve(1000 * 1000);\n"
            "for (int i = 0; i < 1000 * 1000; ++i)\n"
            "    points.append(QPointF(i, signal(i)));\n"
            "curve->setPoints(points);   // x 升序自动检测\n"
            "plot->addItem(curve);\n"
            "\n"
            "// 加速原理：\n"
            "// 1) x 升序 → 可见区间二分查找，只处理屏幕内的数据；\n"
            "// 2) 点数 > 像素预算(宽度*2) → 按像素列 min/max 抽稀：\n"
            "//    每列保留 首/最小/最大/末 4 个点，峰谷形状不丢；\n"
            "// 3) 放大后点数少于预算时，直接按原始点绘制。\n"
            "\n"
            "// 乱序数据也能画（自动退化为线性扫描可见区间）。\n");
        pages << page;
    }

    // ============ 8. 点样式一览 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"点样式一览");
        page.description = QStringLiteral(
            u"内置点样式从左到右、从上到下：实心圆、空心圆圈、实心方块、空心方块、"
            u"实心菱形、空心菱形、实心三角、空心三角、十字架、X 叉号、带圈的十字架。"
            u"每行 6 个，size 统一 14px。本页 Y 轴隐藏了刻度数值（setTickLabelsVisible）。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;
            plot->xAxis()->setRange(0.5, 6.5);
            plot->yAxis()->setRange(-0.5, 1.5);
            plot->yAxis()->setTickLabelsVisible(false);   // 需求：刻度可不显示数值
            plot->xAxis()->setGridVisible(false);
            plot->yAxis()->setGridVisible(false);

            const QVector<AuPlotPointStyle> styles = {
                AuPlotPointStyle::Circle,       AuPlotPointStyle::HollowCircle,
                AuPlotPointStyle::Square,       AuPlotPointStyle::HollowSquare,
                AuPlotPointStyle::Diamond,      AuPlotPointStyle::HollowDiamond,
                AuPlotPointStyle::Triangle,     AuPlotPointStyle::HollowTriangle,
                AuPlotPointStyle::Cross,        AuPlotPointStyle::XCross,
                AuPlotPointStyle::RingedCross,
            };
            for (int i = 0; i < styles.size(); ++i) {
                AuPlotPoints *points = new AuPlotPoints;
                points->setPointStyle(styles.at(i));
                points->setPointSize(14);
                points->setPointColor(QColor(0x2f, 0x6f, 0xdb));
                points->addPoint(1.0 + (i % 6), i < 6 ? 1.0 : 0.0);
                plot->addItem(points);
            }
            return plotPanel(plot);
        };
        page.code = QStringLiteral(
            "// 每种样式一个 AuPlotPoints（也可用曲线的 setPointStyle 画在数据点上）\n"
            "const QVector<AuPlotPointStyle> styles = {\n"
            "    AuPlotPointStyle::Circle,        // 实心圆\n"
            "    AuPlotPointStyle::HollowCircle,  // 空心圆圈\n"
            "    AuPlotPointStyle::Square,        // 实心方块\n"
            "    AuPlotPointStyle::HollowSquare,  // 空心方块\n"
            "    AuPlotPointStyle::Diamond,       // 实心菱形\n"
            "    AuPlotPointStyle::HollowDiamond, // 空心菱形\n"
            "    AuPlotPointStyle::Triangle,      // 实心三角\n"
            "    AuPlotPointStyle::HollowTriangle,// 空心三角\n"
            "    AuPlotPointStyle::Cross,         // 十字架\n"
            "    AuPlotPointStyle::XCross,        // X 叉号\n"
            "    AuPlotPointStyle::RingedCross,   // 带圈的十字架\n"
            "};\n"
            "AuPlotPoints *points = new AuPlotPoints;\n"
            "points->setPointStyle(AuPlotPointStyle::RingedCross);\n"
            "points->setPointSize(14);\n"
            "points->setPointColor(QColor(0x2f6fdb));\n"
            "points->addPoint(1.0, 0.0);\n"
            "plot->addItem(points);\n"
            "\n"
            "// 轴刻度不显示数值\n"
            "plot->yAxis()->setTickLabelsVisible(false);\n");
        pages << page;
    }

    // ============ 9. 实时绘图与滚动窗口 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"实时绘图与滚动窗口");
        page.description = QStringLiteral(
            u"定时器 15ms/拍 addPoint 追加数据（O(1)，包围盒增量维护，固定量程下每帧"
            u"只重绘可见区间，百万点无压力）。x 轴为秒，窗口 10 秒一页：[0,10] 画满后"
            u"用 setViewRange 推进到 [10,20]，视觉上曲线从左边重新画起（15ms/拍 ≈ "
            u"667 点/页）。Y 轴默认自动量程（随曲线 min/max 自适应；setRange 固定后"
            u"可用 setAutoRange(true) 恢复，见\"Y 自动量程\"开关）。拖动与滚轮缩放"
            u"只作用于时间轴，方向可用 setDragDirections / setZoomDirections 单独"
            u"配置（下拉可切 仅X / 仅Y / X+Y / 关闭）——横纵耦合缩放在滚动图表里会"
            u"与翻页互相打架。拖走或缩放查看历史时自动暂停推进，把右缘拖回最新窗口"
            u"右缘附近自动恢复跟随，或点\"跟随最新\"立即跳回；勾选\"清除窗口外旧数据\""
            u"后用 removeFirstPoints 裁剪头部（不可回看）。");
        page.createDemo = []() -> QWidget * {
            AuPlot *plot = new AuPlot;
            plot->setDragEnabled(true);
            plot->setZoomEnabled(true);
            // 拖动 / 滚轮只作用于时间轴（X）：滚动图表里横纵耦合缩放会互相打架——
            // 翻页只重置 X，Y 一旦被缩放/拖动改掉，波形就可能显示不全。
            // 方向可用 setDragDirections / setZoomDirections 单独配置
            plot->setDragDirections(Qt::Horizontal);
            plot->setZoomDirections(Qt::Horizontal);
            plot->xAxis()->setRange(0.0, 10.0);
            plot->xAxis()->setLabelPrecision(1);
            // Y 轴不设范围：默认自动量程，随曲线 min/max 自适应（带 5% 留白）

            AuPlotCurve *curve = new AuPlotCurve;
            curve->setLineColor(QColor(0x2f, 0x6f, 0xdb));
            plot->addItem(curve);

            struct RtState {
                double nextX = 0.0;          ///< 下一拍的 x（秒）
                double windowStart = 0.0;    ///< 最新数据所在窗口起点（始终推进）
                bool following = true;       ///< 视图贴着最新窗口时自动推进，否则暂停
                const double window = 10.0;  ///< 10 秒一页
                const double step = 0.015;   ///< 15ms/拍
            };
            QSharedPointer<RtState> state(new RtState);

            QLabel *status = new QLabel;
            QCheckBox *trimBox = new QCheckBox(QStringLiteral(u"清除窗口外旧数据（不可回看）"));
            QPushButton *pauseBtn = new QPushButton(QStringLiteral(u"暂停"));
            QPushButton *followBtn = new QPushButton(QStringLiteral(u"跟随最新"));

            // 滚轮缩放方向演示：仅X / 仅Y / X+Y / 关闭（setZoomDirections）
            QComboBox *zoomMode = new QComboBox;
            zoomMode->addItems({ QStringLiteral(u"滚轮缩放：仅X"),
                                 QStringLiteral(u"滚轮缩放：仅Y"),
                                 QStringLiteral(u"滚轮缩放：X+Y"),
                                 QStringLiteral(u"滚轮缩放：关闭") });
            QObject::connect(zoomMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
                             plot, [plot](int index) {
                switch (index) {
                case 1: plot->setZoomDirections(Qt::Vertical); break;
                case 2: plot->setZoomDirections(Qt::Horizontal | Qt::Vertical); break;
                case 3: plot->setZoomDirections(Qt::Orientations()); break; // Qt6 QFlags 不再接受裸 0
                default: plot->setZoomDirections(Qt::Horizontal); break;
                }
            });

            // Y 自动量程：勾选=随曲线 min/max 自适应（默认）；
            // 取消=固定在当前范围；重新勾选即恢复自动（setAutoRange(true)）
            QCheckBox *yAutoBox = new QCheckBox(QStringLiteral(u"Y 自动量程"));
            yAutoBox->setChecked(true);
            QObject::connect(yAutoBox, &QCheckBox::toggled, plot, [plot](bool on) {
                if (on)
                    plot->yAxis()->setAutoRange(true);
                else
                    plot->yAxis()->setRange(plot->yAxis()->minimum(),
                                            plot->yAxis()->maximum());
            });

            // 跟随判定：视图右缘贴着最新窗口右缘（±2% 窗口宽）视为跟随。
            // 拖动 / 缩放（含 setViewRange 翻页本身）都会发 rangeChanged；
            // 拖走或缩放后暂停推进，把右缘拖回来即磁吸恢复
            QObject::connect(plot->xAxis(), &AuPlotAxis::rangeChanged, plot,
                             [state](double mn, double mx) {
                const double tol = state->window * 0.02;
                const double span = mx - mn;
                state->following = qAbs(mx - (state->windowStart + state->window)) < tol
                                   && span < state->window + tol * 2.5;
            });

            QTimer *timer = new QTimer(plot);
            timer->setInterval(15);
            QObject::connect(timer, &QTimer::timeout, plot,
                             [plot, curve, state, status, trimBox]() {
                const double x = state->nextX;
                const double y = qSin(2.0 * M_PI * x / 4.0) * 3.0
                                 + (int(QRandomGenerator::global()->bounded(200)) - 100) / 500.0;
                curve->addPoint(x, y);
                state->nextX += state->step;

                // 最新数据所在窗口始终推进（与是否跟随无关）
                while (state->nextX > state->windowStart + state->window)
                    state->windowStart += state->window;

                // 仅在跟随模式下推进可视范围：视觉上曲线从左边重新画起；
                // 用户拖走 / 缩放查看历史时暂停，不抢视图
                if (state->following) {
                    const double mn = state->windowStart;
                    const double mx = state->windowStart + state->window;
                    if (qAbs(plot->xAxis()->minimum() - mn) > 1e-9
                        || qAbs(plot->xAxis()->maximum() - mx) > 1e-9)
                        plot->xAxis()->setViewRange(mn, mx);
                }

                // 可选：裁掉窗口外旧数据，避免内存无限增长
                if (trimBox->isChecked()) {
                    const int keep = qRound(state->window / state->step) + 40;
                    const int drop = curve->pointCount() - keep;
                    if (drop > 0)
                        curve->removeFirstPoints(drop);
                }
                status->setText(QStringLiteral(u"%1 | 已追加 %2 点 | 最新窗口 [%3, %4] 秒")
                                    .arg(state->following ? QStringLiteral(u"跟随中")
                                                          : QStringLiteral(u"已暂停推进（拖回右缘或点\"跟随最新\"恢复）"))
                                    .arg(curve->pointCount())
                                    .arg(state->windowStart, 0, 'f', 0)
                                    .arg(state->windowStart + state->window, 0, 'f', 0));
            });
            timer->start();

            QObject::connect(pauseBtn, &QPushButton::clicked, plot, [timer, pauseBtn]() {
                if (timer->isActive()) {
                    timer->stop();
                    pauseBtn->setText(QStringLiteral(u"继续"));
                } else {
                    timer->start();
                    pauseBtn->setText(QStringLiteral(u"暂停"));
                }
            });
            QObject::connect(followBtn, &QPushButton::clicked, plot, [plot, state]() {
                plot->xAxis()->setViewRange(state->windowStart,
                                            state->windowStart + state->window);
                state->following = true;
            });

            QHBoxLayout *controls = new QHBoxLayout;
            controls->addWidget(pauseBtn);
            controls->addWidget(followBtn);
            controls->addWidget(zoomMode);
            controls->addWidget(yAutoBox);
            controls->addWidget(trimBox);
            controls->addWidget(status, 1);
            return plotPanel(plot, controls);
        };
        page.code = QStringLiteral(
            "// 交互默认关闭：拖动 / 缩放要先开启；方向可单独配置\n"
            "plot->setDragEnabled(true);\n"
            "plot->setZoomEnabled(true);\n"
            "plot->setDragDirections(Qt::Horizontal);   // 拖动只平移时间轴\n"
            "plot->setZoomDirections(Qt::Horizontal);   // 滚轮只缩放时间轴：\n"
            "                                           //   仅X=Horizontal / 仅Y=Vertical\n"
            "                                           //   两者一起=Horizontal|Vertical\n"
            "\n"
            "// Y 轴默认自动量程（随曲线 min/max 自适应，带 5% 留白）：\n"
            "// 不调用 setRange 即可；固定后再恢复自动：\n"
            "plot->yAxis()->setAutoRange(true);\n"
            "\n"
            "// 定时器 15ms/拍：addPoint O(1)，固定量程下每帧只画可见区间\n"
            "// x 轴为秒，10 秒一页：step=0.015，window=10（≈667 点/页）\n"
            "auto *timer = new QTimer(plot);\n"
            "timer->setInterval(15);\n"
            "connect(timer, &QTimer::timeout, plot, [plot, curve]() {\n"
            "    curve->addPoint(x, y);              // x 递增（秒）\n"
            "    while (x > windowStart + window)    // 最新窗口始终推进\n"
            "        windowStart += window;\n"
            "    if (following)                      // 视图贴着最新窗口才推进：\n"
            "        plot->xAxis()->setViewRange(windowStart, windowStart + window);\n"
            "    if (trim)\n"
            "        curve->removeFirstPoints(dropCount);\n"
            "});\n"
            "timer->start();\n"
            "\n"
            "// 跟随判定：监听 xAxis 的 rangeChanged，视图右缘贴着最新窗口\n"
            "// 右缘即跟随；用户拖走 / 缩放查看历史时暂停推进，拖回或点\n"
            "// \"跟随最新\"按钮恢复。\n"
            "// setViewRange 只推进可见范围、不设交互边界，历史数据可自由回看。\n");
        pages << page;
    }

    return pages;
}
