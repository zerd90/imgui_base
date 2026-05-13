# 自定义控件需要完成什么

本文分两层说明：一是本仓库里基于 **`ImGui::IImGuiItem`** 的封装约定；二是底层 Dear ImGui 里「一条控件」通常要接好的事情（与是否使用 `IImGuiItem` 无关）。

---

## 一、基于 `IImGuiItem` 时（本仓库推荐路径）

### 1. 类与接口

| 事项 | 说明 |
|------|------|
| **继承方式** | `class YourItem : public IImGuiItem`，保证可在外部调用 `show()` / `showDisabled()`。 |
| **实现 `showItem()`** | 纯绘制 + 交互逻辑写在这里；由基类 `show()` 在合适时机调用。 |
| **`showItem()` 返回值** | 表示「本帧是否发生原生意义上的激活」：例如按钮被点击、复选框/开关状态被用户切换。为 `true` 时，基类会把 `ImGuiItemNativeActive` 置为 `true` 并调用对应 `addActionCallback` 回调。 |
| **`updateItemStatus()`** | 默认实现用 **`GetItemRectSize()`** 和 **`IsItemHovered` / `IsItemActive` / `IsItemActivated` / `IsItemDeactivated`** 更新 `mItemStatus` 并触发回调。若控件由多个 ImGui 子项拼成、或最后一项不是整块热区，可像 **`IImGuiInput`** 一样重写，自行修正 `mItemSize` 或查询方式。 |

### 2. 与 `show()` 的配合关系（阅读 `ImGuiItem.cpp` 即可）

1. 记录 **`mItemPos`**（`GetCursorScreenPos()`）。  
2. 调用 **`showItem()`**。  
3. 根据返回值设置 **`ImGuiItemNativeActive`**。  
4. 若有 **Tooltip**，基类会 `SetItemTooltip`。  
5. 调用 **`updateItemStatus()`**。

因此：**最后一帧在 ImGui 里登记的「当前项」必须与你的热区一致**，否则 `IsItemHovered()` 等会与视觉错位；见下文「ImGui 层」的 `ItemAdd` / `ButtonBehavior`。

### 3. 标签与 ID

- **`mLabel`** 既参与 **`GetID`**（需唯一、稳定），也可用于显示；隐藏显示可用 **`##后缀`**，`CalcTextSize(..., true)` 的第三个参数为 `true` 时可忽略 `##` 之后内容。  
- 避免在窗口根节点使用空字符串作 ID（ImGui 会断言）；需要无可见文字时用 **`"##something"`**。

### 4. 可选能力

- **`mHoveredFlags`**：在构造或外部设置，影响 `IsItemHovered(mHoveredFlags)`。  
- **`setToolTip` / `showDisabled`**：沿用基类即可。  
- **`setItemWidth` / `setItemHeight`**：是否在 `showItem()` 里尊重 `mManualItemSize` 由具体控件决定（内置 `ImGuiCheckbox` 未强行使用宽度，但 `ImGuiButton` 等会用到）。

---

## 二、Dear ImGui 层：一条「典型」自定义控件应接什么

不经过 `IImGuiItem`、直接写 `bool MyWidget(...)` 时，下列内容仍适用；与内置 **`Checkbox`** 等实现同一套路即可与布局、裁剪、导航、调试工具一致。

### 1. 布局（占坑）

- 用 **`ItemSize`** 声明本控件在父布局中占用的最小区域（常与 `window->DC.CursorPos` 算出的包围盒一致）。  
- 需要整行高度与文字基线对齐时，传入与 **`Checkbox`** 类似的 `text_baseline_y`（例如 `style.FramePadding.y`）。

### 2. 注册为「一个 Item」（核心）

- **`ItemAdd(bb, id, ...)`**：写入 **`g.LastItemData`**（矩形、导航矩形、状态标志等），并参与裁剪与导航扫描。  
- **`id`** 一般来自 **`window->GetID(label)`**，且需配合 **`PushID` / `PopID`** 保证在树中唯一。  
- 若 **`window->SkipItems`** 为真，应尽早返回，避免无意义绘制。

### 3. 交互（鼠标 + 键盘/手柄）

- 可点击类控件：在 **`ItemAdd`** 之后对同一 `id` 与包围盒调用 **`ButtonBehavior`**（在 `imgui_internal.h`），以正确维护 **`ActiveId` / `HoveredId`**、焦点与点击释放语义。  
- 仅当不用 **`ItemAdd`** 又需要 **`ButtonBehavior`** 时，需自行 **`KeepAliveID`**（见 ImGui 源码注释）。  
- 需要键盘导航高亮时，可调用 **`RenderNavCursor`**（内部 API）。

### 4. 绘制

- 使用当前 **`ImGuiWindow::DrawList`** 或 **`GetWindowDrawList()`**，在 **`ItemAdd`** 判定可见后再画，避免无效图元。  
- 颜色优先 **`GetColorU32(ImGuiCol_*)`**，需要临时样式用 **`PushStyleColor` / `PushStyleVar`**，避免直接改全局 **`g.Style`**。

### 5. 值变化与「编辑」语义

- 用户操作导致业务值变化时，调用 **`MarkItemEdited(id)`**，以便 **`IsItemDeactivatedAfterEdit()`** 等 API 行为正确（与 `Checkbox` 在值变化时一致）。

### 6. 上下文与「全局状态」

- 没有另一套平行 C 全局变量；当前线程上下文为 **`GImGui` → `ImGuiContext` → `CurrentWindow`**。  
- 常规自定义控件应通过 **`ItemSize` / `ItemAdd` / `ButtonBehavior` / `MarkItemEdited`** 间接更新状态，而不是手写修改大量 `g.xxx` 字段。

### 7. 本仓库源码中的参考

| 参考 | 路径 |
|------|------|
| 基类生命周期与状态回调 | `tools/ImGuiItem.cpp`（`IImGuiItem::show`、`updateItemStatus`） |
| 仅用公开 API 的短实现 | `ImGuiCheckbox::showItem`（`Checkbox`） |
| 内部 API 拼控件 | `demo.cpp` 中的 **`ImGuiSwitch::showItem`**（`ItemSize` + `ItemAdd` + `ButtonBehavior` + 自绘） |
| 复合控件如何改 `updateItemStatus` | `IImGuiInput::updateItemStatus`（`ImGuiItem.cpp`） |
| ImGui 官方参考实现 | `imgui-source/imgui_widgets.cpp` 中的 **`Checkbox`** |

---

## 三、自检清单（实现完可逐项对照）

- [ ] `SkipItems` 时提前返回。  
- [ ] 布局：`ItemSize` 与最终可点区域一致或合理。  
- [ ] `ItemAdd` 注册 `id` 与包围盒；裁剪外行为符合预期。  
- [ ] 点击/导航：`ButtonBehavior`（或等价且文档认可的路径）。  
- [ ] 绘制在可见分支内；颜色走样式 API。  
- [ ] 值变化时按需 `MarkItemEdited`。  
- [ ] 若继承 `IImGuiItem`：`showItem()` 返回值与 `ImGuiItemNativeActive` 语义一致；`updateItemStatus()` 与最后一项 ImGui 状态一致。  

按上述接好后，**`IsItemHovered` / `IsItemActive` / 同一窗口布局 / 导航 / 禁用（`BeginDisabled`）** 才会与视觉和交互统一。
