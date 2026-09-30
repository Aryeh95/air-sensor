#pragma once
#include "esphome.h"
#include "aqi_algo.h"

// Screens for the GDEY0426T82 4.26" panel: 800x480, landscape.
//
// Main page                                            Trend page (Key2)
// +--------------------------------------------------+ +--------------------------------------+
// | Date          Outside AQI 42 · Good         Time | | Date          Outside AQI ...   Time |
// +---------------+----------------------------------+ +--------------------------------------+
// |               |  CO2 ↑     |  Temp     | Humidity | | CO2, last 24 h                       |
// |    [icon]     |------------+-----------+----------| |  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  |
// |    Status     |  PM1       |  PM2.5 ↓  | PM4      | | PM2.5, last 24 h                     |
// |    Action     |------------+-----------+----------| |  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  |
// |    Reason     |  PM10      |  VOC      | NOx      | |  -24h   -18h   -12h   -6h      now   |
// |  US AQI 23    |   (tiles past a limit are inverted)| +--------------------------------------+
// +---------------+----------------------------------+
//
// The fonts used below (montserrat_20/24/36/48) are the ids from the YAML; ESPHome declares
// them in main.cpp before this header is included.

namespace layout {
static const int WIDTH = 800;
static const int HEIGHT = 480;
static const int MARGIN = 16;
static const int HEADER_HEIGHT = 50;
static const int HEADER_TEXT_Y = (HEADER_HEIGHT - 4) / 2;

// Left panel: recommendation
static const int LEFT_WIDTH = 330;
static const int LEFT_CENTER = LEFT_WIDTH / 2;

// Right panel: 3x3 grid of readings
static const int GRID_X = LEFT_WIDTH + 12;
static const int GRID_Y = HEADER_HEIGHT + 6;
static const int GRID_COLS = 3;
static const int GRID_ROWS = 3;
static const int TILE_WIDTH = (WIDTH - MARGIN - GRID_X) / GRID_COLS;
static const int TILE_HEIGHT = (HEIGHT - MARGIN - GRID_Y) / GRID_ROWS;
static const int TILE_PADDING = 10;
}  // namespace layout

static const Color PAPER = Color::WHITE;
static const Color INK = Color::BLACK;

enum class Trend { DOWN = -1, FLAT = 0, UP = 1 };

// ---------------------------------------------------------------------------
// Reading history: 24 h of 5-minute averages for the trend page, plus the last
// ~24 min of raw readings for the trend arrows. Kept in RAM, so it restarts on reboot.
// ---------------------------------------------------------------------------
class ReadingHistory {
 public:
  static constexpr uint32_t BUCKET_MS = 5UL * 60UL * 1000UL;
  static constexpr int SLOTS = 288;  // 24 h
  static constexpr int RECENT = 48;  // raw readings, one every 30 s
  static constexpr uint32_t TREND_MS = 10UL * 60UL * 1000UL;

  void add(uint32_t now, float co2, float pm25) {
    recent_[recent_head_] = {now, co2, pm25};
    recent_head_ = (recent_head_ + 1) % RECENT;
    if (recent_count_ < RECENT)
      recent_count_++;

    uint32_t bucket = now / BUCKET_MS;
    if (!bucket_open_) {
      open_bucket_(bucket);
    } else if (bucket != bucket_id_) {
      close_bucket_();
      uint32_t gap = bucket - bucket_id_ - 1;  // buckets with no readings at all
      if (gap > (uint32_t) SLOTS)
        gap = 0;  // millis() wrapped (every ~49 days): treat as contiguous
      for (uint32_t i = 0; i < gap; i++)
        push_(NAN, NAN);
      open_bucket_(bucket);
    }
    if (!std::isnan(co2)) {
      co2_sum_ += co2;
      co2_n_++;
    }
    if (!std::isnan(pm25)) {
      pm_sum_ += pm25;
      pm_n_++;
    }
  }

  // Changes once per finished 5-minute bucket
  uint32_t version() const { return version_; }

