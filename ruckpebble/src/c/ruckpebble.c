#include <pebble.h>
#include <stdlib.h>
#include <string.h>

#ifndef MESSAGE_KEY_sim_steps_enabled
#define MESSAGE_KEY_sim_steps_enabled 8
#define MESSAGE_KEY_sim_steps_spm 9
#endif

#define PROFILE_COUNT 3
#define PROFILE_NAME_MAX_LEN 33

typedef struct {
  int32_t ruck_weight_value;  // tenths
  int32_t terrain_factor;     // hundredths
  int32_t grade_percent;      // tenths
} ProfileSettings;

typedef struct {
  int32_t weight_value;       // tenths
  int32_t weight_unit;        // 0=kg, 1=lb
  int32_t ruck_weight_unit;   // 0=kg, 1=lb
  int32_t stride_value;       // tenths
  int32_t stride_unit;        // 0=cm, 1=in
  int32_t sim_steps_enabled;  // 0/1
  int32_t sim_steps_spm;      // steps/min
  int32_t active_profile;     // 0..PROFILE_COUNT-1
  ProfileSettings profiles[PROFILE_COUNT];
  char profile_names[PROFILE_COUNT][PROFILE_NAME_MAX_LEN];
} Settings;

enum {
  SETTINGS_PERSIST_KEY = 1
};

static const Settings SETTINGS_DEFAULTS = {
  .weight_value = 800,
  .weight_unit = 0,
  .ruck_weight_unit = 0,
  .stride_value = 780,
  .stride_unit = 0,
  .sim_steps_enabled = 1,
  .sim_steps_spm = 122,
  .active_profile = 0,
  .profiles = {
    { .ruck_weight_value = 136, .terrain_factor = 200, .grade_percent = 0 },
    { .ruck_weight_value = 80, .terrain_factor = 100, .grade_percent = 0 },
    { .ruck_weight_value = 120, .terrain_factor = 150, .grade_percent = 0 }
  },
  .profile_names = {
    "Two Mabels, offroad",
    "One Mabel, roads and tracks",
    ""
  }
};

static Window *s_profile_window;
static MenuLayer *s_profile_menu_layer;
static Window *s_window;
static Layer *s_grid_layer;
static TextLayer *s_top_time_layer;
static TextLayer *s_top_left_layer;
static TextLayer *s_top_right_layer;
static BitmapLayer *s_mid_left_icon_layer;
static TextLayer *s_mid_left_value_layer;
static BitmapLayer *s_mid_center_icon_layer;
static TextLayer *s_mid_center_value_layer;
static BitmapLayer *s_mid_right_icon_layer;
static TextLayer *s_mid_right_value_layer;
static BitmapLayer *s_bottom_left_icon_layer;
static TextLayer *s_bottom_left_value_layer;
static TextLayer *s_bottom_left_secondary_layer;
static BitmapLayer *s_bottom_right_icon_layer;
static TextLayer *s_bottom_right_value_layer;
static TextLayer *s_bottom_right_secondary_layer;
static GBitmap *s_runner_icon;
static GBitmap *s_heart_icon;
static GBitmap *s_timer_icon;
static GBitmap *s_steps_icon;
static GBitmap *s_fire_icon;
static GBitmap *s_profile_weight_icon;
static GBitmap *s_profile_terrain_icon;
static GBitmap *s_profile_grade_icon;

static Settings s_settings;
static time_t s_start_time;
static bool s_health_available = false;
static time_t s_day_start;
static int32_t s_steps_baseline = 0;
static int32_t s_last_steps = 0;
static time_t s_last_time = 0;
static int64_t s_speed_mmps = 0;

#define EMULATOR_TIME_SCALE 10

static int64_t prv_weight_to_kg1000(int32_t value_tenths, int32_t unit) {
  if (unit == 1) {
    return ((int64_t)value_tenths * 453592) / 10000;
  }
  return (int64_t)value_tenths * 100;
}

static int64_t prv_stride_to_mm(int32_t value_tenths, int32_t unit) {
  if (unit == 1) {
    return ((int64_t)value_tenths * 254) / 10;
  }
  return (int64_t)value_tenths;
}

static int32_t prv_active_profile_index(void) {
  if (s_settings.active_profile < 0 || s_settings.active_profile >= PROFILE_COUNT) {
    return 0;
  }
  return s_settings.active_profile;
}

