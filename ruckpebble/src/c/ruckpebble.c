#include <pebble.h>
#include <stdlib.h>
#include <string.h>

#ifndef MESSAGE_KEY_sim_steps_enabled
#define MESSAGE_KEY_sim_steps_enabled 8
#define MESSAGE_KEY_sim_steps_spm 9
#endif

#define PROFILE_COUNT 3
#define PROFILE_NAME_MAX_LEN 33
#define TERRAIN_TYPE_MAX_LEN 16
#define SCREEN_PADDING 5
#define PROFILE_ROW_HEIGHT 62

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
  char profile_terrain_types[PROFILE_COUNT][TERRAIN_TYPE_MAX_LEN];
} Settings;

enum {
  SETTINGS_PERSIST_KEY = 1,
  LIFETIME_DISTANCE_M_PERSIST_KEY = 2,
  LIFETIME_CALORIES_PERSIST_KEY = 3
};

static const Settings SETTINGS_DEFAULTS = {
  .weight_value = 800,
  .weight_unit = 0,
  .ruck_weight_unit = 0,
  .stride_value = 780,
  .stride_unit = 0,
  .sim_steps_enabled = 0,
  .sim_steps_spm = 122,
  .active_profile = 0,
  .profiles = {
    { .ruck_weight_value = 136, .terrain_factor = 100, .grade_percent = 0 },
    { .ruck_weight_value = 80, .terrain_factor = 120, .grade_percent = 0 },
    { .ruck_weight_value = 136, .terrain_factor = 130, .grade_percent = 0 }
  },
  .profile_names = {
    "30lb, road",
    "15lb, trail",
    ""
  },
  .profile_terrain_types = {
    "road",
    "gravel",
    "mixed"
  }
};

static Window *s_profile_window;
static MenuLayer *s_profile_menu_layer;
static Window *s_music_window;
static MenuLayer *s_music_menu_layer;
static Window *s_window;
static Layer *s_grid_layer;
static TextLayer *s_top_time_layer;
static TextLayer *s_top_left_layer;
static TextLayer *s_top_right_layer;
static TextLayer *s_top_stats_right_layer;
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
static int16_t s_profile_cell_height = PROFILE_ROW_HEIGHT;

static Settings s_settings;
static time_t s_start_time;
static bool s_health_available = false;
static time_t s_day_start;
static int32_t s_steps_baseline = 0;
static int32_t s_last_steps = 0;
static time_t s_last_time = 0;
static int64_t s_speed_mmps = 0;
static int32_t s_session_distance_m = 0;
static int32_t s_session_calories = 0;
static int32_t s_lifetime_distance_m = 0;
static int32_t s_lifetime_calories = 0;
static bool s_session_totals_committed = false;

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

static int32_t prv_normalize_terrain_factor(int32_t terrain_factor_hundredths) {
  if (terrain_factor_hundredths <= 110) {
    return 100;
  }
  if (terrain_factor_hundredths <= 125) {
    return 120;
  }
  if (terrain_factor_hundredths <= 140) {
    return 130;
  }
  return 150;
}

static const char *prv_terrain_label_from_factor(int32_t terrain_factor_hundredths) {
  int32_t normalized = prv_normalize_terrain_factor(terrain_factor_hundredths);
  if (normalized == 100) {
    return "Road";
  }
  if (normalized == 120) {
    return "Gravel";
  }
  if (normalized == 130) {
    return "Mixed";
  }
  return "Sand";
}

static void prv_set_profile_terrain_type(int32_t profile_index, const char *terrain_type) {
  if (profile_index < 0 || profile_index >= PROFILE_COUNT) {
    return;
  }
  if (!terrain_type || terrain_type[0] == '\0') {
    s_settings.profile_terrain_types[profile_index][0] = '\0';
    return;
  }
  strncpy(s_settings.profile_terrain_types[profile_index], terrain_type, TERRAIN_TYPE_MAX_LEN - 1);
  s_settings.profile_terrain_types[profile_index][TERRAIN_TYPE_MAX_LEN - 1] = '\0';
}

