#ifndef AUCOMMON_GLOBAL_H
#define AUCOMMON_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AuCommon 库时导出符号，使用方自动导入。
#if defined(AUCOMMON_LIBRARY)
#  define AUCOMMON_EXPORT Q_DECL_EXPORT
#else
#  define AUCOMMON_EXPORT Q_DECL_IMPORT
#endif

#endif // AUCOMMON_GLOBAL_H
