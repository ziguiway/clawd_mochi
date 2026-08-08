#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "wifi_config_service.h"

class WeatherService {
public:
    enum class LocationSource : uint8_t { IP, GPS, MANUAL };

    explicit WeatherService(WifiConfigService* wifiService);

    void init();
    void update();
    void requestRefresh();
    bool setLocationOverride(float latitude, float longitude,
                             const String& city, LocationSource source);
    void clearLocationOverride();

    bool isValid() const { return _valid; }
    bool isLoading() const { return _loading; }
    const char* getCity() const { return _city; }
    const char* getLocationLabel() const { return _locationLabel; }
    const char* getCondition() const;
    int getTemperature() const { return _temperature; }
    int getHighTemperature() const { return _highTemperature; }
    int getLowTemperature() const { return _lowTemperature; }
    int getHumidity() const { return _humidity; }
    int getWeatherCode() const { return _weatherCode; }
    uint32_t getVersion() const { return _version; }
    float getLatitude() const { return _latitude; }
    float getLongitude() const { return _longitude; }
    LocationSource getLocationSource() const { return _locationSource; }
    bool hasLocationOverride() const { return _locationOverride; }
    const char* getLocationSourceName() const;

private:
    static constexpr unsigned long WEATHER_REFRESH_MS = 30UL * 60UL * 1000UL;
    static constexpr unsigned long LOCATION_REFRESH_MS = 24UL * 60UL * 60UL * 1000UL;
    static constexpr unsigned long FIRST_RETRY_MS = 30UL * 1000UL;
    static constexpr unsigned long MAX_RETRY_MS = 5UL * 60UL * 1000UL;

    WifiConfigService* _wifiService;
    Preferences _prefs;
    bool _valid;
    volatile bool _loading;
    bool _locationValid;
    bool _locationOverride;
    bool _refreshRequested;
    float _latitude;
    float _longitude;
    float _fallbackLatitude;
    float _fallbackLongitude;
    char _fallbackCity[48];
    LocationSource _fallbackSource;
    LocationSource _locationSource;
    char _city[24];
    char _locationLabel[48];
    int _temperature;
    int _highTemperature;
    int _lowTemperature;
    int _humidity;
    int _weatherCode;
    unsigned long _lastWeatherMs;
    unsigned long _lastLocationMs;
    unsigned long _lastAttemptMs;
    volatile uint8_t _failureCount;
    volatile uint32_t _version;
    TaskHandle_t _refreshTask;

    static void refreshTaskEntry(void* parameter);
    void runRefresh();
    bool fetchLocation();
    bool applyFallbackLocation();
    bool fetchWeather();
    unsigned long retryDelayMs() const;
    void copyCity(const char* city);
};