  // Number of points for the graph, newest first (point 0 is the current, unfinished bucket)
  int points() const { return size_ + (bucket_open_ ? 1 : 0); }
  void point(int age, float &co2, float &pm25) const {
    if (bucket_open_) {
      if (age == 0) {
        co2 = co2_n_ ? co2_sum_ / co2_n_ : NAN;
        pm25 = pm_n_ ? pm_sum_ / pm_n_ : NAN;
        return;
      }
      age--;
    }
    int idx = (head_ + SLOTS - 1 - age) % SLOTS;
    co2 = co2_[idx];
    pm25 = pm_[idx];
  }

  // Compares the latest reading with the newest one at least 10 minutes older
  Trend trend(bool for_co2) const {
    if (preview_) return for_co2 ? preview_co2_ : preview_pm_;
    if (recent_count_ < 2)
      return Trend::FLAT;
    const Sample &latest = recent_[(recent_head_ + RECENT - 1) % RECENT];
    for (int i = 1; i < recent_count_; i++) {
      const Sample &old = recent_[(recent_head_ + RECENT - 1 - i) % RECENT];
      if (latest.ms - old.ms < TREND_MS)
        continue;
      float a = for_co2 ? old.co2 : old.pm25;
      float b = for_co2 ? latest.co2 : latest.pm25;
      if (std::isnan(a) || std::isnan(b))
        return Trend::FLAT;
      float threshold = for_co2 ? 50.0f : std::max(2.0f, a * 0.25f);
      if (b - a >= threshold)
        return Trend::UP;
      if (a - b >= threshold)
        return Trend::DOWN;
      return Trend::FLAT;
    }
    return Trend::FLAT;
  }

  // For the desktop preview only
  void push_bucket_for_preview(float co2, float pm25) {
    push_(co2, pm25);
  }
  void set_trends_for_preview(Trend co2, Trend pm25) {
    preview_ = true;
    preview_co2_ = co2;
    preview_pm_ = pm25;
  }

 private:
  struct Sample {
    uint32_t ms;
    float co2, pm25;
  };
  void open_bucket_(uint32_t id) {
    bucket_open_ = true;
    bucket_id_ = id;
    co2_sum_ = pm_sum_ = 0;
    co2_n_ = pm_n_ = 0;
  }
  void close_bucket_() {
    push_(co2_n_ ? co2_sum_ / co2_n_ : NAN, pm_n_ ? pm_sum_ / pm_n_ : NAN);
  }
  void push_(float co2, float pm25) {
    co2_[head_] = co2;
    pm_[head_] = pm25;
    head_ = (head_ + 1) % SLOTS;
    if (size_ < SLOTS)
      size_++;
    version_++;
  }

  float co2_[SLOTS]{};
  float pm_[SLOTS]{};
  int head_ = 0, size_ = 0;
  uint32_t version_ = 0;
  bool bucket_open_ = false;
  uint32_t bucket_id_ = 0;
  float co2_sum_ = 0, pm_sum_ = 0;
  int co2_n_ = 0, pm_n_ = 0;
  Sample recent_[RECENT]{};
  int recent_head_ = 0, recent_count_ = 0;
  bool preview_ = false;
  Trend preview_co2_ = Trend::FLAT, preview_pm_ = Trend::FLAT;
};

// ---------------------------------------------------------------------------
// What's on screen. The display only redraws when this changes.
// ---------------------------------------------------------------------------
struct Tile {
  const char *label = "";
  std::string unit;
  std::string value;
  bool alert = false;
  Trend trend = Trend::FLAT;
};

struct DisplayModel {
  int page = 0;
  std::string date, clock;
  bool wifi_ok = true;
  std::string outdoor;
  AQIAdvice advice;
  std::string aqi_line;
  Tile tiles[9];
  uint32_t history_version = 0;

  std::string signature() const {
    std::string s = std::to_string(page) + '|' + date + '|' + clock + '|' + (wifi_ok ? '1' : '0') + '|' + outdoor;
    if (page == 1)
      return s + '|' + std::to_string(history_version) + '|' + tiles[0].value + '|' + tiles[4].value;
    s += '|' + advice.status + '|' + advice.action + '|' + advice.reason + '|' + aqi_line;
    for (const auto &t : tiles)
      s += '|' + t.value + (t.alert ? '!' : ' ') + std::to_string((int) t.trend) + t.unit;
    return s;
  }
};

