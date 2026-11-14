#include "HeaderCheckBoxRenderer.h"

#include "AuColumnDef.h"
#include "AuGridModel.h"
#include "AuGridStyle.h"

#include <QDebug>
#include <QPainter>
#include <QPainterPath>

// 复选框绘制在列头右侧
static QRect checkBoxRect(const QRect &nodeRect)
{
    const int size = 16;
    const int margin = 6;
    const int x = nodeRect.right() - margin - size;
    const int y = nodeRect.top() + (nodeRect.height() - size) / 2;
    return QRect(x, y, size, size);
}

// 聚合当前列数据：全部勾选 = Checked / 部分 = PartiallyChecked / 无 = Unchecked
static Qt::CheckState columnCheckState(const AuGridModel *model, int column)
{
    int checkedCount = 0;
    const int rows = model ? model->rowCount() : 0;
    for (int row = 0; row < rows; ++row) {
        if (model->cellValue(row, column).toBool())
            ++checkedCount;
    }
    if (rows == 0 || checkedCount == 0)
        return Qt::Unchecked;
    return checkedCount == rows ? Qt::Checked : Qt::PartiallyChecked;
}

void HeaderCheckBoxRenderer::setModel(AuGridModel *model)
{
    m_model = model;
}

void HeaderCheckBoxRenderer::paint(QPainter *painter,
                                   const QRect &rect,
                                   const AuColumnDef &columnDef,
                                   int column,
                                   int,
                                   const AuGridModel *model,
                                   const AuGridStyle *style,
                                   const QColor &textColor,
                                   const AuHeaderRendererContext &context) const
{
    painter->save();

    // 表头文字（左侧），右侧预留复选框空间
    const QRect node(0, 0, rect.width(), rect.height());
    QRect textRect = rect.adjusted(6, 0, -6, 0);
    textRect.setRight(rect.left() + checkBoxRect(node).left() - 4);
    painter->setPen(textColor);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      columnDef.headerName());

    // 复选框（右侧），三态由列数据实时聚合
    const QRect box = checkBoxRect(rect);
    const Qt::CheckState state = columnCheckState(model, column);
    const bool over = context.hovered && checkBoxRect(node).contains(context.headerPos);

    QColor checkedColor = style ? style->color(AuGridStyle::CheckboxCheckedColor, model) : QColor();
    QColor borderColor = style ? style->color(AuGridStyle::CheckboxBorderColor, model) : QColor();
    QColor markColor = style ? style->color(AuGridStyle::CheckboxCheckmarkColor, model) : QColor();
    if (!checkedColor.isValid())
        checkedColor = QColor(0, 120, 215);
    if (!borderColor.isValid())
        borderColor = QColor(150, 150, 150);
    if (!markColor.isValid())
        markColor = Qt::white;

    painter->setRenderHint(QPainter::Antialiasing);
    if (state != Qt::Unchecked) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(over ? checkedColor.lighter(115) : checkedColor);
        painter->drawRoundedRect(box, 3, 3);
        painter->setPen(QPen(markColor, 2));
        if (state == Qt::PartiallyChecked) {
            const int lineY = box.center().y();
            painter->drawLine(box.left() + 4, lineY, box.right() - 4, lineY);
        } else {
            QPainterPath path;
            path.moveTo(box.left() + 4, box.center().y());
            path.lineTo(box.center().x() - 1, box.bottom() - 4);
            path.lineTo(box.right() - 4, box.top() + 4);
            painter->drawPath(path);
        }
    } else {
        if (over) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(checkedColor.red(), checkedColor.green(),
                                     checkedColor.blue(), 35));
            painter->drawRoundedRect(box, 3, 3);
        }
        painter->setPen(QPen(borderColor, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(box.adjusted(1, 1, -1, -1), 3, 3);
    }

    painter->restore();
}

bool HeaderCheckBoxRenderer::wantsHoverCursor(int, int, const QRect &rect, const QPoint &headerPos) const
{
    return checkBoxRect(QRect(0, 0, rect.width(), rect.height())).contains(headerPos);
}

bool HeaderCheckBoxRenderer::hitTest(int, int, const QRect &rect, const QPoint &headerPos) const
{
    return checkBoxRect(QRect(0, 0, rect.width(), rect.height())).contains(headerPos);
}

void HeaderCheckBoxRenderer::headerMouseClick(int column, int, const QRect &, const QPoint &)
{
    if (m_model.isNull())
        return;

    // 全选时点击 = 清空；半选 / 全不选时点击 = 全选
    const Qt::CheckState state = columnCheckState(m_model, column);
    const bool target = (state != Qt::Checked);

    for (int row = 0; row < m_model->rowCount(); ++row)
        m_model->setCellValue(row, column, target);

    qDebug() << "HeaderCheckBoxRenderer: column" << column << "->" << (target ? "check all" : "uncheck all");
}
