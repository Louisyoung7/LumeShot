# 全局快捷键失效：排查与修复记录

## 1. 问题现象

环境：

- Ubuntu GNOME，X11 会话
- 输入法 fcitx5
- LumeShot 默认热键 Alt+A
- 同时运行多个带全局热键的程序，例如 Manggo（Alt+Z / Alt+X）

复现步骤：

1. 按 Alt+A，正常唤起区域截图。
2. 右键取消，或完成一次框选。
3. 按 Alt+Q（另一个程序的全局热键）。
4. 再按 Alt+A，没有反应。
5. 打开一次设置界面再关闭，Alt+A 又恢复正常。

异常特征：

- 日志显示 grab 一直处于已注册状态。
- 快捷键处理器收不到任何 KeyPress。
- 键盘映射没有变化。
- 打开设置界面会 ungrab 再 grab，相当于重新注册，热键因此“自己好了”。

## 2. 根因

X11 的键盘抓取有两类：

- 被动 grab：`XGrabKey`，只在指定组合键按下时通知本客户端。
- 主动 grab：`XGrabKeyboard`，直接独占整个键盘。

问题出在主动 grab：

- 其他程序或输入法会短暂调用 `XGrabKeyboard`。
- 主动 grab 期间，键盘事件全部发给持有者。
- 此时被动 grab 依然存在，`XGrabKey` 也不报 `BadAccess`。
- 但被动 grab 收不到按键，表现就是热键“莫名失效”。

关键证据：

- 失效时用 `XQueryPointer` 探测键盘状态，拿到 `keyboard grab probe status=1`。
- 说明有其他客户端正在持有主动键盘 grab。

结论：

- 只依赖 `XGrabKey` 无法保证全局热键一定可用。
- 需要一条不依赖 grab 与焦点的按键通道作为兜底。

## 3. 排查中排除的方向

以下方向都验证过，均不是本次问题的根因：

- 键盘映射变化导致 keycode 失效。
  测试中 modmap 未变化，keycode 仍有效。
- Alt/Super 掩码被写死。
  已改为按当前修饰键映射推导，问题依旧存在。
- `QSocketNotifier` 失联。
  失效时 fd 不可读，符合“按键根本没送到”的表现。
- 快捷键被其他程序抢占。
  重新 grab 没有报 `BadAccess`，grab 实际仍在自己手里。
- 只处理 `KeyPress`、忽略 `MappingNotify`。
  这是隐患，但不是本问题的直接原因。

## 4. 修复方案

采用双通道触发。

通道一：被动 grab

- 保留 `XGrabKey`。
- 负责正常情况下触发，也用于发现快捷键冲突。

通道二：XInput2 原始按键

- 用 `XIAllMasterDevices` 在 root 窗口选择 `XI_RawKeyPress`。
- 原始事件在抓取与焦点之外上报，键盘被其他程序独占时仍能收到。
- 两条通道都收到同一次按压时，250ms 内只触发一次。

## 5. 关键实现

代码位置：[globalhotkey.cpp](../src/platform/globalhotkey.cpp)

修饰位推导：

- Shift / Ctrl / Alt / Super 的掩码从当前修饰键映射推导。
- 推导失败时才回退到 `ShiftMask`、`ControlMask`、`Mod1Mask`、`Mod4Mask`。
- 输入法或布局变化后，旧掩码会失效，必须每次重新计算。

锁定位处理：

- CapsLock、NumLock 对应的修饰位不固定。
- 按当前映射推导，注册时抓 4 种组合：无锁、Caps、Num、Caps+Num。
- 记录实际抓取的完整掩码。
- 释放时按记录的掩码逐个 ungrab，避免残留 grab。

事件循环：

- `QSocketNotifier` 监听 X11 连接 fd。
- `XEventsQueued(QueuedAfterReading)` 循环取事件。
- 处理 `KeyPress`、XI2 `GenericEvent`、`MappingNotify`。

映射变化：

- 收到 `MappingNotify` 后调用 `XRefreshKeyboardMapping`。
- 按当前布局重新解析 keycode 与修饰位。
- 先 ungrab 旧掩码，再按新掩码重新注册。
- 重新注册会调用 `XSync`，期间到达的事件可能已进入 Xlib 队列，所以注册后继续排空队列。

失败重试：

- 重新注册失败时每 2 秒重试一次，最多 5 次。
- 避免热键一直失效到用户手动打开设置界面。

原始按键校验：

- `XI_RawKeyPress` 不带修饰位。
- 用 `XQueryPointer` 查询当前键盘状态。
- 只比对 Shift / Ctrl / Alt / Super，忽略 CapsLock、NumLock 等扩展修饰位。
- 比对通过后触发。

去重：

- 同一次物理按压会被被动 grab 和 XI2 各上报一次。
- 用 250ms 时间窗去重，保证只唤起一次截图。

其他改动：

- [globalhotkey.h](../src/platform/globalhotkey.h) 增加事件循环、重试、XI2 相关成员与方法。
- [CMakeLists.txt](../CMakeLists.txt) 增加 `libXi` 链接。

## 6. 验证

已验证通过：

- 按 Alt+A 唤起截图。
- 按 Alt+Q 后再按 Alt+A，仍能唤起。
- 相同操作重复多次，行为一致。

建议回归项：

- 框选保存。
- 钉图。
- 右键取消。
- 长截图。
- 设置界面改键后再按新热键。
- CapsLock、NumLock 开启状态下触发。

## 7. 经验总结

- `XGrabKey` 成功不代表热键可用。
- 主动键盘 grab 会静默吞掉被动 grab 的按键。
- 全局热键应有独立于 grab 的按键来源。
- 与键盘映射相关的一切掩码都应动态推导。
- 注册与释放必须使用同一份掩码记录。
- 同一按键的多条上报通道必须去重。