static const char *prv_profile_terrain_label(int32_t profile_index, int32_t terrain_factor_hundredths) {
  if (profile_index >= 0 && profile_index < PROFILE_COUNT) {
    const char *terrain_type = s_settings.profile_terrain_types[profile_index];
    if (strcmp(terrain_type, "road") == 0) {
      return "Road";
    }
    if (strcmp(terrain_type, "gravel") == 0) {
      return "Gravel";
    }
    if (strcmp(terrain_type, "mixed") == 0) {
      return "Mixed";
    }
    if (strcmp(terrain_type, "sand") == 0) {
      return "Sand";
    }
    if (strcmp(terrain_type, "snow") == 0) {
      return "Snow";
    }
  }
  return prv_terrain_label_from_factor(terrain_factor_hundredths);
}

static const char *prv_profile_display_name(int32_t row, char *fallback, size_t fallback_size) {
  if (row >= 0 && row < PROFILE_COUNT && s_settings.profile_names[row][0] != '\0') {
    return s_settings.profile_names[row];
  }
  if (row == 0) {
    return "Two Mabels, offroad";
  }
  if (row == 1) {
    return "One Mabel, roads and tracks";
  }
  snprintf(fallback, fallback_size, "Profile %ld", (long)(row + 1));
  return fallback;
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

  int y_top = 56;
  int y_bottom = 137;

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(8, y_top), GPoint(w - 8, y_top));
  graphics_draw_line(ctx, GPoint(8, y_bottom), GPoint(w - 8, y_bottom));
  graphics_draw_line(ctx, GPoint(w / 2, 30), GPoint(w / 2, y_top));
  graphics_draw_line(ctx, GPoint(w / 2, y_bottom), GPoint(w / 2, h - 1));
}

static void prv_load_settings(void) {
  s_settings = SETTINGS_DEFAULTS;
  if (persist_exists(SETTINGS_PERSIST_KEY)) {
    persist_read_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
  }
  for (int i = 0; i < PROFILE_COUNT; ++i) {
    s_settings.profiles[i].terrain_factor = prv_normalize_terrain_factor(s_settings.profiles[i].terrain_factor);
    if (s_settings.profile_terrain_types[i][0] == '\0') {
      if (s_settings.profiles[i].terrain_factor == 100) {
        prv_set_profile_terrain_type(i, "road");
      } else if (s_settings.profiles[i].terrain_factor == 120) {
        prv_set_profile_terrain_type(i, "gravel");
      } else if (s_settings.profiles[i].terrain_factor == 130) {
        prv_set_profile_terrain_type(i, "mixed");
      } else {
        prv_set_profile_terrain_type(i, "sand");
      }
    }
  }
}

static void prv_save_settings(void) {
  persist_write_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
}

static void prv_send_lifetime_totals(void) {
  DictionaryIterator *iter = NULL;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed for totals: %d", (int)result);
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_lifetime_distance_m_total, s_lifetime_distance_m);
  dict_write_int32(iter, MESSAGE_KEY_lifetime_calories_total, s_lifetime_calories);
  dict_write_end(iter);
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed for totals: %d", (int)result);
  }
}

