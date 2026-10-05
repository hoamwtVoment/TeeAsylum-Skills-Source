# Tee Asylum Inventory / Tour

> 源码与本轮背包、普通装备、Boss 战扩展由 [101024awa](https://github.com/101024awa) 提供，[hoamwtVoment](https://github.com/hoamwtVoment) 整理上传。

基于 DDNet-HunterN 的 Item Asylum 风格二维服务器，普通 DDNet 客户端即可连接。当前为 **36 件出生随机装备、5 个升级模块、5 件升级武器，总目录 46 项**；保留 FFA、TDM、GG、ELIM、ZS、JGN，新增默认大厅巡回 `asylum_tour` 和 **10 Hour Burst Man 两阶段 AI Boss**。

## 玩法

- 大厅没有武器，每轮从 16 张战斗图抽取 3 张，触碰 1/2/3 号选图板投票；45 秒后切换，战斗结束返回大厅。巡回支持一人游玩，无人时停止大厅倒计时。
- 随机池为 7 近战、21 远程、8 特殊。按 `1/2/3` 切换，左键使用；升级模块放在槽位 4，按 `4` 后左键消耗模块升级远程武器。
- M1911、Pixel gun、9mm、Suppressed pistol、SSG-08 分别达到 5/5/4/6/10 次本武器有效击杀后获得升级模块。
- 有限弹匣、空弹自动装填、R 主动装填；America、Medkit、Holy Mantle 使用一次后对应物品槽消失。Darkheart、Lilynette、Blaster 有专属技能。
- 普通角色 100 HP、零出生护甲；脱战 5 秒后每秒回复最大生命的 2.5%，GG 不回血。
- Boss 每名挑战者 6 命，生命随人数增加；半血后回满进入第二阶段，速度提高、承伤翻倍，并切换阶段音乐。

Boss 第一阶段使用 Darkheart 三连冲刺、Twilight、Star Platinum、Vampirism 抓取拖拽和霰弹枪；第二阶段增加 Grand Volley、Strong Left。Boss 生命为 `2000 + 666 ×（人数−1）`，上限 11990，使用同一场地，不整体传送玩家。

## 启动与配置

从 Actions 下载 Windows/Linux 产物，解压后使用包内配置启动：

```text
TeeAsylum-Server.exe -f autoexec.cfg
./TeeAsylum-Server -f autoexec.cfg
```

Windows 也可运行 `Start-TeeAsylum.cmd`。默认 `autoexec.cfg` 是大厅巡回；旧独立房间玩法使用 `-f autoexec-room-modes.cfg`。

源码仓库的正式配置放在 `package/`，不覆盖开发者自己的根目录 `autoexec.cfg`。手动构建后应将可执行文件、`data/`、`room_config/`、`package/` 内的配置、许可证放到同一运行目录；Windows 另带 `sqlite3.dll`。

E/R 通过客户端自行绑定：

```text
bind e "say /asylum_e"
bind r "say /asylum_r"
```

音乐已经内嵌地图，无需另装播放器。Boss 的 `blacktee` 外观需将包内 `client-skins/blacktee.png` 手动复制到 DDNet 的 skins 目录；服务器不会推送皮肤图片。

## 管理员

```text
room_setting 0 asylum_items
room_setting 0 asylum_status
room_setting 0 asylum_give <CID> <装备ID>
room_setting 0 asylum_god <CID> <0或1>
room_setting 0 asylum_inspect <CID>
room_setting 0 asylum_map asylum_10hourburstman
```

这些不是普通聊天命令。`asylum_give` 自动选择对应类别槽位；`asylum_inspect` 只读输出生命、护甲、四槽物品、充能和装填状态；`asylum_map` 仅限 room 0 切换全服物理地图。

## 构建

Ubuntu：

```bash
sudo apt install build-essential cmake python3 libsqlite3-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDEV=ON
cmake --build build --target DDNet-Server --parallel 8
```

Windows（Visual Studio 2022 x64）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DDEV=ON
cmake --build build --config Release --target DDNet-Server --parallel 8
```

本轮只导入游戏源码与开服需要的地图、音乐、配置；不保留原包的 `runtime-config` 外层、`work` 测试目录或 BUILD 说明。本仓库保留现有 CI、产物名称与之前的编译兼容修复。新地图与可选皮肤的 0.7 客户端显示不当作已验收功能。