static void prv_set_profile_name(int32_t profile_index, const char *name) {
  if (profile_index < 0 || profile_index >= PROFILE_COUNT) {
    return;
  }
  if (!name) {
    s_settings.profile_names[profile_index][0] = '\0';
    return;
  }
  strncpy(s_settings.profile_names[profile_index], name, PROFILE_NAME_MAX_LEN - 1);
  s_settings.profile_names[profile_index][PROFILE_NAME_MAX_LEN - 1] = '\0';
}

static ProfileSettings *prv_active_profile(void) {
  return &s_settings.profiles[prv_active_profile_index()];
}

static int64_t prv_grade_q(void) {
  return (int64_t)prv_active_profile()->grade_percent * 10;
}

static int64_t prv_isqrt(int64_t x) {
  int64_t op = x;
  int64_t res = 0;
  int64_t one = (int64_t)1 << 62;

  while (one > op) {
    one >>= 2;
  }
  while (one != 0) {
    if (op >= res + one) {
      op -= res + one;
      res = (res >> 1) + one;
    } else {
      res >>= 1;
    }
    one >>= 2;
  }
  return res;
}

static int64_t prv_pandolf_metabolic_mw(int64_t weight_kg1000, int64_t load_kg1000, int64_t speed_mmps) {
  if (weight_kg1000 <= 0) {
    return 0;
  }
  int64_t total = weight_kg1000 + load_kg1000;
  int64_t ratio_q = (load_kg1000 * 1000000) / weight_kg1000;
  int64_t ratio_sq_q = (ratio_q * ratio_q) / 1000000;
  int64_t term1 = (weight_kg1000 * 3) / 2;
  int64_t term2 = (2 * total * ratio_sq_q) / 1000000;

  int64_t v_q = speed_mmps;          // m/s * 1000
  int64_t v2_q = v_q * v_q;          // scale 1e6
  int64_t termA_q = (v2_q * 3) / 2;  // scale 1e6
  int64_t G_q = prv_grade_q();       // scale 1e4
  int64_t termB_q = v_q * G_q;       // scale 1e7
  termB_q = (termB_q * 35) / 100;    // scale 1e7
  termB_q = termB_q / 10;            // scale 1e6
  int64_t inner_q = termA_q + termB_q;
  int64_t mu_q = prv_active_profile()->terrain_factor; // scale 1e2
  int64_t term3_base = (total * inner_q * mu_q) / (100 * 1000000);

  int64_t v2_03_q = (v2_q * 3) / 10;           // scale 1e6
  int64_t sqrt_03_v2_q = prv_isqrt(v2_03_q);   // scale 1e3
  int64_t sqrt_term_q = (sqrt_03_v2_q * 1000) / 7; // scale 1e6

  int64_t v_lr_q = (v_q * ratio_q) / 1000000;  // scale 1e3
  int64_t vl_term_q = (v_lr_q * v_lr_q) / 4;   // scale 1e6

  int64_t mult_base_q = 1000000 + sqrt_term_q + vl_term_q;
  int64_t mult_q = (mult_base_q * 11) / 10;    // scale 1e6
  int64_t term3 = (term3_base * mult_q) / 1000000;
  (void)mult_q;

  return term1 + term2 + term3;
}

// ACSM walking estimate (no ruck adjustment): kcal/hour from bodyweight, speed, and grade.
static int64_t prv_walking_kcal_per_hour(int64_t weight_kg1000, int64_t speed_mmps) {
  // speed in m/min, Q1000
  int64_t speed_m_min_q1000 = speed_mmps * 60;
  int64_t grade_q1000 = prv_active_profile()->grade_percent; // tenths of percent maps to grade fraction * 1000

  // VO2 in ml/kg/min, Q1000: 3.5 + 0.1*S + 1.8*S*G
  int64_t vo2_q1000 = 3500;
  vo2_q1000 += speed_m_min_q1000 / 10;
  vo2_q1000 += (speed_m_min_q1000 * grade_q1000 * 1800) / 1000000;
  if (vo2_q1000 < 0) {
    vo2_q1000 = 0;
  }

  // kcal/h = VO2 * kg * 60 / 200
  return (vo2_q1000 * weight_kg1000 * 3) / 10000000;
}

static void prv_set_text_style(TextLayer *layer, GFont font, GTextAlignment align, GColor color) {
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, color);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, align);
}

