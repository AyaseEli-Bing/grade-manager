# 班级成绩管理系统 · UI/UX 设计规范

> 版本 v1.0 | 设计师：颜好看 | 技术栈：Tauri v2 + 原生 HTML / CSS / JS（无框架、无构建步骤）
> 三轴刻度：DESIGN_VARIANCE 3 / MOTION_INTENSITY 2 / VISUAL_DENSITY 7
> 配套 Token 源文件：`src/design-tokens.css`
> 本文件是前后端的唯一设计契约。前端实现与本文件不一致时，以本文件为准。

---

## 1. 设计语言与对标说明

### 1.1 寄存器判定：Product（工具型）

本项目属于 **Product 寄存器**——设计服务于功能，不承载品牌形象表达。判定依据：

- 用户是班主任/任课教师，每天在同一界面上停留数十分钟核对数字
- 界面价值 = 单位时间内可读出多少准确信息，而非第一眼惊艳度
- 无营销诉求、无转化漏斗、无 Hero 区

因此全书遵循 Product 寄存器策略：**中性色主导 + 单一强调色 ≤10% 表面积 + Sans-Serif + 功能性动效**。任何装饰性处理（大圆角、斑马色块、渐变标题、卡片悬浮投影），以及任何为了「好看」而牺牲信息密度的动作，都属于违反。

### 1.2 对标品牌

| 对标对象 | 借鉴什么 | 明确不借鉴什么 |
|---|---|---|
| **Linear** | 1px 线条分组取代卡片阴影、34px 紧凑导航行、键盘优先的焦点环、亚克力级克制的动效 | 深色主题（本项目是办公软件，浅色更耐看）、以及它的 Indigo 强调色 |
| **Notion** | 数据表的行 hover 微反馈、可编辑单元格"平时无框、悬停显形"的行为模式 | 过度留白的段落式布局（浪费纵向空间） |
| **Apache ECharts / 财务类表格软件** | 总分这些关键数值用 `tabular-nums` 纵向对齐，靠列宽与字重建立层级 | 高饱和图表配色 |

一句话氛围：**冷静、精确、像一本摊开的记分册**。打开文件就该看到数据，而不是看到设计。

### 1.3 三轴刻度校准

| 轴 | 取值 | 具体落法 |
|---|---|---|
| DESIGN_VARIANCE | 3 | Flex/Grid 对称布局，无负 margin 重叠、无非对称瀑布流。唯一允许的"变化"是内容宽度自适应（学号列固定、备注列 flex） |
| MOTION_INTENSITY | 2 | 只保留 hover 变色(120ms)、按下反馈(80ms)、下拉展开(200ms)、模态进出(240ms)。全站零装饰动画、零脉冲、零骨架流光以外的时间性动效 |
| VISUAL_DENSITY | 7 | 表格行高 44px、表格正文 13px、模块间距 16px、页面级间距 24px。统计卡不用投影盒，用 `1px solid var(--border-default)` 分组 |

### 1.4 主题与平台

- **浅色单主题**。v1 不做深色模式，不做主题切换入口。
- 桌面端固定三栏骨架（顶栏 / 侧栏 / 内容），窗口最小宽度建议 1024px，低于此宽度侧栏自动折叠为图标条。
- 无触摸交互，但保持点击热区 ≥44px（鼠标长期操作同样受益）。

---

## 2. 配色表

### 2.1 四层配比

| 层级 | 占比 | 承载内容 |
|---|---|---|
| 中性色 | 约 88% | 背景、边框、分割线、正文、次级文本 |
| 强调色（深青单一色） | 约 7% | 主按钮、选中导航、选中行、可排序表头 hover、聚焦环 |
| 语义色 | 约 4% | 危险/成功/警告，仅出现在 Toast、删除确认、分数异常单元格 |
| 效果色 | 约 1% | 遮罩、聚焦环透明层、阴影 |

### 2.2 A1 原始层（仅存在于 `design-tokens.css` 第 1 节）

**中性 · 微冷灰蓝 Neutral**

| Token | 值 | 用途 |
|---|---|---|
| `--gray-0` | `#FFFFFF` | 卡片 / 表格 / 弹窗表面 |
| `--gray-25` | `#FCFDFD` | 侧边栏底色、状态栏底色 |
| `--gray-50` | `#F7F9FA` | 应用底色、表格行 hover |
| `--gray-100` | `#EEF1F4` | 表头、分段控件槽、禁用/ secondary 悬停 |
| `--gray-200` | `#E1E6EB` | 默认边框、禁用输入框底 |
| `--gray-300` | `#C6CED6` | 强边框、空状态图标（装饰） |
| `--gray-400` | `#98A3AE` | 仅禁用文本与未激活排序箭头 |
| `--gray-500` | `#66727F` | 辅助文本（AA 下限） |
| `--gray-600` | `#4D5866` | 次级文本、表头字 |
| `--gray-700` | `#3A444F` | 保留 |
| `--gray-800` | `#262D35` | 正文主色、Toast 底 |
| `--gray-900` | `#171C21` | 保留 |

**强调 · 深青 Teal**

| Token | 值 | 用途 |
|---|---|---|
| `--teal-50` | `#EEF7F7` | 选中行/选中导航项底色 |
| `--teal-100` | `#D3EAEA` | 备用 |
| `--teal-200` | `#A8D6D6` | 分布图第一档柱、选中边框 |
| `--teal-300` | `#71BBBB` | 分布图第二档柱 |
| `--teal-500` | `#1B8182` | 分布图第三档柱、科目对比条填充 |
| `--teal-600` | `#0F6C72` | **基准强调色**（主按钮、Checkbox 选中、聚焦环） |
| `--teal-700` | `#0E575C` | 主按钮 hover、强调链接、及格以上高分文字 |
| `--teal-800` | `#10474B` | 按钮按下态 |
| `--teal-900` | `#0D3A3D` | 保留 |

**选型理由（必须保留，防止后期漂移）**：避开 Tailwind 默认 Indigo `#6366F1`（业界公认的 AI 生成首罪症状）和常见 Emerald `#059669`，同时严格满足「非紫非粉」硬约束。深青在白底上是低疲劳的仪表感颜色，教师盯几百个分数的场景下，比蓝更静、比绿更专业。

### 2.3 A2 语义层（业务层唯一可直接引用）

| Token | 映射 | 对比度（对白底） | 用途 |
|---|---|---|---|
| `--bg-app` | gray-50 | — | 应用底色 |
| `--bg-surface` | gray-0 | — | 表格 / 卡片 / 弹窗 |
| `--bg-surface-subtle` | gray-100 | — | 表头、表脚、分段控件槽 |
| `--bg-scrim` | gray-900 / 32% | — | 模态遮罩 |
| `--fg-default` | gray-800 | **13.9 : 1** | 正文、表格主文本 |
| `--fg-secondary` | gray-600 | **7.2 : 1** | 表头、卡片标题、次级说明 |
| `--fg-muted` | gray-500 | **4.9 : 1** | 辅助说明、占位符、状态栏 |
| `--fg-subtle` | gray-400 | 2.6 : 1 | 仅 disabled 与装饰元素（WCAG 对 inactive 组件豁免） |
| `--fg-on-accent` | gray-0 | **6.2 : 1** | 强调色底上的文字 |
| `--accent-default` | teal-600 | **6.2 : 1** | 主按钮、选中态 |
| `--accent-hover` | teal-700 | **8.3 : 1** | hover |
| `--accent-active` | teal-800 | — | active |
| `--accent-emphasis` | teal-700 | **8.3 : 1** | 可点击表头 hover、链接 |
| `--border-default` | gray-200 | — | 卡片、输入框、分隔线 |
| `--border-subtle` | gray-100 | — | 表格行分隔（比主边框更弱） |
| `--border-strong` | gray-300 | — | hover 边框 |

