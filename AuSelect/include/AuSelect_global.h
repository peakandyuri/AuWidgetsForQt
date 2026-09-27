#ifndef AUSELECT_GLOBAL_H
#define AUSELECT_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AuSelect 库时导出符号，使用方自动导入。
#if defined(AUSELECT_LIBRARY)
#  define AUSELECT_EXPORT Q_DECL_EXPORT
#else
#  define AUSELECT_EXPORT Q_DECL_IMPORT
#endif

#endif // AUSELECT_GLOBAL_H