static void prv_grid_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int w = bounds.size.w;
  int h = bounds.size.h;

  int y_top = 32;
  int y_mid = 76;
  int y_bottom = 142;

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(8, y_top), GPoint(w - 8, y_top));
  graphics_draw_line(ctx, GPoint(8, y_mid), GPoint(w - 8, y_mid));
  graphics_draw_line(ctx, GPoint(8, y_bottom), GPoint(w - 8, y_bottom));
  graphics_draw_line(ctx, GPoint(w / 2, y_top), GPoint(w / 2, y_mid));
  graphics_draw_line(ctx, GPoint(w / 2, y_bottom), GPoint(w / 2, h - 38));
}

static void prv_load_settings(void) {
  s_settings = SETTINGS_DEFAULTS;
  if (persist_exists(SETTINGS_PERSIST_KEY)) {
    persist_read_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
  }
}

static void prv_save_settings(void) {
  persist_write_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
}

static void prv_update_display(void) {
  if (!s_top_time_layer) {
    return;
  }
  time_t now = time(NULL);
  int64_t elapsed_real_s = (int64_t)(now - s_start_time);
  if (elapsed_real_s < 1) {
    elapsed_real_s = 1;
  }
  int64_t elapsed_s = elapsed_real_s;
  if (s_settings.sim_steps_enabled) {
    elapsed_s *= EMULATOR_TIME_SCALE;
  }

  int32_t steps = 0;
  int32_t steps_total_day = 0;
  if (s_health_available) {
    steps_total_day = (int32_t)health_service_sum(HealthMetricStepCount, s_day_start, now);
    if (steps_total_day < 0) {
      steps_total_day = 0;
    }
  }
  if (s_settings.sim_steps_enabled) {
    steps = (int32_t)((elapsed_s * (int64_t)s_settings.sim_steps_spm) / 60);
    if (s_health_available) {
      steps_total_day = s_steps_baseline + steps;
    } else {
      steps_total_day = steps;
    }
  } else if (s_health_available) {
    steps = steps_total_day - s_steps_baseline;
    if (steps < 0) {
      steps = 0;
    }
  }

  int64_t stride_mm = prv_stride_to_mm(s_settings.stride_value, s_settings.stride_unit);
  int64_t distance_mm = (int64_t)steps * stride_mm;
  if (s_last_time == 0) {
    s_last_time = now;
    s_last_steps = steps;
  }
  int64_t speed_mmps = s_speed_mmps;
  int64_t delta_s = (int64_t)(now - s_last_time);
  int64_t delta_scaled_s = delta_s;
  if (s_settings.sim_steps_enabled) {
    delta_scaled_s *= EMULATOR_TIME_SCALE;
  }
  if (delta_scaled_s >= 5) {
    int32_t delta_steps = steps - s_last_steps;
    if (delta_steps < 0) {
      delta_steps = 0;
    }
    speed_mmps = (int64_t)delta_steps * stride_mm / delta_scaled_s;
    s_last_time = now;
    s_last_steps = steps;
    s_speed_mmps = speed_mmps;
  }
  if (speed_mmps > 5000) {
    speed_mmps = 5000;
  }

  bool use_imperial = (s_settings.weight_unit == 1);
  int64_t unit_mm = use_imperial ? 1609344 : 1000000;
  const char *distance_unit_label = use_imperial ? "mi" : "km";
  int64_t distance_x100 = (distance_mm * 100) / unit_mm;

  int64_t pace_sec = 0;
  if (distance_mm > 0) {
    pace_sec = (elapsed_s * unit_mm) / distance_mm;
  }

  ProfileSettings *profile = prv_active_profile();
  int64_t weight_kg1000 = prv_weight_to_kg1000(s_settings.weight_value, s_settings.weight_unit);
  int64_t load_kg1000 = prv_weight_to_kg1000(profile->ruck_weight_value, s_settings.ruck_weight_unit);
  int64_t metabolic_mw = prv_pandolf_metabolic_mw(weight_kg1000, load_kg1000, speed_mmps);
  int64_t ruck_kcal_per_hour = (metabolic_mw * 3600) / 4184 / 1000;
  int64_t ruck_kcal_total = (ruck_kcal_per_hour * elapsed_s) / 3600;
  int64_t walk_kcal_per_hour = prv_walking_kcal_per_hour(weight_kg1000, speed_mmps);
  int64_t walk_kcal_total = (walk_kcal_per_hour * elapsed_s) / 3600;

  static char top_time_buf[16];
  static char top_left_buf[24];
  static char top_right_buf[24];
  static char pace_value_buf[16];
  static char hr_value_buf[16];
  static char timer_value_buf[16];
  static char steps_value_buf[16];
  static char steps_total_value_buf[16];
  static char calories_value_buf[16];
  static char calories_walk_value_buf[16];

  struct tm *now_tm = localtime(&now);
  if (now_tm) {
    strftime(top_time_buf, sizeof(top_time_buf), clock_is_24h_style() ? "%H:%M" : "%I:%M", now_tm);
  } else {
    snprintf(top_time_buf, sizeof(top_time_buf), "--:--");
  }

  if (pace_sec > 0) {
    int pace_min = (int)(pace_sec / 60);
    int pace_rem = (int)(pace_sec % 60);
    snprintf(top_left_buf, sizeof(top_left_buf), "%d:%02d/%s",
             pace_min, pace_rem, distance_unit_label);
    snprintf(pace_value_buf, sizeof(pace_value_buf), "%d:%02d", pace_min, pace_rem);
  } else {
    snprintf(top_left_buf, sizeof(top_left_buf), "--:--/%s", distance_unit_label);
    snprintf(pace_value_buf, sizeof(pace_value_buf), "--:--");
  }

  snprintf(top_right_buf, sizeof(top_right_buf), "%ld.%02ld%s",
           (long)(distance_x100 / 100),
           (long)(distance_x100 % 100 < 0 ? -(distance_x100 % 100) : (distance_x100 % 100)),
           distance_unit_label);
  snprintf(timer_value_buf, sizeof(timer_value_buf), "%ld:%02ld",
           (long)(elapsed_s / 60), (long)(elapsed_s % 60));
  snprintf(steps_value_buf, sizeof(steps_value_buf), "%ld", (long)steps);
  snprintf(steps_total_value_buf, sizeof(steps_total_value_buf), "%ld", (long)steps_total_day);
  snprintf(calories_value_buf, sizeof(calories_value_buf), "%ld", (long)ruck_kcal_total);
  snprintf(calories_walk_value_buf, sizeof(calories_walk_value_buf), "%ld", (long)walk_kcal_total);

  if (health_service_metric_accessible(HealthMetricHeartRateBPM, now - 300, now)
      & HealthServiceAccessibilityMaskAvailable) {
    HealthValue heart_rate = health_service_peek_current_value(HealthMetricHeartRateBPM);
    if (heart_rate > 0) {
      snprintf(hr_value_buf, sizeof(hr_value_buf), "%ld", (long)heart_rate);
    } else {
      snprintf(hr_value_buf, sizeof(hr_value_buf), "--");
    }
  } else {
    snprintf(hr_value_buf, sizeof(hr_value_buf), "--");
  }

  text_layer_set_text(s_top_time_layer, top_time_buf);
  text_layer_set_text(s_top_left_layer, top_left_buf);
  text_layer_set_text(s_top_right_layer, top_right_buf);
  text_layer_set_text(s_mid_left_value_layer, pace_value_buf);
  text_layer_set_text(s_mid_center_value_layer, hr_value_buf);
  text_layer_set_text(s_mid_right_value_layer, timer_value_buf);
  text_layer_set_text(s_bottom_left_value_layer, steps_value_buf);
  text_layer_set_text(s_bottom_left_secondary_layer, steps_total_value_buf);
  text_layer_set_text(s_bottom_right_value_layer, calories_value_buf);
  text_layer_set_text(s_bottom_right_secondary_layer, calories_walk_value_buf);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_display();
}