**语义状态**

| 组 | fg | bg | solid | fg 在 bg 上的对比度 |
|---|---|---|---|---|
| success | `#17653F` | `#EFF8F2` | `#1A7348` | 约 8.0 : 1 |
| warn | `#87550A` | `#FDF7EA` | `#96600A` | 约 7.0 : 1 |
| danger | `#9E2C24` | `#FDF2F1` | `#C0362C` | 约 7.0 : 1 |
| info | teal-700 | teal-50 | teal-600 | 约 8.0 : 1 |

info 直接复用强调色家族，**不新增色相**——这是刻意保持调色板最小化的克制策略。

### 2.4 C 扩展：排名前三（纯 Token 着色，不使用图标或符号代替）

| 名次 | fg | bg | border | fg-on-bg 对比度 |
|---|---|---|---|---|
| 第 1 名 | `#8F6208` | `#FDF6E3` | `#EEDDAE` | 5.0 : 1 |
| 第 2 名 | `#53606D` | `#F2F5F8` | `#DCE2E8` | 5.9 : 1 |
| 第 3 名 | `#8A5232` | `#FAF0E9` | `#E9D5C5` | 5.6 : 1 |
| 第 4 名及以后 | `--fg-secondary` | 无 | 无 | — |

三色均为**降饱和的大地色**（古铜 / 银蓝 / 陶铜），不是荧光金银。原因：这是在 44px 行高、几十行重复出现的密集表格里，高饱和金银会形成横向色噪。

### 2.5 C 扩展：分数字段状态

只有异常值着色，普通分不着色——这是密度 7 的核心纪律。

| 状态 | 触发条件 | 表现 |
|---|---|---|
| 优秀 | ≥ 该科满分的 90% | `--score-excellent-fg` 文字色 + `--score-excellent-bg` 单元格底 |
| 不及格 | < 该科满分的 60% | `--score-fail-fg` 文字色 + `--score-fail-bg` 单元格底 |
| 非法 | 超出满分 / 负数 / 非数字 | `--score-invalid-*` + 单元格内嵌 1px danger 描边 + 行尾 alert 图标 |
| 未录入 | 该单元格为空 | `--score-empty-fg` 显示短横线 `—`，字号同列但字重 400 |
| 编辑中 | 单元格 input 获得焦点 | 白底 + `--score-editing-brd` 强调色描边 |

### 2.6 C 扩展：成绩分布可视化

| Token | 值 | 语义 |
|---|---|---|
| `--dist-bin-fail` | `#C0362C` | 低于 60% |
| `--dist-bin-b1` | teal-200 | 60 ~ 70% |
| `--dist-bin-b2` | teal-300 | 70 ~ 80% |
| `--dist-bin-b3` | teal-500 | 80 ~ 90% |
| `--dist-bin-b4` | teal-700 | 90% 以上 |
| `--dist-track-bg` | gray-100 | 柱底槽 |

设计意图：不及格用独立的红（一眼警告），及格区间用**单色相递增**（越往上越深 = 越优秀），不依赖图例也能读懂重心。零图表库，纯 CSS 高度百分比或 SVG `<rect>`。

### 2.7 禁止清单（配色）

- 禁止 `#7C3AED` / `#A855F7` / `#9333EA` / `#EC4899` 及其相互组成的任何渐变
- 禁止 Indigo→Pink 渐变 + 发光边框 + 毛玻璃 的三位一体组合
- 禁止把 Tailwind 默认 Indigo `#6366F1` 作为强调色
- 禁止渐变文字（`background-clip: text` + 渐变背景）
- 禁止超过 2 个色相同时作为「语义主色」（当前只有：深青=信息与操作，红=危险，绿=成功，琥珀=警告）

---

## 3. 排版规范

### 3.1 字体栈

```css
--font-body: -apple-system, BlinkMacSystemFont, "PingFang SC", "Hiragino Sans GB",
             "Microsoft YaHei", "Source Han Sans SC", "Noto Sans CJK SC",
             "Segoe UI", Roboto, sans-serif;
--font-display: var(--font-body);
--font-mono: ui-monospace, "SF Mono", "JetBrains Mono", Menlo, Consolas,
            "PingFang SC", monospace;
```

三条原则：

1. **零网络字体依赖**。Tauri 离线桌面应用，任何 `@font-face` 远程加载都会在断网时造成 FOUT/FOIT。全部使用系统字体。
2. **标题与正文同栈**。工具型产品不引入第二套字体，层级靠字重（400/510/590）与字号建立，不靠「换个更花的字体」。
3. **等宽字体只用于数值列**。分数、学号、总分、平均分这些需要纵向对齐的列用 `--font-mono`，姓名/备注保持中文比例字体（等宽中文字在长文本里很压迫）。

### 3.2 字号阶梯

| Token | 值 | 行高 | 字重 | 用途 |
|---|---|---|---|---|
| `--text-xs` | 11px | 1.5 | 400 | 状态栏、表格脚注、分组标题 |
| `--text-sm` | 12px | 1.5 | 510 | 徽章、 Popover 副文本、卡片单位后缀 |
| `--text-base` | 13px | 1.45 | 400 | **表格正文、输入框**（密度 7 基准） |
| `--text-md` | 14px | 1.55 | 400/510 | 按钮、导航项、卡片正文、模态框正文 |
| `--text-lg` | 16px | 1.4 | 510 | 区块小标题、模态框标题 |
| `--text-xl` | 20px | 1.35 | 510 | 视图页面标题 |
| `--text-2xl` | 24px | 1.25 | 590 | 统计卡片数值（KPI） |
| `--text-3xl` | 30px | 1.25 | 590 | 空状态标题（唯一允许的大字号，本产品无 Hero） |

正文最小 11px（仅状态栏），表格正文 13px，**任何地方不低于 11px**。

### 3.3 字距

| 场景 | 字距 |
|---|---|
| 中文正文 / 表格文字 | `0` |
| 考试代号、科目代号等全大写字样 | `0.06em` |
| ≥ 20px 的标题 | `-0.01em` |
| 数值列（配合 `font-variant-numeric: tabular-nums`） | `0` |

中文不做正负字距大范围调整——中文本身是方块字，负字距会导致笔画粘连。

### 3.4 数值排版规则（关键）

所有分数、总分、平均分、名次列**必须**：

```css
font-family: var(--font-mono);
font-variant-numeric: tabular-nums;
```

否则 90 和 88 在同一列里会因为字宽不同導致小数点/个位不对齐，老师在纵向扫读排名时会读错行。这是本产品最容易出錯也最影響信任感的細節。

### 3.5 行长与换行

- 表格：中文单行不超过 20 字，超出 `text-overflow: ellipsis` + `title` 属性兜底（备注列允许）
- 备注列：允许 2 行折行，`line-height: 1.45`，超过 2 行省略
- 姓名列：不折行，固定列宽，超出省略

---

## 4. 图标规范

### 4.1 锁定方案：内联 SVG 描边图标集（24 网格 / 1.5px）

**本项目锁定唯一图标方案**，前端不得再引入第二套、不得使用字体图标、不得使用图片图标、**不得使用 emoji**。

统一模板（所有图标共享同一套属性）：

```html
<svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="1.5" stroke-linecap="round"
     stroke-linejoin="round" aria-hidden="true" focusable="false">
  <!-- path 内容 -->
</svg>
```

强制属性说明：

