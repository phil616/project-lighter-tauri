# 源码仓库与 CI 构建约定

仓库地址：https://github.com/phil616/project-lighter-tauri.git

## 入库范围

Git 只保留源码、构建脚本、配置模板、文档和编译所需的资源。

| 文件 | 管理方式 |
| --- | --- |
| C++、CMake、QSS/QRC、GitHub 工作流、文档 | 普通 Git，文本统一 LF |
| 应用图标、第三方样式库的小图片、文档截图 | 普通 Git，关闭文本转换和文本差异比较 |
| `package-lock.json`、`Cargo.lock` | 保留，确保归档 Tauri 工程的依赖可复现 |
| exe、DLL、静态库、调试符号、安装包、压缩包 | 忽略，由 CI 生成并上传构建产物 |
| 工具链、依赖下载、缓存、本地配置和备份 | 忽略 |

`.gitattributes` 中的 `binary` 仅表示图片、图标等资源按二进制处理，不会把本地 exe 添加到 Git。
本项目不使用 Git LFS 跟踪生成产物，不需要安装 Git LFS 即可克隆和构建。

## 本地构建文件

`build*/`、`cmake-build-*/`、`_packages/`、`release-windows/`、`release-linux/`、
`release-macos/`、`stage/`、`out/` 和前端 `dist/` 均已忽略。
本地安装的 Qt、MinGW 以及之前生成的 exe 可以继续使用，不会进入提交。

不要强制添加这些目录或文件。GitHub Actions 的上传步骤只保存 CI 构建产物，不会向源码分支提交 exe。

## CI 源码检查

两个构建工作流都会在安装依赖、编译之前运行：

```bash
python packaging/check_source_tree.py
```

检查读取 Git 索引中的路径；若有人误将 exe、DLL、安装包、压缩包或构建目录加入 Git，CI 会失败并列出文件。
应用图标、编译资源、文档图片和依赖锁文件正常保留。

可在本地复查：

```bash
git ls-files '*.exe' '*.exe.dist' '*.dll' '*.pdb' '*.lib' '*.a' '*.msi' '*.zip' '*.7z'
git ls-files -ci --exclude-standard
git check-ignore -v build-windows/lighter.exe release-windows/bin/lighter.exe _packages/Qt/
python packaging/check_source_tree.py
git diff --cached --check
```

前两条命令应没有输出。本地构建目录中的文件不会被源码检查误判为已跟踪文件。

## Windows 免安装构建

在 GitHub Actions 中手动运行 **Windows Portable Build**。该工作流使用 Qt 6.8.3 和 MinGW 13.1，
完成源码检查、编译和测试后，收集 exe、Qt DLL、编译器运行库、平台插件及许可，上传整个运行目录。
下载并解压 `lighter-windows-x64-portable`，双击 `bin/lighter.exe`。

常规推送和 PR 的 Qt 工作流也会上传运行目录。创建 `v*` 标签时，发布工作流才额外生成
安装包并发布到 GitHub Releases；所有产物均位于 Actions/Release 下载区，不进入 Git 源码历史。
