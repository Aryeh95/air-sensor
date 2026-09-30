#pragma once
#include "esphome.h"

// Layout for the GDEY0426T82 4.26" panel: 800x480, landscape.
//
// +--------------------------------------------------------------+
// | Date                     Air Quality                    Time |
// +--------------------+-----------------------------------------+
// |                    |  CO2        |  Temp       |  Humidity   |
// |       [icon]       |-------------+-------------+-------------|
// |       Status       |  PM1        |  PM2.5      |  PM4        |
// |       Action       |-------------+-------------+-------------|
// |       Reason       |  PM10       |  VOC        |  NOx        |
// +--------------------+-----------------------------------------+

// The fonts used below (montserrat_20/24/36/48) are the ids from the YAML; ESPHome declares
// them in main.cpp before this header is included.

namespace layout {
static const int WIDTH = 800;
static const int HEIGHT = 480;
static const int MARGIN = 16;
static const int HEADER_HEIGHT = 50;

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

// Handles NaN while the sensor boots
inline std::string format_sensor(const char *format, float val) {
    if (std::isnan(val)) {
        return "--";  // Simply return dashes for booting/NaN sensors
    }
    char buf[32];
    snprintf(buf, sizeof(buf), format, val);
    return std::string(buf);
}

// One cell of the readings grid: label top-left, large value in the middle, unit underneath
inline void draw_tile(esphome::display::Display &it, int col, int row, const char *label, const char *unit,
                      const std::string &value, Color ink) {
    const int x = layout::GRID_X + col * layout::TILE_WIDTH;
    const int y = layout::GRID_Y + row * layout::TILE_HEIGHT;
    const int center_x = x + layout::TILE_WIDTH / 2;

    it.print(x + layout::TILE_PADDING, y + layout::TILE_PADDING, montserrat_20, ink,
             esphome::display::TextAlign::TOP_LEFT, label);
    it.print(center_x, y + layout::TILE_HEIGHT / 2 + 4, montserrat_48, ink, esphome::display::TextAlign::CENTER,
             value.c_str());
    it.print(center_x, y + layout::TILE_HEIGHT - layout::TILE_PADDING, montserrat_20, ink,
             esphome::display::TextAlign::BOTTOM_CENTER, unit);
}

// Note: We pass the display buffer 'it' by reference (&)
void draw_main_screen(esphome::display::Display &it, esphome::image::Image *icon_image,
                      esphome::ESPTime current_time, const std::string &aqi_status, const std::string &aqi_action,
                      const std::string &aqi_reason, float temp, float hum, float pm10, float pm25, float pm40,
                      float pm100, float co2, float voc, float nox) {
    // Black ink on a white background
    Color background_color = Color::WHITE;
    Color primary_color = Color::BLACK;

    it.fill(background_color);

    // Header
    const int header_text_y = (layout::HEADER_HEIGHT - 4) / 2;
    if (current_time.is_valid()) {
        // strftime takes: X, Y, Font, Color, Align, Format string, Time object
        it.strftime(layout::MARGIN, header_text_y, montserrat_24, primary_color,
                    esphome::display::TextAlign::CENTER_LEFT, "%a %b %d", current_time);
        it.strftime(layout::WIDTH - layout::MARGIN, header_text_y, montserrat_24, primary_color,
                    esphome::display::TextAlign::CENTER_RIGHT, "%H:%M", current_time);
    } else {
        // Fallback for before time sync is complete
        it.print(layout::MARGIN, header_text_y, montserrat_24, primary_color,
                 esphome::display::TextAlign::CENTER_LEFT, "----");
        it.print(layout::WIDTH - layout::MARGIN, header_text_y, montserrat_24, primary_color,
                 esphome::display::TextAlign::CENTER_RIGHT, "--:--");
    }
    it.print(layout::WIDTH / 2, header_text_y, montserrat_24, primary_color, esphome::display::TextAlign::CENTER,
             "Air Quality");
    it.filled_rectangle(layout::MARGIN, layout::HEADER_HEIGHT - 3, layout::WIDTH - 2 * layout::MARGIN, 3,
                        primary_color);

    // Left panel: icon + recommendation
    it.image(layout::LEFT_CENTER, layout::HEADER_HEIGHT + 20, icon_image, esphome::display::ImageAlign::TOP_CENTER,
             primary_color, background_color);
    it.print(layout::LEFT_CENTER, 250, montserrat_48, primary_color, esphome::display::TextAlign::TOP_CENTER,
             aqi_status.c_str());
    it.print(layout::LEFT_CENTER, 318, montserrat_36, primary_color, esphome::display::TextAlign::TOP_CENTER,
             aqi_action.c_str());
    it.print(layout::LEFT_CENTER, 372, montserrat_24, primary_color, esphome::display::TextAlign::TOP_CENTER,
             aqi_reason.c_str());

    // Divider between the panels
    it.filled_rectangle(layout::LEFT_WIDTH, layout::HEADER_HEIGHT + 16, 3,
                        layout::HEIGHT - layout::HEADER_HEIGHT - 32, primary_color);

    // Right panel: grid lines
    const int grid_right = layout::GRID_X + layout::GRID_COLS * layout::TILE_WIDTH;
    const int grid_bottom = layout::GRID_Y + layout::GRID_ROWS * layout::TILE_HEIGHT;
    for (int r = 1; r < layout::GRID_ROWS; r++) {
        const int y = layout::GRID_Y + r * layout::TILE_HEIGHT;
        it.line(layout::GRID_X + layout::TILE_PADDING, y, grid_right - layout::TILE_PADDING, y, primary_color);
    }
    for (int c = 1; c < layout::GRID_COLS; c++) {
        const int x = layout::GRID_X + c * layout::TILE_WIDTH;
        it.line(x, layout::GRID_Y + layout::TILE_PADDING, x, grid_bottom - layout::TILE_PADDING, primary_color);
    }

    // Right panel: readings
    draw_tile(it, 0, 0, "CO2", "ppm", format_sensor("%.0f", co2), primary_color);
    draw_tile(it, 1, 0, "Temp", "°C", format_sensor("%.1f", temp), primary_color);
    draw_tile(it, 2, 0, "Humidity", "%", format_sensor("%.0f", hum), primary_color);

    draw_tile(it, 0, 1, "PM1", "µg/m³", format_sensor("%.1f", pm10), primary_color);
    draw_tile(it, 1, 1, "PM2.5", "µg/m³", format_sensor("%.1f", pm25), primary_color);
    draw_tile(it, 2, 1, "PM4", "µg/m³", format_sensor("%.1f", pm40), primary_color);

    draw_tile(it, 0, 2, "PM10", "µg/m³", format_sensor("%.1f", pm100), primary_color);
    draw_tile(it, 1, 2, "VOC", "index", format_sensor("%.0f", voc), primary_color);
    draw_tile(it, 2, 2, "NOx", "index", format_sensor("%.0f", nox), primary_color);
}