| 属性 | 值 | 原因 |
|---|---|---|
| `viewBox` | `0 0 24 24` | 全项目统一画布，任意缩放不失真 |
| `fill` | `none` | 纯描边，保证轻量化观感 |
| `stroke` | `currentColor` | 颜色由父元素 `color` 继承，天然支持 hover/active/disabled 变色，无需写第二套图标 |
| `stroke-width` | `1.5` | 24 画布下最清晰的描边粗细；2 太重，1.25 在高 DPI 屏上会发虚 |
| `stroke-linecap` / `linejoin` | `round` | 统一圆头，避免视觉风格割裂 |
| `aria-hidden` | `true` | 装饰性图标对读屏器隐藏；**若图标独立充当按钮语义，父按钮必须带 `aria-label`** |

**尺寸分级（只有三档，不得出现第四档）**

| 档位 | 像素 | 使用场景 |
|---|---|---|
| 16px | 行内 | 导航项、按钮内、Toast、表头排序箭头、多选框勾 |
| 20px | 小图标按钮 | 表格行操作按钮（编辑/删除）独立成按钮时 |
| 24px | 独立图标按钮 | 顶栏图标按钮、模态框关闭按钮 |
| 48px | 插图 | **仅空状态插图**（`--empty-icon-size`，严格执行 1.5px 描边 = 视觉上更细更轻） |

> 实现建议：在 `src/icons.js` 里导出一个 `icon(name, size)` 函数返回 SVG 字符串，全站统一调用，避免同一图标在多处手写不一致。

### 4.2 完整图标清单（全项目需要的全部图标）

#### A. 侧边导航（4 枚，16px）

| 名称 | 语义 | SVG path 内容（置于统一模板内） |
|---|---|---|
| `nav-students` | 学生管理 | `<path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/>` |
| `nav-scores` | 成绩录入 | `<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M3 9h18"/><path d="M9 9v11"/><path d="M14 13h4"/><path d="M14 16.5h4"/>` |
| `nav-stats` | 统计排名 | `<path d="M18 20V10"/><path d="M12 20V4"/><path d="M6 20v-6"/>` |
| `nav-subjects` | 科目管理 | `<path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/>` |

#### B. 顶栏（6 枚，16px 为主）

| 名称 | 语义 | 尺寸 | SVG path 内容 |
|---|---|---|---|
| `search` | 搜索 | 16px | `<circle cx="11" cy="11" r="7"/><path d="M20 20l-4.35-4.35"/>` |
| `chevron-down` | 考试下拉展开 | 16px | `<path d="M6 9l6 6 6-6"/>` |
| `import` | 导入 | 16px | `<path d="M12 3v12"/><path d="M8 11l4 4 4-4"/><path d="M4 17v2a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-2"/>` |
| `export` | 导出 | 16px | `<path d="M12 15V3"/><path d="M8 7l4-4 4 4"/><path d="M4 17v2a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-2"/>` |
| `plus` | 新增 | 16px | `<path d="M12 5v14"/><path d="M5 12h14"/>` |
| `close` | 关闭 / 清空搜索 | 24px（弹窗）/ 16px（搜索框） | `<path d="M18 6L6 18"/><path d="M6 6l12 12"/>` |

#### C. 表格与行操作（7 枚）

| 名称 | 语义 | 尺寸 | SVG path 内容 |
|---|---|---|---|
| `edit` | 编辑行 / 编辑科目 | 20px | `<path d="M17 3a2.828 2.828 0 1 1 4 4L7.5 20.5 2 22l1.5-5.5L17 3z"/>` |
| `trash` | 删除行 / 删除科目 | 20px | `<path d="M3 6h18"/><path d="M8 6V4a1 1 0 0 1 1-1h6a1 1 0 0 1 1 1v2"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/><path d="M10 11v6"/><path d="M14 11v6"/>` |
| `check` | 勾选 / 校验通过 / 全选框 | 16px | `<path d="M20 6L9 17l-5-5"/>` |
| `minus` | 半选（表头全选框 indeterminate） | 16px | `<path d="M5 12h14"/>` |
| `sort` | 该列可排序但未激活 | 16px | `<path d="M8 5v14"/><path d="M5 8l3-3 3 3"/><path d="M16 19V5"/><path d="M13 16l3 3 3-3"/>` |
| `sort-asc` | 当前升序 | 16px | `<path d="M12 19V5"/><path d="M6 11l6-6 6 6"/>` |
| `sort-desc` | 当前降序 | 16px | `<path d="M12 5v14"/><path d="M6 13l6 6 6-6"/>` |

#### D. 校验与状态（6 枚）

| 名称 | 语义 | 尺寸 | SVG path 内容 |
|---|---|---|---|
| `alert-triangle` | 单元格分数超满分 / 非法值 | 16px | `<path d="M12 3.5L2.5 20h19L12 3.5z"/><path d="M12 10v4"/><path d="M12 17h.01"/>` |
| `check-circle` | Toast 成功 | 16px | `<path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><path d="M22 4L12 14.01l-3-3"/>` |
| `x-circle` | Toast 错误 | 16px | `<circle cx="12" cy="12" r="9"/><path d="M15 9l-6 6"/><path d="M9 9l6 6"/>` |
| `alert-circle` | Toast 警告 / 删除确认主图标 | 16px（Toast）/ 40px（弹窗） | `<circle cx="12" cy="12" r="9"/><path d="M12 8v4"/><path d="M12 16h.01"/>` |
| `info-circle` | Toast 提示 | 16px | `<circle cx="12" cy="12" r="9"/><path d="M12 16v-4"/><path d="M12 8h.01"/>` |
| `inbox` | 空状态插图 | 48px | `<path d="M22 12h-6l-2 3h-4l-2-3H2"/><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/>` |

#### E. 统计页专用（2 枚）

| 名称 | 语义 | 尺寸 | SVG path 内容 |
|---|---|---|---|
| `filter` | 科目筛选 / 视图切换 | 16px | `<path d="M22 3H2l8 9.46V19l4 2v-8.54L22 3z"/>` |
| `refresh` | 重算排名 / 刷新统计 | 16px | `<path d="M23 4v6h-6"/><path d="M1 20v-6h6"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10"/><path d="M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/>` |

**合计 25 枚**：导航 4 / 顶栏 6 / 表格与行操作 7 / 校验与状态 6 / 统计 2。项目范围外不得新增。

### 4.3 图标使用纪律

1. 图标颜色**永远**跟随父元素 `color`，禁止在 CSS 里给 `.icon { stroke: #xxx }` 写死色值。
2. 纯图标按钮必须带 `aria-label`（如删除按钮 `aria-label="删除学生 张明"`）。
3. **绝对禁止**用任何符号/emoji 代替上述功能，包括但不限于：用皇冠表示第一名、用奖杯表示排名、用星星表示优秀、用对钩符号文本代替 `check` 图标。

---

## 5. 应用骨架（App Shell）

所有 4 个视图共享同一骨架。

```
┌──────────────────────────────────────────────────────────────┐
│ TopBar  高 48px                                              │
│ [应用标题]   [当前考试 dropdown]     [搜索框]   [导入][导出]   │
├────────────┬─────────────────────────────────────────────────┤
│ Sidebar    │  View Toolbar  高 48px                          │
│ 宽 216px   │  [视图标题 20px]              [操作按钮组]        │
│            ├─────────────────────────────────────────────────┤
│ 学生管理   │                                                 │
│ 成绩录入   │        View Content                             │
│ 统计排名   │        （各视图不同，见第 6 章）                  │
│ 科目管理   │                                                 │
│            │                                                 │
│ ─────────  │                                                 │
│ 数据概览   │                                                 │
│ 学生 42 人 │                                                 │
│ 科目 6 科  │                                                 │
├────────────┴─────────────────────────────────────────────────┤
│ StatusBar 高 28px   共 42 条 · 最近保存 14:32 · 考试：期中     │
└──────────────────────────────────────────────────────────────┘
```