struct Readings {
  float temperature, humidity, pm1, pm25, pm4, pm10, co2, voc, nox;
};

enum class SensorState { WAITING, OK, OFFLINE, MISSING };

// Outdoor air quality sources, in order of preference
enum class OutdoorSource { PURPLEAIR = 0, AIRNOW = 1 };

struct UiState {
  static constexpr uint32_t OFFLINE_MS = 2UL * 60UL * 1000UL;        // no reading for 2 min = offline
  static constexpr uint32_t STARTUP_MS = 2UL * 60UL * 1000UL;        // no reading at all after 2 min = missing
  static constexpr uint32_t PURPLEAIR_STALE_MS = 45UL * 60UL * 1000UL;  // PurpleAir older than 45 min: use AirNow
  static constexpr uint32_t AIRNOW_STALE_MS = 3UL * 3600UL * 1000UL;     // AirNow older than 3 h is ignored
  static constexpr uint32_t TREND_PAGE_MS = 2UL * 60UL * 1000UL;     // trend page returns to main after 2 min

  DisplayModel model;
  std::string signature;
  bool has_model = false;
  bool force = false;
  int page = 0;
  uint32_t page_since = 0;
  ReadingHistory history;
  bool seen_reading = false;
  uint32_t last_reading_ms = 0;
  struct Outdoor {
    bool seen = false;
    uint32_t ms = 0;
    int aqi = -1;
    float pm25 = NAN;
  } outdoor[2];  // indexed by OutdoorSource
};
inline UiState ui;

// --- Called from the YAML ---------------------------------------------------

// Every SEN66 reading (after the last value, CO2, has been published)
inline void record_reading(float co2, float pm25) {
  ui.seen_reading = true;
  ui.last_reading_ms = millis();
  ui.history.add(ui.last_reading_ms, co2, pm25);
}

// A successful outdoor update from PurpleAir or AirNow
inline void set_outdoor(OutdoorSource source, int aqi, float pm25) {
  auto &o = ui.outdoor[(int) source];
  o.seen = true;
  o.ms = millis();
  o.aqi = aqi;
  o.pm25 = pm25;
}

inline void request_redraw() { ui.force = true; }

inline void toggle_page() {
  ui.page = ui.page == 0 ? 1 : 0;
  ui.page_since = millis();
  ui.force = true;
}

// The outdoor reading to use right now: PurpleAir if it's recent, otherwise AirNow
struct ActiveOutdoor {
  bool ok = false;        // a recent reading is available
  bool seen_any = false;  // any source has ever reported
  int aqi = -1;
  float pm25 = NAN;
  const char *source = "";
};
inline ActiveOutdoor active_outdoor(uint32_t now) {
  ActiveOutdoor a;
  const auto &pa = ui.outdoor[(int) OutdoorSource::PURPLEAIR];
  const auto &an = ui.outdoor[(int) OutdoorSource::AIRNOW];
  a.seen_any = pa.seen || an.seen;
  if (pa.seen && now - pa.ms < UiState::PURPLEAIR_STALE_MS) {
    a = {true, true, pa.aqi, pa.pm25, "PurpleAir"};
  } else if (an.seen && now - an.ms < UiState::AIRNOW_STALE_MS) {
    a = {true, true, an.aqi, an.pm25, "AirNow"};
  }
  return a;
}

inline SensorState sensor_state(uint32_t now) {
  if (!ui.seen_reading)
    return now < UiState::STARTUP_MS ? SensorState::WAITING : SensorState::MISSING;
  return now - ui.last_reading_ms > UiState::OFFLINE_MS ? SensorState::OFFLINE : SensorState::OK;
}

// Handles NaN while the sensor boots
inline std::string format_sensor(const char *format, float val) {
  if (std::isnan(val))
    return "--";
  char buf[32];
  snprintf(buf, sizeof(buf), format, val);
  return std::string(buf);
}

struct Frame {
  AQIAdvice advice;
  int indoor_aqi = -1;
  bool redraw = false;
};

