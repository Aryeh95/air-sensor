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
// Full refresh on demand (Key3). The epaper_spi driver decides between a full and a partial
// refresh with a protected counter and has no public way to reset it, so this reaches it
// through a derived class. The next update after calling this is a full refresh.
// ---------------------------------------------------------------------------
struct EPaperFullRefresh : public esphome::epaper_spi::EPaperBase {
  static void arm(esphome::epaper_spi::EPaperBase *display) {
    static_cast<EPaperFullRefresh *>(display)->update_count_ = 0;
  }
};
