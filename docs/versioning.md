# 版本管理

Git tag 是当前 Qt 应用版本的唯一来源。发布标签采用 `v主版本.次版本.修订版本`，例如 `v2.0.2`。
配置阶段由 `cmake/Version.cmake` 读取标签，去掉 `v` 后生成统一的 `2.0.2`。

## 标签选择

- 标签触发的 GitHub 构建使用 `GITHUB_REF_NAME`，并验证该标签确实指向当前构建提交。
- 分支、PR、手动构建和本地构建使用当前提交最近可达的有效版本标签；支持附注标签和轻量标签。
- 不使用其他分支上不可达的标签，也不把字典序或版本号最大的标签当作当前版本。
- 缺少 Git 元数据、没有可达的版本标签、标签格式错误或发布标签指向其他提交时，配置直接失败。

CI checkout 使用 `fetch-depth: 0` 获取完整历史和标签。本地浅克隆先执行
`git fetch --unshallow --tags`；普通克隆缺少标签时执行 `git fetch --tags`。
GitHub 自动生成的无 Git 元数据源码 ZIP 不能直接推导版本，请使用 Git 克隆并检出相应标签。

## 版本使用位置

| 位置 | 来源 |
| --- | --- |
| CMake `PROJECT_VERSION` | `cmake/Version.cmake` |
| 侧栏和关于页 | 生成的 `generated/lighter/version.h` |
| `lighter --version` / Qt 应用版本 | 同一个生成头文件 |
| Windows exe 的文件版本和产品版本 | `native/windows.rc.in`，数值字段补第四位 `0` |
| CPack 安装包版本及文件名 | `PROJECT_VERSION` |
| Actions 构建产物名 | 读取 CMake 生成的 `build/version.json`，不再单独解析 Git |
| 运行目录中的版本记录 | `share/lighter/version.json`，记录版本、标签和完整提交 ID |

例如，基于 `v2.0.2` 的 Windows 免安装产物名为 `lighter-2.0.2-windows-x64-portable`，
界面、命令行、exe 产品版本均显示 `2.0.2`；Windows 数值文件版本为 `2.0.2.0`。
分支提交继续沿用最近的标签版本，具体构建通过版本记录和界面提示中的提交 ID 区分。
已有构建目录会在 HEAD 或标签变化后自动重新配置。

`legacy/tauri/` 是迁移前的归档快照，其清单版本保留历史值，不参与当前 Qt 应用构建和发布。
迁移文档中的旧版本章节同样属于历史说明。

## 发布

先提交源码改动，再为要发布的提交打标签并推送：

```bash
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin main
git push origin vX.Y.Z
```

日常无需修改 CMake、C++ 或工作流中的版本常量。已经发布的标签保持其提交指向；
版本机制修复会随之后的新提交和新标签生效，不自动移动已有标签或替换已发布的产物。

## 验证

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

测试覆盖无标签、轻量/附注标签、非标签提交、不可达标签、发布标签与提交不匹配、浅克隆，
并检查 `--version` 与构建版本一致；有 Ninja 时还会验证新增标签能更新已有构建目录。
