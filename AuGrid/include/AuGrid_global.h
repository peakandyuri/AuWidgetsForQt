#ifndef AUGRID_GLOBAL_H
#define AUGRID_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AuGrid 库时导出符号，使用方自动导入。
#if defined(AUGRID_LIBRARY)
#  define AUGRID_EXPORT Q_DECL_EXPORT
#else
#  define AUGRID_EXPORT Q_DECL_IMPORT
#endif

#endif // AUGRID_GLOBAL_H
