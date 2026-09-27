#ifndef AUPLOT_GLOBAL_H
#define AUPLOT_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AuPlot 库时导出符号，使用方自动导入。
#if defined(AUPLOT_LIBRARY)
#  define AUPLOT_EXPORT Q_DECL_EXPORT
#else
#  define AUPLOT_EXPORT Q_DECL_IMPORT
#endif

#endif // AUPLOT_GLOBAL_H
