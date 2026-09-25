#ifndef AGPLOT_GLOBAL_H
#define AGPLOT_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AgPlot 库时导出符号，使用方自动导入。
#if defined(AGPLOT_LIBRARY)
#  define AGPLOT_EXPORT Q_DECL_EXPORT
#else
#  define AGPLOT_EXPORT Q_DECL_IMPORT
#endif

#endif // AGPLOT_GLOBAL_H
