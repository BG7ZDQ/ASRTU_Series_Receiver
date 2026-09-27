# 翻译维护

界面源语言是中文。语言优先级为：命令行 `--language=zh|en|ja`、程序目录内 `ui-language.ini` 的 `[UI] Language`、系统语言。Inno 安装器每次将用户本次选择的语言写入 `decoder/ui-language.ini`，不改变地面站配置；现有快捷方式和直接运行 EXE 都读取此设置，启动器会向子窗口传递实际使用的语言。无安装配置的便携版默认按系统语言选择：中文直接显示源文本，日文加载 `translations/asrtu_ja.qm`，其他语言加载 `translations/asrtu_en.qm`。`--language en` 形式也受支持。

相关文件：

- `assets/translations/asrtu_en.ts` — 可编辑英文翻译源
- `assets/translations/asrtu_en.qm` — 发布用二进制翻译包
- `assets/translations/asrtu_ja.ts` — 可编辑日文翻译源
- `assets/translations/asrtu_ja.qm` — 发布用日文二进制翻译包
- `tools/fill_asrtu_en.py` — 当前翻译映射维护脚本
- `tools/fill_asrtu_ja.py` — 日文翻译映射维护脚本
- `libs/common/translation.cpp` — 安装语言、命令行覆盖及系统语言回退逻辑
- `packaging/inno/THIRD_PARTY_NOTICE*.txt`、`SDRSHARP_NOTICE*.txt` — 安装声明及安装目录中的声明；英文使用 `.en.txt`，日文使用 `.ja.txt`，无语言后缀为中文

更新源字符串后，使用 Qt 5 的 `lupdate` 重新扫描，再运行映射脚本和 `lrelease`：

```powershell
lupdate apps\dsp\*.cpp apps\dsp\*.h apps\launcher\*.cpp apps\satnogs-uploader\*.cpp apps\satnogs-uploader\*.h -ts assets\translations\asrtu_en.ts assets\translations\asrtu_ja.ts
python tools\fill_asrtu_en.py
python tools\fill_asrtu_ja.py
lrelease assets\translations\asrtu_en.ts -qm assets\translations\asrtu_en.qm
lrelease assets\translations\asrtu_ja.ts -qm assets\translations\asrtu_ja.qm
```

新增语言时复制 TS 文件、填写翻译，并在 `installSystemTranslation` 中加入 locale 到文件名的映射。不要把用户输入、卫星名称、呼号或协议字段送入翻译系统。

Tiny Doppler 使用子模块内的 `assets/translations/tiny_en.ts` 和
`tiny_ja.ts`，在子模块中运行 `lupdate app -ts assets/translations/tiny_en.ts assets/translations/tiny_ja.ts`
及 `lrelease` 后提交。启动器会把用户选择的语言传给 Tiny Doppler。
