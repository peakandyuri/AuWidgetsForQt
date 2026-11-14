TEMPLATE = subdirs

SUBDIRS += \
    Example \
    FeatureDemo \
    JsonDemo \
    AuWindowKitDemo

# 各工程对根目录库工程（AuCommon/AuGrid/...）的依赖无法在本层用
# .depends 声明——.depends 仅在同层 SUBDIRS 条目间生效，AuGrid 等
# 是根层的条目。该依赖已上提到根 AuWidgetsForQt.pro 的
# Examples.depends（成员所需库的并集），保证本子树后于所有库构建。
