#include "salary_counter_service.h"

#include "../utils/logger.h"

SalaryCounterService::SalaryCounterService(TimeService* timeService)
    : _timeService(timeService)
    , _initialized(false)
    , _state(SalaryCounterState::READY)
    , _monthlyCents(0)
    , _workDaysX100(2175)
    , _workMinutesPerDay(480)
    , _accumulatedSeconds(0)
    , _segmentStartedMs(0)
    , _segmentStartedEpoch(0)
{
}

SalaryCounterService::~SalaryCounterService() {
    if (_initialized) _preferences.end();
}

void SalaryCounterService::init() {
    if (_initialized) return;
    _preferences.begin("mochi-salary", false);
    _initialized = true;

    _monthlyCents = _preferences.getUInt("monthly", 0);
    _workDaysX100 = constrain(
        _preferences.getUShort("days100", 2175), 100, 3100);
    _workMinutesPerDay = constrain(
        _preferences.getUShort("workmin", 480), 60, 1440);
    _accumulatedSeconds = _preferences.getUInt("elapsed", 0);
    _segmentStartedEpoch = _preferences.getUInt("started", 0);

    const uint8_t storedState = _preferences.getUChar(
        "state", static_cast<uint8_t>(SalaryCounterState::READY));
    _state = storedState <= static_cast<uint8_t>(SalaryCounterState::FINISHED)
        ? static_cast<SalaryCounterState>(storedState)
        : SalaryCounterState::READY;
    if (!isConfigured()) _state = SalaryCounterState::READY;

    restoreRunningSession();
}

bool SalaryCounterService::configure(uint32_t monthlyCents,
                                     uint16_t workDaysX100,
                                     uint16_t workMinutesPerDay) {
    if (isSessionActive() ||
        monthlyCents < 100 || monthlyCents > 100000000UL ||
        workDaysX100 < 100 || workDaysX100 > 3100 ||
        workMinutesPerDay < 60 || workMinutesPerDay > 1440) {
        return false;
    }

    _monthlyCents = monthlyCents;
    _workDaysX100 = workDaysX100;
    _workMinutesPerDay = workMinutesPerDay;
    if (_state != SalaryCounterState::FINISHED) {
        _state = SalaryCounterState::READY;
    }
    persistConfig();
    persistSession();
    return true;
}

bool SalaryCounterService::start(uint32_t clientEpoch) {
    if (!isConfigured() || isSessionActive()) return false;
    _accumulatedSeconds = 0;
    _segmentStartedMs = millis();
    _segmentStartedEpoch = currentEpoch(clientEpoch);
    _state = SalaryCounterState::RUNNING;
    persistSession();
    LOG_INFO("Salary", "计薪会话开始");
    return true;
}

bool SalaryCounterService::pause() {
    if (_state != SalaryCounterState::RUNNING) return false;
    _accumulatedSeconds = getActiveSeconds();
    _segmentStartedMs = 0;
    _segmentStartedEpoch = 0;
    _state = SalaryCounterState::PAUSED;
    persistSession();
    LOG_INFO("Salary", "计薪会话暂停 elapsed=%lu",
             static_cast<unsigned long>(_accumulatedSeconds));
    return true;
}

bool SalaryCounterService::resume(uint32_t clientEpoch) {
    if (_state != SalaryCounterState::PAUSED) return false;
    _segmentStartedMs = millis();
    _segmentStartedEpoch = currentEpoch(clientEpoch);
    _state = SalaryCounterState::RUNNING;
    persistSession();
    LOG_INFO("Salary", "计薪会话继续");
    return true;
}

bool SalaryCounterService::finish() {
    if (!isSessionActive()) return false;
    if (_state == SalaryCounterState::RUNNING) {
        _accumulatedSeconds = getActiveSeconds();
    }
    _segmentStartedMs = 0;
    _segmentStartedEpoch = 0;
    _state = SalaryCounterState::FINISHED;
    persistSession();
    LOG_INFO("Salary", "计薪会话完成 elapsed=%lu",
             static_cast<unsigned long>(_accumulatedSeconds));
    return true;
}

bool SalaryCounterService::reset() {
    _accumulatedSeconds = 0;
    _segmentStartedMs = 0;
    _segmentStartedEpoch = 0;
    _state = SalaryCounterState::READY;
    persistSession();
    LOG_INFO("Salary", "今日计薪已重置");
    return true;
}

