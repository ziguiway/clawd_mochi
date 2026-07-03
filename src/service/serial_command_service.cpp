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
            if (_inputBuffer.length() > 64) _inputBuffer = "";
        }
    }
}

void SerialCommandService::processCommand(const String& cmd) {
    if (cmd == "CC:ping") { Serial.println("CC:pong"); return; }
    if (cmd.startsWith("CC:")) {
        auto* opMode = OperationModeService::current();
        if (opMode) opMode->setMode(OperationModeService::Mode::SERIAL);
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
    else if (cmd == "log clear") clearLogs();
    else if (cmd.startsWith("log level ")) setLogLevel(cmd.substring(10));
    else if (cmd.startsWith("cc ")) handleClaudeCommand(cmd.substring(3));
    else if (cmd == "cc") handleClaudeCommand("");
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
    Serial.println("  log           - 显示最近日志");
    Serial.println("  log clear     - 清除日志");
    Serial.println("  log level N   - 设置日志级别 (debug/info/warn/error)");
    Serial.println("  cc <event>,<hook>,<tool>,<detail>,<model> - 注入 Claude 状态");
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

void SerialCommandService::showLogs() {
    Serial.println("\n--- 最近日志 ---");
    Serial.println(Logger::getInstance().getLogs(20));
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
