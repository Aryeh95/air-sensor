#pragma once
// Air quality advice, US AQI maths and humidity compensation.
// Plain C++ with no ESPHome dependency, so it can be unit-tested on a PC.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

// Which picture the display shows next to the advice
enum class AdviceIcon { OK, VENTILATE, PURIFIER, KEEP_CLOSED, TOO_DRY, TOO_HUMID, WAITING, SENSOR_FAULT };

struct AQIAdvice {
  std::string status;
  std::string action;
  std::string reason;
  AdviceIcon icon = AdviceIcon::OK;
};

// Thresholds shared by the advice logic and the display's highlighted tiles.
// CO2 / PM2.5 / humidity match the WHO 2021 based logic used by the HA Air Quality Card.
namespace aqi_limits {
constexpr float CO2_ACT = 1000;        // ppm: ventilate
constexpr float CO2_URGENT = 1500;     // ppm: ventilate now
constexpr float PM25_ACT = 25;         // ug/m3: run a purifier
constexpr float PM25_URGENT = 35;      // ug/m3
constexpr float PM10_ACT = 45;         // ug/m3: WHO 2021 24-hour guideline
constexpr float VOC_ACT = 150;         // Sensirion VOC index (100 = normal for the room)
constexpr float NOX_ACT = 20;          // Sensirion NOx index (1 = normal). No official limit; adjust to taste
constexpr float HUMIDITY_LOW = 30;     // %
constexpr float HUMIDITY_HIGH = 60;    // %
constexpr float OUTDOOR_PM25_OK = 15;  // ug/m3: WHO 2021 24-hour guideline. At or below this, airing out is fine
}  // namespace aqi_limits

// ---------------------------------------------------------------------------
// US EPA AQI for PM2.5 (breakpoints as revised in 2024)
// ---------------------------------------------------------------------------
struct AqiBreakpoint {
  float c_lo, c_hi;
  int i_lo, i_hi;
};
static const AqiBreakpoint PM25_BREAKPOINTS[] = {
    {0.0f, 9.0f, 0, 50},       {9.1f, 35.4f, 51, 100},    {35.5f, 55.4f, 101, 150},
    {55.5f, 125.4f, 151, 200}, {125.5f, 225.4f, 201, 300}, {225.5f, 325.4f, 301, 500},
};

// AQI from a PM2.5 concentration in ug/m3. Returns -1 for no data.
// Uses the instantaneous reading, so it's an estimate: the official AQI averages over time.
inline int us_aqi_from_pm25(float c) {
  if (std::isnan(c))
    return -1;
  c = std::floor(std::max(c, 0.0f) * 10.0f) / 10.0f;  // EPA truncates to 0.1 ug/m3
  for (const auto &b : PM25_BREAKPOINTS) {
    if (c <= b.c_hi)
      return (int) std::lround((b.i_hi - b.i_lo) / (b.c_hi - b.c_lo) * (c - b.c_lo) + b.i_lo);
  }
  return 500;
}

// PM2.5 concentration (ug/m3) from an AQI value, e.g. one reported by AirNow. NAN for no data.
inline float pm25_from_us_aqi(int aqi) {
  if (aqi < 0)
    return NAN;
  for (const auto &b : PM25_BREAKPOINTS) {
    if (aqi <= b.i_hi)
      return (float) (aqi - b.i_lo) / (b.i_hi - b.i_lo) * (b.c_hi - b.c_lo) + b.c_lo;
  }
  return 325.4f;
}

// Short category name for an AQI value
inline const char *us_aqi_category(int aqi) {
  if (aqi < 0)
    return "";
  if (aqi <= 50)
    return "Good";
  if (aqi <= 100)
    return "Moderate";
  if (aqi <= 150)
    return "Sensitive";  // "Unhealthy for Sensitive Groups"
  if (aqi <= 200)
    return "Unhealthy";
  if (aqi <= 300)
    return "Very Unhealthy";
  return "Hazardous";
}

// ---------------------------------------------------------------------------
// PurpleAir correction
// ---------------------------------------------------------------------------
// US EPA correction for PurpleAir's Plantower sensors (Barkjohn et al. 2021, extended for smoke,
// as used on the AirNow Fire and Smoke Map). cf1 is the "CF=1" PM2.5 reading averaged over the
// A and B channels (ug/m3), rh is the PurpleAir's own humidity reading (%).
inline float epa_correct_purpleair(float cf1, float rh) {
  if (std::isnan(cf1) || std::isnan(rh))
    return NAN;
  const float x = std::max(cf1, 0.0f);
  float pm;
  if (x < 30) {
    pm = 0.524f * x - 0.0862f * rh + 5.75f;
  } else if (x < 50) {
    const float w = x / 20 - 1.5f;
    pm = (0.786f * w + 0.524f * (1 - w)) * x - 0.0862f * rh + 5.75f;
  } else if (x < 210) {
    pm = 0.786f * x - 0.0862f * rh + 5.75f;
  } else if (x < 260) {
    const float w = x / 50 - 4.2f;
    pm = (0.69f * w + 0.786f * (1 - w)) * x - 0.0862f * rh * (1 - w) + 2.966f * w + 5.75f * (1 - w) +
         8.84e-4f * x * x * w;
  } else {
    pm = 2.966f + 0.69f * x + 8.84e-4f * x * x;
  }
  return std::max(pm, 0.0f);
}

// EPA quality check: the two laser counters must roughly agree (not more than 5 ug/m3 AND 70% apart)
inline bool purpleair_channels_agree(float a, float b) {
  if (std::isnan(a) || std::isnan(b))
    return false;
  const float diff = std::fabs(a - b);
  const float mean = (a + b) / 2;
  return !(diff > 5.0f && mean > 0 && diff / mean > 0.7f);
}

