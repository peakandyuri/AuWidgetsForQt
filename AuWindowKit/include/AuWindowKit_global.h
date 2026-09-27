#ifndef AUWINDOWKIT_GLOBAL_H
#define AUWINDOWKIT_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AuWindowKit 库时导出符号，使用方自动导入。
#if defined(AUWINDOWKIT_LIBRARY)
#  define AUWINDOWKIT_EXPORT Q_DECL_EXPORT
#else
#  define AUWINDOWKIT_EXPORT Q_DECL_IMPORT
#endif

#endif // AUWINDOWKIT_GLOBAL_H