### 5.1 顶栏 TopBar（`--layout-topbar-h: 48px`）

| 区块 | 内容 | 规范 |
|---|---|---|
| 左 | 应用标题「班级成绩管理系统」 | 14px / 590 / `--fg-default`，左侧留白 `--space-4` |
| 左中 | 当前考试切换下拉 | 宽 200px，触发器高 32px，`--bg-surface` + `--border-strong`；展开为 Popover 列表；无考试时显示「暂无考试」并禁用 |
| 右中（弹性占位） | 全局搜索框 | 宽 240px（可伸长），高 32px，内嵌 16px `search` 图标 + 占位符「搜索学号或姓名」，聚焦时右侧出现 16px `close` 清空按钮 |
| 右 | 导入 / 导出 按钮 | Secondary 变体，各含 16px `import` / `export` 图标 + 文字，间距 `--space-2` |

顶栏底色 `--bg-surface`，底部 `1px solid var(--border-navbar)`，`position: sticky; top: 0; z-index: var(--z-sticky)`。

> 搜索框语义：在「学生管理」视图过滤姓名/学号；在其他视图同样可用（按姓名过滤），前端只需保持行为一致。

### 5.2 侧边导航 Sidebar（`--layout-sidebar-w: 216px`）

- 底色 `--bg-sidebar`，右侧 `1px solid var(--border-default)`
- 主区 4 个导航项，每项高 `--nav-item-height: 34px`，圆角 `--radius-md`，左边距 `--space-3`，项间距 `--nav-item-gap: 8px`
- 图标 16px + 文字 14px，两者间距 `--space-2`
- **选中态**：底色 `--nav-bg-active`（teal-50）+ 文字 `--nav-fg-active`（teal-700）+ 字重 510。不使用左边框、不使用色条（AI 模板症状）
- **hover 态**：底色 `--nav-bg-hover`（gray-100），文字提亮到 `--fg-default`
- 底部「数据概览」分组：学生数 / 科目数，11px `--fg-muted`，上方用 `--space-4` 间距 + 1px 分隔线
- 窗口 <1024px 时折叠为 `--layout-sidebar-w-min: 56px` 图标条，文字隐藏，`title` 属性保留原 4 个名称

### 5.3 视图工具条 Toolbar（`--layout-toolbar-h: 48px`）

左：视图标题 20px / 510 / `--fg-default`
右：该视图的主操作按钮组（每个视图不同，见第 6 章）
底部：无分隔线（与内容区的空白本身足够，再画线会压低视觉重心）

### 5.4 状态栏 StatusBar（28px）

- 底色 `--statusbar-bg`，顶部 1px `--statusbar-brd`，11px `--fg-muted`
- 内容：`共 N 条 · 已选 M 项 · 最近保存 HH:MM · 当前考试：xxx`
- 已选 M 项仅在 M > 0 时出现，且用 `--fg-secondary` 提亮

---

## 6. 四个视图的详细布局与组件清单

### 6.1 视图一：学生管理

**路由/标识**：`data-view="students"`

**布局结构**

```
Toolbar:  学生管理                              [+ 新增学生] [批量删除(禁用)]
Content:  表格容器 (--bg-surface, radius 8, 1px border)
          thead sticky (36px, gray-100 底)
          tbody 行高 44px
          最后一行为合计/占位？否 —— 无脚注
```

**表格列定义**

| 列 | 宽度 | 对齐 | 排序 | 内容规范 |
|---|---|---|---|---|
| checkbox | 44px | 中 | 否 | 16px 自定义 Checkbox，点击区域扩展到整格 44×44 |
| 学号 | `--table-first-col-w` 88px | 左 | 可 | 等宽字体，13px |
| 姓名 | 120px | 左 | 可 | 13px / 510，超出省略 |
| 性别 | 64px | 中 | 可 | 13px，值：「男」「女」；不用 Badge，纯文字（列窄，Badge 会撑高行） |
| 备注 | flex 自适应 | 左 | 否 | 13px / 400，最多 2 行，超出省略 + `title` |
| 操作 | 88px | 右 | 否 | 两个 20px 图标按钮：`edit` / `trash`，各带 `aria-label` |

**组件清单**

1. 表格（含 sticky 表头、行 hover、行选中、表头排序）
2. Checkbox（含 unchecked / hover / checked / indeterminate / disabled）
3. Button primary（新增学生）、secondary danger（批量删除）
4. 新增/编辑表单：本产品统一使用**居中模态框**（规格见 §7.8），字段为 学号 / 姓名 / 性别（单选）/ 备注（多行）
5. 空状态 A（首次无学生）
6. 空状态 B（搜索无结果）
7. 确认删除弹窗
8. Toast

**交互反馈**

- 勾选 ≥1 行：工具条「批量删除」从 disabled 变为 danger 可用，状态栏显示「已选 M 项」
- 点击表头「姓名/学号/性别」：切换 asc / desc / none 三态，表头右侧图标依次为 `sort` / `sort-asc` / `sort-desc`
- 删除必须走确认弹窗，删除后 Toast「已删除学生 张明」

---

### 6.2 视图二：成绩录入

**标识**：`data-view="scores"`

**布局结构**

```
Toolbar:  成绩录入        [考试: 已在顶栏切换]   [仅显示未录入] [保存状态提示]
Content:  表格容器
          第一列 固定列(left sticky): 序号 / 学号 / 姓名
          其后每一列 = 一个科目，列头 = "语文 150"
          末列 操作列（右 sticky 可选）
```

**表格列定义**

| 列 | 宽度 | 对齐 | 说明 |
|---|---|---|---|
| 序号 | 52px | 中 | 等宽，13px，不可编辑 |
| 学号 | 88px | 左 | 等宽，13px，不可编辑（与学生表一致，便于对照） |
| 姓名 | 100px | 左 | 13px / 510，不可编辑，横向滚动时 left sticky |
| 科目列 ×N | `--table-score-col-w` 76px | 中 | 每个单元格是一个内联 `<input>`，`inputmode="decimal"` |
| 操作 | 64px | 右 | `trash` 图标按钮，语义为「清空该行本科成绩」 |

**单元格编辑规范（这是本视图的核心）**

| 状态 | 视觉 |
|---|---|
| Default | 无边框、透明底，看起来像纯文本；文字 13px / 400 / 等宽 / 居中；已录入值用 `--fg-default`，未录入显示 `—` 用 `--score-empty-fg` |
| Hover | 出现 `1px solid var(--cell-input-brd-hover)` 的浅框（圆角 4px），提示可编辑 |
| Focus | 白底 + `1px solid var(--score-editing-brd)`（深青）+ `--input-ring-focus` 焦点环 |
| Valid | 失焦后按 §2.5 规则着色（≥90% 优秀 / <60% 不及格） |
| Invalid | 红底 + 红字 + 行尾 `alert-triangle` 图标，`aria-invalid="true"`，`aria-describedby` 指向行内错误文本「超出满分 150」 |

**键盘流**：Enter 提交并下移一格，Tab 右移，Shift+Tab 左移，Esc 撤销本次编辑。上下左右也可以用方向键在编辑态外移动焦点。**这是教师连续录入 40 人 × 6 科的核心效率点，必须实现。**

**自动校验**

- 输入非数字 / 负数 / > 该科满分 → 立即 Invalid 态，不写入数据，Toast 警告（同一用户连续错误时 Toast 合并，不同一条一条弹）
- 留空 → 视为未录入，`—`，不计入统计

**空状态**

