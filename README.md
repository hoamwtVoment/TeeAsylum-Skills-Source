DDNet-PvP HunterN猎人杀
===
> 源码由 [101024awa](https://github.com/101024awa) 提供，[hoamwtVoment](https://github.com/hoamwtVoment) 代为上传。

模式规则：

1.每回合都会秘密随机选择猎人。猎人必须消灭所有平民。

2.猎人造成双倍伤害，有一把瞬杀锤和破片榴弹，而平民没有锤子，只能使用常规武器。

3.活着的玩家看不到死去玩家的信息。

4.如果猎人死亡，将通知其他猎人。

5.在游戏开始时，玩家只知道自己的身份。

Rules:

1.Each round will secretly randomly select Hunter(s). Hunter(s) must eliminate all the Civilians.

2.The Hunter deals double damage and has an instant-kill hammer and fragmentation grenades, while Civilians have no hammer and can only use regular weapons.

3.The living players cannot see messages from dead players.

4.If the Hunter dies, the other Hunters will be notified.

5.At the beginning of the game, players only know their own identity.

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

更新记录
---
### 2026-10-03

**修复**
- 进出 tune zone 时，即使没有设置提示语，服务器也会给玩家发送一条空聊天消息。现在只有设置了提示语才会发送。
- `map_merge` 工具用 `free()` 释放了 `new` 创建的对象，改为 `delete`。

**代码清理**
- 删除 `CPlayer::Tick()` 里两段重复且从未执行过的旁观自由视角代码（比较的是 0.6 协议的 `SPEC_FREEVIEW`）。自由视角位置仍在 `OnDirectInput` 中按输入设置，行为不变。
- 按上游 DDNet 的做法屏蔽 OpenSSL 3.0 的弃用警告。
- `map_merge`：`&&` 与 `||` 混用处补上括号，删除未使用的变量。
- Linux 和 Windows 构建现在都没有编译警告。

**构建与 CI**
- `actions/checkout`、`actions/upload-artifact` 升级到 v7。v3 已被 GitHub 停用，之前的构建会直接失败。
- Windows 构建环境从已下线的 `windows-2019` 换成 `windows-2022`；Linux 固定为 `ubuntu-24.04`，避免 `ubuntu-latest` 切换版本时出问题。
- 一个平台构建失败时，不再取消另一个平台的构建。
- 补上 `libs/sqlite3/windows/lib64/sqlite3.dll`。它之前被 `.gitignore` 的 `*.dll` 规则漏掉，新克隆的仓库在 Windows 上无法完成 CMake 配置。
- Windows 构建产物现在附带 `sqlite3.dll`。

**文档**
- README 注明源码来源，构建步骤补上创建 build 目录。