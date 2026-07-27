#pragma once

#include <Arduino.h>
#include "wifi_config_service.h"

class WeatherService {
public:
    explicit WeatherService(WifiConfigService* wifiService);

    void init();
    void update();
    void requestRefresh();

    bool isValid() const { return _valid; }
    bool isLoading() const { return _loading; }
    const char* getCity() const { return _city; }
    const char* getCondition() const;
    int getTemperature() const { return _temperature; }
    int getHighTemperature() const { return _highTemperature; }
    int getLowTemperature() const { return _lowTemperature; }
    int getHumidity() const { return _humidity; }
    int getWeatherCode() const { return _weatherCode; }
    uint32_t getVersion() const { return _version; }

private:
    static constexpr unsigned long WEATHER_REFRESH_MS = 30UL * 60UL * 1000UL;
    static constexpr unsigned long LOCATION_REFRESH_MS = 24UL * 60UL * 60UL * 1000UL;
    static constexpr unsigned long RETRY_INTERVAL_MS = 5UL * 60UL * 1000UL;

    WifiConfigService* _wifiService;
    bool _valid;
    volatile bool _loading;
    bool _locationValid;
    bool _refreshRequested;
    float _latitude;
    float _longitude;
    char _city[24];
    int _temperature;
    int _highTemperature;
    int _lowTemperature;
    int _humidity;
    int _weatherCode;
    unsigned long _lastWeatherMs;
    unsigned long _lastLocationMs;
    unsigned long _lastAttemptMs;
    volatile uint32_t _version;
    TaskHandle_t _refreshTask;

    static void refreshTaskEntry(void* parameter);
    void runRefresh();
    bool fetchLocation();
    bool fetchWeather();
    void copyCity(const char* city);
};