// Works out the advice and what should be on screen. frame.redraw says whether it changed.
// The outdoor AQI in the header is PM2.5-based from PurpleAir, or AirNow's overall AQI (which
// also counts ozone) when PurpleAir isn't available.
inline Frame update_state(Readings r, esphome::ESPTime time, bool wifi_ok, bool fahrenheit) {
  using namespace aqi_limits;
  const uint32_t now = millis();
  Frame f;

  SensorState state = sensor_state(now);
  if (state != SensorState::OK)
    r = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};  // never show stale readings

  switch (state) {
    case SensorState::OFFLINE:
      f.advice = {"Offline", "Check Sensor", "No new readings", AdviceIcon::SENSOR_FAULT};
      break;
    case SensorState::MISSING:
      f.advice = {"No Sensor", "Check Wiring", "SEN66 not found", AdviceIcon::SENSOR_FAULT};
      break;
    default:
      f.advice = AQIAlgo::get_advice(r.co2, r.pm25, r.voc, r.humidity, active_outdoor(now).pm25);
      break;
  }
  f.indoor_aqi = us_aqi_from_pm25(r.pm25);

  if (ui.page == 1 && now - ui.page_since > UiState::TREND_PAGE_MS)
    ui.page = 0;

  DisplayModel m;
  m.page = ui.page;
  if (time.is_valid()) {
    m.date = time.strftime("%a %b %d");
    m.clock = time.strftime("%H:%M");
  } else {
    m.date = "----";
    m.clock = "--:--";
  }
  m.wifi_ok = wifi_ok;

  char buf[48];
  ActiveOutdoor outdoor = active_outdoor(now);
  if (outdoor.ok) {
    snprintf(buf, sizeof(buf), "Outside AQI %d · %s", outdoor.aqi, us_aqi_category(outdoor.aqi));
    m.outdoor = buf;
  } else if (outdoor.seen_any) {
    m.outdoor = "Outside AQI --";
  }

  m.advice = f.advice;
  if (f.indoor_aqi >= 0) {
    snprintf(buf, sizeof(buf), "US AQI %d · %s", f.indoor_aqi, us_aqi_category(f.indoor_aqi));
    m.aqi_line = buf;
  } else {
    m.aqi_line = "US AQI --";
  }

  auto tile = [](const char *label, std::string unit, std::string value, bool alert, Trend trend = Trend::FLAT) {
    Tile t;
    t.label = label;
    t.unit = std::move(unit);
    t.value = std::move(value);
    t.alert = alert;
    t.trend = trend;
    return t;
  };
  const bool ok = state == SensorState::OK;
  float temperature = fahrenheit ? r.temperature * 9.0f / 5.0f + 32.0f : r.temperature;
  m.tiles[0] = tile("CO2", "ppm", format_sensor("%.0f", r.co2), r.co2 > CO2_ACT,
                    ok ? ui.history.trend(true) : Trend::FLAT);
  m.tiles[1] = tile("Temp", fahrenheit ? "°F" : "°C", format_sensor("%.1f", temperature), false);
  m.tiles[2] = tile("Humidity", "%", format_sensor("%.0f", r.humidity),
                    r.humidity < HUMIDITY_LOW || r.humidity > HUMIDITY_HIGH);
  m.tiles[3] = tile("PM1", "µg/m³", format_sensor("%.1f", r.pm1), false);
  m.tiles[4] = tile("PM2.5", "µg/m³", format_sensor("%.1f", r.pm25), r.pm25 > PM25_ACT,
                    ok ? ui.history.trend(false) : Trend::FLAT);
  m.tiles[5] = tile("PM4", "µg/m³", format_sensor("%.1f", r.pm4), false);
  m.tiles[6] = tile("PM10", "µg/m³", format_sensor("%.1f", r.pm10), r.pm10 > PM10_ACT);
  m.tiles[7] = tile("VOC", "index", format_sensor("%.0f", r.voc), r.voc >= VOC_ACT);
  m.tiles[8] = tile("NOx", "index", format_sensor("%.0f", r.nox), r.nox >= NOX_ACT);
  m.history_version = ui.history.version();

  std::string sig = m.signature();
  f.redraw = ui.force || !ui.has_model || sig != ui.signature;
  if (f.redraw) {
    ui.model = m;
    ui.signature = sig;
    ui.has_model = true;
    ui.force = false;
  }
  return f;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