// ---------------------------------------------------------------------------
// Humidity compensation for a temperature offset
// ---------------------------------------------------------------------------
// Saturation vapour pressure (hPa), Magnus formula
inline float saturation_vapour_pressure(float t_celsius) {
  return 6.112f * std::exp(17.62f * t_celsius / (243.12f + t_celsius));
}

// When the enclosure makes the sensor read warm, correcting the temperature alone would leave the
// relative humidity wrong. This keeps the absolute amount of water in the air the same and
// recalculates RH at the corrected temperature.
inline float compensate_humidity(float rh, float t_raw, float t_offset) {
  if (std::isnan(rh) || std::isnan(t_raw) || t_offset == 0.0f)
    return rh;
  float rh_corrected = rh * saturation_vapour_pressure(t_raw) / saturation_vapour_pressure(t_raw + t_offset);
  return std::min(std::max(rh_corrected, 0.0f), 100.0f);
}

// ---------------------------------------------------------------------------
// Advice
// ---------------------------------------------------------------------------
class AQIAlgo {
 public:
  // outdoor_pm25 is optional (NAN if unknown), e.g. from AirNow
  static AQIAdvice get_advice(float co2, float pm25, float voc_index, float humidity, float outdoor_pm25 = NAN) {
    using namespace aqi_limits;
    AQIAdvice advice;

    // If any of the sensors are NaN, the sensor is probably booting.
    if (std::isnan(co2) || std::isnan(pm25) || std::isnan(voc_index) || std::isnan(humidity)) {
      advice.status = "Starting";
      advice.action = "No Data";
      advice.reason = "Please Wait...";
      advice.icon = AdviceIcon::WAITING;
      return advice;
    }

    // =========================================================
    // 1. OVERALL STATUS (matches the WHO 2021 based card logic)
    // =========================================================
    if (co2 > CO2_URGENT || pm25 > PM25_URGENT) {
      advice.status = "Poor";
    } else if (co2 > CO2_ACT || pm25 > PM25_ACT) {
      advice.status = "Fair";
    } else if (co2 > 800 || pm25 > 15) {
      advice.status = "Moderate";
    } else if (co2 > 600 || pm25 > 5) {
      advice.status = "Good";
    } else {
      advice.status = "Excellent";
    }

    // =========================================================
    // 2 & 3. ACTION AND REASON (waterfall, most urgent first)
    // =========================================================
    if (co2 > CO2_URGENT) {
      set(advice, "Urgent Vent", reason("CO2", co2, " ppm"), AdviceIcon::VENTILATE);
    } else if (pm25 > PM25_URGENT) {
      set(advice, "Run Purifier", reason("PM2.5", pm25, " µg/m³"), AdviceIcon::PURIFIER);
    } else if (voc_index >= VOC_ACT) {
      set(advice, "Ventilate", reason("VOC Index", voc_index, ""), AdviceIcon::VENTILATE);
    } else if (pm25 > PM25_ACT && co2 > CO2_ACT) {
      set(advice, "Ventilate", "PM & CO2 High", AdviceIcon::VENTILATE);
    } else if (pm25 > PM25_ACT) {
      set(advice, "Run Purifier", reason("PM2.5", pm25, " µg/m³"), AdviceIcon::PURIFIER);
    } else if (co2 > CO2_ACT) {
      set(advice, "Ventilate", reason("CO2", co2, " ppm"), AdviceIcon::VENTILATE);
    } else if (humidity < HUMIDITY_LOW) {
      set(advice, "Too Dry", reason("Humidity", humidity, "%"), AdviceIcon::TOO_DRY);
    } else if (humidity > HUMIDITY_HIGH) {
      set(advice, "Too Humid", reason("Humidity", humidity, "%"), AdviceIcon::TOO_HUMID);
    } else if (co2 > 800 || pm25 > 15) {
      if (co2 > 800) {
        set(advice, "Cons. Vent", reason("CO2", co2, " ppm"), AdviceIcon::VENTILATE);
      } else {
        set(advice, "Cons. Vent", reason("PM2.5", pm25, " µg/m³"), AdviceIcon::VENTILATE);
      }
    } else {
      set(advice, "No Actions", "Optimal Air", AdviceIcon::OK);
    }

    // =========================================================
    // SMART OUTDOOR OVERRIDE
    // Only hold back ventilation when the outside air is actually polluted, not merely a
    // little worse than inside. Stale CO2 is still worth a short airing.
    // =========================================================
    bool outdoor_bad = !std::isnan(outdoor_pm25) && outdoor_pm25 > OUTDOOR_PM25_OK && outdoor_pm25 > pm25;
    if (outdoor_bad && advice.icon == AdviceIcon::VENTILATE) {
      if (advice.action == "Urgent Vent") {
        set(advice, "Vent Briefly", "Outside PM2.5 High", AdviceIcon::VENTILATE);
      } else if (pm25 > 15) {
        set(advice, "Run Purifier", "Worse Outside", AdviceIcon::PURIFIER);
      } else {
        set(advice, "Keep Closed", "Worse Outside", AdviceIcon::KEEP_CLOSED);
      }
    }
    return advice;
  }

 private:
  static void set(AQIAdvice &advice, const char *action, const std::string &reason, AdviceIcon icon) {
    advice.action = action;
    advice.reason = reason;
    advice.icon = icon;
  }

  // e.g. "CO2 at 860 ppm"
  static std::string reason(const char *prefix, float val, const char *suffix) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s at %.0f%s", prefix, val, suffix);
    return std::string(buf);
  }
};
