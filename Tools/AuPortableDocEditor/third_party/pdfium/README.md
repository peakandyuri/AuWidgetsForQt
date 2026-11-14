# pdfium（第三方渲染库）

`pdfium.dll` 用于在没有 QtPdf 模块的环境下渲染页面缩略图（运行时经
`QLibrary` 动态加载，见 `src/PdfPreviewRenderer.cpp`），构建脚本会自动
把它拷贝到可执行文件目录。

## 来源

- 二进制出自 **bblanchon/pdfium-binaries** 项目（Chromium 官方 PDFium
  最通用的第三方构建渠道），本副本随 **pypdfium2 5.14.0** 的 win_amd64
  wheel 发行
- 主页：<https://github.com/bblanchon/pdfium-binaries>
- PDFium 源码：<https://pdfium.googlesource.com/pdfium/>

## 许可证

PDFium 采用 BSD 风格许可（见 `licenses/LICENSES/BSD-3-Clause.txt` 与
`licenses/data/windows_x64/BUILD_LICENSES/pdfium.txt`）；其静态链接的
freetype、icu、libjpeg-turbo、libpng、zlib、abseil 等组件的许可证在
`licenses/data/windows_x64/BUILD_LICENSES/` 目录下，共 18 份，全部随库分发。

## 升级方式

替换本目录下的 `pdfium.dll` 即可（建议连同 `licenses/` 一起更新）。
