#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "time_service.h"

enum class SalaryCounterState : uint8_t {
    READY,
    RUNNING,
    PAUSED,
    FINISHED
};

class SalaryCounterService {
public:
    explicit SalaryCounterService(TimeService* timeService);
    ~SalaryCounterService();

    void init();

    bool configure(uint32_t monthlyCents, uint16_t workDaysX100,
                   uint16_t workMinutesPerDay);
    bool start(uint32_t clientEpoch = 0);
    bool pause();
    bool resume(uint32_t clientEpoch = 0);
    bool finish(uint32_t maxActiveSeconds = 0);
    bool reset();
    bool rolloverToDate(uint32_t dateKey);

    SalaryCounterState getState() const { return _state; }
    const char* getStateName() const;
    bool isConfigured() const { return _monthlyCents > 0; }
    bool isSessionActive() const {
        return _state == SalaryCounterState::RUNNING ||
               _state == SalaryCounterState::PAUSED;
    }
    bool isRunning() const { return _state == SalaryCounterState::RUNNING; }
    bool isPaused() const { return _state == SalaryCounterState::PAUSED; }

    uint32_t getMonthlyCents() const { return _monthlyCents; }
    uint16_t getWorkDaysX100() const { return _workDaysX100; }
    uint16_t getWorkMinutesPerDay() const { return _workMinutesPerDay; }
    uint32_t getRecordDateKey() const { return _recordDateKey; }
    uint32_t getActiveSeconds() const;
    uint64_t getActiveMilliseconds() const;
    uint64_t getEarnedTenThousandths() const;
    uint64_t getLiveEarnedTenThousandths() const;
    uint64_t getDailyTargetTenThousandths() const;
    uint32_t getRateTenThousandthsPerSecond() const;
    uint16_t getProgressPermille() const;

private:
    static constexpr uint32_t MAX_RECOVERY_SECONDS = 16UL * 60UL * 60UL;

    TimeService* _timeService;
    Preferences _preferences;
    bool _initialized;
    SalaryCounterState _state;
    uint32_t _monthlyCents;
    uint16_t _workDaysX100;
    uint16_t _workMinutesPerDay;
    uint32_t _accumulatedSeconds;
    uint32_t _segmentStartedMs;
    uint32_t _segmentStartedEpoch;
    uint32_t _recordDateKey;

    uint32_t currentEpoch(uint32_t clientEpoch = 0) const;
    uint32_t currentDateKey(uint32_t clientEpoch = 0) const;
    uint32_t paidSecondsPerDay() const;
    uint64_t monthlyPaidSecondsX100() const;
    void restoreRunningSession();
    void persistConfig();
    void persistSession();
};