- 无考试：「当前还没有任何考试，先新建一场考试」+ 主按钮（若考试新建入口在顶栏，则此处文案改为引导到顶栏）
- 无学生：「还没有学生数据，请先到「学生管理」添加学生」+ 次按钮跳转
- 无科目：「还没有科目，请先到「科目管理」添加科目」+ 次按钮跳转

**顶部辅助**：工具条右侧放一个「保存状态」文本（12px `--fg-muted`）：「全部已保存」/「有 3 处未通过校验」。不加 spinner，不用自动保存的隐藏承诺。

---

### 6.3 视图三：统计排名

**标识**：`data-view="stats"`

**布局结构（自上而下）**

```
Row 1: 分段控件 [按学生查 | 按科目查]        (segmented control)
Row 2: 指标卡 ×4，横向等分，gap 16px
       [平均分] [最高分] [最低分] [及格率]
       —— 卡片：白底 + 1px border + radius 8 + 无投影
Row 3: 主表格（随 Row 1 切换两种形态）+ 右侧分布图卡片（宽 280px）
```

**按学生查（默认）**

| 列 | 宽度 | 内容 |
|---|---|---|
| 排名 | 64px | 前三用奖牌色 Badge（见 §2.4），第 4 名起纯数字 |
| 学号 | 88px | 等宽 |
| 姓名 | 100px | 510 |
| 各科 N 列 | 76px | 分数，沿用 §2.5 着色规则 |
| 总分 | 88px | 等宽 / **590** / 右留出强调感（唯一使用字重强调数值的地方） |
| 平均分 | 80px | 等宽，保留 1 位小数（`tabular-nums` 必须） |
| 较上次 | 88px | 仅当存在上一场考试时出现；正 `--card-delta-up-fg` + 负 `--card-delta-down-fg`，前缀 `+` / `-`，不用箭头符号代替数字含义（数字本身 + 颜色已经足够，且必须有数字，不得仅靠颜色） |

**按科目查**

| 列 | 宽度 | 内容 |
|---|---|---|
| 科目名 | 120px | 14px / 510 |
| 满分 | 72px | 等宽，灰色小字后缀「分」 |
| 平均分 | 88px | 等宽 590 |
| 最高分 | 80px | 等宽 |
| 最低分 | 80px | 等宽 |
| 及格率 | 96px | 百分比 + **一条横向对比条**（`--subject-bar-track` 底槽 + `--subject-bar-fill` 填充） |

**成绩分布可视化（右侧卡片，宽 280px）**

- 标题「成绩分布」14px / 510
- 主体：5 根垂直柱状图，横轴为分数段（不及格 / 60-70 / 70-80 / 80-90 / 90+），纵轴为人数
- 实现约束：**纯 CSS**（外层 flex-end 容器 + 内层 `height: %`，`--duration-enter` 过渡）或**内联 SVG `<rect>`**。禁止引入任何图表库。
- 柱体色沿用 `--dist-bin-*`
- 每根柱上方标人数（11px `--fg-muted`），下方标分数段（11px `--dist-axis-fg`）
- 无数据时显示对应的空状态缩略版

**交互**

- 分段控件切换：200ms 淡入切换表格内容，不重刷页面
- 表格支持按总分平均分名次列排序，前三名的奖牌色跟随实际排名变化（不是固定给第 1/2/3 行的物理位置——如果数据重排了，颜色跟着走）
- 分布图 ≤3s 内完成首次渲染，后续数据变更用 200ms 高度过渡

---

### 6.4 视图四：科目管理

**标识**：`data-view="subjects"`

**布局结构**

```
Toolbar:  科目管理                                    [+ 新增科目]
Content:  表格容器（与学生管理共用表格样式）
          empty 时 → 空状态 C
```

**表格列定义**

| 列 | 宽度 | 对齐 | 内容 |
|---|---|---|---|
| 序号 | 52px | 中 | 等宽，同时也是拖拽顺序位（v1 不做拖拽，保留序号即可） |
| 科目名 | flex 自适应（最小 160px） | 左 | 14px / 510 |
| 满分 | 120px | 右 | 等宽数字 + `--card-unit-fg` 后缀「 分」 |
| 已录人数 | 96px | 右 | 等宽「38 / 42」，分子分母，分母用 `--fg-muted` |
| 操作 | 88px | 右 | `edit` / `trash` 20px 图标按钮 |

**删除风险提示**：删除科目会连带删除该科所有已录成绩，确认弹窗必须明确写出影响面：

> 删除科目「物理」
> 该科目的 42 条成绩记录将一并删除，且无法恢复。
> [取消] [删除科目]

danger 主按钮在右，**「取消」在左**。这是唯一一处主按钮放右边的情况，因为它是一个不可逆操作，给用户一个「先看到取消」的减速带。

**校验**

- 科目名必填，重名时行内错误「已存在同名科目」
- 满分必须是 1 ~ 1000 的整数，越界时行内错误「满分需在 1 到 1000 之间」

---

### 6.5 通用组件清单（4 视图共享）

| 组件 | 变体 | 必须覆盖的状态 |
|---|---|---|
| Button | primary / secondary / ghost / danger | Default / Hover / Active / Focus-visible / Disabled / Loading |
| Input | text / number / search | Default / Hover / Focus / Error / Disabled / Readonly |
| Select / Popover | 考试切换、筛选 | Collapsed / Expanded(200ms) / Item hover / Item selected / Empty |
| Table | 通用 | Default / Row hover / Row selected / Header sortable & active / Sticky header / Column sticky |
| Checkbox | 批量选择 | Unchecked / Hover / Checked / Indeterminate / Disabled |
| Badge | neutral / success / warn / danger / accent / rank-1 / rank-2 / rank-3 | 静态，不参与交互 |
| Modal | 新增/编辑表单、确认删除 | Entering(240ms) / Open / Closing(200ms) |
| Toast | info / success / warn / danger | Entering(200ms) / Visible / Leaving(150ms) / 堆叠（最多 3 条，超出丢弃最旧） |
| Segmented | 按学生查/按科目查 | Item default / Item active |
| Progress bar | 科目及格率对比条 | Determinate（无 indeterminate） |
| Empty state | 见 6.6 | 静态 + 主/次 CTA |
| Skeleton | 表格首屏数据加载时 | 行骨架，1200ms 微光循环，**prefers-reduced-motion 时变为静态灰块** |

### 6.6 五态覆盖检查表

每个视图、每个数据区域都必须回答这五问：

| 状态 | 学生管理 | 成绩录入 | 统计排名 | 科目管理 |
|---|---|---|---|---|
| Loading | 表格骨架屏 6 行 | 表格骨架屏 8 行 × N 列 | 卡片 + 表格双骨架 | 表格骨架 4 行 |
| Empty | 「还没有学生，添加第一个学生」 | 按缺考试/缺学生/缺科目分三种文案 | 「还没有可统计的成绩」 | 「还没有科目，添加第一个科目」 |
| Error | 加载失败：内联错误卡 + 「重试」按钮 | 同上 | 同上 | 同上 |
| Populated | 正常表格 | 正常表格 | 卡片 + 表格 + 分布图 | 正常表格 |
| Edge | 姓名超长省略；备注 200 字截断；>500 行时启用虚拟滚动或分页 | 分数输入 999999、粘贴带空格、全角数字兼容 | 并列名次处理（同分同名次，下一名次跳号）；只有 1 个学生时图表降级为单柱 | 科目名 20 字截断；满分 1000 边界；删除最后一科后再进统计页走空状态 |

### 6.7 通用交互文案（全部真实业务文案，禁止占位）

