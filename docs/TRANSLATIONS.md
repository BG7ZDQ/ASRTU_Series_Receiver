# 翻译维护

界面源语言是中文。中文系统直接显示源文本，日文系统加载 `translations/asrtu_ja.qm`，其他系统加载 `translations/asrtu_en.qm`。调试时可使用隐藏参数 `--language=zh`、`--language=ja` 或 `--language=en` 强制语言。

相关文件：

- `asrtu-qt/translations/asrtu_en.ts` — 可编辑英文翻译源
- `asrtu-qt/translations/asrtu_en.qm` — 发布用二进制翻译包
- `asrtu-qt/translations/asrtu_ja.ts` — 可编辑日文翻译源
- `asrtu-qt/translations/asrtu_ja.qm` — 发布用日文二进制翻译包
- `tools/fill_asrtu_en.py` — 当前翻译映射维护脚本
- `tools/fill_asrtu_ja.py` — 日文翻译映射维护脚本
- `asrtu-qt/src/translation.cpp` — 系统语言判断与加载逻辑

更新源字符串后，使用 Qt 5 的 `lupdate` 重新扫描，再运行映射脚本和 `lrelease`：

```powershell
lupdate asrtu-qt\src\*.cpp asrtu-qt\src\*.h -ts asrtu-qt\translations\asrtu_en.ts asrtu-qt\translations\asrtu_ja.ts
python tools\fill_asrtu_en.py
python tools\fill_asrtu_ja.py
lrelease asrtu-qt\translations\asrtu_en.ts -qm asrtu-qt\translations\asrtu_en.qm
lrelease asrtu-qt\translations\asrtu_ja.ts -qm asrtu-qt\translations\asrtu_ja.qm
```

新增语言时复制 TS 文件、填写翻译，并在 `installSystemTranslation` 中加入 locale 到文件名的映射。不要把用户输入、卫星名称、呼号或协议字段送入翻译系统。
