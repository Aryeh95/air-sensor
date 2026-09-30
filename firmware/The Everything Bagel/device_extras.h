#pragma once
// Device-only helpers (not used by the desktop preview): AirNow parsing and forcing a
// full e-paper refresh.
#include "esphome.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/epaper_spi/epaper_spi.h"

// ---------------------------------------------------------------------------
// AirNow current observations
// https://docs.airnowapi.org/CurrentObservationsByZip/docs
// The response is a JSON array with one entry per pollutant (O3, PM2.5, PM10), each with an AQI.
// ---------------------------------------------------------------------------
struct AirNowResult {
  bool ok = false;
  int aqi = -1;       // overall AQI: the highest of the pollutants, as AirNow reports it
  int pm25_aqi = -1;  // PM2.5 AQI, -1 if the station doesn't report PM2.5
};

inline AirNowResult parse_airnow(const std::string &body) {
  AirNowResult r;
  JsonDocument doc = esphome::json::parse_json(body);
  JsonArray observations = doc.as<JsonArray>();
  if (observations.isNull())
    return r;
  for (JsonObject o : observations) {
    const int aqi = o["AQI"] | -1;
    const char *parameter = o["ParameterName"] | "";
    if (aqi < 0)
      continue;
    r.ok = true;
    r.aqi = std::max(r.aqi, aqi);
    if (strcmp(parameter, "PM2.5") == 0)
      r.pm25_aqi = aqi;
  }
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
  r.pm25 = epa_correct_purpleair(r.raw_cf1, r.humidity);
  if (std::isnan(r.pm25)) {
    r.error = "Missing PM2.5 or humidity in the response";
    return r;
  }
  r.ok = true;
  return r;
}

// ---------------------------------------------------------------------------
// Full refresh on demand (Key3). The epaper_spi driver decides between a full and a partial
// refresh with a protected counter and has no public way to reset it, so this reaches it
// through a derived class. The next update after calling this is a full refresh.
// ---------------------------------------------------------------------------
struct EPaperFullRefresh : public esphome::epaper_spi::EPaperBase {
  static void arm(esphome::epaper_spi::EPaperBase *display) {
    static_cast<EPaperFullRefresh *>(display)->update_count_ = 0;
  }
};