struct Icons {
  esphome::image::Image *ok, *ventilate, *purifier, *keep_closed, *too_dry, *too_humid, *waiting, *fault, *wifi_off;
};

inline esphome::image::Image *icon_for(const Icons &icons, AdviceIcon icon) {
  switch (icon) {
    case AdviceIcon::VENTILATE:
      return icons.ventilate;
    case AdviceIcon::PURIFIER:
      return icons.purifier;
    case AdviceIcon::KEEP_CLOSED:
      return icons.keep_closed;
    case AdviceIcon::TOO_DRY:
      return icons.too_dry;
    case AdviceIcon::TOO_HUMID:
      return icons.too_humid;
    case AdviceIcon::WAITING:
      return icons.waiting;
    case AdviceIcon::SENSOR_FAULT:
      return icons.fault;
    default:
      return icons.ok;
  }
}

inline void draw_header(esphome::display::Display &it, const DisplayModel &m, const Icons &icons) {
  using esphome::display::TextAlign;
  const int y = layout::HEADER_TEXT_Y;
  it.print(layout::MARGIN, y, montserrat_24, INK, TextAlign::CENTER_LEFT, m.date.c_str());
  it.print(layout::WIDTH / 2, y, montserrat_24, INK, TextAlign::CENTER, m.outdoor.c_str());
  it.print(layout::WIDTH - layout::MARGIN, y, montserrat_24, INK, TextAlign::CENTER_RIGHT, m.clock.c_str());
  if (!m.wifi_ok) {
    // Wi-Fi-off icon just left of the clock
    int x1, y1, w, h;
    it.get_text_bounds(layout::WIDTH - layout::MARGIN, y, m.clock.c_str(), montserrat_24, TextAlign::CENTER_RIGHT, &x1,
                       &y1, &w, &h);
    it.image(x1 - 10, y, icons.wifi_off, esphome::display::ImageAlign::CENTER_RIGHT, INK, PAPER);
  }
  it.filled_rectangle(layout::MARGIN, layout::HEADER_HEIGHT - 3, layout::WIDTH - 2 * layout::MARGIN, 3, INK);
}

inline void draw_trend_arrow(esphome::display::Display &it, int x, int cy, Trend trend, Color color) {
  if (trend == Trend::UP)
    it.filled_triangle(x, cy + 8, x + 16, cy + 8, x + 8, cy - 9, color);
  else if (trend == Trend::DOWN)
    it.filled_triangle(x, cy - 8, x + 16, cy - 8, x + 8, cy + 9, color);
}

// One cell of the readings grid. Readings past their limit are drawn inverted.
inline void draw_tile(esphome::display::Display &it, int col, int row, const Tile &t) {
  using esphome::display::TextAlign;
  const int x = layout::GRID_X + col * layout::TILE_WIDTH;
  const int y = layout::GRID_Y + row * layout::TILE_HEIGHT;
  const int cx = x + layout::TILE_WIDTH / 2;
  const int cy = y + layout::TILE_HEIGHT / 2 + 4;
  Color fg = INK;
  if (t.alert) {
    it.filled_rectangle(x + 4, y + 4, layout::TILE_WIDTH - 8, layout::TILE_HEIGHT - 8, INK);
    fg = PAPER;
  }

  it.print(x + layout::TILE_PADDING, y + layout::TILE_PADDING, montserrat_20, fg, TextAlign::TOP_LEFT, t.label);
  // Leave room for the trend arrow to the right of the value
  const int value_x = t.trend == Trend::FLAT ? cx : cx - 12;
  it.print(value_x, cy, montserrat_48, fg, TextAlign::CENTER, t.value.c_str());
  if (t.trend != Trend::FLAT) {
    int x1, y1, w, h;
    it.get_text_bounds(value_x, cy, t.value.c_str(), montserrat_48, TextAlign::CENTER, &x1, &y1, &w, &h);
    draw_trend_arrow(it, x1 + w + 8, cy, t.trend, fg);
  }
  it.print(cx, y + layout::TILE_HEIGHT - layout::TILE_PADDING, montserrat_20, fg, TextAlign::BOTTOM_CENTER,
           t.unit.c_str());
}