static void prv_health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate || event == HealthEventSignificantUpdate) {
    prv_update_display();
  }
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_weight_value);
  if (t) {
    s_settings.weight_value = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_weight_unit);
  if (t) {
    s_settings.weight_unit = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_ruck_weight_unit);
  if (t) {
    s_settings.ruck_weight_unit = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_stride_length_value);
  if (t) {
    s_settings.stride_value = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_stride_length_unit);
  if (t) {
    s_settings.stride_unit = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile1_ruck_weight_value);
  if (t) {
    s_settings.profiles[0].ruck_weight_value = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile1_terrain_factor);
  if (t) {
    s_settings.profiles[0].terrain_factor = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile1_grade_percent);
  if (t) {
    s_settings.profiles[0].grade_percent = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile2_ruck_weight_value);
  if (t) {
    s_settings.profiles[1].ruck_weight_value = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile2_terrain_factor);
  if (t) {
    s_settings.profiles[1].terrain_factor = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile2_grade_percent);
  if (t) {
    s_settings.profiles[1].grade_percent = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile3_ruck_weight_value);
  if (t) {
    s_settings.profiles[2].ruck_weight_value = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile3_terrain_factor);
  if (t) {
    s_settings.profiles[2].terrain_factor = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile3_grade_percent);
  if (t) {
    s_settings.profiles[2].grade_percent = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_profile1_name);
  if (t && t->type == TUPLE_CSTRING) {
    prv_set_profile_name(0, t->value->cstring);
  }
  t = dict_find(iter, MESSAGE_KEY_profile2_name);
  if (t && t->type == TUPLE_CSTRING) {
    prv_set_profile_name(1, t->value->cstring);
  }
  t = dict_find(iter, MESSAGE_KEY_profile3_name);
  if (t && t->type == TUPLE_CSTRING) {
    prv_set_profile_name(2, t->value->cstring);
  }
  t = dict_find(iter, MESSAGE_KEY_sim_steps_enabled);
  if (t) {
    s_settings.sim_steps_enabled = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_sim_steps_spm);
  if (t) {
    s_settings.sim_steps_spm = t->value->int32;
  }

  prv_save_settings();
  if (s_profile_menu_layer) {
    menu_layer_reload_data(s_profile_menu_layer);
  }
  prv_update_display();
}

