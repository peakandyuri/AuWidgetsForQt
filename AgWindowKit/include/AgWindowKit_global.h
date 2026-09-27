#ifndef AGWINDOWKIT_GLOBAL_H
#define AGWINDOWKIT_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AgWindowKit 库时导出符号，使用方自动导入。
#if defined(AGWINDOWKIT_LIBRARY)
#  define AGWINDOWKIT_EXPORT Q_DECL_EXPORT
#else
#  define AGWINDOWKIT_EXPORT Q_DECL_IMPORT
#endif

#endif // AGWINDOWKIT_GLOBAL_H
