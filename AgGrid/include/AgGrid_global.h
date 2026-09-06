#ifndef AGGRID_GLOBAL_H
#define AGGRID_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AgGrid 库时导出符号，使用方自动导入。
#if defined(AGGRID_LIBRARY)
#  define AGGRID_EXPORT Q_DECL_EXPORT
#else
#  define AGGRID_EXPORT Q_DECL_IMPORT
#endif

#endif // AGGRID_GLOBAL_H