static void prv_start_session(void) {
  s_start_time = time(NULL);
  s_last_time = 0;
  s_last_steps = 0;
  s_speed_mmps = 0;
  if (s_health_available) {
    s_steps_baseline = (int32_t)health_service_sum(HealthMetricStepCount, s_day_start, s_start_time);
  } else {
    s_steps_baseline = 0;
  }
}

static uint16_t prv_profile_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  (void)menu_layer;
  (void)section_index;
  (void)context;
  return PROFILE_COUNT;
}

static int16_t prv_profile_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  (void)cell_index;
  (void)context;
  GRect bounds = layer_get_bounds(menu_layer_get_layer(menu_layer));
  return bounds.size.h / PROFILE_COUNT;
}

static void prv_profile_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  (void)context;
  int row = (int)cell_index->row;
  if (row >= PROFILE_COUNT) {
    return;
  }
  ProfileSettings *p = &s_settings.profiles[row];
  static char legacy_title[16];
  static char weight_value[8];
  static char terrain_value[8];
  static char grade_value[8];
  const char *title_text = legacy_title;
  const int16_t y = 0;
  const int16_t row_h = layer_get_bounds((Layer *)cell_layer).size.h;
  const int16_t value_y = row_h - 26;
  const int16_t icon_size = 20;
  const GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const GFont value_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  bool is_highlighted = menu_cell_layer_is_highlighted(cell_layer);
  GColor bg = is_highlighted ? GColorDarkGray : GColorBlack;

  if (s_settings.profile_names[row][0] != '\0') {
    title_text = s_settings.profile_names[row];
  } else if (row == 0) {
    title_text = "Two Mabels, offroad";
  } else if (row == 1) {
    title_text = "One Mabel, roads and tracks";
  } else {
    snprintf(legacy_title, sizeof(legacy_title), "Profile %d", row + 1);
    title_text = legacy_title;
  }
  snprintf(weight_value, sizeof(weight_value), "%ld.%ld",
           (long)(p->ruck_weight_value / 10), (long)labs(p->ruck_weight_value % 10));
  snprintf(terrain_value, sizeof(terrain_value), "%ld.%02ld",
           (long)(p->terrain_factor / 100), (long)labs(p->terrain_factor % 100));
  snprintf(grade_value, sizeof(grade_value), "%ld.%ld",
           (long)(p->grade_percent / 10), (long)labs(p->grade_percent % 10));

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, layer_get_bounds((Layer *)cell_layer), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, title_text, title_font, GRect(6, y + 2, 138, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  if (s_profile_weight_icon) {
    graphics_draw_bitmap_in_rect(ctx, s_profile_weight_icon, GRect(2, y + value_y + 2, icon_size, icon_size));
  }
  if (s_profile_terrain_icon) {
    graphics_draw_bitmap_in_rect(ctx, s_profile_terrain_icon, GRect(50, y + value_y + 2, icon_size, icon_size));
  }
  if (s_profile_grade_icon) {
    graphics_draw_bitmap_in_rect(ctx, s_profile_grade_icon, GRect(98, y + value_y + 2, icon_size, icon_size));
  }
  graphics_draw_text(ctx, weight_value, value_font, GRect(24, y + value_y + 3, 24, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, terrain_value, value_font, GRect(72, y + value_y + 3, 24, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, grade_value, value_font, GRect(120, y + value_y + 3, 24, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void prv_profile_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  (void)menu_layer;
  (void)context;
  if (cell_index->row >= PROFILE_COUNT) {
    return;
  }
  s_settings.active_profile = cell_index->row;
  prv_save_settings();
  prv_start_session();
  window_stack_remove(s_profile_window, true);
  prv_update_display();
}

static void prv_profile_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_profile_menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_profile_menu_layer, window);
  menu_layer_set_callbacks(s_profile_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_profile_get_num_rows_callback,
    .get_cell_height = prv_profile_get_cell_height_callback,
    .draw_row = prv_profile_draw_row_callback,
    .select_click = prv_profile_select_callback,
  });
  menu_layer_set_normal_colors(s_profile_menu_layer, GColorBlack, GColorWhite);
  menu_layer_set_highlight_colors(s_profile_menu_layer, GColorBlack, GColorWhite);
  s_profile_weight_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_WEIGHT);
  s_profile_terrain_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_TERRAIN);
  s_profile_grade_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_GRADE);
  menu_layer_set_selected_index(s_profile_menu_layer, (MenuIndex) { .section = 0, .row = prv_active_profile_index() },
                                MenuRowAlignCenter, false);
  layer_add_child(window_layer, menu_layer_get_layer(s_profile_menu_layer));
}

