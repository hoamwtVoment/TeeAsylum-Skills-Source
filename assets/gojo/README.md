# 无量空处背景

`unlimited-void.jpg` 为用户提供的原图，保持原始文件不裁剪、不改色。

`scripts/asylum_map_assets.py` 将它解码嵌入现有地图，配合 `gojo_domain_black` 包络，仅对受领域影响的客户端显示。16:9 图片置于黑色底之后、游戏层与角色之前，使用原图比例；较宽或缩放较大的视口由黑底补齐。

普通开服/构建不需要原 JPG 或 ffmpeg：已打包的地图包含图像。重新嵌入资源时需要原图、ffmpeg、ffprobe。
