#ifndef AUJSON_GLOBAL_H
#define AUJSON_GLOBAL_H

// AuJson 库导出宏：编译 AuJson 库时导出符号，使用方自动导入。
// 核心层不依赖 Qt，因此不使用 Q_DECL_EXPORT/Q_DECL_IMPORT，直接用平台宏。
#if defined(AUJSON_LIBRARY)
#  if defined(_WIN32)
#    define AUJSON_API __declspec(dllexport)
#  else
#    define AUJSON_API __attribute__((visibility("default")))
#  endif
#elif defined(AUJSON_STATIC)
#  define AUJSON_API
#else
#  if defined(_WIN32)
#    define AUJSON_API __declspec(dllimport)
#  else
#    define AUJSON_API
#  endif
#endif

#endif // AUJSON_GLOBAL_H