| 场景 | 文案 |
|---|---|
| 删除学生确认 | 标题「删除学生 张明」；正文「该学生的所有成绩记录将一并删除，且无法恢复。」；按钮「取消」「删除」 |
| 批量删除确认 | 标题「删除 5 名学生」；正文「这些学生的所有成绩记录将一并删除，且无法恢复。」；按钮「取消」「删除 5 名」 |
| 删除科目确认 | 见 6.4 |
| 新增成功 Toast | 「已添加学生 李雷」 |
| 编辑成功 Toast | 「已保存 李雷 的信息」 |
| 删除成功 Toast | 「已删除学生 张明」 |
| 导入成功 Toast | 「已导入 42 名学生」 |
| 导入失败 Toast | 「导入失败：文件第 3 行学号重复」 |
| 导出成功 Toast | 「已导出到 桌面/期中成绩.xlsx」 |
| 分数越界警告 Toast | 「语文 分数不能超过 150」 |
| 保存完成 Toast | 「成绩已保存」 |
| 搜索无结果 | 「没有匹配「张」的学生」+ 次按钮「清除搜索」 |
| 表单必填错误 | 「请输入姓名」「请输入学号」 |

严禁出现：`Welcome to`、`Lorem ipsum`、`Sign up today`、`Get started`、`Elevate your`、以及任何英文占位。

---

## 7. 逐页面实现提示词（前端直接执行）

### 7.1 全局：文件与引入顺序

> 建立 `src/index.html`、`src/styles/base.css`、`src/styles/tokens.css`（由 `design-tokens.css` 复制或 `@import`）、`src/styles/components.css`、`src/styles/views.css`、`src/icons.js`、`src/app.js`。
> `index.html` 的 `<head>` 里按此顺序引入：`design-tokens.css` → `base.css` → `components.css` → `views.css`。
> `design-tokens.css` 必须是第一个加载的样式文件。**任何其他 CSS 文件中不允许出现 `#` 开头的颜色值，`#fff` 和 `#000` 也不例外**——需要白色就用 `var(--gray-0)`，需要黑色就用 `var(--gray-900)`。

### 7.2 全局：Base 重置

> `base.css` 里写全局重置：`box-sizing: border-box`；`body { margin: 0; background: var(--bg-app); color: var(--fg-default); font-family: var(--font-body); font-size: var(--text-md); line-height: var(--leading-body); }`；所有表格 `border-collapse: collapse`。
> 给 `<html>` 加 `-webkit-font-smoothing: antialiased`。
> 全局禁用文本选择的例外：表格数据区允许选择（老师要复制到 Excel），导航栏和按钮加 `user-select: none`。

### 7.3 骨架 App Shell

> 用 CSS Grid 搭三区骨架：`.app { display: grid; grid-template-columns: var(--layout-sidebar-w) 1fr; grid-template-rows: var(--layout-topbar-h) 1fr var(--layout-statusbar-h); height: 100vh; }`。顶栏 `grid-column: 1 / -1`，状态栏同样跨列。
> 侧栏 `background: var(--bg-sidebar); border-right: 1px solid var(--border-default);`。内容区 `overflow: auto; background: var(--bg-app); padding: var(--layout-content-pad-y) var(--layout-content-pad-x);`。
> 顶栏内部用 flex，`justify-content: flex-start`，搜索框用 `margin-left: auto` 推到右（不是 `justify-content: space-between`，因为要留弹性）。
> 窗口宽度 < 1024px 时给 `.app` 加 `.app--collapsed`，此时 `grid-template-columns: var(--layout-sidebar-w-min) 1fr`，侧栏项文字 `display: none`，图标居中，保留 `title` 属性。

### 7.4 视图一「学生管理」提示词

> 内容区结构：`<section class="view" data-view="students">`，内含 `.toolbar`（左 `<h1>` 标题 20px/510 + 右两个按钮）和 `.panel`（表格容器）。
> `.panel { background: var(--bg-surface); border: 1px solid var(--table-brd); border-radius: var(--table-radius); overflow: hidden; }`。**不要用 box-shadow**，这是本项目的分组方式与其他产品的核心区别。
> 表头：`thead th { position: sticky; top: 0; z-index: var(--z-sticky); background: var(--table-header-bg); color: var(--table-header-fg); font-size: var(--table-header-size); font-weight: var(--table-header-weight); height: var(--table-header-h); text-align: left; }`，并在 `thead` 上加 `box-shadow: var(--table-sticky-shadow)` 作为滚动时的下边界。
> 行：`tbody tr { height: var(--table-row-h); border-bottom: 1px solid var(--table-row-brd); transition: background-color var(--duration-fast) var(--ease-standard); }`；`:last-child { border-bottom-color: var(--table-row-brd-last); }`。
> hover：`tbody tr:hover { background: var(--table-row-bg-hover); }`；选中：`tbody tr[aria-selected="true"] { background: var(--table-row-bg-selected); box-shadow: inset 0 0 0 1px var(--table-row-brd-selected); }`——**用 inset shadow 而不是 border，避免改变表格盒模型导致行高抖动**。
> 单元格：`padding: 0 var(--table-cell-pad-x)`（垂直用行高撑，不写 padding-top/bottom，防止与 44px 行高冲突）。
> 姓名溢出：`td.name { max-width: 120px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }`，同时在 `<td>` 上加 `title` 属性放完整值。
> Checkbox 用 `<button role="checkbox" aria-checked>` 自定义（不用原生 input，方便控制 16px 尺寸与 indeterminate 态），内部放 16px `check` / `minus` 图标；按钮本身撑到 44×44 点击区。
> 排序：给可排序列的 `<th>` 加 `<button class="th-sort">` 内含列标题 + 16px 图标，三态切换通过 `aria-sort="none|ascending|descending"` 驱动，图标用 CSS `[aria-sort="none"] .icon--sort-none { display: inline }` 之类控制显隐。

### 7.5 视图二「成绩录入」提示词

> 表格用 `<table>` 但要支持横向滚动：外层 `.panel { overflow-x: auto; }`。
> 前三列（序号/学号/姓名）`position: sticky; left: 0 / 52px / 140px;` 并设 `background: var(--bg-surface)`（滚动时不能透出下面的内容）；hover 行时 sticky 列的背景要跟着变，用 `tr:hover .col-sticky { background: var(--table-row-bg-hover); }` 显式同步。
> 分数单元格：`<td class="score-cell"><input type="text" inputmode="decimal" class="score-input" value="" placeholder="—" /></td>`。
> `.score-input { width: 100%; height: 32px; border: 1px solid transparent; background: transparent; text-align: center; font-family: var(--font-mono); font-variant-numeric: tabular-nums; font-size: var(--table-cell-size); color: var(--fg-default); border-radius: var(--radius-sm); transition: border-color var(--duration-fast) var(--ease-standard), background-color var(--duration-fast) var(--ease-standard); }`
> hover 时才显形：`.score-cell:hover .score-input { border-color: var(--cell-input-brd-hover); }`；focus 时 `.score-input:focus { background: var(--cell-input-bg-focus); border-color: var(--score-editing-brd); box-shadow: var(--input-ring-focus); outline: none; }`。
> 校验失败时给 `<td>` 加 `data-invalid="true"`，并把 `<input>` 的 `aria-invalid="true"`；样式：`td[data-invalid="true"] .score-input { color: var(--score-invalid-fg); background: var(--score-invalid-bg); border-color: var(--score-invalid-brd); }`，同时在 `<td>` 右上角绝对定位一个 16px `alert-triangle` 图标（`color: var(--score-invalid-brd)`）。
> 键盘流：给 `.score-input` 绑 `keydown`，Enter → `blur()` 并把焦点移到下一行同列的 input；ArrowUp/Down 在非编辑态移焦点；Escape → 恢复原值并 blur。用 `data-row` / `data-col` 索引算目标。
> 横向可能有 10+ 科目列，务必让表格总宽超出时出现横向滚动条，`table { min-width: 100%; width: max-content; }`。

