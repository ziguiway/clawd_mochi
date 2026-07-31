# Clawd Mochi 大学课表自动导入与英文名称映射方案

- 日期：2026-07-31
- 状态：设计评审稿
- 范围：自动导入、课程名称英文映射、设备数据交付
- 本阶段限制：只确定产品与技术方案，不修改固件

## 1. 目标

课表功能的首要入口必须是自动导入，而不是让学生逐门录入。

用户完成一次教务系统登录后，系统应自动读取当前学期课表、转换为
Clawd Mochi 的统一格式、生成适合 240×240 屏幕的英文课程名，并把
课表同步到设备。手动添加与编辑只用于校对、补课和异常修正。

设备屏幕不显示中文。中文课程名只作为导入原文保留在管理端，设备使用
类似 `DATA STRUCTURES`、`OPERATING SYSTEMS` 的英文显示名。

## 2. 开源项目调研结论

### 2.1 小爱课程表适配器

[Mi-AI-Schedule-BISTU](https://github.com/ZZHow1024/Mi-AI-Schedule-BISTU)
展示了小爱课程表适配器的典型三段式结构：

1. `Provider` 在本地、已登录的教务页面中执行，可访问 DOM，负责提取
   原始课表内容。
2. `Parser` 把 Provider 输出转换为统一课程对象，包括课程名、地点、
   教师、星期、周次和节次。
3. `Timer` 定义开学日期、学期周数以及每一节课的起止时间。

该项目的实际 Parser 还处理了周次范围、连续节次和重复课程去重。这说明
“学校页面抓取”和“通用课表模型”必须解耦，不能把某个学校的 HTML 结构
写死到设备固件中。

### 2.2 WakeUp 课程表

[WakeUp 导入文档](https://www.wakeup.fun/doc/import_schedule.html)提供了
多入口策略：学校教务、Excel、HTML、分享/导出文件和分享口令。它还明确
要求用户在导入后检查课程是否完整。

可借鉴的不是某一种文件格式，而是两项产品原则：

- 自动导入是主入口，但必须有导入预览和人工确认。
- 教务适配没有覆盖时，仍可用 Excel、HTML 或分享文件完成批量导入，
  不能退化为逐门手工添加。

### 2.3 ClassIsland 与 Class Widgets

[ClassIsland](https://github.com/ClassIsland/ClassIsland)支持从 Excel、
CSES 和其他课表软件导入，并将课表编辑、时间表和多周轮换分开管理。

[Class Widgets](https://github.com/Class-Widgets/Class-Widgets)使用通用
课程表交换格式 CSES，使课表能在不同软件之间导入和导出。

可借鉴的原则是：Clawd Mochi 内部必须先定义稳定的中间格式，所有学校
适配器、Excel、ICS、HTML 和分享文件都只负责转换到该格式。

### 2.4 中国科大 class-arrange

[class-arrange](https://github.com/RaymondzyLei/class-arrange)把每学期课程
发布为独立 `courses.json`，按学期隔离本地数据，并通过 revision 和
updates 文件处理课程变化。其教务同步使用浏览器自动化登录和 API 抓取，
再执行数据归一化和校验。

可借鉴的原则：

- 浏览器登录和数据抓取运行在设备外部。
- 原始数据、标准化数据和设备展示数据分层。
- 每次同步生成 revision，只有课表发生变化才写入设备。

## 3. 总体技术决策

### 3.1 不让 ESP32 直接登录教务系统

ESP32-C3 不适合承担通用教务登录，原因包括：

- 各学校使用不同的 CAS、统一身份认证、验证码和二次验证。
- 登录页面经常依赖 JavaScript、重定向、Cookie 和动态 API。
- 项目规定 HTTPS 至少保留 80 KB 空闲堆和 64 KB 最大连续块；复杂登录
  会放大 TLS 内存和碎片风险。
- 在设备中保存学号和教务密码不符合隐私与安全要求。
- 学校页面变化频繁，若解析器写进固件，每次适配失效都需要升级固件。

因此自动导入采用“设备外抓取、设备内展示”的结构。

### 3.2 推荐架构

```text
教务系统
   │ 用户在真实登录页完成登录、验证码或二次验证
   ▼
Mochi Timetable Import（浏览器扩展）
   │
   ├─ School Adapter / Provider：读取 DOM 或调用课表 API
   ├─ Parser：转换为统一课程模型
   ├─ Timer：转换学期、周次、节次和实际时间
   ├─ Validator：去重、冲突检查、字段校验
   └─ Name Mapper：中文原名 → 英文全名 → 屏幕短名
   │
   ▼
导入预览（用户确认）
   │ 局域网 POST，凭一次性导入令牌
   ▼
Clawd Mochi /timetable/import
   │
   └─ LittleFS 中的版本化课表数据
```

首版正式产品不要求用户运行 Python、终端命令、独立脚本或浏览器扩展。
用户在 WakeUp、小爱课程表等手机 App 中完成学校教务导入，再把分享口令、
分享链接或导出文件交给设备控制页。控制页解析来源数据、完成课程名称映射
和预览确认，然后把标准课表发送到局域网设备 API。

教务凭据不经过 Clawd Mochi。需要访问来源分享接口时，只访问对应 App 的
官方 HTTPS 域名。Python/Playwright 版本仅保留为适配器开发和自动化测试
工具，不出现在正式用户流程中。直接登录学校的浏览器扩展路线推迟到后续，
只有分享与文件路线无法满足覆盖率时再评估。

## 4. 自动导入的具体实现

### 4.1 用户流程

1. 用户在 Web 控制器点击 `IMPORT CLASSES`。
2. 控制页打开来源选择：WakeUp、小爱课程表、ICS 或其他文件。
3. 用户粘贴分享口令/链接，或者通过手机文件选择器选择导出文件。
4. Source Detector 自动识别来源格式。
5. 来源适配器读取分享数据并转换为统一课程模型。
6. Validator 检查周次、节次、冲突和空字段。
7. Name Mapper 自动生成英文显示名和短名。
8. 用户检查课程数量和未识别名称，点击 `IMPORT TO MOCHI`。
9. 设备校验 schema、大小和 revision，原子替换旧课表并显示成功状态。

“自动导入”指用户不需要逐门录入。学校账号登录由现有课表 App 完成；
Mochi 只接收用户主动分享的课表，不接触登录会话。

### 4.2 适配器接口

每个来源格式适配器是独立目录，不进入固件：

```text
adapters/
  xiaoai-share/
    manifest.json
    provider.js
    parser.js
    fixtures/
      share-response.json
      expected.json
```

`manifest.json`：

```json
{
  "id": "xiaoai-share",
  "name": "XiaoAi Timetable Share",
  "hosts": ["i.ai.mi.com"],
  "mode": "share-api",
  "version": 1,
  "terms": "auto"
}
```

统一接口：

```text
detect(page)                 -> confidence
extract(page, selectedTerm)  -> rawPayload
parse(rawPayload)            -> ImportedCourse[]
getCalendar(page)            -> TermCalendar
```

适配器支持两种抓取模式：

- `api`：优先。复用登录后的 Cookie 调用教务课表 API，结构稳定、解析简单。
- `dom`：API 不可用时读取课表页面 DOM，类似小爱课程表 Provider。

适配器禁止读取成绩、身份证号、联系方式等与课表无关的数据。

### 4.3 统一课表模型

```json
{
  "schemaVersion": 1,
  "revision": "sha256:...",
  "schoolId": "bistu",
  "term": {
    "id": "2026-fall",
    "name": "2026 Fall",
    "startDate": "2026-09-07",
    "weekCount": 18,
    "timezone": "Asia/Shanghai"
  },
  "sections": [
    {"index": 1, "start": "08:00", "end": "08:45"}
  ],
  "courses": [
    {
      "id": "school-course-id-or-stable-hash",
      "courseCode": "CS203",
      "sourceName": "数据结构",
      "englishName": "Data Structures",
      "displayName": "DATA STRUCTURES",
      "shortName": "DATA STRUCT.",
      "teacher": "陈敏",
      "location": "N301",
      "day": 5,
      "weeks": [1, 2, 3, 4, 5, 6, 7, 8],
      "sectionStart": 3,
      "sectionEnd": 4
    }
  ]
}
```

设备展示只依赖 `displayName`、`shortName`、地点、时间、星期和周次。中文
原名用于 Web 导入预览和后续重新映射，不交给 Adafruit_GFX 绘制。

### 4.4 数据校验

同步前必须检查：

- 学期开始日期有效，周数范围建议为 1–30。
- 星期为 1–7，节次存在且起止顺序正确。
- 周次非空、去重且不超过学期周数。
- 同一课程的重复记录合并。
- 同一周、同一天、同一时间的重叠课程标记为冲突，不静默删除。
- 课程原名不能为空。
- 英文显示名只能包含设备支持的 ASCII 字符。
- JSON 大小设置上限，避免一次请求耗尽设备堆。
- revision 未变化时不重复写 LittleFS。

设备接收时先写临时文件，完整校验通过后再重命名替换，掉电或网络中断不能
损坏当前可用课表。

### 4.5 适配覆盖策略

第一版不承诺“一套代码自动识别全国所有学校”。正确做法是：

1. 先支持目标用户所在学校。
2. 将同类教务系统归为模板，例如正方、强智、URP、青果。
3. 学校仅覆盖 URL、字段选择器和少量差异。
4. 每个适配器必须附带脱敏 fixture 和回归测试。
5. 页面结构变化时只更新导入助手中的适配器，不更新设备固件。

未适配学校仍提供批量导入，而不是逐门录入：

- WakeUp/小爱分享或导出文件
- CSES
- ICS
- Excel
- 教务系统保存的 HTML

这些入口共用同一个 Parser、Validator 和 Name Mapper。

## 5. 课程名称映射的具体实现

### 5.1 设计原则

课程名翻译不能在 ESP32 上完成，也不能每次运行时调用 AI。它应在导入助手
中一次性完成，并生成确定性的英文名称。

系统同时保存四个字段：

- `sourceName`：教务系统原始名称。
- `englishName`：完整英文名称。
- `displayName`：设备优先显示名称。
- `shortName`：空间不足时的短名称。

用户修改英文名称后，覆盖规则按 `schoolId + courseCode` 保存。下学期再次
导入同一课程时自动复用，不需要重复修改。

### 5.2 映射优先级

映射按以下顺序执行：

1. **用户覆盖表**：准确度最高。
2. **学校课程代码表**：若学校提供官方中英文课程目录，优先使用。
3. **精确课程词典**：维护常见完整课程名。
4. **组合词典**：将规范化后的中文词组组合为英文。
5. **可选在线翻译**：仅在用户主动开启时用于未知课程，并必须进入确认页。
6. **未知标记**：默认无云模式无法可靠翻译时，不编造结果，要求用户修正。

不使用拼音冒充英文，也不直接把未知课程显示成错误的英文。

### 5.3 名称规范化

映射前先统一：

- 全角/半角字符
- 中文和英文括号
- 多余空格
- 罗马数字与阿拉伯数字
- `（一）`、`1`、`I` 等课程序号
- `A/B`、`上/下`、`实验/实践` 等后缀

例如：

```text
数据结构（A） → 数据结构 + A
大学英语Ⅱ     → 大学英语 + II
高等数学(上)  → 高等数学 + I
```

### 5.4 词典结构

```json
{
  "exact": {
    "数据结构": {
      "english": "Data Structures",
      "display": "DATA STRUCTURES",
      "short": "DATA STRUCT."
    },
    "操作系统原理": {
      "english": "Principles of Operating Systems",
      "display": "OPERATING SYSTEMS",
      "short": "OPERATING SYS."
    }
  },
  "tokens": {
    "高等": "Advanced",
    "数学": "Mathematics",
    "大学": "College",
    "英语": "English",
    "计算机": "Computer",
    "网络": "Networks",
    "原理": "Principles",
    "实验": "Lab"
  }
}
```

精确词典解决语义和习惯译法，组合词典扩大覆盖面。政治课、专业术语和学校
特色课程必须走精确映射，不能只做逐词直译。

### 5.5 自动缩写算法

缩写不是按字符数截断，而是使用设备真实字体度量：

1. 使用与固件一致的 `FreeMonoBold18pt7b` 或最终选定字体计算像素宽度。
2. 首先尝试完整 `displayName` 单行。
3. 单行不下时尝试最多两行，并优先在单词边界换行。
4. 两行仍不下时使用词典中的 `shortName`。
5. 若没有短名，按规则缩写低信息词，再缩写长单词。
6. 仍然超宽则在导入预览中标红，要求用户调整；绝不在设备上截成不可辨认
   的半个单词。

低信息词优先删除或缩写：

```text
INTRODUCTION TO ARTIFICIAL INTELLIGENCE
→ INTRO TO AI

PRINCIPLES OF COMPUTER ORGANIZATION
→ COMPUTER ORG.

COLLEGE PHYSICAL EDUCATION
→ COLLEGE PE
```

常用稳定缩写表：

```text
INTRODUCTION → INTRO
MATHEMATICS  → MATH
COMPUTER     → COMP.
STRUCTURES   → STRUCT.
SYSTEMS      → SYS.
ORGANIZATION → ORG.
ENGINEERING  → ENG.
LABORATORY   → LAB
ARTIFICIAL INTELLIGENCE → AI
PHYSICAL EDUCATION      → PE
```

### 5.6 示例

| 原始名称 | 完整英文 | 设备显示名 | 空间不足时 |
|---|---|---|---|
| 数据结构 | Data Structures | DATA STRUCTURES | DATA STRUCT. |
| 高等数学A | Advanced Mathematics A | ADV. MATHEMATICS A | ADV. MATH A |
| 计算机网络 | Computer Networks | COMPUTER NETWORKS | COMP. NETWORKS |
| 操作系统原理 | Principles of Operating Systems | OPERATING SYSTEMS | OPERATING SYS. |
| 计算机组成原理 | Principles of Computer Organization | COMPUTER ORGANIZATION | COMPUTER ORG. |
| 人工智能导论 | Introduction to Artificial Intelligence | INTRO TO AI | AI INTRO |
| 大学体育 | College Physical Education | COLLEGE PE | PE |

## 6. ESP32 侧职责与内存约束

设备不运行浏览器、不解析教务 HTML、不翻译中文。设备只负责：

- 接收已标准化、已确认的课表。
- 校验 schema、revision、字段长度和 ASCII 范围。
- 把课表持久化到 LittleFS。
- 按日期、星期和当前教学周选择当前课/下一课。
- 使用英文显示名绘制设备 UI。

课表模块必须延迟加载。进入课表视图时只读取当前日所需索引和课程，退出后
立即释放临时 JSON、解析缓冲和课程对象。禁止为课表新增全屏 framebuffer。

建议将导入 JSON 转换为紧凑的设备文件：

```text
/timetable/meta.json
/timetable/index.bin
/timetable/names.bin
```

运行时按课程索引读取固定长度记录，避免把整个学期 JSON 常驻 RAM。

## 7. 安全与隐私

- 教务账号和密码只输入学校官方页面。
- 不在设备、日志、配置文件或云端保存密码。
- 浏览器上下文在导入结束后销毁，Cookie 默认不持久化。
- 设备生成短期一次性导入令牌，成功使用或超时后立即失效。
- 设备 API 只接受同一局域网请求，并限制请求体大小和速率。
- 日志只记录学校适配器、课程数量、revision 和错误类别，不记录课程详情、
  Cookie、账号或密码。

## 8. 测试与验收

### 8.1 名称映射

- 精确词典测试。
- 中文规范化测试。
- 组合翻译测试。
- 用户覆盖优先级测试。
- 使用设备真实字体的单行/双行像素宽度测试。
- 未识别课程必须进入人工确认，不能静默生成错误英文。

### 8.2 自动导入

- 每个学校至少保存一份脱敏 HTML 或 JSON fixture。
- Provider 输出快照测试。
- Parser 字段、周次、节次和去重测试。
- Timer 的开学日期和节次时间测试。
- 冲突课程、单双周、跨连续节、周末课和实验课测试。
- 登录超时、验证码、页面结构变化和空课表错误提示。
- 同一 revision 重复同步不写 Flash。
- 中断同步后旧课表仍可正常使用。

### 8.3 完成标准

第一所目标学校满足以下条件才算“自动导入完成”：

1. 用户登录后无需逐门录入。
2. 课程数量、名称、地点、教师、周次、星期和节次与教务系统一致。
3. 所有课程都生成经过确认的英文设备显示名。
4. 一键同步到设备并可在断网后使用。
5. 连续导入、覆盖、回滚和异常中断均不会损坏现有课表。
6. 实机屏幕使用真实字体展示，名称不溢出、不截断、不闪烁。

## 9. 推荐开发顺序

1. 确定第一所目标学校及其教务系统类型。
2. 定义并冻结 `schemaVersion: 1`。
3. 完成导入助手骨架和一次性设备同步协议。
4. 实现首个学校的 Provider、Parser、Timer 和 fixtures。
5. 实现课程名精确词典、用户覆盖和真实字体宽度计算。
6. 完成导入预览与英文名称校对 UI。
7. 最后才实现 ESP32 的课表存储、状态计算和屏幕绘制。

该顺序先证明“真实教务系统能够自动导入”，再投入设备 UI 编码，避免出现
界面完成但核心数据源不可用的情况。

## 10. 参考资料

- [Mi-AI-Schedule-BISTU：Provider / Parser / Timer](https://github.com/ZZHow1024/Mi-AI-Schedule-BISTU)
- [小爱课程表开发者文档](https://open-schedule-prod.ai.xiaomi.com/docs/)
- [WakeUp：导入课表](https://www.wakeup.fun/doc/import_schedule.html)
- [ClassIsland](https://github.com/ClassIsland/ClassIsland)
- [Class Widgets / CSES](https://github.com/Class-Widgets/Class-Widgets)
- [class-arrange](https://github.com/RaymondzyLei/class-arrange)
- [Adafruit GFX 字体说明](https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts)

## 11. 首个适配目标：广东外语外贸大学

### 11.1 已确认信息

学校官方教务部“常用系统”链接指向：

```text
https://jwxt.gdufs.edu.cn/
```

公开登录页同时提供学生选课子系统：

```text
https://jwxt.gdufs.edu.cn/jsxsd/
```

截至 2026-07-31，对未登录页面进行只读检查可确认：

- 学生登录表单目标为 `/jsxsd/xk/LoginToXk`。
- 登录会话预处理调用 `/jsxsd/Logon.do?method=logon&flag=sess`。
- 学生子系统使用 Path 为 `/jsxsd` 的 HttpOnly 会话 Cookie。
- 页面加载了 `qzTable.js`、`qzForm.js`、`qzDate.js` 等资源。
- URL 结构、`jsxsd` 子系统名称和登录端点与强智教务系统特征高度一致。
- 登录页包含密码登录及验证交互，导入助手必须保留可见浏览器，让用户自行
  完成认证，不能尝试绕过验证码或滑块。

学校官方入口来源：
[广东外语外贸大学教务部](https://jwb.gdufs.edu.cn/)。

系统类型当前标记为：

```text
vendor: qiangzhi
confidence: high
status: post-login schedule endpoint pending verification
```

这里不能仅根据公开登录页就声称课表接口已完全确认。课表查询 URL、请求
参数和返回字段必须在学生本人正常登录后的浏览器 Network 记录中验证。

### 11.2 GDUFS 适配器目录

```text
adapters/
  qiangzhi/
    shared/
      term.js
      week-parser.js
      section-parser.js
  gdufs/
    manifest.json
    provider.js
    parser.js
    timer.js
    name-overrides.json
    fixtures/
      schedule-page.sanitized.html
      schedule-response.sanitized.json
      expected.schedule.json
```

`manifest.json` 初稿：

```json
{
  "id": "gdufs",
  "name": "Guangdong University of Foreign Studies",
  "vendor": "qiangzhi",
  "hosts": ["jwxt.gdufs.edu.cn"],
  "loginUrl": "https://jwxt.gdufs.edu.cn/jsxsd/",
  "authenticatedPathPrefix": "/jsxsd/",
  "mode": "api-preferred-dom-fallback",
  "version": 1
}
```

### 11.3 GDUFS 自动导入流程

1. 导入助手打开 `https://jwxt.gdufs.edu.cn/jsxsd/`。
2. 用户在学校页面完成密码、验证码、滑块或学校要求的其他认证。
3. 助手只通过“已进入学生主页或课表页”判断登录成功，不读取密码输入框。
4. 助手打开“学生课表查询”，并记录该页自身发出的课表请求。
5. 若课表通过 JSON 接口返回，Provider 在同一浏览器上下文复用 Cookie
   调用接口；若只返回 HTML，则读取课表 DOM。
6. 用户选择当前学期；默认读取已选课程的个人课表，而不是全校开课目录。
7. Parser 输出课程名、课程代码、教师、地点、星期、周次和节次。
8. Timer 从系统节次表读取实际起止时间；若系统未提供，首次导入要求用户
   确认学校作息表。
9. Validator 处理重复教学班、单双周、连续节次、跨校区地点和时间冲突。
10. Name Mapper 生成英文名称和设备短名。
11. 用户确认预览后，一键同步到 Clawd Mochi。

### 11.4 课表接口验证计划

强智系统常见课表路径只能作为探测候选，不能直接写成最终实现。导入助手
在已登录上下文中按以下顺序确认：

1. 从学生主页菜单的真实 `href` 找到课表查询页。
2. 监听该页面发出的 XHR/fetch/form 请求。
3. 记录请求方法、学期参数、必要请求头和响应 Content-Type。
4. 保存脱敏响应 fixture。
5. 关闭浏览器并使用 fixture 完成 Parser 单元测试。
6. 再进行第二次真实登录，验证适配器没有依赖一次性参数或固定 Cookie。

强智旧版中常见的 `/jsxsd/xskb/` 路径可以用于发现，但只有真实菜单或
Network 记录命中后才写入 `provider.js`。

### 11.5 GDUFS 字段解析重点

广东外语外贸大学可能包含大量语言类、双语类和跨校区课程，Parser 必须
特别覆盖：

- 中文、英文或中英混合课程原名。
- 多位教师或外教姓名。
- 白云山校区、大学城校区及具体楼栋教室。
- 单周、双周、非连续周次。
- 连堂课和同课程不同地点的记录。
- 实验、口语、实践等同名但不同教学活动。
- 课程名相同但课程代码不同的情况。

稳定 ID 优先使用教务系统课程代码和教学班号；缺少教学班号时，使用
`课程代码 + 教师 + 地点 + 星期 + 节次 + 周次` 计算稳定哈希。

### 11.6 GDUFS 课程英文名称策略

广外是外语类高校，部分课程或培养方案可能已经提供官方英文名称。映射优先
级调整为：

1. 教务响应中的官方英文课程名。
2. 学校官方中英文培养方案或课程目录。
3. `gdufs/name-overrides.json` 的课程代码映射。
4. 通用精确课程词典。
5. 通用组合翻译。
6. 用户在导入预览中修正。

GDUFS 覆盖表按课程代码维护：

```json
{
  "CS203": {
    "source": "数据结构",
    "english": "Data Structures",
    "display": "DATA STRUCTURES",
    "short": "DATA STRUCT."
  }
}
```

课程代码比中文名称更稳定，可以避免同名课程在不同学院使用不同官方译名时
发生冲突。用户确认过的映射保存在电脑端导入助手中，后续学期自动复用。

### 11.7 首次真实验证所需材料

后续进入编码阶段时，需要由学生本人在本机完成一次正常登录，并提供以下
脱敏开发材料：

- 学生课表查询页保存的 HTML，或课表 XHR/Fetch 的响应 JSON。
- 当前学期标识及页面上的学期名称。
- 学校节次与实际起止时间。
- 一张课表页面截图，用于核对行列与合并单元格。

材料必须删除姓名、学号、Cookie、Token、联系方式等个人信息。课程名称、
周次、节次和地点可以保留，也可以使用同结构的替换值。

在取得这些材料之前，可以完成导入助手骨架、强智适配器接口、Parser 测试
框架和名称映射模块，但不能宣称 GDUFS 自动导入已经通过真实系统验证。

## 12. 研究生计算机课程名称覆盖

本项目第一阶段的课程名称词典以计算机专业研究生为优先，不以本科公共课
词表代替研究生课程覆盖。首版映射数据见：

[`data/gdufs-graduate-cs-course-name-map.json`](data/gdufs-graduate-cs-course-name-map.json)

### 12.1 覆盖边界

词典分成三种可追溯来源：

1. `gdufs_official_bilingual`：已从广外培养方案核对中英文名称及课程代码。
2. `gdufs_official_chinese`：已核对广外中文课程名，英文由项目规范化翻译。
3. `cs_graduate_curated`：计算机研究生常见课程扩展，不宣称是广外官方课名。

首版重点覆盖：

- 计算理论、算法、离散数学、统计与优化。
- 操作系统、体系结构、网络、分布式、云计算、数据库和编译。
- 软件工程、软件体系结构、测试、形式化方法和程序分析。
- 机器学习、深度学习、强化学习、自然语言处理、计算机视觉和生成式 AI。
- 大数据、数据挖掘、信息检索、知识图谱和推荐系统。
- 密码学、网络攻防、取证、漏洞挖掘、隐私计算和系统安全。
- 论文写作、研究方法、前沿讲座、专业实践和学位过程课程。

### 12.2 映射与显示规则

- 优先按 `courseCode` 命中，课程代码缺失时才对标准化后的中文别名做精确匹配。
- `officialEnglish` 只保存学校原文；`canonicalEnglish` 用于统一检索和编辑。
- 设备优先显示 `displayName`，宽度不足时显示 `shortName`。
- 最终是否放得下必须使用固件实际字体的像素宽度测量，不能仅按字符数判断。
- `ML`、`NLP`、`HPC`、`LLM` 等只作为空间不足时的短名，不覆盖完整英文名称。
- 未命中的课程保留中文原名供控制页审核，绝不静默生成不可靠缩写。

### 12.3 维护与验收

每次新增培养方案时，先导出其中全部课程名称，再生成三类报告：

- 已按课程代码命中的课程。
- 已按中文别名命中的课程。
- 未命中或同名冲突、需要人工确认的课程。

广外发布新版研究生培养方案后，应新增带年份的来源版本，而不是直接覆盖
旧映射。自动导入验收要求：当前学生课表中所有计算机课程均能产生
`canonicalEnglish`，且所有设备显示名均通过 240×240 屏幕实际字体宽度测试。
