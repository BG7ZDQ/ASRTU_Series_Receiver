# 开源发布检查表

正式创建公开仓库前逐项确认：

- [x] 项目本体采用 MIT License
- [x] 补充 `LICENSE` 和 BG7ZDQ 版权信息
- [ ] 确认 `gr-lilacsat`、`gr-hyacinthsat`、Qt、Qwt、GNU Radio 和 SGP4 的声明完整
- [ ] 不提交 SDR#、上传代理或其他没有明确再分发授权的二进制
- [ ] 不提交 `config.cfg`、呼号、经纬度、WebSocket 凭证、录音和日志
- [ ] 不提交 `build*`、`portable`、`stage`、`dist`、`bin`、`obj`、`outputs` 和 `work`
- [ ] 在干净目录按 `docs/BUILDING.md` 完整构建一次
- [ ] 在中文和非中文系统测试 `.qm` 翻译包
- [ ] 测试低分辨率与 100%/150%/200% DPI；启动器初始客户区面积不超过可用屏幕面积的 30%
- [ ] 对公开安装包生成 SHA-256，并在 Release 页面记录版本、日期和已知限制

项目本体的 MIT License 仅覆盖 BG7ZDQ 对本仓库原创代码拥有的权利；第三方组件继续遵循各自上游许可证。