static void prv_commit_session_totals(const char *reason) {
  if (s_session_totals_committed) {
    return;
  }
  if (s_session_distance_m <= 0 && s_session_calories <= 0) {
    s_session_totals_committed = true;
    return;
  }
  int64_t lifetime_distance_m = (int64_t)s_lifetime_distance_m + s_session_distance_m;
  int64_t lifetime_calories = (int64_t)s_lifetime_calories + s_session_calories;
  if (lifetime_distance_m > INT32_MAX) {
    lifetime_distance_m = INT32_MAX;
  }
  if (lifetime_calories > INT32_MAX) {
    lifetime_calories = INT32_MAX;
  }
  s_lifetime_distance_m = (int32_t)lifetime_distance_m;
  s_lifetime_calories = (int32_t)lifetime_calories;
  persist_write_int(LIFETIME_DISTANCE_M_PERSIST_KEY, s_lifetime_distance_m);
  persist_write_int(LIFETIME_CALORIES_PERSIST_KEY, s_lifetime_calories);
  s_session_totals_committed = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "Session totals committed (%s): +%ld m +%ld kcal, lifetime=%ldm/%ldkcal",
          reason ? reason : "n/a",
          (long)s_session_distance_m, (long)s_session_calories,
          (long)s_lifetime_distance_m, (long)s_lifetime_calories);
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
  static char distance_buf[16];
  static char profile_name_buf[24];
  static char pace_header_buf[16];
  static char pace_value_buf[16];
  static char hr_value_buf[16];
  static char timer_value_buf[16];
  static char steps_value_buf[16];
  static char steps_total_value_buf[16];
  static char calories_value_buf[16];
  static char calories_walk_value_buf[16];

  const char *profile_name = prv_profile_display_name(prv_active_profile_index(), profile_name_buf, sizeof(profile_name_buf));
  struct tm *now_tm = localtime(&now);
  if (now_tm) {
    strftime(top_time_buf, sizeof(top_time_buf), clock_is_24h_style() ? "%H:%M" : "%I:%M", now_tm);
  } else {
    snprintf(top_time_buf, sizeof(top_time_buf), "--:--");
  }
  if (pace_sec > 0) {
    int pace_min = (int)(pace_sec / 60);
    int pace_rem = (int)(pace_sec % 60);
    snprintf(pace_header_buf, sizeof(pace_header_buf), "%d:%02d/%s", pace_min, pace_rem, distance_unit_label);
    snprintf(pace_value_buf, sizeof(pace_value_buf), "%d:%02d", pace_min, pace_rem);
  } else {
    snprintf(pace_header_buf, sizeof(pace_header_buf), "--:--/%s", distance_unit_label);
    snprintf(pace_value_buf, sizeof(pace_value_buf), "--:--");
  }
  snprintf(distance_buf, sizeof(distance_buf), "%ld.%02ld%s",
           (long)(distance_x100 / 100), (long)labs(distance_x100 % 100), distance_unit_label);
  snprintf(timer_value_buf, sizeof(timer_value_buf), "%ld:%02ld",
           (long)(elapsed_s / 60), (long)(elapsed_s % 60));
  snprintf(steps_value_buf, sizeof(steps_value_buf), "%ld", (long)steps);
  snprintf(steps_total_value_buf, sizeof(steps_total_value_buf), "%ld", (long)steps_total_day);
  snprintf(calories_value_buf, sizeof(calories_value_buf), "%ld", (long)ruck_kcal_total);
  snprintf(calories_walk_value_buf, sizeof(calories_walk_value_buf), "%ld", (long)walk_kcal_total);

  int64_t session_distance_m = distance_mm / 1000;
  if (session_distance_m < 0) {
    session_distance_m = 0;
  }
  if (session_distance_m > INT32_MAX) {
    session_distance_m = INT32_MAX;
  }
  s_session_distance_m = (int32_t)session_distance_m;
  if (ruck_kcal_total < 0) {
    ruck_kcal_total = 0;
  }
  if (ruck_kcal_total > INT32_MAX) {
    ruck_kcal_total = INT32_MAX;
  }
  s_session_calories = (int32_t)ruck_kcal_total;

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

  text_layer_set_text(s_top_time_layer, profile_name);
  text_layer_set_text(s_top_left_layer, top_time_buf);
  text_layer_set_text(s_top_right_layer, pace_header_buf);
  text_layer_set_text(s_top_stats_right_layer, distance_buf);
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
  (void)context;
  APP_LOG(APP_LOG_LEVEL_INFO, "Config inbox received");
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
    s_settings.profiles[0].terrain_factor = prv_normalize_terrain_factor(t->value->int32);
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
    s_settings.profiles[1].terrain_factor = prv_normalize_terrain_factor(t->value->int32);
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
    s_settings.profiles[2].terrain_factor = prv_normalize_terrain_factor(t->value->int32);
  }
  t = dict_find(iter, MESSAGE_KEY_profile1_terrain_type);
  if (t) {
    prv_set_profile_terrain_type(0, t->value->cstring);
  }
  t = dict_find(iter, MESSAGE_KEY_profile2_terrain_type);
  if (t) {
    prv_set_profile_terrain_type(1, t->value->cstring);
  }
  t = dict_find(iter, MESSAGE_KEY_profile3_terrain_type);
  if (t) {
    prv_set_profile_terrain_type(2, t->value->cstring);
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
  t = dict_find(iter, MESSAGE_KEY_request_lifetime_totals);
  if (t && t->value->int32 == 1) {
    prv_send_lifetime_totals();
  }

  prv_save_settings();
  APP_LOG(APP_LOG_LEVEL_INFO, "Config applied: active_profile=%ld", (long)s_settings.active_profile);
  if (s_profile_menu_layer) {
    menu_layer_reload_data(s_profile_menu_layer);
  }
  prv_update_display();
}

