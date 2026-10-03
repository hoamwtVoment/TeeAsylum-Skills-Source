Tee Asylum
===
> 源码由 [101024awa](https://github.com/101024awa) 提供，[hoamwtVoment](https://github.com/hoamwtVoment) 代为上传。

随机三槽武器 PvP 模式，灵感来自 Roblox 的 [Item Asylum](https://itemasylum.wiki/Item_asylum)，由 [Bamcane/DDNet-Teeworlds-Hunter](https://github.com/Bamcane/DDNet-Teeworlds-Hunter)（猎人杀）改编而来。普通 DDNet 客户端即可连接。

本版加入 Item Asylum 风格的 6 种模式和 12 件新装备，装备总数 24。玩法参考 [Item Asylum Wiki](https://itemasylum.wiki/Item_asylum)，并按 DDNet 的二维移动、血量、投射物和网络协议做了适配。

怎么玩
---
每次出生随机获得近战、远程、特殊各一件装备。按 `1/2/3` 或滚轮切换槽位，左键使用。特殊模式的槽位内容会随身份变化，以聊天和屏幕上的装备提示为准。

投票菜单可以切换模式、地图、重开比赛，或创建独立房间，默认是 FFA。至少两名玩家进入同一房间才会开始正式比赛。

游戏模式
---
| 模式 | 规则 |
|---|---|
| [FFA](https://itemasylum.wiki/FFA) 自由混战 | 每次出生随机近战、远程、特殊各一件；击杀 +1，自杀或环境死亡 -1；25 分或 8 分钟结算，平分加时。 |
| [TDM](https://itemasylum.wiki/Tdm) 团队死斗 | 红蓝两队，队友免伤；团队 75 分或 6 分钟结算，平分加时。 |
| [GG](https://itemasylum.wiki/GG) 武器升级赛 | 槽位 1 是进阶武器，槽位 2 急救包，槽位 3 冲刺；必须用当前进阶武器击杀才升级，共 17 个阶段，最后用“黄金勺”获胜。死亡保留进度。 |
| [ELIM](https://itemasylum.wiki/ELIM) 三命淘汰 | 每人 3 命，死亡重抽；每次击杀生命上限 +2，最多 20；命数耗尽进入观战。3 分钟后开启收缩安全区，圈外每秒损失 1 生命，最后存活者获胜。 |
| [ZS](https://itemasylum.wiki/ZS) 感染生存 | 随机选出初始感染者；生还者死亡后变成感染者，感染者无限复活，同阵营免伤。生还者撑过 4 分钟获胜，每次新感染延长 15 秒；全员感染则感染者获胜。 |
| [JGN](https://itemasylum.wiki/JGN) 巨人讨伐 | 随机一人当巨人，只有 1 命，生命上限随人数在 60 到 200 之间，免击退。挑战者各 3 命且互相免伤。巨人被击败或 4 分钟结束时挑战者获胜，挑战者全部淘汰则巨人获胜。 |

ELIM、ZS、JGN 开局锁定名单，中途加入、离开重进或切到观战再回来的玩家要等下一局。每局结束后自动开始下一局。

装备
---
一般角色 10 生命、2 护甲；默认死亡 2 秒后复活，出生保护 1 秒（攻击会解除），弹药无限。FFA 共有 7 × 9 × 8 = 504 种三槽组合。

| 类别 | 装备 |
|---|---|
| 近战（7） | 平底锅、小刀、球棒、巨大汤勺、黑心剑、能量剑、垂死平底锅 |
| 远程（9） | 左轮、冲锋枪、霰弹枪、电磁炮、榴弹炮、弩、冰冻射线、英式火箭筒、America |
| 特殊（8） | 急救包、冲刺汽水、冲击波、破片雷、神圣斗篷、重抽骰子、滑翔伞、连斩 |

新增装备：
- [巨大汤勺](https://itemasylum.wiki/Comically_large_spoon)：扩大近战范围，强击退。
- [黑心剑](https://itemasylum.wiki/Darkheart)：命中敌人回复 1 生命。
- [能量剑](https://itemasylum.wiki/Energy_sword)：快速的 4 伤害斩击。
- [垂死平底锅](https://itemasylum.wiki/Dying_pan)：命中时向下击退。
- [弩](https://itemasylum.wiki/Crossbow)：8 伤害，远距离命中最多 10。
- [冰冻射线](https://itemasylum.wiki/Freeze_ray)：1 伤害并冻结 1 秒。
- [英式火箭筒](https://itemasylum.wiki/British_bazooka)：随机发射 2 伤害软弹或 5 伤害爆炸弹。
- [America](https://itemasylum.wiki/America)：10 伤害激光，后坐力很大；JGN 的随机装备池里禁用。
- [神圣斗篷](https://itemasylum.wiki/Holy_mantle)：抵挡下一次攻击，破盾后有 0.25 秒保护，20 秒充能。
- [重抽骰子](https://itemasylum.wiki/Re-roll_dice)：原地重抽三槽装备，不恢复血甲，10 秒冷却。
- [滑翔伞](https://itemasylum.wiki/Parasol)：持有时减缓下落，使用可以上升。
- [连斩](https://itemasylum.wiki/Cleave)：原作的连续斩击合并为前方一次 4 伤害斩击，10 秒冷却。

与原作的差异
---
原作通常是 100 HP，这里沿用 DDNet 较小的生命值刻度，伤害经过缩放和联机平衡调整，不能直接照搬 Wiki 上的数字。原作的三维布娃娃、爆头、肢体动画、模型、音乐和特殊界面没有导入，装备效果用 DDNet 现有的锤击、激光、投射物、冻结、击退、音效和文字提示来表现。

Wiki 上的 BOSS、MU、TC、KIT、XMAS、VIP 等模式尚未实现。

管理员命令
---
以下命令通过 RCON 使用，不是玩家聊天指令：
- `room_setting 0 asylum_items`：查看装备 ID。
- `room_setting 0 asylum_status`：查看当前模式状态。
- `room_setting 0 asylum_loadout <客户端ID> <近战ID> <远程ID> <特殊ID>`：给普通角色指定装备。固定身份和 GG 模式不能覆盖。

在Ubuntu上使用CMake构建
---
1.安装依赖库
```
    sudo apt install build-essential cmake python3 libsqlite3-dev
```
2.编译服务端
```
    mkdir build && cd build
    cmake ..
    make -j16
```
