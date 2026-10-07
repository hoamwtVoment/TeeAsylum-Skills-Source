# Tee Asylum Inventory / Tour

> 源码与本轮背包、普通装备、Boss 战扩展由 [101024awa](https://github.com/101024awa) 提供，[hoamwtVoment](https://github.com/hoamwtVoment) 整理上传。

因为101024不会用给特湖边，所以代码可能不是最新的。

基于 DDNet-HunterN 的 Item Asylum 风格二维服务器，普通 DDNet 客户端即可连接。当前为 **36 件普通出生随机装备、5 个升级模块、5 件升级武器、12 件大神装备，总目录 58 项**；保留 FFA、TDM、GG、ELIM、ZS、JGN，支持默认大厅巡回 `asylum_tour` 和 **10 Hour Burst Man 两阶段 AI Boss**。

## 玩法

- 大厅没有武器，每轮从 16 张战斗图抽取 3 张，触碰 1/2/3 号选图板投票；45 秒后切换，战斗结束返回大厅。巡回支持一人游玩，无人时停止大厅倒计时。
- 随机池为 7 近战、21 远程、8 特殊。按 `1/2/3` 切换，左键使用；升级模块放在槽位 4，按 `4` 后左键消耗模块升级远程武器。
- M1911、Pixel gun、9mm、Suppressed pistol、SSG-08 分别达到 5/5/4/6/10 次本武器有效击杀后获得升级模块。
- 有限弹匣、空弹自动装填、R 主动装填；America、Medkit、Holy Mantle 使用一次后对应物品槽消失。Darkheart、Lilynette、Blaster 有专属技能。
- 普通角色 100 HP、零出生护甲；脱战 5 秒后每秒回复最大生命的 2.5%，GG 不回血。
- Boss 每名挑战者 6 命，生命随人数增加；半血后回满进入第二阶段，速度提高、承伤翻倍，并切换阶段音乐。

Boss 第一阶段使用 Darkheart 三连冲刺、Twilight、Star Platinum、Vampirism 抓取拖拽和霰弹枪；第二阶段增加 Grand Volley、Strong Left。Boss 生命为 `2000 + 666 ×（人数−1）`，上限 11990，使用同一场地，不整体传送玩家。

### 大神装备

默认不会随机获得大神装备：`asylum_god_chance` 为 `0`，`asylum_god_only` 为 `0`。在 RCON 中设置对应房间：

```text
room_setting 0 asylum_god_chance 5
room_setting 0 asylum_god_only 1
```

`chance` 支持 `0–100`，每个槽位独立按百分比抽取；`only 1` 优先于概率，只抽大神装备。全部关闭时将两项都设为 `0`。设置从下一次出生或重抽生效，不移除已持有的装备。大厅无武器，GG 和固定身份配装仍按各自规则。

保留 main 的装备 ID `0–45`；旧大神分支的 `32–43` 调整为 `46–57`，已有测试命令和手写配装配置需要按新 ID 修改：

| ID | 大神装备 |
|---|---|
| 46 | 封禁之锤 / Banhammer |
| 47 | 白桦树 / Birch tree |
| 48 | 天顶剑 / Zenith |
| 49 | 沙皇炸弹 |
| 50 | 黑洞射线枪 |
| 51 | 审判 / Judge |
| 52 | 失控列车 |
| 53 | 惊吓 / Jumpscare |
| 54 | 摩艾 / Moyai |
| 55 | 动感星期五 / Microphone |
| 56 | 恋符MasterSpark |
| 57 | The World（ザ・ワールド） |

巨石强森跳脸、BF 原声和 The World 录音保留；The World 的减速、暂停同时作用于 Boss AI 和它的弹幕。素材与效果说明见 [大神素材说明](assets/asylum/README.md)。

## 五条悟身份（开发分支）

五个技能不属于普通/大神装备池，没有自动获取入口，只有 RCON 显式变身后才会拥有：

```text
asylum_gojo <CID> 1
asylum_gojo <CID> 0
asylum_gojo_status <CID>
asylum_gojo_energy <CID> <0–200>
```

自动定位玩家所在房间，不用 `room_setting`。开启后 1–5 分别为苍拳、苍、赫、茈、无量空处；按住蓄力、松开释放。苍有普通到最大输出的连续蓄力，普通苍更小、更灵活，并非只能放最大输出。

咒力蓝条上限 200；最大苍半径 224，已提速、减轻操控惯性并强化吸力；茈保持圆形，高速穿墙，实墙只使它略微缩小。二段跳耗尽后可付 12 点无限继续踏空，蓄力略微减速。领域外的人被边界阻挡，背景采用用户提供的无量空处图片；赫按实际飞行朝向判直击，持续推着目标走、约每 40ms 低伤命中。各技能保留不同的旋涡、冲击波、圆球能量裂纹和符文特效。

最低蓄力为苍/赫 0.5 秒、茈 1 秒，点按取消；苍赫合成也与主动茈共享冷却。测试无限咒力及无限“无限”用 `asylum_test_inf_cursedenergy <CID> <0或1>`，不会跳过蓄力或冷却。

大神武器测试开关：`asylum_test_inf_ammo <CID> <0或1>`（无限弹匣，不取消攻击间隔）；`asylum_test_noattackinterval <CID> <0或1>`（取消通用攻击间隔，并允许 Birch tree、MasterSpark、The World 连续尝试，不跳过持续时间/蓄力）。两者都可直接 RCON 使用，或通过 `room_setting <房间ID> ...` 使用。

默认大厅没有武器，需要先切到战斗图；GG、感染者和 JGN 巨人保留固定身份限制。当前只做管理员可调用身份，未接任何随机掉落、奖励或模式自动变身。具体操作与初版数值见 [五条悟说明](doc/gojo.md)。

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

### 便携测试指令

直接在 RCON 使用，自动定位目标玩家的房间，不用加 `room_setting`：

```text
asylum_test_nocd <CID> <0或1>
asylum_test_reset_cd <CID>
asylum_test_god <CID> <0或1>
asylum_test_weapon <CID> <装备ID>
asylum_test_loadout <CID> <近战ID> <远程ID> <特殊ID>
asylum_test_items [CID]
```

分别用于无冷却、单次清空冷却、可被击中的锁血无敌、单件换装、三槽配装和查看装备 ID。无冷却不跳过技能前摇、持续时间、弹匣装填和 R 充能；管理员测试配装不受大神随机概率限制。`asylum_test_god` 保留命中、击退、冻结与时停效果，和 main 原有的 `asylum_god` 完全免伤模式分开。四槽模块与升级武器可通过 `asylum_test_weapon` 或 `asylum_give` 指定。

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