static void prv_inbox_dropped_handler(AppMessageResult reason, void *context) {
  (void)context;
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void prv_outbox_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  (void)failed;
  (void)context;
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

static void prv_start_session(void) {
  s_start_time = time(NULL);
  s_last_time = 0;
  s_last_steps = 0;
  s_speed_mmps = 0;
  s_session_distance_m = 0;
  s_session_calories = 0;
  s_session_totals_committed = false;
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
  (void)menu_layer;
  (void)cell_index;
  (void)context;
  return s_profile_cell_height;
}

static void prv_profile_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  (void)context;
  int row = (int)cell_index->row;
  if (row >= PROFILE_COUNT) {
    return;
  }
  ProfileSettings *p = &s_settings.profiles[row];
  static char legacy_title[16];
  static char weight_value[12];
  static char terrain_value[12];
  static char grade_value[8];
  const char *title_text = legacy_title;
  const char *weight_unit = (s_settings.ruck_weight_unit == 1) ? "lb" : "kg";
  const int16_t y = 0;
  GRect bounds = layer_get_bounds((Layer *)cell_layer);
  const int16_t row_w = bounds.size.w;
  const int16_t row_h = bounds.size.h;
  const int16_t content_x = SCREEN_PADDING;
  const int16_t content_w = row_w - (2 * SCREEN_PADDING);
  const int16_t value_y = row_h - 32;
  const int16_t icon_y = y + value_y + 2;
  const int16_t weight_col_x = content_x;
  const int16_t weight_col_w = content_w / 3;
  const int16_t terrain_col_x = weight_col_x + weight_col_w;
  const int16_t terrain_col_w = content_w / 3;
  const int16_t grade_col_x = terrain_col_x + terrain_col_w;
  const int16_t grade_col_w = content_w - (weight_col_w + terrain_col_w);
  const int16_t weight_icon_w = s_profile_weight_icon ? gbitmap_get_bounds(s_profile_weight_icon).size.w : 0;
  const int16_t weight_icon_h = s_profile_weight_icon ? gbitmap_get_bounds(s_profile_weight_icon).size.h : 0;
  const int16_t terrain_icon_w = s_profile_terrain_icon ? gbitmap_get_bounds(s_profile_terrain_icon).size.w : 0;
  const int16_t terrain_icon_h = s_profile_terrain_icon ? gbitmap_get_bounds(s_profile_terrain_icon).size.h : 0;
  const int16_t grade_icon_w = s_profile_grade_icon ? gbitmap_get_bounds(s_profile_grade_icon).size.w : 0;
  const int16_t grade_icon_h = s_profile_grade_icon ? gbitmap_get_bounds(s_profile_grade_icon).size.h : 0;
  const GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const GFont value_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  bool is_highlighted = menu_cell_layer_is_highlighted(cell_layer);
  GColor bg = is_highlighted ? GColorWhite : GColorBlack;
  GColor fg = is_highlighted ? GColorBlack : GColorWhite;

  title_text = prv_profile_display_name(row, legacy_title, sizeof(legacy_title));
  snprintf(weight_value, sizeof(weight_value), "%ld.%ld%s",
           (long)(p->ruck_weight_value / 10), (long)labs(p->ruck_weight_value % 10), weight_unit);
  snprintf(terrain_value, sizeof(terrain_value), "%s", prv_profile_terrain_label(row, p->terrain_factor));
  snprintf(grade_value, sizeof(grade_value), "%ld.%ld",
           (long)(p->grade_percent / 10), (long)labs(p->grade_percent % 10));

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if (row > 0) {
    graphics_context_set_stroke_color(ctx, is_highlighted ? GColorLightGray : GColorDarkGray);
    graphics_draw_line(ctx, GPoint(content_x, 0), GPoint(content_x + content_w - 1, 0));
  }
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, title_text, title_font, GRect(content_x, y + 2, content_w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (is_highlighted) {
    // Keep icon contrast on the white selected row.
    graphics_context_set_fill_color(ctx, GColorBlack);
    if (s_profile_weight_icon) {
      graphics_fill_rect(ctx, GRect(weight_col_x, icon_y, weight_icon_w, weight_icon_h), 3, GCornersAll);
    }
    if (s_profile_terrain_icon) {
      graphics_fill_rect(ctx, GRect(terrain_col_x, icon_y, terrain_icon_w, terrain_icon_h), 3, GCornersAll);
    }
    if (s_profile_grade_icon) {
      graphics_fill_rect(ctx, GRect(grade_col_x, icon_y, grade_icon_w, grade_icon_h), 3, GCornersAll);
    }
  }
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  if (s_profile_weight_icon) {
    graphics_draw_bitmap_in_rect(ctx, s_profile_weight_icon, GRect(weight_col_x, icon_y, weight_icon_w, weight_icon_h));
  }
  if (s_profile_terrain_icon) {
    graphics_draw_bitmap_in_rect(ctx, s_profile_terrain_icon, GRect(terrain_col_x, icon_y, terrain_icon_w, terrain_icon_h));
  }
  if (s_profile_grade_icon) {
    graphics_draw_bitmap_in_rect(ctx, s_profile_grade_icon, GRect(grade_col_x, icon_y, grade_icon_w, grade_icon_h));
  }
  graphics_draw_text(ctx, weight_value, value_font, GRect(weight_col_x + weight_icon_w + 2, y + value_y + 5, weight_col_w - (weight_icon_w + 2), 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, terrain_value, value_font, GRect(terrain_col_x + terrain_icon_w + 2, y + value_y + 5, terrain_col_w - (terrain_icon_w + 2), 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, grade_value, value_font, GRect(grade_col_x + grade_icon_w + 2, y + value_y + 5, grade_col_w - (grade_icon_w + 2), 22),
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

static void prv_main_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  prv_commit_session_totals("back");
  if (!window_stack_contains_window(s_profile_window)) {
    window_stack_push(s_profile_window, true);
  }
}

static uint16_t prv_music_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  (void)menu_layer;
  (void)section_index;
  (void)context;
  return 3;
}

static void prv_music_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  (void)context;
  static const char *k_titles[] = { "Play / Pause", "Next Track", "Previous Track" };
  int row = cell_index->row;
  if (row < 0 || row > 2) {
    return;
  }
  menu_cell_basic_draw(ctx, cell_layer, k_titles[row], NULL, NULL);
}

static void prv_music_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  (void)menu_layer;
  (void)cell_index;
  (void)context;
  vibes_short_pulse();
}

static void prv_music_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  GRect menu_bounds = GRect(SCREEN_PADDING, SCREEN_PADDING,
                            bounds.size.w - (2 * SCREEN_PADDING),
                            bounds.size.h - (2 * SCREEN_PADDING));
  s_music_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_music_menu_layer, window);
  menu_layer_set_callbacks(s_music_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_music_get_num_rows_callback,
    .draw_row = prv_music_draw_row_callback,
    .select_click = prv_music_select_callback,
  });
  layer_add_child(window_layer, menu_layer_get_layer(s_music_menu_layer));
}

