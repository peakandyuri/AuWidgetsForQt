#ifndef AGCOMMON_GLOBAL_H
#define AGCOMMON_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AgCommon 库时导出符号，使用方自动导入。
#if defined(AGCOMMON_LIBRARY)
#  define AGCOMMON_EXPORT Q_DECL_EXPORT
#else
#  define AGCOMMON_EXPORT Q_DECL_IMPORT
#endif

#endif // AGCOMMON_GLOBAL_H
