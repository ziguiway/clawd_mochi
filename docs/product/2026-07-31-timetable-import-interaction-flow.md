# Clawd Mochi 课表导入交互流程

- 状态：产品路线已确认，具体 UI 确认后再编码
- 首发来源：WakeUp 课程表、小爱课程表、ICS
- 首发验证学校：广东外语外贸大学

## 1. 产品决定

首版不直接登录学校教务系统，不开发浏览器扩展，也不要求用户运行 Python。
用户在手机上的现有课表 App 中完成学校登录和课表导入，再把分享链接、分享
口令或导出文件交给 Mochi。

用户只需要：

1. 在 WakeUp、小爱课程表等 App 中准备好课表。
2. 分享课表，并在 Mochi 控制页粘贴或选择文件。
3. 检查预览并确认同步。

Mochi 不接触教务账号、密码、Cookie 或验证码。手动添加和编辑继续保留，
但只用于修正和兜底。

## 2. 总体流程

```text
手机课表 App
  → 从学校教务导入
  → 分享课表

Mochi 控制页
  → Import Classes
  → 选择来源，或者直接粘贴
  → 自动识别并读取课程
  → 英文名称映射
  → 预览与异常修正
  → Import to Mochi
  → 设备显示课程
```

## 3. 导入首页

```text
IMPORT CLASSES
Bring your timetable to Mochi.
```

页面展示四个来源卡片：

```text
[ WakeUp ]   Share code / exported file
[ XiaoAi ]   Share link
[ Calendar ] ICS file / calendar URL
[ Other ]    HTML / JSON / CSV
```

下方保留一个智能输入框：

```text
Paste a WakeUp code, XiaoAi link, or calendar URL
```

用户不必先选择正确来源。粘贴内容后，`Source Detector` 自动判断格式；只有
无法识别时才要求用户选择来源。页面记住上次使用的来源，下学期默认打开。

## 4. 手机端引导

### 4.1 WakeUp

```text
1. Open your timetable in WakeUp.
2. Choose Share / Export.
3. Copy the share code or export the timetable file.
4. Return here and paste or choose the file.
```

首版按以下优先级适配：

1. WakeUp 分享口令。
2. WakeUp 导出或备份文件。
3. WakeUp 支持的 HTML、CSV 等交换文件。

实际字段和口令解析协议必须通过真实分享样本验证；在验证前不显示
`SUPPORTED`。

### 4.2 小爱课程表

```text
1. Open your timetable in XiaoAi.
2. Choose Share timetable.
3. Copy the share link.
4. Paste it below.
```

解析器从分享链接提取必要参数，并只访问小爱官方 allowlist 域名。控制页在
读取前显示识别出的来源，不跟随任意第三方跳转。

### 4.3 ICS 与其他文件

用户可通过手机系统文件选择器上传 ICS、HTML、JSON 或 CSV，也可粘贴日历
订阅 URL。所有文件首先在浏览器端解析；只有标准化课表发送给设备。

## 5. 自动识别和读取

真实进度状态：

```text
SOURCE DETECTED · XIAOAI
READING CLASSES
MAPPING COURSE NAMES
CHECKING CONFLICTS
READY TO REVIEW
```

处理顺序：

1. `Source Detector` 判断 WakeUp、小爱、ICS、HTML、JSON 或 CSV。
2. 分享链接只允许访问相应 App 的官方 HTTPS 域名。
3. `Source Adapter` 转换为统一课表模型。
4. `Validator` 检查周次、星期、时间、重复项和冲突。
5. `Name Mapper` 生成英文全名、设备显示名和短名。
6. 删除分享 token 和临时解析内容，只保留标准化课表。

失败信息必须具体区分：

- 分享链接已过期。
- 分享口令无效。
- 文件格式不支持。
- 未找到课程。
- 课程字段不完整。
- 来源接口已经变化。

## 6. 导入预览

摘要：

```text
2026 FALL · WEEKS 1–18
12 COURSES · 34 WEEKLY SESSIONS
SOURCE · XIAOAI
SCHOOL · GDUFS
```

课程分为：

- `READY`：课程和英文名称都已识别。
- `REVIEW NEEDED`：英文名称或缩写需要确认。
- `CONFLICT`：时间、周次或重复记录存在冲突。

课程卡片：

```text
机器学习
MACHINE LEARNING · ML
MON 08:30–10:05 · W1–16
N301 · PROF. CHEN
```

用户可以编辑英文名、短名、时间、教师和地点。中文原名只在控制页用于核对，
不进入设备显示层。

没有异常时只需要点击：

```text
IMPORT 12 COURSES TO MOCHI
```

## 7. 完成页

```text
TIMETABLE READY
12 courses imported.
Next class: MACHINE LEARNING · MON 08:30

[ VIEW ON DEVICE ]  [ DONE ]
```

设备保存标准课表、来源类型、适配器版本和 revision。设备不保存分享 token、
教务凭据或来源 App 的账号信息。

## 8. 异常流程

### 分享链接过期

保留旧课表，引导用户回到来源 App 重新分享。

### 来源暂未支持

提供 ICS、JSON、CSV 和手动编辑入口。允许用户提交一份脱敏样例用于适配，
但不把未支持格式描述为自动导入。

### 来源接口变化

显示具体来源和适配器版本；旧课表不受影响，并允许切换到文件导出路线。

### 多个学期

读取成功后让用户选择学期。默认推荐当前日期对应学期，但不自动覆盖选择。

## 9. 广外首发验证

1. 分别确认 WakeUp 和小爱课程表是否支持广外研究生教务导入。
2. 获取不含个人身份信息的真实分享口令、分享链接或导出文件。
3. 验证课程名称、教师、地点、周次、星期和节次。
4. 应用计算机研究生课程英文映射。
5. 对比来源 App、Mochi 预览和实体设备三处结果。

只有完整走通后，相应来源才标记为：

```text
SUPPORTED · VERIFIED FOR GDUFS
```

## 10. 编码前需要确认的 UI

1. 导入首页及来源卡片。
2. WakeUp 和小爱手机引导页。
3. 智能粘贴后的来源识别状态。
4. 自动读取进度页。
5. 课程预览、异常修正和冲突状态。
6. 成功页及设备预览入口。
