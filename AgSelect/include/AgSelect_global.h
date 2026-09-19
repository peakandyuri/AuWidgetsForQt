#ifndef AGSELECT_GLOBAL_H
#define AGSELECT_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AgSelect 库时导出符号，使用方自动导入。
#if defined(AGSELECT_LIBRARY)
#  define AGSELECT_EXPORT Q_DECL_EXPORT
#else
#  define AGSELECT_EXPORT Q_DECL_IMPORT
#endif

#endif // AGSELECT_GLOBAL_H
