TEMPLATE = subdirs

SUBDIRS += \
    AuCommon \
    AuDatePicker \
    AuGrid \
    AuJson \
    AuPlot \
    AuSelect \
    AuWindowKit \
    Tools \
    Examples

AuGrid.depends += AuCommon
AuSelect.depends += AuCommon
AuDatePicker.depends += AuCommon
# .depends 只在同层 SUBDIRS 条目间生效：Examples 子树对库工程的依赖
# 无法在 Examples.pro 内声明，需在公共祖先层（本文件）挂在聚合条目上，
# 覆盖其成员所需库的并集（Example/FeatureDemo/JsonDemo/AuWindowKitDemo）
Examples.depends += AuCommon AuGrid AuSelect AuDatePicker AuJson AuPlot AuWindowKit
# Tools 下 AuPortableDocEditor / AuSnipping 自包含，无仓库内依赖