### 7.6 视图三「统计排名」提示词

> 分段控件：`<div class="segmented" role="tablist">` 内含两个 `<button role="tab" aria-selected>`。样式 `background: var(--segmented-bg); border-radius: var(--segmented-radius); padding: var(--segmented-pad); display: inline-flex; gap: 0;`，激活项 `background: var(--segmented-item-bg-active); color: var(--segmented-item-fg-active); box-shadow: var(--segmented-item-shadow-active); height: var(--segmented-item-h); border-radius: var(--radius-md);`。
> 指标卡：`<div class="kpi-grid">` + 4 个 `<article class="kpi-card">`。`.kpi-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: var(--card-gap); }`；`.kpi-card { background: var(--card-bg); border: 1px solid var(--card-brd); border-radius: var(--card-radius); box-shadow: var(--card-shadow); padding: var(--card-pad); }`；卡片内：标题 `--card-title-size` / `--card-title-fg`，数值 `--card-value-size` / `--card-value-weight` / `--card-value-fg` / 等宽 + tabular-nums，单位后缀 `--card-unit-size` / `--card-unit-fg`。
> **卡片默认无投影**（`--card-shadow: none`），与其他桌面表格软件一致。
> 主区用 `display: grid; grid-template-columns: 1fr 280px; gap: var(--space-4);` 左表格右分布图。
> 前三名：用一个 `.rank-badge` span 包住名次数字，按名次加 class：`.rank-badge--1 { color: var(--rank-1-fg); background: var(--rank-1-bg); border: 1px solid var(--rank-1-brd); }`，2、3 同理，第 4 名起 `.rank-badge--plain { color: var(--rank-plain-fg); background: transparent; border-color: transparent; }`。Badge 尺寸：`display: inline-flex; align-items: center; justify-content: center; min-width: 24px; height: 20px; padding: 0 var(--badge-pad-x); border-radius: var(--radius-sm); font-size: var(--badge-size); font-weight: var(--badge-weight); font-variant-numeric: tabular-nums;`。
> **注意**：名次必须从实际数据排序算出，不要写死「表格第一行 = 第一名」。同分时并列同名次且下一名次跳号（1、2、2、4）。
> 分布图（纯 CSS）：`<ul class="dist-chart">` 内含 5 个 `<li class="dist-col">`，每个内含 `.dist-bar`（高度用内联 style 的百分比）与 `.dist-count` / `.dist-label`。`.dist-chart { display: flex; align-items: flex-end; gap: var(--space-2); height: 160px; }`；`.dist-bar { width: 100%; background: var(--dist-track-bg); border-radius: var(--radius-sm) var(--radius-sm) 0 0; position: relative; }`——实际填充用内层 `<span class="dist-fill">` 从底部往上：`position: absolute; bottom: 0; height: XX%; background: var(--dist-bin-b2); transition: height var(--duration-enter) var(--ease-out);`。不及格那根用 `var(--dist-bin-fail)`。
> 对比条（按科目查）：`<div class="subject-bar"><span class="subject-bar__fill" style="width: 72%"></span></div>`，槽 `--subject-bar-track`，填充 `--subject-bar-fill`，高度 6px，圆角 `--radius-full`，过渡 200ms。旁边必须同时写数字「72.0%」，不得只靠条长传达含义（无障碍）。

### 7.7 视图四「科目管理」提示词

> 表格结构与视图一完全一致，复用 `.panel` / `table` 的样式，**不要复制粘贴一份新 CSS 而改数值**——若要差异，用 `[data-view="subjects"]` 作用域覆盖。
> 满分列右对齐，`<span class="num">150</span><span class="unit"> 分</span>`，unit 用 `--card-unit-fg`。
> 已录人数列：`38 / 42`，斜杠两侧各留 2px 空格，分母用 `--fg-muted`，整体等宽 + tabular-nums。
> 表单模态框：新增与编辑共用同一个 Modal，通过标题区分（「新增科目」/「编辑科目 语文」）。字段：科目名（必填 text）、满分（必填 number，min 1 max 1000）。错误反馈显示在字段正下方（不是顶部汇总），11px `--danger-fg`，输入框加 `border-color: var(--input-brd-invalid)`。

### 7.8 通用：删除确认 Modal

> 结构：`<div class="modal-scrim">` + `<div class="modal" role="dialog" aria-modal="true" aria-labelledby="modal-title">`。
> scrim：`position: fixed; inset: 0; background: var(--bg-scrim); z-index: var(--z-modal); display: grid; place-items: center;`。
> modal：`width: var(--modal-width); background: var(--modal-bg); border-radius: var(--modal-radius); box-shadow: var(--modal-shadow); padding: var(--modal-pad); display: flex; flex-direction: column; gap: var(--modal-gap);`
> 顶部一个 40px 圆形 danger 图标容器：`width: var(--modal-icon-size); height: var(--modal-icon-size); border-radius: var(--radius-full); background: var(--modal-danger-icon-bg); display: grid; place-items: center;` 内放 20px `alert-circle` 图标 `color: var(--danger-solid)`。
> 标题 16px/590，正文 14px/400 `--fg-secondary`，按钮行 `display: flex; justify-content: flex-end; gap: var(--space-2);`，顺序为「取消」在左（secondary）、「删除」在右（danger）。
> 打开时焦点自动落到「取消」按钮（安全默认），Tab 循环不能逃出 modal（焦点陷阱），Esc 关闭，点击 scrim 关闭。**但删除类弹窗点击 scrim 不关闭**——防止手滑丢失已勾选的批量选择。
> 进出动画：scrim `opacity 0→1`，modal `opacity + translateY(4px)→0`，时长 `--duration-modal`，缓动 `--ease-out` 进 / `--ease-in` 出。

### 7.9 通用：Toast

> 容器 `<div class="toast-stack" role="status" aria-live="polite">` 固定在右下角：`position: fixed; bottom: var(--toast-offset-bottom); right: var(--toast-offset-right); z-index: var(--z-toast); display: flex; flex-direction: column; gap: var(--toast-gap); align-items: flex-end;`
> 单条：`.toast { display: flex; align-items: center; gap: var(--toast-gap); min-width: var(--toast-min-width); max-width: var(--toast-max-width); padding: var(--toast-pad); background: var(--toast-bg); color: var(--toast-fg); border-radius: var(--toast-radius); box-shadow: var(--toast-shadow); font-size: var(--text-md); }`
> Toast 用**深色底 + 浅色字**（不跟随主色）：它要在整屏密集的数据表上脱颖而出，深灰中性色比用强调色更醒目，同时不会干扰长期视线。
> 图标 16px，按类型取色：`--toast-success-icon` / `--toast-danger-icon` / `--toast-info-icon`。
> 进入 200ms `translateY(8px) → 0` + `opacity 0 → 1`，退出 150ms `opacity → 0`。
> 自动消失 3 秒（错误信息 5 秒）；堆叠上限 3 条，超出先剔除最旧。
> `prefers-reduced-motion` 下只做 opacity 变化，不做位移。

### 7.10 通用：空状态与骨架屏