const char* SalaryCounterService::getStateName() const {
    switch (_state) {
        case SalaryCounterState::RUNNING: return "running";
        case SalaryCounterState::PAUSED: return "paused";
        case SalaryCounterState::FINISHED: return "finished";
        case SalaryCounterState::READY:
        default: return isConfigured() ? "ready" : "unconfigured";
    }
}

uint32_t SalaryCounterService::getActiveSeconds() const {
    if (_state != SalaryCounterState::RUNNING) {
        return _accumulatedSeconds;
    }
    return _accumulatedSeconds +
           static_cast<uint32_t>((millis() - _segmentStartedMs) / 1000UL);
}

uint64_t SalaryCounterService::getEarnedTenThousandths() const {
    const uint64_t denominator = monthlyPaidSecondsX100();
    if (denominator == 0) return 0;
    return static_cast<uint64_t>(_monthlyCents) *
           getActiveSeconds() * 10000ULL / denominator;
}

uint64_t SalaryCounterService::getDailyTargetTenThousandths() const {
    if (_workDaysX100 == 0) return 0;
    return static_cast<uint64_t>(_monthlyCents) *
           10000ULL / _workDaysX100;
}

uint32_t SalaryCounterService::getRateTenThousandthsPerSecond() const {
    const uint64_t denominator = monthlyPaidSecondsX100();
    if (denominator == 0) return 0;
    return static_cast<uint32_t>(
        static_cast<uint64_t>(_monthlyCents) * 10000ULL / denominator);
}

uint16_t SalaryCounterService::getProgressPermille() const {
    const uint32_t daySeconds = paidSecondsPerDay();
    if (daySeconds == 0) return 0;
    return static_cast<uint16_t>(min<uint64_t>(
        1000ULL, static_cast<uint64_t>(getActiveSeconds()) * 1000ULL /
                     daySeconds));
}

uint32_t SalaryCounterService::currentEpoch(uint32_t clientEpoch) const {
    if (_timeService && _timeService->getEpoch() > 1000000000UL) {
        return _timeService->getEpoch();
    }
    return clientEpoch > 1000000000UL ? clientEpoch : 0;
}

uint32_t SalaryCounterService::paidSecondsPerDay() const {
    return static_cast<uint32_t>(_workMinutesPerDay) * 60UL;
}

uint64_t SalaryCounterService::monthlyPaidSecondsX100() const {
    return static_cast<uint64_t>(_workDaysX100) * paidSecondsPerDay();
}

void SalaryCounterService::restoreRunningSession() {
    if (_state != SalaryCounterState::RUNNING) return;

    const uint32_t nowEpoch = currentEpoch();
    if (_segmentStartedEpoch == 0 || nowEpoch < _segmentStartedEpoch) {
        _state = SalaryCounterState::PAUSED;
        _segmentStartedEpoch = 0;
        persistSession();
        LOG_WARN("Salary", "缺少可信时间，重启后自动暂停");
        return;
    }

    const uint32_t recovered = nowEpoch - _segmentStartedEpoch;
    if (recovered > MAX_RECOVERY_SECONDS) {
        _state = SalaryCounterState::PAUSED;
        _segmentStartedEpoch = 0;
        persistSession();
        LOG_WARN("Salary", "恢复跨度过长，自动暂停 seconds=%lu",
                 static_cast<unsigned long>(recovered));
        return;
    }

    _accumulatedSeconds += recovered;
    _segmentStartedMs = millis();
    _segmentStartedEpoch = nowEpoch;
    persistSession();
    LOG_INFO("Salary", "计薪会话恢复 seconds=%lu",
             static_cast<unsigned long>(recovered));
}

void SalaryCounterService::persistConfig() {
    if (!_initialized) return;
    _preferences.putUInt("monthly", _monthlyCents);
    _preferences.putUShort("days100", _workDaysX100);
    _preferences.putUShort("workmin", _workMinutesPerDay);
}

void SalaryCounterService::persistSession() {
    if (!_initialized) return;
    _preferences.putUChar("state", static_cast<uint8_t>(_state));
    _preferences.putUInt("elapsed", _accumulatedSeconds);
    _preferences.putUInt("started", _segmentStartedEpoch);
}
