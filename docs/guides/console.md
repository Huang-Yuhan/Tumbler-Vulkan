# 运行时控制台命令参考

## 基本操作

| 按键 | 功能 |
|------|------|
| `~` | 打开/关闭控制台（可通过 `SetToggleKey` 修改） |
| `Enter` | 执行当前命令 |
| `Up/Down` | 浏览命令历史 |
| `Tab` | 自动补全命令名和参数 |

---

## 内置命令

### `help`

列出所有已注册的命令及其用法。

```
help
```

### `actors`

列出场景中所有 Actor。

```
actors
```

### `select <name>`

按名称选中一个 Actor（用于后续操作）。

```
select MainLight
select StingSword
select Floor
```

### `actor.move <target> <x> <y> <z>`

移动指定 Actor。`target` 可以是 Actor 名称或 `selected`（当前选中的 Actor）。

```
actor.move selected 0 2 0
actor.move MainLight 5 0 -3
actor.move Floor 0 -10 0
```

### `render.path <path>`

切换渲染管线。`path` 可选值：

| 值 | 说明 |
|----|------|
| `forward` | 前向渲染（默认） |
| `deferred` | 延迟渲染（G-Buffer + Lighting Pass） |
| `gpu` | GPU Driven（WIP，未实现） |

```
render.path deferred
render.path forward
```

### `destroy <target>`

销毁指定 Actor。`target` 可以是 Actor 名称或 `selected`。

```
destroy selected
destroy SecondLight_Blue
```

---

## 添加自定义命令

通过 `RuntimeConsole::RegisterCommand()` 注册：

```cpp
console.RegisterCommand({
    .Name = "mycommand",
    .Usage = "mycommand <arg1>",
    .Description = "Description of the command.",
    .Handler = [](const std::vector<std::string>& args) {
        // 命令逻辑
    }
});
```

带 Tab 补全的命令需要同时设置 `.AutocompleteHandler` 回调，返回候选列表。