> 空状态：`.empty { padding: var(--empty-pad); display: flex; flex-direction: column; align-items: center; gap: var(--empty-gap); text-align: center; }`；插图 48px `inbox` 图标 `color: var(--empty-icon-fg)`（这是唯一允许 48px 图标的地方），标题 `--empty-title-size` / 510 / `--empty-title-fg`，描述 `--empty-desc-size` / `--empty-desc-fg` / `max-width: var(--empty-max-width)`，下方一个 primary 或 secondary 按钮。
> 空状态必须放在 `.panel` 内部替换 `<tbody>` 内容的位置（而不是替换整个 `.panel`），保留表头，用户才知道这张表是干什么的。
> 骨架屏：`.skeleton { background: var(--skeleton-bg); border-radius: var(--skeleton-radius); }`，行数与真实表格一致（6 行），每行用一个 `height: 20px; margin: 12px 0;`。微光动画 `animation: skeleton var(--skeleton-duration) var(--ease-standard) infinite;` keyframes 改 `background-color` 从 `--skeleton-bg` 到 `--skeleton-shine` 再回来（`--skeleton-duration` 在 reduced-motion 下被置为 0ms，动画自然停止）。

### 7.11 无障碍强制项

> 所有可交互元素必须是原生 `<button>` / `<input>` / `<a>`，或用 `role` + `tabindex="0"` 补齐，并绑定 Enter/Space 键盘处理。
> 焦点样式统一走 `:focus-visible`，`design-tokens.css` 第 12 节已提供全局基线，**任何地方禁止写 `outline: none` 而不给替代指示**。
> 图标按钮必须有 `aria-label`。纯装饰图标必须有 `aria-hidden="true"`。
> 表格用真正的 `<table>` + `<thead>` + `<th scope="col">`，可排序列的 `<th>` 上加 `aria-sort`。
> 颜色不能是唯一的信息载体：不及格除了红色还要有行尾 alert 图标；排名前三除了颜色还要有数字；及格率除了条形还要有数字。
> 全局包裹 `@media (prefers-reduced-motion: reduce)` 已在 Token 层把时长归零，前端只要确保动画属性用的是 `var(--duration-*)` 就能自动生效——**因此禁止在组件 CSS 里写死 `150ms` 这类数字**。

---

## 8. 11 项视觉自查清单结果

| # | 检查项 | 结果 | 证据与说明 |
|---|---|---|---|
| 1 | 无 emoji 作为功能图标 | **通过** | 全 UI 图标来自第 4 章锁定的 25 枚内联 SVG 描边图标。经正则 `[\x{1F300}-\x{1F9FF}\x{2600}-\x{26FF}\x{2700}-\x{27BF}]` 扫描本文档与 `design-tokens.css`，零命中。排名前三明确禁止用皇冠/奖杯表情，改用 `--rank-1/2/3` 三组色彩 Token + 数字。 |
| 2 | 无紫色→粉色渐变 | **通过** | `design-tokens.css` 不存在 `#7C3AED` `#A855F7` `#9333EA` `#EC4899` 任何一个值，也不存在 Indigo→Pink 的 gradient 声明。全项目零装饰性渐变，连 `--dist-bin-*` 分布图都是纯色块。§2.7 已列为永久禁止项。 |
| 3 | 无空洞占位文案 | **通过** | 第 6.7 节给出全部 13 条真实中文业务文案（含删除确认、Toast、错误提示、搜索无结果）。全文无 `Welcome to` / `Lorem ipsum` / `Sign up today` / `Get started` 类占位。表单校验错误具体到字段原因（「已存在同名科目」「满分需在 1 到 1000 之间」）。 |
| 4 | 无硬编码颜色值 | **通过** | `design-tokens.css` 头部明示：裸色值只允许出现在该文件 §1 原始层，其他 CSS 文件只能引用 Token。文档进而禁止 `#fff` / `#000` 例外（改用 `var(--gray-0)` / `var(--gray-900)`）。阴影与聚焦环不写 `rgba()`，改用 `--rgb-shadow` / `--rgb-teal-600` 三元组 + `rgb(var(--x) / alpha)` 派生。动效时长也全部 Token 化。 |
| 5 | 无弹跳缓动 | **通过** | `--ease-standard: cubic-bezier(0.2, 0, 0, 1)`、`--ease-out: cubic-bezier(0.16, 1, 0.3, 1)`、`--ease-in: cubic-bezier(0.4, 0, 1, 1)` 均为无过冲曲线。全文件不存在 `cubic-bezier(0.68, -0.55, 0.265, 1.55)` 或任何 y<0 / y>1 的控制点。最长的动画 240ms（模态），远低于 500ms 上限。 |
| 6 | 颜色全部走 Token 体系 | **通过** | 采用四层架构：A1 原始（gray/teal/green/amber/red/medal 六族）→ A2 语义（`--bg-*` `--fg-*` `--border-*` `--accent-*` `--success/warn/danger-*`）→ C 扩展（`--rank-*` `--score-*` `--dist-*` `--subject-bar-*`）→ B 组件（`--btn-*` `--input-*` `--table-*` `--card-*` `--toast-*` 等 13 组）。组件层无跨层直接引用 Primitive。 |
| 7 | 间距全走 4px 网格 | **通过** | `--space-1` 到 `--space-16` 严格取值 4/8/12/16/20/24/32/40/48/64。布局相关尺寸（栏高、列宽、行高、卡片 padding）全部引用这些变量或常量布局 Token，不存在 5/7/13/15/18/22/30 等非标值。 |
| 8 | 字体栈锁定且层级清晰 | **通过** | 系统字体栈优先 `PingFang SC`（macOS）/ `Microsoft YaHei`（Windows），末位兜底 `Source Han Sans SC` / `Noto Sans CJK SC`，离线可用无 FOUT。标题与正文同栈，层级由 8 级字号 + 3 级字重（400/510/590）建立；数值列独立使用 `--font-mono` + `tabular-nums` 保证纵向对齐。 |
| 9 | 图标方案锁定一套 | **通过** | 唯一方案：内联 SVG 描边图标，统一 `viewBox="0 0 24 24"`、`fill="none"`、`stroke="currentColor"`、`stroke-width="1.5"`、`linecap/linejoin="round"`。尺寸仅 16 / 20 / 24 / 48（48 仅空状态插图）四档。第 4.2 节枚举全部 25 枚并给出完整 path 数据，前端直接复制即可，不存在混用第二套图标库的空间。 |
| 10 | 对比度与无障碍达标 | **通过** | 正文 `--fg-default` 13.9:1、次级文本 7.2:1、辅助文本 4.9:1、强调色 6.2:1、Toast 深底白字 13.9:1，均 ≥ WCAG AA 4.5:1。排名前三 Token 在其背景上分别为 5.0 / 5.9 / 5.6:1。`--fg-subtle` 仅用于 disabled（WCAG 1.4.3 豁免）与装饰元素，不承载文本信息。全局 `:focus-visible` 焦点环已内置；`prefers-reduced-motion` 在 Token 层把全部时长归零。所有动画 ≤240ms。 |
| 11 | 组件九态完整覆盖 | **通过** | 第 6.5 节列出 12 个共享组件的变体与状态矩阵，第 6.6 节对 4 个视图逐一给出 Loading / Empty / Error / Populated / Edge 五态方案。Button 覆盖 Default/Hover/Active/Focus/Disabled/Loading/Error/Empty/Success 九态；表格行覆盖 hover / selected / sticky / 溢出；空状态细分到「无考试 / 无学生 / 无科目 / 搜索无结果」四种不同文案。 |

**清单结论：11 项全部通过，无 blocking 项。**

---

## 9. 变更记录

| 日期 | 变更 | 原因 | 影响范围 |
|---|---|---|---|
| v1.0 | 建立设计系统基线 | 首版，依据产品需求与 Tauri 桌面技术栈确定 | 全部 4 视图 |

> 变更规则：本文件与 `src/design-tokens.css` 是同一份契约的两半。修改任一 Token 都必须同步另一处，并在此表追加一行，不得直接覆写。
