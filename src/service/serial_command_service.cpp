#include "serial_command_service.h"
#include "operation_mode_service.h"
#include "../utils/logger.h"

SerialCommandService::SerialCommandService(WifiConfigService* wifiService,
                                           ClaudeCodeService* ccService,
                                           TimeService* timeService)
    : _wifiService(wifiService), _ccService(ccService), _timeService(timeService)
{
}

void SerialCommandService::init() {
    Serial.println("\n========================================");
    Serial.println("  Clawd Mochi - Claude Code Companion");
    Serial.println("========================================");
    Serial.println("输入 'help' 查看可用命令\n");
}

void SerialCommandService::update() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (_inputBuffer.length() > 0) {
                _inputBuffer.trim();
                processCommand(_inputBuffer);
                _inputBuffer = "";
            }
        } else {
            _inputBuffer += c;
            if (_inputBuffer.length() > 128) _inputBuffer = "";
        }
    }
}

void SerialCommandService::processCommand(const String& cmd) {
    if (cmd == "CC:ping") {
        // 回 pong:<mode>,供 hook/daemon 判断当前模式(LAN 走 UDP,SERIAL 走串口)
        const char* mode = "lan";
        auto* opMode = OperationModeService::current();
        if (opMode && opMode->isSerial()) mode = "serial";
        Serial.print("CC:pong:");
        Serial.println(mode);
        return;
    }
    if (cmd.startsWith("CC:")) {
        _ccService->processPacket(cmd.c_str(), cmd.length());
        Serial.println("ok");
        return;
    }
    if (cmd == "help" || cmd == "?") printHelp();
    else if (cmd == "status") printStatus();
    else if (cmd == "ip") printIP();
    else if (cmd == "time") showTime();
    else if (cmd == "time sync") syncTime();
    else if (cmd == "log") showLogs();
    else if (cmd.startsWith("log mark ")) {
        const String marker = cmd.substring(9);
        if (marker.isEmpty() || marker.length() > 48) {
            Serial.println("usage: log mark <text>");
        } else {
            LOG_INFO("Test", "Serial marker: %s", marker.c_str());
            Serial.println("日志标记已写入");
        }
    }
    else if (cmd.startsWith("log ")) showLogs(
        constrain(cmd.substring(4).toInt(), 1, 100));
    else if (cmd == "log clear") clearLogs();
    else if (cmd.startsWith("log level ")) setLogLevel(cmd.substring(10));
    else if (cmd.startsWith("cc ")) handleClaudeCommand(cmd.substring(3));
    else if (cmd == "cc") handleClaudeCommand("");
    else if (cmd.startsWith("wifi ")) handleWifiCommand(cmd.substring(5));
    else if (cmd == "wifi") handleWifiCommand("");
    else if (cmd == "reset") resetFactory();
    else if (cmd == "restart" || cmd == "reboot") restart();
    else { Serial.println("未知命令: " + cmd); Serial.println("输入 'help' 查看可用命令"); }
}

void SerialCommandService::printHelp() {
    Serial.println("\n可用命令:");
    Serial.println("  help / ?      - 显示帮助信息");
    Serial.println("  status        - 显示当前状态");
    Serial.println("  ip            - 显示 IP 地址");
    Serial.println("  time          - 显示当前时间");
    Serial.println("  time sync     - 手动同步时间");
    Serial.println("  log [N]       - 显示最近 N 条日志 (默认 20)");
    Serial.println("  log mark TEXT - 写入串口测试日志标记");
    Serial.println("  log clear     - 清除日志");
    Serial.println("  log level N   - 设置日志级别 (debug/info/warn/error)");
    Serial.println("  cc <event>,<hook>,<tool>,<detail>,<model> - 注入 Claude 状态");
    Serial.println("  wifi <ssid> <password> - 保存 WiFi 并立即连接");
    Serial.println("  wifi status   - 显示 WiFi 连接状态");
    Serial.println("  wifi clear    - 清除已保存的 WiFi 凭据");
    Serial.println("  reset         - 恢复出厂设置");
    Serial.println("  restart       - 重启设备\n");
}

void SerialCommandService::printStatus() {
    Serial.println("\n--- 状态 ---");
    Serial.println("WiFi: " + String(_wifiService->isConnected() ? "已连接" : "未连接"));
    Serial.println("SSID: " + _wifiService->getSSID());
    Serial.println("IP: " + _wifiService->getIP());
    Serial.println("Claude Code: " + String(_ccService->getStatusText()));
    Serial.println("时间: " + _timeService->getDateTime());
    Serial.println("------------\n");
}

void SerialCommandService::printIP() { Serial.println("IP: " + _wifiService->getIP()); }

