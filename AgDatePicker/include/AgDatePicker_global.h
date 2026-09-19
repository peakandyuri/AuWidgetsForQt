#ifndef AGDATEPICKER_GLOBAL_H
#define AGDATEPICKER_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AgDatePicker 库时导出符号，使用方自动导入。
#if defined(AGDATEPICKER_LIBRARY)
#  define AGDATEPICKER_EXPORT Q_DECL_EXPORT
#else
#  define AGDATEPICKER_EXPORT Q_DECL_IMPORT
#endif

#endif // AGDATEPICKER_GLOBAL_H