static void prv_music_window_unload(Window *window) {
  (void)window;
  menu_layer_destroy(s_music_menu_layer);
  s_music_menu_layer = NULL;
}

static void prv_main_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  window_stack_push(s_music_window, true);
}

static void prv_main_click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_BACK, prv_main_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_main_up_click_handler);
}

static void prv_profile_window_load(Window *window) {
  window_set_background_color(window, GColorBlack);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  int16_t usable_height = bounds.size.h - (2 * SCREEN_PADDING);
  s_profile_cell_height = usable_height / PROFILE_COUNT;
  int16_t menu_height = s_profile_cell_height * PROFILE_COUNT;
  GRect menu_bounds = GRect(SCREEN_PADDING, SCREEN_PADDING,
                            bounds.size.w - (2 * SCREEN_PADDING), menu_height);
  s_profile_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_profile_menu_layer, window);
  menu_layer_set_callbacks(s_profile_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_profile_get_num_rows_callback,
    .get_cell_height = prv_profile_get_cell_height_callback,
    .draw_row = prv_profile_draw_row_callback,
    .select_click = prv_profile_select_callback,
  });
  menu_layer_set_center_focused(s_profile_menu_layer, false);
  menu_layer_pad_bottom_enable(s_profile_menu_layer, false);
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
  int x0 = SCREEN_PADDING;
  int y0 = SCREEN_PADDING;
  int w = bounds.size.w - (2 * SCREEN_PADDING);
  int h = bounds.size.h - (2 * SCREEN_PADDING);
  int header_name_w = (w * 2) / 3;

  window_set_background_color(window, GColorBlack);
  window_set_click_config_provider(window, prv_main_click_config_provider);

  s_grid_layer = layer_create(GRect(x0, y0, w, h));
  layer_set_update_proc(s_grid_layer, prv_grid_layer_update_proc);
  layer_add_child(window_layer, s_grid_layer);

  s_top_time_layer = text_layer_create(GRect(x0, y0, header_name_w, 30));
  s_top_left_layer = text_layer_create(GRect(x0 + header_name_w, y0, w - header_name_w, 30));
  s_top_right_layer = text_layer_create(GRect(x0, y0 + 30, w / 2, 24));
  s_top_stats_right_layer = text_layer_create(GRect(x0 + (w / 2), y0 + 30, w - (w / 2), 24));

  s_mid_left_icon_layer = bitmap_layer_create(GRect(x0 + (w / 4) - 12, y0 + 70, 24, 24));
  s_mid_left_value_layer = text_layer_create(GRect(x0, y0 + 90, w / 2, 36));
  s_mid_center_icon_layer = bitmap_layer_create(GRect(x0 + (w / 2) - 12, y0 + 70, 24, 24));
  s_mid_center_value_layer = text_layer_create(GRect(x0 + (w / 2) - 24, y0 + 92, 48, 30));
  s_mid_right_icon_layer = bitmap_layer_create(GRect(x0 + ((w * 3) / 4) - 12, y0 + 70, 24, 24));
  s_mid_right_value_layer = text_layer_create(GRect(x0 + (w / 2), y0 + 90, w - (w / 2), 36));

  s_bottom_left_icon_layer = bitmap_layer_create(GRect(x0 + (w / 4) - 12, y0 + 144, 24, 24));
  s_bottom_left_value_layer = text_layer_create(GRect(x0, y0 + 162, w / 2, 28));
  s_bottom_left_secondary_layer = text_layer_create(GRect(x0, y0 + 188, w / 2, 28));
  s_bottom_right_icon_layer = bitmap_layer_create(GRect(x0 + ((w * 3) / 4) - 12, y0 + 144, 24, 24));
  s_bottom_right_value_layer = text_layer_create(GRect(x0 + (w / 2), y0 + 162, w - (w / 2), 28));
  s_bottom_right_secondary_layer = text_layer_create(GRect(x0 + (w / 2), y0 + 188, w - (w / 2), 28));

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

  prv_set_text_style(s_top_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentLeft, GColorWhite);
  prv_set_text_style(s_top_left_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentRight, GColorWhite);
  prv_set_text_style(s_top_right_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_top_stats_right_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18), GTextAlignmentCenter, GColorWhite);
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
  text_layer_set_overflow_mode(s_top_stats_right_layer, GTextOverflowModeTrailingEllipsis);

  layer_add_child(window_layer, text_layer_get_layer(s_top_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_top_left_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_top_right_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_top_stats_right_layer));
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
  text_layer_destroy(s_top_stats_right_layer);
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
  if (persist_exists(LIFETIME_DISTANCE_M_PERSIST_KEY)) {
    s_lifetime_distance_m = persist_read_int(LIFETIME_DISTANCE_M_PERSIST_KEY);
  }
  if (persist_exists(LIFETIME_CALORIES_PERSIST_KEY)) {
    s_lifetime_calories = persist_read_int(LIFETIME_CALORIES_PERSIST_KEY);
  }

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

  s_music_window = window_create();
  window_set_window_handlers(s_music_window, (WindowHandlers) {
    .load = prv_music_window_load,
    .unload = prv_music_window_unload,
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
  app_message_register_inbox_dropped(prv_inbox_dropped_handler);
  app_message_register_outbox_failed(prv_outbox_failed_handler);
  app_message_open(1024, 64);
  APP_LOG(APP_LOG_LEVEL_INFO, "App initialized, waiting for config updates");

  window_stack_push(s_window, false);
  window_stack_push(s_profile_window, true);
}

static void prv_deinit(void) {
  prv_commit_session_totals("deinit");
  tick_timer_service_unsubscribe();
  if (s_health_available) {
    health_service_events_unsubscribe();
  }
  window_destroy(s_music_window);
  window_destroy(s_profile_window);
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
