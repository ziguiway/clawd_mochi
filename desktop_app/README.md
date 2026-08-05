# Mochi Desktop (上位机)

Clawd Mochi 的桌面控制台：把 PC 屏幕无线推流到 Mochi 的 240×240 屏幕，后续将迁入网页端 controller 的设备控制功能。

## 功能（一期）

- 桌面推流到 Mochi（TCP 3333，`ESPF` + uint32 LE 长度 + JPEG 帧）
- 三种截屏模式：**鼠标跟随裁切（默认）/ 全屏缩放 / 固定区域**
- 设备自动发现（监听固件 UDP 4211 `CC_DISCOVER` 广播）+ 手动 IP
- FPS 3–15（默认 8）、JPEG 质量 30–80（默认 50）、多显示器选择
- 240×240 本地实时预览（像素风"虚拟 Mochi 屏"）
- 系统托盘常驻、断线指数退避自动重连
- 其余功能页（表情/时钟/信息面板/街机/媒体/系统设置）为 IA 占位，后续迁入

## 开发

```bash
npm install        # 若 electron 二进制下载超时: 用 npmmirror registry
npm run dev        # vite + electron 热更新
npm run typecheck  # TS 检查
npm run build      # 渲染层构建到 dist/renderer
```

## 打包

```bash
npm run pack:mac   # dist → release/ 下出 universal .dmg
npm run pack:win   # 出 NSIS 安装包 + portable exe（需在 Windows 或 wine 环境）
```

一期不做代码签名：
- **macOS**：首次打开提示"未验证的开发者"，右键 → 打开 即可。
- **Windows**：SmartScreen 提示 → 更多信息 → 仍要运行。

## macOS 屏幕录制权限

首次推流时系统会要求"屏幕录制"权限；应用内会显示引导条，点 OPEN SETTINGS 跳转系统设置勾选后**重启应用**。

## 安全说明

桌面帧在局域网内**明文传输**，发现/控制消息未认证。仅在可信任的私有 LAN 使用，公共 WiFi 下请勿开启推流。

## 架构

```
src/main/main.js        主进程: 截屏(desktopCapturer) / 编码(sharp) / TCP 推流 / 发现 / 托盘
src/preload/preload.js  contextBridge IPC 白名单
src/renderer/           React + Vite + TS 控制台界面
src/renderer/src/lib/DeviceClient.ts   设备 HTTP REST 统一封装(扩展点)
design/                 高保真静态设计原型(像素/桌宠风, 深色 + #fb6b10)
```

扩展约定：左侧导航分组镜像固件 `InteractiveView` 枚举；新功能页 = 新路由 + `src/pages/` 下模块，与设备通信走 `DeviceClient`。
