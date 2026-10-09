#pragma once
// Device-only helpers (not used by the desktop preview): AirNow parsing and forcing a
// full e-paper refresh.
#include "esphome.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/epaper_spi/epaper_spi.h"

// ---------------------------------------------------------------------------
// AirNow current observations by ZIP code or lat/long
// https://docs.airnowapi.org/webservices ("Current Observations: By Zip Code or Lat/Long")
// This service replaced /aq/observation/zipCode/current/, which AirNow retired on 1 October 2026.
// The response is a JSON array with one entry per pollutant (OZONE, PM2.5, PM10), each from the
// closest monitor for that pollutant, with a NowCast AQI. With no data, it's an error object instead.
// ---------------------------------------------------------------------------
struct AirNowResult {
  bool ok = false;
  int aqi = -1;       // overall AQI: the highest of the pollutants
  int pm25_aqi = -1;  // PM2.5 AQI, -1 if no monitor nearby reports PM2.5
  std::string error;
};

inline AirNowResult parse_airnow(const std::string &body) {
  AirNowResult r;
  JsonDocument doc = esphome::json::parse_json(body);
  JsonArray observations = doc.as<JsonArray>();
  if (observations.isNull()) {
    // e.g. {"WebServiceError":[{"Message":"..."}]}
    const char *message = doc["WebServiceError"][0]["Message"] | "unreadable response";
    r.error = message;
    return r;
  }
  for (JsonObject o : observations) {
    const int aqi = o["nowcastAQI"] | (o["AQI"] | -1);  // "AQI" in the retired service
    const char *parameter = o["parameterName"] | (o["ParameterName"] | "");
    if (aqi < 0)
      continue;
    r.ok = true;
    r.aqi = std::max(r.aqi, aqi);
    if (strcmp(parameter, "PM2.5") == 0)
      r.pm25_aqi = aqi;
  }
  if (!r.ok)
    r.error = "no observations within 25 miles";
  return r;
}

// ---------------------------------------------------------------------------
// PurpleAir sensor data
// https://api.purpleair.com/#api-sensors-get-sensor-data
// Requested fields: name, location_type, last_seen, humidity, pm2.5_cf_1, pm2.5_cf_1_a, pm2.5_cf_1_b
// ---------------------------------------------------------------------------
struct PurpleAirResult {
  bool ok = false;
  float pm25 = NAN;  // EPA-corrected PM2.5, ug/m3
  float raw_cf1 = NAN;
  float humidity = NAN;
  bool humidity_assumed = false;  // the sensor didn't report humidity, so 50% was used
  std::string name;
  std::string error;
};

inline PurpleAirResult parse_purpleair(const std::string &body) {
  PurpleAirResult r;
  JsonDocument doc = esphome::json::parse_json(body);
  JsonObject root = doc.as<JsonObject>();
  if (root.isNull()) {
    r.error = "Unreadable response";
    return r;
  }
  JsonObject s = root["sensor"];
  if (s.isNull()) {
    const char *why = root["description"] | (const char *) nullptr;
    if (why == nullptr)
      why = root["error"] | "unknown error";
    r.error = std::string("No sensor data: ") + why;
    return r;
  }
  r.name = s["name"] | "";
  if ((s["location_type"] | 0) != 0) {
    r.error = "Sensor '" + r.name + "' is marked as indoor";
    return r;
  }
  const uint32_t data_time = root["data_time_stamp"] | 0u;
  const uint32_t last_seen = s["last_seen"] | 0u;
  if (data_time && last_seen && data_time - last_seen > 30 * 60) {
    r.error = "Sensor '" + r.name + "' hasn't reported for over 30 minutes";
    return r;
  }
  const float a = s["pm2.5_cf_1_a"] | NAN;
  const float b = s["pm2.5_cf_1_b"] | NAN;
  if (!std::isnan(a) && !std::isnan(b)) {
    if (!purpleair_channels_agree(a, b)) {
      r.error = "Channels A and B disagree (" + std::to_string(a) + " vs " + std::to_string(b) + ")";
      return r;
    }
    r.raw_cf1 = (a + b) / 2;
  } else {
    r.raw_cf1 = s["pm2.5_cf_1"] | NAN;  // single-channel sensor
  }
  r.humidity = s["humidity"] | NAN;
  if (std::isnan(r.humidity)) {
    // Some sensors don't report humidity. The correction's humidity term is small
    // (about 0.9 ug/m3 per 10% RH), so a typical value is better than no reading.
    r.humidity = 50;
    r.humidity_assumed = true;
  }
  r.pm25 = epa_correct_purpleair(r.raw_cf1, r.humidity);
  if (std::isnan(r.pm25)) {
    r.error = "No PM2.5 in the response";
    return r;
  }
  r.ok = true;
  return r;
}

// Handles one PurpleAir response: logs it and, if the reading is usable, makes it the
// outdoor reading. Returns false if the caller should try the backup sensor.
inline bool handle_purpleair_response(int status, const std::string &body, const char *which) {
  if (status != 200) {
    ESP_LOGW("purpleair", "%s: HTTP %d (check the API key and sensor number)", which, status);
    return false;
  }
  PurpleAirResult r = parse_purpleair(body);
  if (!r.ok) {
    ESP_LOGW("purpleair", "%s: not using it: %s", which, r.error.c_str());
    return false;
  }
  ESP_LOGI("purpleair", "%s (%s): raw %.1f, humidity %.0f%%%s, EPA-corrected PM2.5 %.1f ug/m3", which, r.name.c_str(),
           r.raw_cf1, r.humidity, r.humidity_assumed ? " (not reported, assumed)" : "", r.pm25);
  set_outdoor(OutdoorSource::PURPLEAIR, us_aqi_from_pm25(r.pm25), r.pm25);
  return true;
}

// ---------------------------------------------------------------------------
// The epaper_spi driver keeps its refresh counter and state protected, so this reaches them
// through a derived class.
// ---------------------------------------------------------------------------
struct EPaperControl : public esphome::epaper_spi::EPaperBase {
  // Makes the next update a full refresh (the driver does one when the counter is 0)
  static void arm_full_refresh(esphome::epaper_spi::EPaperBase *display) {
    static_cast<EPaperControl *>(display)->update_count_ = 0;
  }
  // Still drawing the previous frame. The driver ignores update() until it's done.
  static bool busy(esphome::epaper_spi::EPaperBase *display) {
    return static_cast<EPaperControl *>(display)->state_ != esphome::epaper_spi::EPaperState::IDLE;
  }
};