inline void draw_main_page(esphome::display::Display &it, const DisplayModel &m, const Icons &icons) {
  using esphome::display::TextAlign;
  const int cx = layout::LEFT_CENTER;

  // Left panel: icon + recommendation + US AQI
  it.image(cx, layout::HEADER_HEIGHT + 14, icon_for(icons, m.advice.icon), esphome::display::ImageAlign::TOP_CENTER,
           INK, PAPER);
  it.print(cx, 214, montserrat_48, INK, TextAlign::TOP_CENTER, m.advice.status.c_str());
  it.print(cx, 278, montserrat_36, INK, TextAlign::TOP_CENTER, m.advice.action.c_str());
  it.print(cx, 330, montserrat_24, INK, TextAlign::TOP_CENTER, m.advice.reason.c_str());
  it.line(cx - 110, 388, cx + 110, 388, INK);
  it.print(cx, 402, montserrat_24, INK, TextAlign::TOP_CENTER, m.aqi_line.c_str());

  // Divider between the panels
  it.filled_rectangle(layout::LEFT_WIDTH, layout::HEADER_HEIGHT + 16, 3,
                      layout::HEIGHT - layout::HEADER_HEIGHT - 32, INK);

  // Grid lines
  const int grid_right = layout::GRID_X + layout::GRID_COLS * layout::TILE_WIDTH;
  const int grid_bottom = layout::GRID_Y + layout::GRID_ROWS * layout::TILE_HEIGHT;
  for (int r = 1; r < layout::GRID_ROWS; r++) {
    const int y = layout::GRID_Y + r * layout::TILE_HEIGHT;
    it.line(layout::GRID_X + layout::TILE_PADDING, y, grid_right - layout::TILE_PADDING, y, INK);
  }
  for (int c = 1; c < layout::GRID_COLS; c++) {
    const int x = layout::GRID_X + c * layout::TILE_WIDTH;
    it.line(x, layout::GRID_Y + layout::TILE_PADDING, x, grid_bottom - layout::TILE_PADDING, INK);
  }

  for (int i = 0; i < 9; i++)
    draw_tile(it, i % 3, i / 3, m.tiles[i]);
}

inline void dotted_line(esphome::display::Display &it, int x1, int y1, int x2, int y2, int step) {
  const int n = std::max(std::abs(x2 - x1), std::abs(y2 - y1)) / step;
  for (int i = 0; i <= n; i++) {
    const int x = x1 + (x2 - x1) * i / std::max(n, 1);
    const int y = y1 + (y2 - y1) * i / std::max(n, 1);
    it.draw_pixel_at(x, y, INK);
  }
}