void SerialCommandService::showTime() {
    Serial.println("时间: " + _timeService->getDateTime());
    Serial.println("同步: " + String(_timeService->isSynced() ? "是" : "否"));
}

void SerialCommandService::syncTime() {
    Serial.println("正在同步时间...");
    _timeService->syncNow();
    Serial.println("时间: " + _timeService->getDateTime());
}

void SerialCommandService::showLogs(size_t maxLines) {
    Serial.println("\n--- 最近日志 ---");
    Serial.println(Logger::getInstance().getLogs(maxLines));
    Serial.println("----------------\n");
}

void SerialCommandService::clearLogs() { Logger::getInstance().clearLogs(); Serial.println("日志已清除"); }

void SerialCommandService::setLogLevel(const String& level) {
    if (level == "debug") { Logger::getInstance().setLevel(LogLevel::DEBUG); Serial.println("日志级别: DEBUG"); }
    else if (level == "info") { Logger::getInstance().setLevel(LogLevel::INFO); Serial.println("日志级别: INFO"); }
    else if (level == "warn") { Logger::getInstance().setLevel(LogLevel::WARN); Serial.println("日志级别: WARN"); }
    else if (level == "error") { Logger::getInstance().setLevel(LogLevel::ERROR); Serial.println("日志级别: ERROR"); }
    else Serial.println("无效级别，可选: debug/info/warn/error");
}

void SerialCommandService::resetFactory() { Serial.println("正在恢复出厂设置..."); _wifiService->reset(); }

void SerialCommandService::restart() { Serial.println("正在重启..."); delay(500); ESP.restart(); }

void SerialCommandService::handleClaudeCommand(const String& args) {
    // 格式: cc <event>,<hook>,<tool>,<detail>,<model>
    // event: thinking/working/done/error/permission/idle/sweeping/sleeping
    String parts[5];
    int idx = 0, start = 0;
    for (int i = 0; i < 5 && start <= (int)args.length(); i++) {
        int comma = args.indexOf(',', start);
        if (comma < 0) { parts[i] = args.substring(start); idx = i + 1; break; }
        parts[i] = args.substring(start, comma);
        start = comma + 1; idx = i + 1;
    }
    if (idx == 0 || parts[0].length() == 0) { Serial.println("usage: cc <event>[,<hook>[,<tool>[,<detail>[,<model>]]]]"); return; }

    ClaudeCodeService::Status status;
    const String& e = parts[0];
    if (e == "thinking")        status = ClaudeCodeService::Status::THINKING;
    else if (e == "working")    status = ClaudeCodeService::Status::WORKING;
    else if (e == "done")       status = ClaudeCodeService::Status::DONE;
    else if (e == "error")      status = ClaudeCodeService::Status::ERROR;
    else if (e == "permission") status = ClaudeCodeService::Status::PERMISSION;
    else if (e == "sweeping")   status = ClaudeCodeService::Status::SWEEPING;
    else if (e == "sleeping")   status = ClaudeCodeService::Status::SLEEPING;
    else if (e == "idle")       status = ClaudeCodeService::Status::IDLE;
    else { Serial.println("unknown event: " + e); return; }

    _ccService->injectStatus(status,
                             parts[1].length() ? parts[1].c_str() : nullptr,
                             parts[2].length() ? parts[2].c_str() : nullptr,
                             parts[3].length() ? parts[3].c_str() : nullptr,
                             parts[4].length() ? parts[4].c_str() : nullptr);
    Serial.println("ok");
}

void SerialCommandService::handleWifiCommand(const String& args) {
    if (args == "status") {
        printStatus();
        return;
    }
    if (args == "clear") {
        _wifiService->clearCredentials();
        Serial.println("WiFi 凭据已清除");
        return;
    }

    const int separator = args.indexOf(' ');
    if (separator <= 0) {
        Serial.println("usage: wifi <ssid> <password>");
        return;
    }
    const String ssid = args.substring(0, separator);
    const String password = args.substring(separator + 1);
    if (ssid.isEmpty() || ssid.length() > 32) {
        Serial.println("WiFi 设置失败: SSID 长度必须为 1-32");
        return;
    }
    if (password.length() < 8 || password.length() > 63) {
        Serial.println("WiFi 设置失败: 密码长度必须为 8-63");
        return;
    }

    _wifiService->saveCredentials(ssid.c_str(), password.c_str());
    if (!_wifiService->connectToWifi(ssid.c_str(), password.c_str())) {
        Serial.println("WiFi 设置失败: 无法启动连接");
        return;
    }
    Serial.println("WiFi 凭据已保存，正在连接...");
}
