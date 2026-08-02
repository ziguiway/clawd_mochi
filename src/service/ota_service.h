#pragma once

#include <Arduino.h>
class WifiConfigService;
class TimeService;

class OtaService {
public:
    enum class State : uint8_t {
        IDLE, CHECKING, AVAILABLE, UP_TO_DATE, DOWNLOADING, UPLOADING,
        VERIFYING, REBOOTING, FAILED
    };

    OtaService(WifiConfigService* wifi, TimeService* time);
    void init();
    void update();

    bool checkNow();
    bool installRemote();
    bool beginUpload(const String& filename, size_t totalSize);
    bool writeUpload(const uint8_t* data, size_t length);
    bool finishUpload();
    void abortUpload();
    void cancel();

    State state() const { return _state; }
    const char* stateText() const;
    const String& currentVersion() const { return _currentVersion; }
    const String& latestVersion() const { return _latestVersion; }
    const String& channel() const { return _channel; }
    const String& releaseNotes() const { return _releaseNotes; }
    const String& lastError() const { return _lastError; }
    const String& lastCheck() const { return _lastCheck; }
    size_t progressBytes() const { return _progressBytes; }
    size_t totalBytes() const { return _totalBytes; }
    bool updateAvailable() const { return _state == State::AVAILABLE; }
    bool uploadActive() const { return _uploadActive; }

private:
    WifiConfigService* _wifi;
    TimeService* _time;
    State _state;
    String _currentVersion;
    String _latestVersion;
    String _channel;
    String _firmwareUrl;
    String _filesystemUrl;
    String _firmwareSha256;
    String _filesystemSha256;
    String _releaseNotes;
    String _lastError;
    String _lastCheck;
    String _uploadFilename;
    uint8_t _uploadType;
    bool _uploadActive;
    bool _dailyCheckDone;
    uint32_t _lastCheckDate;
    size_t _progressBytes;
    size_t _totalBytes;
    class UpdateClass* _updater;

    bool parseManifest(const String& payload);
    bool downloadToUpdate(const String& url, size_t expectedSize,
                          const String& expectedSha256, uint8_t command);
    bool isNewerVersion(const String& candidate) const;
    bool shouldCheckToday() const;
    void setFailure(const String& error);
};