// One 24-hour chart. `co2` picks which series to plot.
inline void draw_chart(esphome::display::Display &it, int x0, int y0, int w, int h, bool co2, const char *title,
                       const char *unit, const char *fmt, float limit, float min_span, float floor_value,
                       float round_to) {
  using esphome::display::TextAlign;
  const ReadingHistory &hist = ui.history;
  const int n = std::min(hist.points(), ReadingHistory::SLOTS);

  float lo = NAN, hi = NAN, latest = NAN;
  int valid = 0;
  for (int age = 0; age < n; age++) {
    float c, p;
    hist.point(age, c, p);
    float v = co2 ? c : p;
    if (std::isnan(v))
      continue;
    if (std::isnan(latest))
      latest = v;
    lo = std::isnan(lo) ? v : std::min(lo, v);
    hi = std::isnan(hi) ? v : std::max(hi, v);
    valid++;
  }

  char buf[96];
  it.print(x0, y0 - 8, montserrat_24, INK, TextAlign::BOTTOM_LEFT, title);
  if (valid > 0) {
    char lo_s[16], hi_s[16], now_s[16];
    snprintf(lo_s, sizeof(lo_s), fmt, lo);
    snprintf(hi_s, sizeof(hi_s), fmt, hi);
    snprintf(now_s, sizeof(now_s), fmt, latest);
    snprintf(buf, sizeof(buf), "now %s   min %s   max %s %s", now_s, lo_s, hi_s, unit);
    it.print(x0 + w, y0 - 8, montserrat_20, INK, TextAlign::BOTTOM_RIGHT, buf);
  }
  it.rectangle(x0, y0, w, h, INK);
  for (int k = 1; k < 4; k++)  // every 6 hours
    dotted_line(it, x0 + w * k / 4, y0, x0 + w * k / 4, y0 + h, 4);

  if (valid < 2) {
    it.print(x0 + w / 2, y0 + h / 2, montserrat_20, INK, TextAlign::CENTER, "Collecting data...");
    return;
  }

  // Scale: rounded, never narrower than min_span, never below floor_value
  float vmin = std::floor(lo / round_to) * round_to;
  float vmax = std::ceil(hi / round_to) * round_to;
  if (vmax - vmin < min_span) {
    float mid = (vmin + vmax) / 2;
    vmin = std::floor((mid - min_span / 2) / round_to) * round_to;
    vmax = vmin + min_span;
  }
  if (vmin < floor_value) {
    vmax += floor_value - vmin;
    vmin = floor_value;
  }
  snprintf(buf, sizeof(buf), fmt, vmax);
  it.print(x0 - 8, y0, montserrat_20, INK, TextAlign::TOP_RIGHT, buf);
  snprintf(buf, sizeof(buf), fmt, vmin);
  it.print(x0 - 8, y0 + h, montserrat_20, INK, TextAlign::BOTTOM_RIGHT, buf);

  auto to_y = [&](float v) { return y0 + h - 2 - (int) std::lround((v - vmin) / (vmax - vmin) * (h - 4)); };
  auto to_x = [&](int age) { return x0 + w - 2 - (w - 4) * age / (ReadingHistory::SLOTS - 1); };

  // Dashed line at the limit where the advice kicks in
  if (limit > vmin && limit < vmax) {
    const int ly = to_y(limit);
    for (int x = x0 + 2; x < x0 + w - 2; x += 12)
      it.line(x, ly, std::min(x + 6, x0 + w - 2), ly, INK);
    snprintf(buf, sizeof(buf), fmt, limit);
    it.print(x0 + 8, ly - 3, montserrat_20, INK, TextAlign::BOTTOM_LEFT, buf);
  }

  // The data, 2 px thick, with gaps where there were no readings
  bool have_prev = false;
  int px = 0, py = 0;
  for (int age = n - 1; age >= 0; age--) {
    float c, p;
    hist.point(age, c, p);
    float v = co2 ? c : p;
    if (std::isnan(v)) {
      have_prev = false;
      continue;
    }
    const int x = to_x(age), y = to_y(v);
    if (have_prev) {
      it.line(px, py, x, y, INK);
      it.line(px, py + 1, x, y + 1, INK);
    } else {
      it.filled_rectangle(x, y, 2, 2, INK);
    }
    px = x;
    py = y;
    have_prev = true;
  }
}

inline void draw_trend_page(esphome::display::Display &it) {
  using esphome::display::TextAlign;
  const int x0 = 90, w = 694, h = 140;
  draw_chart(it, x0, 100, w, h, true, "CO2, last 24 h", "ppm", "%.0f", aqi_limits::CO2_ACT, 200, 400, 100);
  draw_chart(it, x0, 296, w, h, false, "PM2.5, last 24 h", "µg/m³", "%.0f", aqi_limits::PM25_ACT, 10, 0, 5);
  static const char *const LABELS[] = {"-24h", "-18h", "-12h", "-6h", "now"};
  for (int k = 0; k <= 4; k++) {
    TextAlign align = k == 0 ? TextAlign::TOP_LEFT : (k == 4 ? TextAlign::TOP_RIGHT : TextAlign::TOP_CENTER);
    it.print(x0 + w * k / 4, 296 + h + 6, montserrat_20, INK, align, LABELS[k]);
  }
}

// The display lambda calls this
inline void draw_screen(esphome::display::Display &it, const Icons &icons) {
  const DisplayModel &m = ui.model;
  it.fill(PAPER);
  draw_header(it, m, icons);
  if (m.page == 1)
    draw_trend_page(it);
  else
    draw_main_page(it, m, icons);
}