static void prv_profile_window_unload(Window *window) {
  (void)window;
  menu_layer_destroy(s_profile_menu_layer);
  s_profile_menu_layer = NULL;
  gbitmap_destroy(s_profile_weight_icon);
  gbitmap_destroy(s_profile_terrain_icon);
  gbitmap_destroy(s_profile_grade_icon);
  s_profile_weight_icon = NULL;
  s_profile_terrain_icon = NULL;
  s_profile_grade_icon = NULL;
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  int w = bounds.size.w;

  window_set_background_color(window, GColorBlack);

  s_grid_layer = layer_create(bounds);
  layer_set_update_proc(s_grid_layer, prv_grid_layer_update_proc);
  layer_add_child(window_layer, s_grid_layer);

  s_top_time_layer = text_layer_create(GRect(0, 0, w, 30));
  s_top_left_layer = text_layer_create(GRect(8, 36, (w / 2) - 10, 28));
  s_top_right_layer = text_layer_create(GRect((w / 2) + 2, 36, (w / 2) - 10, 28));

  s_mid_left_icon_layer = bitmap_layer_create(GRect(26, 80, 24, 24));
  s_mid_left_value_layer = text_layer_create(GRect(8, 100, 60, 36));
  s_mid_center_icon_layer = bitmap_layer_create(GRect((w / 2) - 12, 80, 24, 24));
  s_mid_center_value_layer = text_layer_create(GRect((w / 2) - 24, 102, 48, 30));
  s_mid_right_icon_layer = bitmap_layer_create(GRect(w - 50, 80, 24, 24));
  s_mid_right_value_layer = text_layer_create(GRect(w - 68, 100, 60, 36));

  s_bottom_left_icon_layer = bitmap_layer_create(GRect(38, 144, 24, 24));
  s_bottom_left_value_layer = text_layer_create(GRect(8, 162, (w / 2) - 12, 28));
  s_bottom_left_secondary_layer = text_layer_create(GRect(8, 188, (w / 2) - 12, 28));
  s_bottom_right_icon_layer = bitmap_layer_create(GRect((w / 2) + 34, 144, 24, 24));
  s_bottom_right_value_layer = text_layer_create(GRect((w / 2) + 4, 162, (w / 2) - 12, 28));
  s_bottom_right_secondary_layer = text_layer_create(GRect((w / 2) + 4, 188, (w / 2) - 12, 28));

  s_runner_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_RUNNER);
  s_heart_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_HEART);
  s_timer_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_TIMER);
  s_steps_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_STEPS);
  s_fire_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_FIRE);
  bitmap_layer_set_bitmap(s_mid_left_icon_layer, s_runner_icon);
  bitmap_layer_set_bitmap(s_mid_center_icon_layer, s_heart_icon);
  bitmap_layer_set_bitmap(s_mid_right_icon_layer, s_timer_icon);
  bitmap_layer_set_bitmap(s_bottom_left_icon_layer, s_steps_icon);
  bitmap_layer_set_bitmap(s_bottom_right_icon_layer, s_fire_icon);
  bitmap_layer_set_background_color(s_mid_left_icon_layer, GColorClear);
  bitmap_layer_set_background_color(s_mid_center_icon_layer, GColorClear);
  bitmap_layer_set_background_color(s_mid_right_icon_layer, GColorClear);
  bitmap_layer_set_background_color(s_bottom_left_icon_layer, GColorClear);
  bitmap_layer_set_background_color(s_bottom_right_icon_layer, GColorClear);

  prv_set_text_style(s_top_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_top_left_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_top_right_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_mid_left_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_mid_center_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_mid_right_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_bottom_left_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_bottom_left_secondary_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_bottom_right_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_bottom_right_secondary_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28), GTextAlignmentCenter, GColorWhite);

  text_layer_set_overflow_mode(s_bottom_right_value_layer, GTextOverflowModeWordWrap);
  text_layer_set_overflow_mode(s_bottom_right_secondary_layer, GTextOverflowModeWordWrap);
  text_layer_set_overflow_mode(s_top_left_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_overflow_mode(s_top_right_layer, GTextOverflowModeTrailingEllipsis);

  layer_add_child(window_layer, text_layer_get_layer(s_top_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_top_left_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_top_right_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_mid_left_icon_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_mid_left_value_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_mid_center_icon_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_mid_center_value_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_mid_right_icon_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_mid_right_value_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bottom_left_icon_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_bottom_left_value_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_bottom_left_secondary_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bottom_right_icon_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_bottom_right_value_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_bottom_right_secondary_layer));
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_grid_layer);
  text_layer_destroy(s_top_time_layer);
  text_layer_destroy(s_top_left_layer);
  text_layer_destroy(s_top_right_layer);
  bitmap_layer_destroy(s_mid_left_icon_layer);
  text_layer_destroy(s_mid_left_value_layer);
  bitmap_layer_destroy(s_mid_center_icon_layer);
  text_layer_destroy(s_mid_center_value_layer);
  bitmap_layer_destroy(s_mid_right_icon_layer);
  text_layer_destroy(s_mid_right_value_layer);
  bitmap_layer_destroy(s_bottom_left_icon_layer);
  text_layer_destroy(s_bottom_left_value_layer);
  text_layer_destroy(s_bottom_left_secondary_layer);
  bitmap_layer_destroy(s_bottom_right_icon_layer);
  text_layer_destroy(s_bottom_right_value_layer);
  text_layer_destroy(s_bottom_right_secondary_layer);
  gbitmap_destroy(s_runner_icon);
  gbitmap_destroy(s_heart_icon);
  gbitmap_destroy(s_timer_icon);
  gbitmap_destroy(s_steps_icon);
  gbitmap_destroy(s_fire_icon);
}

static void prv_init(void) {
  prv_load_settings();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  s_profile_window = window_create();
  window_set_window_handlers(s_profile_window, (WindowHandlers) {
    .load = prv_profile_window_load,
    .unload = prv_profile_window_unload,
  });

  time_t now = time(NULL);
  s_start_time = now;
  struct tm *start_tm = localtime(&now);
  if (start_tm) {
    start_tm->tm_hour = 0;
    start_tm->tm_min = 0;
    start_tm->tm_sec = 0;
    s_day_start = mktime(start_tm);
  } else {
    s_day_start = now;
  }
  HealthServiceAccessibilityMask access = health_service_metric_accessible(HealthMetricStepCount, now, now);
  s_health_available = (access & HealthServiceAccessibilityMaskAvailable);
  if (s_health_available) {
    health_service_events_subscribe(prv_health_handler, NULL);
  }

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(1024, 64);

  window_stack_push(s_window, false);
  window_stack_push(s_profile_window, true);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  if (s_health_available) {
    health_service_events_unsubscribe();
  }
  window_destroy(s_profile_window);
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
