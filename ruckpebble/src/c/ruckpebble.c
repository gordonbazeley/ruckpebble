#include <pebble.h>
#include <stdlib.h>
#include <string.h>


#define PROFILE_COUNT 3
#define PROFILE_NAME_MAX_LEN 33
#define TERRAIN_TYPE_MAX_LEN 16
#define SCREEN_PADDING 5
#define PROFILE_ROW_HEIGHT 62
#define PROFILE_ROW_SEPARATOR_HEIGHT 1
#define PROFILE_GRADE_TEXT_WIDTH 30
#define PACE_HISTORY_SECONDS 60
#define RUCK_CHECKIN_INTERVAL_S 60
#define RUCK_CHECKIN_REPEAT_MS 30000
#define RUCK_CHECKIN_REPEAT_TIMEOUT_S 180

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

typedef enum {
  RUCK_PROMPT_MODE_BACK    = 0,
  RUCK_PROMPT_MODE_DOWN    = 1,
  RUCK_PROMPT_MODE_RESTORE = 2,
  RUCK_PROMPT_MODE_CHECKIN = 3,
} RuckPromptMode;

enum {
  SETTINGS_PERSIST_KEY = 1,
  LIFETIME_DISTANCE_M_PERSIST_KEY = 2,
  LIFETIME_CALORIES_PERSIST_KEY = 3,
  LAST_ACTIVITY_DISTANCE_M_PERSIST_KEY = 4,
  LAST_ACTIVITY_CALORIES_PERSIST_KEY   = 5,
  LAST_ACTIVITY_PACE_SEC_PERSIST_KEY   = 6,
  LAST_ACTIVITY_DURATION_SEC_PERSIST_KEY = 7,
  LAST_ACTIVITY_TIMESTAMP_PERSIST_KEY  = 8,
  APP_STATE_SCHEMA_VERSION_PERSIST_KEY = 9,
  SESSION_IN_PROGRESS_PERSIST_KEY      = 10,
  SESSION_RESUME_START_TIME_PERSIST_KEY = 11,
  SESSION_RESUME_DISTANCE_M_PERSIST_KEY = 12,
  SESSION_RESUME_CALORIES_PERSIST_KEY  = 13,
  SESSION_RESUME_ELAPSED_S_PERSIST_KEY = 14,
  SESSION_RESUME_PROFILE_PERSIST_KEY   = 15,
  SESSION_RESUME_STEPS_PERSIST_KEY     = 16,
};

#define APP_STATE_SCHEMA_VERSION 2

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
    { .ruck_weight_value = 68, .terrain_factor = 100, .grade_percent = 0 },
    { .ruck_weight_value = 136, .terrain_factor = 120, .grade_percent = 20 }
  },
  .profile_names = {
    "30lb, road",
    "15lb, road",
    "30lb, trail"
  },
  .profile_terrain_types = {
    "road",
    "road",
    "gravel"
  }
};

static Window *s_profile_window;
static MenuLayer *s_profile_menu_layer;
static Window *s_status_window;
static Layer *s_status_layer;
static AppTimer *s_status_timer;
static Window *s_ruck_prompt_window;
static Layer *s_ruck_prompt_layer;
static AppTimer *s_checkin_repeat_timer;
static int32_t s_checkin_repeat_elapsed_s;
static int32_t s_ruck_prompt_mode = 0;
static int32_t s_ruck_prompt_selected_row = 0;
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
static bool s_profile_touch_active = false;
static int16_t s_profile_touch_start_x = 0;
static int16_t s_profile_touch_start_y = 0;

static Settings s_settings;
static bool s_session_active = false;
static bool s_post_save_stay = false;
static time_t s_start_time;
static bool s_health_available = false;
static bool s_health_subscribed = false;
static time_t s_day_start;
static int32_t s_steps_baseline = 0;
static int32_t s_last_steps = 0;
static time_t s_last_time = 0;
static int32_t s_logged_steps_total = -1;
static time_t s_logged_steps_time = 0;
static int64_t s_speed_mmps = 0;
static int32_t s_session_distance_m = 0;
static int32_t s_session_calories = 0;
static int32_t s_session_distance_offset_m = 0;
static int32_t s_session_steps = 0;
static int32_t s_session_steps_offset = 0;
static bool s_session_paused = false;
static time_t s_pause_start_time = 0;
static int32_t s_pause_total_s = 0;
static int32_t s_steps_at_pause = 0;
static Layer *s_paused_icon_layer = NULL;
static int32_t s_lifetime_distance_m = 0;
static int32_t s_lifetime_calories = 0;
static bool s_session_totals_committed = false;
static int32_t s_last_activity_distance_m = 0;
static int32_t s_last_activity_calories   = 0;
static int32_t s_last_activity_pace_sec   = 0;
static int32_t s_last_activity_duration_sec = 0;
static int32_t s_last_activity_timestamp  = 0;
static int32_t s_session_pace_sec         = 0;
static int32_t s_current_pace_sec         = 0;
static int32_t s_current_spm              = 0;  // steps/min over trailing PACE_HISTORY_SECONDS window; drives round's cadence readout (no HR sensor there)
static char s_status_text[64] = "Saving ruck...";
static int32_t s_step_history[PACE_HISTORY_SECONDS];
static time_t s_step_history_time[PACE_HISTORY_SECONDS];

#define EMULATOR_TIME_SCALE 10

static int64_t prv_weight_to_kg1000(int32_t value_tenths) {
  return (int64_t)value_tenths * 100;
}

static int64_t prv_stride_to_mm(int32_t value_tenths) {
  return (int64_t)value_tenths;
}

static int32_t prv_lb_tenths_to_kg_tenths(int32_t value_tenths) {
  return (int32_t)(((int64_t)value_tenths * 453592 + 500000) / 1000000);
}

static int32_t prv_kg_tenths_to_lb_tenths(int32_t value_tenths) {
  return (int32_t)(((int64_t)value_tenths * 1000000 + 226796) / 453592);
}

static int32_t prv_in_tenths_to_cm_tenths(int32_t value_tenths) {
  return (int32_t)(((int64_t)value_tenths * 254 + 50) / 100);
}

static bool prv_use_imperial_distance_units(void) {
  MeasurementSystem system = health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters);
  return system == MeasurementSystemImperial;
}

static int32_t prv_active_profile_index(void) {
  if (s_settings.active_profile < 0 || s_settings.active_profile >= PROFILE_COUNT) {
    return 0;
  }
  return s_settings.active_profile;
}

static void prv_status_timer_callback(void *context);

static void prv_show_status_message(const char *text, uint32_t duration_ms);
static void prv_health_handler(HealthEventType event, void *context);
static void prv_reset_session_state(void);
static void prv_save_ruck(void);

static bool prv_step_count_available(time_t now) {
  HealthServiceAccessibilityMask access = health_service_metric_accessible(HealthMetricStepCount, s_day_start, now);
  return (access & HealthServiceAccessibilityMaskAvailable);
}

static void prv_ensure_health_subscription(time_t now) {
  bool available = prv_step_count_available(now);
  if (available && !s_health_subscribed) {
    health_service_events_subscribe(prv_health_handler, NULL);
    s_health_subscribed = true;
  }
  s_health_available = available;
}

static int32_t prv_current_step_count(time_t now) {
  (void)now;
  if (!s_health_available) {
    return 0;
  }

  HealthValue steps = health_service_peek_current_value(HealthMetricStepCount);
  if (steps <= 0) {
    steps = health_service_sum_today(HealthMetricStepCount);
  }
  if (steps < 0) {
    return 0;
  }
  if (steps > INT32_MAX) {
    return INT32_MAX;
  }
  return (int32_t)steps;
}

static void prv_log_health_step_cadence(time_t now, int32_t steps_total_day) {
  if (!s_health_available || steps_total_day == s_logged_steps_total) {
    return;
  }

  if (s_logged_steps_total >= 0 && s_logged_steps_time > 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Health steps changed: total=%ld delta=%ld interval_s=%ld",
            (long)steps_total_day,
            (long)(steps_total_day - s_logged_steps_total),
            (long)(now - s_logged_steps_time));
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Health steps initial total=%ld", (long)steps_total_day);
  }

  s_logged_steps_total = steps_total_day;
  s_logged_steps_time = now;
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

#if defined(PBL_ROUND)
// A row/rect placed at a flat x-inset runs under the bezel near a round display's top/bottom;
// this returns the widest x/w that still clears the circle across a given y-span (narrower of
// its top/bottom edge, minus margin). PBL_DISPLAY_WIDTH is a per-platform SDK define, so this
// works for any round platform's actual circle (chalk's 180px, gabbro's 260px, ...).
static int16_t prv_round_half_width(int16_t y_abs, int16_t r) {
  int16_t dy = (int16_t)abs(y_abs - r);
  if (dy >= r) {
    return 0;
  }
  return (int16_t)prv_isqrt((int64_t)r * r - (int64_t)dy * dy);
}

static void prv_round_row_x(int16_t y_top, int16_t y_bot, int16_t margin, int16_t *out_x, int16_t *out_w) {
  const int16_t r = PBL_DISPLAY_WIDTH / 2;
  int16_t hw_top = prv_round_half_width(y_top, r);
  int16_t hw_bot = prv_round_half_width(y_bot, r);
  int16_t hw = hw_top < hw_bot ? hw_top : hw_bot;
  hw -= margin;
  if (hw < 20) {
    hw = 20;
  }
  *out_x = r - hw;
  *out_w = hw * 2;
}
#endif

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
  // 60/136 are reference-218 offsets (same frame prv_window_load scales row positions from),
  // not fixed pixels - otherwise these sit still while the rows around them move on round
  // platforms and end up drawn through the icons instead of between the rows.
  int y_top = (bounds.size.h * 60) / 218;
  int y_bottom = (bounds.size.h * 136) / 218;

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(8, y_top), GPoint(w - 8, y_top));
  graphics_draw_line(ctx, GPoint(8, y_bottom), GPoint(w - 8, y_bottom));
}

static void prv_save_in_progress_session(void) {
  time_t now = time(NULL);
  int32_t pause_total = s_pause_total_s;
  if (s_session_paused) {
    pause_total += (int32_t)(now - s_pause_start_time);
  }
  int32_t elapsed_s = (int32_t)(now - s_start_time) - pause_total;
  if (elapsed_s < 0) elapsed_s = 0;
  persist_write_int(SESSION_IN_PROGRESS_PERSIST_KEY, 1);
  persist_write_int(SESSION_RESUME_ELAPSED_S_PERSIST_KEY, elapsed_s);
  persist_write_int(SESSION_RESUME_DISTANCE_M_PERSIST_KEY, s_session_distance_m);
  persist_write_int(SESSION_RESUME_CALORIES_PERSIST_KEY, s_session_calories);
  persist_write_int(SESSION_RESUME_STEPS_PERSIST_KEY, s_session_steps);
  persist_write_int(SESSION_RESUME_PROFILE_PERSIST_KEY, (int32_t)s_settings.active_profile);
}

static void prv_clear_in_progress_session(void) {
  persist_delete(SESSION_IN_PROGRESS_PERSIST_KEY);
}

static bool prv_load_in_progress_session(void) {
  return persist_exists(SESSION_IN_PROGRESS_PERSIST_KEY) &&
         persist_read_int(SESSION_IN_PROGRESS_PERSIST_KEY) != 0;
}

static bool prv_has_resumable_session_for_profile(int32_t profile_index) {
  if (!prv_load_in_progress_session()) {
    return false;
  }
  if (!persist_exists(SESSION_RESUME_PROFILE_PERSIST_KEY)) {
    return false;
  }
  return persist_read_int(SESSION_RESUME_PROFILE_PERSIST_KEY) == profile_index;
}

static void prv_resume_in_progress_session(void) {
  time_t now = time(NULL);
  int32_t saved_elapsed_s = persist_exists(SESSION_RESUME_ELAPSED_S_PERSIST_KEY) ?
      persist_read_int(SESSION_RESUME_ELAPSED_S_PERSIST_KEY) : 0;

  prv_reset_session_state();
  s_session_distance_offset_m = persist_exists(SESSION_RESUME_DISTANCE_M_PERSIST_KEY) ?
      persist_read_int(SESSION_RESUME_DISTANCE_M_PERSIST_KEY) : 0;
  s_session_distance_m = s_session_distance_offset_m;
  s_session_steps_offset = persist_exists(SESSION_RESUME_STEPS_PERSIST_KEY) ?
      persist_read_int(SESSION_RESUME_STEPS_PERSIST_KEY) : 0;
  if (s_session_steps_offset < 0) {
    s_session_steps_offset = 0;
  }
  s_session_steps = s_session_steps_offset;

  s_start_time = now - (time_t)saved_elapsed_s;
  s_session_active = true;

  prv_ensure_health_subscription(now);
  s_steps_baseline = s_health_available ? prv_current_step_count(now) : 0;

  APP_LOG(APP_LOG_LEVEL_INFO, "Resumed session: elapsed=%lds dist_offset=%ldm steps_offset=%ld",
          (long)saved_elapsed_s, (long)s_session_distance_offset_m, (long)s_session_steps_offset);
}

static void prv_load_settings(void) {
  s_settings = SETTINGS_DEFAULTS;
  if (persist_exists(SETTINGS_PERSIST_KEY)) {
    persist_read_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
  }
  if (s_settings.weight_unit == 1) {
    s_settings.weight_value = prv_lb_tenths_to_kg_tenths(s_settings.weight_value);
    s_settings.weight_unit = 0;
  }
  if (s_settings.ruck_weight_unit == 1) {
    for (int i = 0; i < PROFILE_COUNT; ++i) {
      s_settings.profiles[i].ruck_weight_value = prv_lb_tenths_to_kg_tenths(s_settings.profiles[i].ruck_weight_value);
    }
    s_settings.ruck_weight_unit = 0;
  }
  if (s_settings.stride_unit == 1) {
    s_settings.stride_value = prv_in_tenths_to_cm_tenths(s_settings.stride_value);
    s_settings.stride_unit = 0;
  }
  s_settings.sim_steps_enabled = 0;
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

static void prv_show_status_message(const char *text, uint32_t duration_ms) {
  if (!text || text[0] == '\0') {
    text = "Saving ruck...";
  }
  strncpy(s_status_text, text, sizeof(s_status_text) - 1);
  s_status_text[sizeof(s_status_text) - 1] = '\0';

  if (s_status_layer) {
    layer_mark_dirty(s_status_layer);
  }

  if (!window_stack_contains_window(s_status_window)) {
    window_stack_push(s_status_window, true);
  }
  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
  }
  s_status_timer = app_timer_register(duration_ms, prv_status_timer_callback, NULL);
}

static void prv_send_lifetime_totals(bool insert_timeline_pin) {
  APP_LOG(APP_LOG_LEVEL_INFO, "prv_send_lifetime_totals: dist=%ld cal=%ld last_dist=%ld last_ts=%ld insert_pin=%d",
    (long)s_last_activity_distance_m, (long)s_last_activity_calories,
    (long)s_last_activity_distance_m, (long)s_last_activity_timestamp,
    (int)insert_timeline_pin);
  DictionaryIterator *iter = NULL;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed for totals: %d", (int)result);
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_lifetime_distance_m_total, s_lifetime_distance_m);
  dict_write_int32(iter, MESSAGE_KEY_lifetime_calories_total, s_lifetime_calories);
  dict_write_int32(iter, MESSAGE_KEY_last_activity_distance_m, s_last_activity_distance_m);
  dict_write_int32(iter, MESSAGE_KEY_last_activity_calories,   s_last_activity_calories);
  dict_write_int32(iter, MESSAGE_KEY_last_activity_pace_sec,   s_last_activity_pace_sec);
  dict_write_int32(iter, MESSAGE_KEY_last_activity_duration_sec, s_last_activity_duration_sec);
  dict_write_int32(iter, MESSAGE_KEY_last_activity_timestamp,  s_last_activity_timestamp);
  if (insert_timeline_pin) {
    dict_write_int32(iter, MESSAGE_KEY_insert_timeline_pin, 1);
  }
  dict_write_end(iter);
  result = app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_INFO, "prv_send_lifetime_totals: send result=%d", (int)result);
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

static void prv_paused_icon_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t bar_w = 28;
  const int16_t bar_h = 88;
  const int16_t gap = 24;
  const int16_t total_w = bar_w * 2 + gap;
  int16_t left_x = (bounds.size.w - total_w) / 2;
  int16_t right_x = left_x + bar_w + gap;
  int16_t top_y = (bounds.size.h - bar_h) / 2;
  graphics_context_set_fill_color(ctx, GColorBlueMoon);
  graphics_fill_rect(ctx, GRect(left_x, top_y, bar_w, bar_h), 8, GCornersAll);
  graphics_fill_rect(ctx, GRect(right_x, top_y, bar_w, bar_h), 8, GCornersAll);
}

static void prv_update_display(void) {
  if (!s_top_time_layer) {
    return;
  }
  time_t now = time(NULL);
  prv_ensure_health_subscription(now);
  int64_t elapsed_real_s = (int64_t)(now - s_start_time) - (int64_t)s_pause_total_s;
  if (s_session_paused) {
    elapsed_real_s -= (int64_t)(now - s_pause_start_time);
  }
  if (elapsed_real_s < 1) {
    elapsed_real_s = 1;
  }
  int64_t elapsed_s = elapsed_real_s;
  if (s_settings.sim_steps_enabled) {
    elapsed_s *= EMULATOR_TIME_SCALE;
  }

  int32_t steps = s_session_steps_offset;
  int32_t steps_total_day = 0;
  steps_total_day = prv_current_step_count(now);
  prv_log_health_step_cadence(now, steps_total_day);
  if (s_health_available) {
    int32_t steps_since_baseline = steps_total_day - s_steps_baseline;
    if (steps_since_baseline < 0) {
      steps_since_baseline = 0;
    }
    steps = s_session_steps_offset + steps_since_baseline;
  }
  if (s_session_paused && steps > s_steps_at_pause) {
    steps = s_steps_at_pause;
  }
  s_session_steps = steps;

  int64_t stride_mm = prv_stride_to_mm(s_settings.stride_value);
  int32_t steps_since_offset = steps - s_session_steps_offset;
  if (steps_since_offset < 0) {
    steps_since_offset = 0;
  }
  int64_t distance_mm = (int64_t)steps_since_offset * stride_mm;
  int64_t total_distance_mm = distance_mm + (int64_t)s_session_distance_offset_m * 1000;
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
  if (!s_session_paused && delta_scaled_s >= 5) {
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

  bool use_imperial = prv_use_imperial_distance_units();
  int64_t unit_mm = use_imperial ? 1609344 : 1000000;
  const char *distance_unit_label = use_imperial ? "mi" : "km";
  int64_t distance_x100 = (total_distance_mm * 100) / unit_mm;

  int64_t session_pace_display_sec = 0;
  int64_t session_pace_storage_sec = 0;
  if (total_distance_mm > 0) {
    session_pace_display_sec = (elapsed_s * unit_mm) / total_distance_mm;
    session_pace_storage_sec = (elapsed_s * 1000000LL) / total_distance_mm;
  }
  s_session_pace_sec = (int32_t)session_pace_storage_sec;

  int64_t current_pace_sec = 0;
  int64_t current_window_s = elapsed_real_s < PACE_HISTORY_SECONDS ? elapsed_real_s : PACE_HISTORY_SECONDS;
  if (s_health_available && current_window_s > 0) {
    int32_t window_start_steps = s_steps_baseline;
    bool have_history = false;
    if (elapsed_real_s >= PACE_HISTORY_SECONDS) {
      time_t window_start_time = now - PACE_HISTORY_SECONDS;
      int history_index = (int)(window_start_time % PACE_HISTORY_SECONDS);
      if (s_step_history_time[history_index] == window_start_time) {
        window_start_steps = s_step_history[history_index];
        have_history = true;
      }
    } else {
      have_history = true;
    }
    if (have_history) {
      int32_t window_steps = steps_total_day - window_start_steps;
      if (window_steps < 0) {
        window_steps = 0;
      }
      s_current_spm = (current_window_s > 0)
          ? (int32_t)(((int64_t)window_steps * 60) / current_window_s) : 0;
      if (window_steps > 0) {
        current_pace_sec = (current_window_s * unit_mm) / ((int64_t)window_steps * stride_mm);
      }
    }
  }
  if (current_pace_sec <= 0 && !s_health_available) {
    current_pace_sec = session_pace_display_sec;
  }
  s_current_pace_sec = (int32_t)current_pace_sec;

  // Write step history AFTER reading it for pace, because both use now % PACE_HISTORY_SECONDS.
  if (s_health_available) {
    int history_index = (int)(now % PACE_HISTORY_SECONDS);
    s_step_history_time[history_index] = now;
    s_step_history[history_index] = steps_total_day;
  }

  ProfileSettings *profile = prv_active_profile();
  int64_t weight_kg1000 = prv_weight_to_kg1000(s_settings.weight_value);
  int64_t load_kg1000 = prv_weight_to_kg1000(profile->ruck_weight_value);
  int64_t session_speed_mmps = 0;
  if (elapsed_s > 0 && total_distance_mm > 0) {
    session_speed_mmps = total_distance_mm / elapsed_s;
  }
  if (session_speed_mmps > 5000) {
    session_speed_mmps = 5000;
  }
  int64_t metabolic_mw = prv_pandolf_metabolic_mw(weight_kg1000, load_kg1000, session_speed_mmps);
  int64_t ruck_kcal_per_hour = (metabolic_mw * 3600) / 4184 / 1000;
  int64_t walk_kcal_per_hour = prv_walking_kcal_per_hour(weight_kg1000, session_speed_mmps);
  int64_t walk_kcal_total = (walk_kcal_per_hour * elapsed_s) / 3600;
  int64_t load_ratio_q1000 = 0;
  if (weight_kg1000 > 0) {
    load_ratio_q1000 = (load_kg1000 * 1000) / weight_kg1000;
  }
  if (load_kg1000 > 0 && ruck_kcal_per_hour < walk_kcal_per_hour) {
    int64_t load_bonus_kcal_per_hour = (walk_kcal_per_hour * load_ratio_q1000) / 1000;
    if (load_bonus_kcal_per_hour < 1) {
      load_bonus_kcal_per_hour = 1;
    }
    ruck_kcal_per_hour = walk_kcal_per_hour + load_bonus_kcal_per_hour;
  }
  int64_t ruck_kcal_total = (ruck_kcal_per_hour * elapsed_s) / 3600;

  static char top_time_buf[16];
  static char distance_buf[20];
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
  if (session_pace_display_sec > 0) {
    int pace_min = (int)(session_pace_display_sec / 60);
    int pace_rem = (int)(session_pace_display_sec % 60);
    snprintf(pace_header_buf, sizeof(pace_header_buf), "%d:%02d/%s", pace_min, pace_rem, distance_unit_label);
  } else {
    snprintf(pace_header_buf, sizeof(pace_header_buf), "--:--/%s", distance_unit_label);
  }
  if (current_pace_sec > 0) {
    int pace_min = (int)(current_pace_sec / 60);
    int pace_rem = (int)(current_pace_sec % 60);
    snprintf(pace_value_buf, sizeof(pace_value_buf), "%d:%02d", pace_min, pace_rem);
  } else {
    snprintf(pace_value_buf, sizeof(pace_value_buf), "--:--");
  }
  snprintf(distance_buf, sizeof(distance_buf), "%lld.%02lld%s",
           (long long)(distance_x100 / 100), (long long)llabs(distance_x100 % 100), distance_unit_label);
  snprintf(timer_value_buf, sizeof(timer_value_buf), "%ld:%02ld",
           (long)(elapsed_s / 60), (long)(elapsed_s % 60));
  snprintf(steps_value_buf, sizeof(steps_value_buf), "%ld", (long)steps);
  if (s_health_available) {
    snprintf(steps_total_value_buf, sizeof(steps_total_value_buf), "%ld", (long)steps_total_day);
  } else {
    snprintf(steps_total_value_buf, sizeof(steps_total_value_buf), "No Health");
  }
  snprintf(calories_value_buf, sizeof(calories_value_buf), "%ld", (long)ruck_kcal_total);
  snprintf(calories_walk_value_buf, sizeof(calories_walk_value_buf), "%ld", (long)walk_kcal_total);

  int64_t session_distance_m = total_distance_mm / 1000;
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

#if defined(PBL_ROUND)
  // Chalk has no HR sensor — show live cadence (steps/min) in that slot instead.
  if (s_current_spm > 0) {
    snprintf(hr_value_buf, sizeof(hr_value_buf), "%ld", (long)s_current_spm);
  } else {
    snprintf(hr_value_buf, sizeof(hr_value_buf), "--");
  }
#else
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
#endif

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

  if (s_paused_icon_layer) {
    layer_set_hidden(s_paused_icon_layer, !s_session_paused);
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_display();
  // Persist session state every minute so a sudden OS kill loses at most 60s of data.
  if (s_session_active && tick_time->tm_sec == 0) {
    prv_save_in_progress_session();
  }
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
    if (t->value->int32 == 1) {
      s_settings.weight_value = prv_lb_tenths_to_kg_tenths(s_settings.weight_value);
    }
    s_settings.weight_unit = 0;
  }
  t = dict_find(iter, MESSAGE_KEY_ruck_weight_unit);
  if (t) {
    if (t->value->int32 == 1) {
      for (int i = 0; i < PROFILE_COUNT; ++i) {
        s_settings.profiles[i].ruck_weight_value = prv_lb_tenths_to_kg_tenths(s_settings.profiles[i].ruck_weight_value);
      }
    }
    s_settings.ruck_weight_unit = 0;
  }
  t = dict_find(iter, MESSAGE_KEY_stride_length_value);
  if (t) {
    s_settings.stride_value = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_stride_length_unit);
  if (t) {
    if (t->value->int32 == 1) {
      s_settings.stride_value = prv_in_tenths_to_cm_tenths(s_settings.stride_value);
    }
    s_settings.stride_unit = 0;
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
    APP_LOG(APP_LOG_LEVEL_INFO, "profile1_grade_percent set to %ld", (long)s_settings.profiles[0].grade_percent);
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
    APP_LOG(APP_LOG_LEVEL_INFO, "profile2_grade_percent set to %ld", (long)s_settings.profiles[1].grade_percent);
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
    APP_LOG(APP_LOG_LEVEL_INFO, "profile3_grade_percent set to %ld", (long)s_settings.profiles[2].grade_percent);
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
    APP_LOG(APP_LOG_LEVEL_INFO, "sim_steps_enabled set to %ld", (long)s_settings.sim_steps_enabled);
  }
  t = dict_find(iter, MESSAGE_KEY_sim_steps_spm);
  if (t) {
    s_settings.sim_steps_spm = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_request_lifetime_totals);
  if (t && t->value->int32 == 1) {
    prv_send_lifetime_totals(false);
  }
  t = dict_find(iter, MESSAGE_KEY_timeline_status_text);
  if (t && t->type == TUPLE_CSTRING) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Timeline status: %s", t->value->cstring);
    prv_show_status_message(t->value->cstring, 2200);
  }

  prv_save_settings();
  APP_LOG(APP_LOG_LEVEL_INFO, "Config applied: active_profile=%ld grades=%ld,%ld,%ld",
          (long)s_settings.active_profile,
          (long)s_settings.profiles[0].grade_percent,
          (long)s_settings.profiles[1].grade_percent,
          (long)s_settings.profiles[2].grade_percent);
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
  prv_clear_in_progress_session();
  prv_reset_session_state();
  s_start_time = time(NULL);
  s_session_active = true;
  time_t now = time(NULL);
  prv_ensure_health_subscription(now);
  if (s_health_available) {
    s_steps_baseline = prv_current_step_count(now);
  } else {
    s_steps_baseline = 0;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Session start: health_available=%d baseline=%ld",
          (int)s_health_available, (long)s_steps_baseline);
}

static void prv_reset_session_state(void) {
  s_last_time = 0;
  s_last_steps = 0;
  s_speed_mmps = 0;
  s_session_distance_m = 0;
  s_session_calories = 0;
  s_session_totals_committed = false;
  s_session_pace_sec = 0;
  s_current_pace_sec = 0;
  s_session_distance_offset_m = 0;
  s_session_steps = 0;
  s_session_steps_offset = 0;
  s_session_paused = false;
  s_pause_start_time = 0;
  s_pause_total_s = 0;
  s_steps_at_pause = 0;
  memset(s_step_history, 0, sizeof(s_step_history));
  memset(s_step_history_time, 0, sizeof(s_step_history_time));
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
  static char weight_value[16];
  static char terrain_value[12];
  static char grade_value[12];
  const char *title_text = legacy_title;
  const char *weight_unit = "lb";
  const int16_t y = 0;
  GRect bounds = layer_get_bounds((Layer *)cell_layer);
  const int16_t row_w = bounds.size.w;
  const int16_t row_h = bounds.size.h;
  int16_t content_x = SCREEN_PADDING;
  int16_t content_w = row_w - (2 * SCREEN_PADDING);
#if defined(PBL_ROUND)
  // All PROFILE_COUNT rows are visible at once at fixed positions (no real scrolling), so
  // each row's absolute screen Y is known up front - chord-inset it the same way the main
  // tracking screen insets its rows, converted back to this cell's local coordinate space.
  {
    int16_t abs_top = SCREEN_PADDING + row * (s_profile_cell_height + PROFILE_ROW_SEPARATOR_HEIGHT);
    int16_t abs_bottom = abs_top + s_profile_cell_height;
    int16_t rx, rw;
    prv_round_row_x(abs_top, abs_bottom, 3, &rx, &rw);
    // The 3-column weight/terrain/grade layout below needs real room - letting rw go as
    // narrow as prv_round_row_x's generic 40px floor drives grade_col_w/weight_col_w
    // negative and corrupts the menu's draw state instead of just looking cramped. Floor
    // it higher here and re-center, accepting minor bezel overhang on the outermost rows
    // over a crash.
    const int16_t MIN_ROW_CONTENT_W = 150;
    if (rw < MIN_ROW_CONTENT_W) {
      rw = MIN_ROW_CONTENT_W;
      rx = (PBL_DISPLAY_WIDTH - rw) / 2;
    }
    content_x = rx - SCREEN_PADDING;
    content_w = rw;
  }
#endif
  const int16_t value_y = row_h - 31;
  const int16_t icon_y = y + value_y + 2;
  const int16_t weight_icon_w = s_profile_weight_icon ? gbitmap_get_bounds(s_profile_weight_icon).size.w : 0;
  const int16_t weight_icon_h = s_profile_weight_icon ? gbitmap_get_bounds(s_profile_weight_icon).size.h : 0;
  const int16_t terrain_icon_w = s_profile_terrain_icon ? gbitmap_get_bounds(s_profile_terrain_icon).size.w : 0;
  const int16_t terrain_icon_h = s_profile_terrain_icon ? gbitmap_get_bounds(s_profile_terrain_icon).size.h : 0;
  const int16_t grade_icon_w = s_profile_grade_icon ? gbitmap_get_bounds(s_profile_grade_icon).size.w : 0;
  const int16_t grade_icon_h = s_profile_grade_icon ? gbitmap_get_bounds(s_profile_grade_icon).size.h : 0;
  const int16_t grade_col_w = grade_icon_w + 1 + PROFILE_GRADE_TEXT_WIDTH;
  // Defensive: a negative remaining_w drives weight/terrain_col_w negative, which corrupts
  // (not just misdraws) menu state further down the graphics stack - never let it through.
  const int16_t remaining_w = (content_w - grade_col_w) > 0 ? (content_w - grade_col_w) : 0;
  const int16_t weight_col_x = content_x;
  const int16_t weight_col_w = remaining_w / 2;
  const int16_t terrain_col_x = weight_col_x + weight_col_w;
  const int16_t terrain_col_w = remaining_w - weight_col_w;
  const int16_t grade_col_x = terrain_col_x + terrain_col_w;
  const GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const GFont value_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  bool is_highlighted = menu_cell_layer_is_highlighted(cell_layer);
  GColor bg = is_highlighted ? GColorWhite : GColorBlack;
  GColor fg = is_highlighted ? GColorBlack : GColorWhite;

  title_text = prv_profile_display_name(row, legacy_title, sizeof(legacy_title));
  int32_t weight_lb_tenths = prv_kg_tenths_to_lb_tenths(p->ruck_weight_value);
  snprintf(weight_value, sizeof(weight_value), "%ld.%ld%s",
           (long)(weight_lb_tenths / 10), (long)labs(weight_lb_tenths % 10), weight_unit);
  snprintf(terrain_value, sizeof(terrain_value), "%s", prv_profile_terrain_label(row, p->terrain_factor));
  int32_t grade_int = (p->grade_percent >= 0) ? ((p->grade_percent + 5) / 10) : ((p->grade_percent - 5) / 10);
  snprintf(grade_value, sizeof(grade_value), "%ld%%", (long)grade_int);

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if (row > 0) {
    graphics_context_set_stroke_color(ctx, is_highlighted ? GColorLightGray : GColorDarkGray);
    graphics_draw_line(ctx, GPoint(content_x, 0), GPoint(content_x + content_w - 1, 0));
  }
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, title_text, title_font, GRect(content_x, y + 1, content_w, 24),
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
  graphics_draw_text(ctx, weight_value, value_font, GRect(weight_col_x + weight_icon_w, y + value_y + 4, weight_col_w - weight_icon_w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, terrain_value, value_font, GRect(terrain_col_x + terrain_icon_w, y + value_y + 4, terrain_col_w - terrain_icon_w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, grade_value, value_font, GRect(grade_col_x + grade_icon_w, y + value_y + 4, grade_col_w - grade_icon_w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static int32_t prv_profile_row_for_touch_y(int16_t touch_y) {
  int16_t menu_y = SCREEN_PADDING;
  int16_t row_span = s_profile_cell_height + PROFILE_ROW_SEPARATOR_HEIGHT;
  int16_t y_in_menu = touch_y - menu_y;
  int32_t row;

  if (y_in_menu < 0) {
    return -1;
  }

  row = y_in_menu / row_span;
  if (row < 0 || row >= PROFILE_COUNT) {
    return -1;
  }

  if ((y_in_menu % row_span) >= s_profile_cell_height) {
    return -1;
  }

  return row;
}

static void prv_profile_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context);

static void prv_profile_touch_handler(const TouchEvent *event, void *context) {
  (void)context;

  if (!s_profile_menu_layer || !event) {
    return;
  }

  if (event->type == TouchEvent_Touchdown) {
    s_profile_touch_active = true;
    s_profile_touch_start_x = event->x;
    s_profile_touch_start_y = event->y;
    return;
  }

  if (!s_profile_touch_active) {
    return;
  }

  if (event->type == TouchEvent_PositionUpdate) {
    int16_t dx = event->x - s_profile_touch_start_x;
    int16_t dy = event->y - s_profile_touch_start_y;
    if (abs(dx) > 12 || abs(dy) > 12) {
      s_profile_touch_active = false;
    }
    return;
  }

  if (event->type == TouchEvent_Liftoff) {
    int16_t dx = event->x - s_profile_touch_start_x;
    int16_t dy = event->y - s_profile_touch_start_y;
    int32_t row = prv_profile_row_for_touch_y(s_profile_touch_start_y);

    s_profile_touch_active = false;

    if (abs(dx) > 12 || abs(dy) > 12 || row < 0) {
      return;
    }

    prv_profile_select_callback(s_profile_menu_layer, &(MenuIndex) { .section = 0, .row = (uint16_t)row }, NULL);
  }
}

static void prv_profile_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  (void)menu_layer;
  (void)context;
  if (cell_index->row >= PROFILE_COUNT) {
    return;
  }
  s_settings.active_profile = cell_index->row;
  prv_save_settings();

  if (prv_has_resumable_session_for_profile(s_settings.active_profile)) {
    s_ruck_prompt_mode = RUCK_PROMPT_MODE_RESTORE;
    s_ruck_prompt_selected_row = 0;
    window_stack_remove(s_profile_window, true);
    if (!window_stack_contains_window(s_ruck_prompt_window)) {
      window_stack_push(s_ruck_prompt_window, true);
    }
  } else {
    prv_start_session();
    window_stack_remove(s_profile_window, true);
    prv_update_display();
  }
}

static void prv_profile_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  window_stack_pop_all(true);
}

static void prv_profile_reset_scroll_offset(void) {
  if (!s_profile_menu_layer) {
    return;
  }
  ScrollLayer *scroll_layer = menu_layer_get_scroll_layer(s_profile_menu_layer);
  scroll_layer_set_content_offset(scroll_layer, GPoint(0, 0), false);
}

static void prv_profile_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  menu_layer_set_selected_next(s_profile_menu_layer, true, MenuRowAlignNone, false);
}

static void prv_profile_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  menu_layer_set_selected_next(s_profile_menu_layer, false, MenuRowAlignNone, false);
}

static void prv_profile_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  MenuIndex selected = menu_layer_get_selected_index(s_profile_menu_layer);
  prv_profile_select_callback(s_profile_menu_layer, &selected, NULL);
}

static void prv_profile_click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_BACK, prv_profile_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_profile_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_profile_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_profile_select_click_handler);
}

static void prv_main_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (!window_stack_contains_window(s_ruck_prompt_window)) {
    s_ruck_prompt_mode = RUCK_PROMPT_MODE_BACK;
    window_stack_push(s_ruck_prompt_window, true);
  }
}

static void prv_ruck_prompt_resume(void) {
  if (window_stack_contains_window(s_ruck_prompt_window)) {
    window_stack_remove(s_ruck_prompt_window, true);
  }
}


static void prv_ruck_prompt_save(void) {
  s_session_active = false;
  prv_clear_in_progress_session();
  if (window_stack_contains_window(s_ruck_prompt_window)) {
    window_stack_remove(s_ruck_prompt_window, true);
  }
  prv_save_ruck();
}

static void prv_ruck_prompt_discard(void) {
  s_session_active = false;
  prv_clear_in_progress_session();
  prv_reset_session_state();
  window_stack_pop_all(true);
}

static void prv_ruck_prompt_select(void) {
  if (s_ruck_prompt_mode == RUCK_PROMPT_MODE_DOWN) {
    if (s_ruck_prompt_selected_row == 0) {
      prv_ruck_prompt_save();
    } else if (s_ruck_prompt_selected_row == 1) {
      prv_ruck_prompt_resume();
    } else {
      prv_ruck_prompt_discard();
    }
    return;
  }

  if (s_ruck_prompt_mode == RUCK_PROMPT_MODE_RESTORE) {
    if (s_ruck_prompt_selected_row == 0) {
      prv_resume_in_progress_session();
    } else {
      prv_clear_in_progress_session();
      prv_start_session();
    }
    if (window_stack_contains_window(s_profile_window)) {
      window_stack_remove(s_profile_window, false);
    }
    if (window_stack_contains_window(s_ruck_prompt_window)) {
      window_stack_remove(s_ruck_prompt_window, true);
    }
    prv_update_display();
    return;
  }

  if (s_ruck_prompt_mode == RUCK_PROMPT_MODE_CHECKIN) {
    if (s_ruck_prompt_selected_row == 0) {
      prv_resume_in_progress_session();
      if (window_stack_contains_window(s_profile_window)) {
        window_stack_remove(s_profile_window, false);
      }
      if (window_stack_contains_window(s_ruck_prompt_window)) {
        window_stack_remove(s_ruck_prompt_window, true);
      }
      prv_update_display();
    } else if (s_ruck_prompt_selected_row == 1) {
      prv_ruck_prompt_discard();
    } else {
      prv_resume_in_progress_session();
      prv_ruck_prompt_save();
    }
    return;
  }

  // RUCK_PROMPT_MODE_BACK
  if (s_ruck_prompt_selected_row == 0) {
    prv_ruck_prompt_discard();
  } else if (s_ruck_prompt_selected_row == 1) {
    prv_ruck_prompt_save();
  } else {
    prv_ruck_prompt_resume();
  }
}

static void prv_ruck_prompt_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (s_ruck_prompt_selected_row > 0) {
    s_ruck_prompt_selected_row -= 1;
  }
  layer_mark_dirty(s_ruck_prompt_layer);
}

static void prv_ruck_prompt_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  int32_t max_row = (s_ruck_prompt_mode == RUCK_PROMPT_MODE_RESTORE) ? 1 : 2;
  if (s_ruck_prompt_selected_row < max_row) {
    s_ruck_prompt_selected_row += 1;
  }
  layer_mark_dirty(s_ruck_prompt_layer);
}

static void prv_ruck_prompt_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  prv_ruck_prompt_select();
}

static int32_t prv_ruck_prompt_resume_row(RuckPromptMode mode) {
  switch (mode) {
    case RUCK_PROMPT_MODE_DOWN: return 1;
    case RUCK_PROMPT_MODE_BACK: return 2;
    default: return 0; // RESTORE, CHECKIN
  }
}

static void prv_ruck_prompt_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  s_ruck_prompt_selected_row = prv_ruck_prompt_resume_row(s_ruck_prompt_mode);
  prv_ruck_prompt_select();
}

static void prv_ruck_prompt_click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_BACK, prv_ruck_prompt_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_ruck_prompt_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_ruck_prompt_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_ruck_prompt_select_click_handler);
}

static void prv_ruck_prompt_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t pad = 8;
  const int16_t row_w = bounds.size.w - 2 * pad;

  static const char *k_titles_back[]    = { "Discard ruck", "Save ruck", "Resume ruck" };
  static const char *k_titles_down[]    = { "Save ruck", "Resume ruck", "Discard ruck" };
  static const char *k_titles_restore[] = { "Resume ruck", "Start new" };
  static const char *k_titles_checkin[] = { "Resume ruck", "Discard ruck", "Save ruck" };
  const char **titles;
  int row_count;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  int16_t row_h = 40;

  if (s_ruck_prompt_mode == RUCK_PROMPT_MODE_DOWN) {
    titles = k_titles_down; row_count = 3;
  } else if (s_ruck_prompt_mode == RUCK_PROMPT_MODE_RESTORE) {
    titles = k_titles_restore; row_count = 2;
  } else if (s_ruck_prompt_mode == RUCK_PROMPT_MODE_CHECKIN) {
    titles = k_titles_checkin; row_count = 3;
  } else {
    titles = k_titles_back; row_count = 3;
  }

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  for (int row = 0; row < row_count; ++row) {
    int16_t y = pad + row * (row_h + 6);
    bool selected = (row == s_ruck_prompt_selected_row);
    if (selected) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(pad, y, row_w, row_h), 4, GCornersAll);
      graphics_context_set_text_color(ctx, GColorBlack);
    } else {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, GRect(pad, y, row_w, row_h), 4, GCornersAll);
      graphics_context_set_text_color(ctx, GColorWhite);
    }
    graphics_draw_text(ctx, titles[row], font,
                       GRect(pad + 6, y + 3, row_w - 12, row_h - 6),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

static void prv_checkin_repeat_timer_callback(void *data) {
  (void)data;
  s_checkin_repeat_timer = NULL;
  s_checkin_repeat_elapsed_s += RUCK_CHECKIN_REPEAT_MS / 1000;
  if (s_checkin_repeat_elapsed_s >= RUCK_CHECKIN_REPEAT_TIMEOUT_S) {
    return;
  }
  vibes_double_pulse();
  s_checkin_repeat_timer = app_timer_register(RUCK_CHECKIN_REPEAT_MS, prv_checkin_repeat_timer_callback, NULL);
}

static void prv_ruck_prompt_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  s_ruck_prompt_selected_row = 0;
  if (s_ruck_prompt_mode != RUCK_PROMPT_MODE_DOWN && s_ruck_prompt_mode != RUCK_PROMPT_MODE_RESTORE &&
      s_ruck_prompt_mode != RUCK_PROMPT_MODE_CHECKIN) {
    s_ruck_prompt_mode = RUCK_PROMPT_MODE_BACK;
  }
  s_ruck_prompt_layer = layer_create(layer_get_bounds(window_layer));
  window_set_click_config_provider(window, prv_ruck_prompt_click_config_provider);
  layer_set_update_proc(s_ruck_prompt_layer, prv_ruck_prompt_layer_update_proc);
  layer_add_child(window_layer, s_ruck_prompt_layer);

  if (s_ruck_prompt_mode == RUCK_PROMPT_MODE_CHECKIN) {
    s_checkin_repeat_elapsed_s = 0;
    s_checkin_repeat_timer = app_timer_register(RUCK_CHECKIN_REPEAT_MS, prv_checkin_repeat_timer_callback, NULL);
  }
}

static void prv_ruck_prompt_window_unload(Window *window) {
  (void)window;
  if (s_checkin_repeat_timer) {
    app_timer_cancel(s_checkin_repeat_timer);
    s_checkin_repeat_timer = NULL;
  }
  layer_destroy(s_ruck_prompt_layer);
  s_ruck_prompt_layer = NULL;
}

static void prv_save_ruck(void) {
  prv_update_display();
  // Capture last-activity snapshot
  time_t now = time(NULL);
  s_last_activity_distance_m = s_session_distance_m;
  s_last_activity_calories   = s_session_calories;
  s_last_activity_pace_sec   = s_session_pace_sec;
  s_last_activity_duration_sec = (int32_t)((now - s_start_time) < 1 ? 1 : (now - s_start_time));
  s_last_activity_timestamp  = (int32_t)now;
  persist_write_int(LAST_ACTIVITY_DISTANCE_M_PERSIST_KEY, s_last_activity_distance_m);
  persist_write_int(LAST_ACTIVITY_CALORIES_PERSIST_KEY,   s_last_activity_calories);
  persist_write_int(LAST_ACTIVITY_PACE_SEC_PERSIST_KEY,   s_last_activity_pace_sec);
  persist_write_int(LAST_ACTIVITY_DURATION_SEC_PERSIST_KEY, s_last_activity_duration_sec);
  persist_write_int(LAST_ACTIVITY_TIMESTAMP_PERSIST_KEY,  s_last_activity_timestamp);
  // Commit session to lifetime totals
  prv_commit_session_totals("save");
  // Proactively push last activity + lifetime totals to phone JS
  prv_send_lifetime_totals(true);
  vibes_short_pulse();
  // Show brief status message then navigate to profile selection
  prv_show_status_message("Saving ruck...", 1500);
}

static void prv_main_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (!window_stack_contains_window(s_ruck_prompt_window)) {
    s_ruck_prompt_mode = RUCK_PROMPT_MODE_DOWN;
    window_stack_push(s_ruck_prompt_window, true);
  }
}


static void prv_status_timer_callback(void *context) {
  (void)context;
  s_status_timer = NULL;
  if (window_stack_contains_window(s_status_window)) {
    window_stack_remove(s_status_window, true);
  }
  if (s_post_save_stay) {
    s_post_save_stay = false;
    return;
  }
  window_stack_pop_all(true);
}

static void prv_status_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  // Black background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // --- Floppy disk icon (white, 36x36, horizontally centred) ---
  const int16_t icon_w = 36, icon_h = 36;
  const int16_t icon_x = (w - icon_w) / 2;
  const int16_t content_h = icon_h + 6 + 60; // icon + gap + text (~2 lines at 24+spacing)
  const int16_t start_y = (h - content_h) / 2;
  const int16_t iy = start_y;

  // Body
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(icon_x, iy, icon_w, icon_h), 2, GCornersAll);

  // Label cutout (top-left, with folded corner)
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(icon_x + 2, iy + 2, 18, 12), 0, GCornerNone);
  // Fold triangle: restore corner pixel (approximate the diagonal fold)
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(icon_x + 16, iy + 2, 4, 4), 0, GCornerNone);

  // Metal shutter (centred, lower portion of disk body)
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(icon_x + 9, iy + 19, 18, 13), 2, GCornersAll);
  // Shutter slot highlight
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(icon_x + 12, iy + 22, 12, 6), 1, GCornersAll);

  // --- Status text, centred below icon ---
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GRect text_rect = GRect(4, start_y + icon_h + 6, w - 8, 60);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_status_text, font, text_rect,
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void prv_status_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_status_layer = layer_create(bounds);
  layer_set_update_proc(s_status_layer, prv_status_layer_update_proc);
  layer_add_child(window_layer, s_status_layer);
}

static void prv_status_window_unload(Window *window) {
  (void)window;
  layer_destroy(s_status_layer);
  s_status_layer = NULL;
}


static void prv_main_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (!s_session_active) return;
  time_t now = time(NULL);
  if (s_session_paused) {
    s_pause_total_s += (int32_t)(now - s_pause_start_time);
    int32_t steps_since_baseline_at_pause = s_steps_at_pause - s_session_steps_offset;
    if (steps_since_baseline_at_pause < 0) {
      steps_since_baseline_at_pause = 0;
    }
    s_steps_baseline = prv_current_step_count(now) - steps_since_baseline_at_pause;
    s_last_time = 0;
    s_session_paused = false;
  } else {
    s_pause_start_time = now;
    int32_t steps_since_baseline = prv_current_step_count(now) - s_steps_baseline;
    if (steps_since_baseline < 0) {
      steps_since_baseline = 0;
    }
    s_steps_at_pause = s_session_steps_offset + steps_since_baseline;
    s_session_paused = true;
  }
  prv_update_display();
}

static void prv_main_click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_UP, prv_main_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_main_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_main_down_click_handler);
}

static void prv_profile_window_load(Window *window) {
  window_set_background_color(window, GColorBlack);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  int16_t usable_height = bounds.size.h - (2 * SCREEN_PADDING);
  s_profile_cell_height = (usable_height - ((PROFILE_COUNT - 1) * PROFILE_ROW_SEPARATOR_HEIGHT)) / PROFILE_COUNT;
  int16_t menu_height = usable_height;
  GRect menu_bounds = GRect(SCREEN_PADDING, SCREEN_PADDING,
                            bounds.size.w - (2 * SCREEN_PADDING), menu_height);
  s_profile_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_profile_menu_layer, window);
  window_set_click_config_provider(window, prv_profile_click_config_provider);
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
                                MenuRowAlignNone, false);
  prv_profile_reset_scroll_offset();
  layer_add_child(window_layer, menu_layer_get_layer(s_profile_menu_layer));
  if (touch_service_is_enabled()) {
    s_profile_touch_active = false;
    touch_service_subscribe(prv_profile_touch_handler, NULL);
  }
}

static void prv_profile_window_unload(Window *window) {
  (void)window;
  if (touch_service_is_enabled()) {
    touch_service_unsubscribe();
  }
  s_profile_touch_active = false;
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
  int w = bounds.size.w - (2 * SCREEN_PADDING);

  // Row Y-offsets below were tuned against emery's usable height (228 - 2*5 = 218).
  // Round platforms get extra top/bottom margin so the header row isn't squeezed at the
  // circle's pole; GRID_REFERENCE_H rescales the rest proportionally into what's left.
  // Gabbro (260px) has enough headroom that this holds without shrinking fonts/icons -
  // emery's numbers come out pixel-identical since scale(h=218) == 218/218 == 1.
  const int GRID_REFERENCE_H = 218;
  int y0 = PBL_IF_ROUND_ELSE(32, SCREEN_PADDING);
  int h = bounds.size.h - y0 - PBL_IF_ROUND_ELSE(28, SCREEN_PADDING);
#define SCALE_TO_H(px) ((int)(((int32_t)(px) * h) / GRID_REFERENCE_H))

  int top2_y = y0 + SCALE_TO_H(28);
  int mid_icon_y = y0 + SCALE_TO_H(70);
  int mid_value_y = y0 + SCALE_TO_H(90);
  int mid_center_value_y = mid_icon_y + SCALE_TO_H(22);
  int bottom_icon_y = y0 + SCALE_TO_H(144);
  int bottom_value_y = y0 + SCALE_TO_H(162);
  int bottom_secondary_y = y0 + SCALE_TO_H(188);
#undef SCALE_TO_H

  int top_x0 = x0, top_w = w;
  int mid_x0 = x0, mid_w = w;
  int bottom_x0 = x0, bottom_w = w;
#if defined(PBL_ROUND)
  int16_t rx, rw;
  prv_round_row_x((int16_t)y0, (int16_t)(top2_y + 26), 3, &rx, &rw);
  top_x0 = rx; top_w = rw;
  prv_round_row_x((int16_t)mid_icon_y, (int16_t)(mid_value_y + 36), 3, &rx, &rw);
  mid_x0 = rx; mid_w = rw;
  prv_round_row_x((int16_t)bottom_icon_y, (int16_t)(bottom_secondary_y + 28), 3, &rx, &rw);
  bottom_x0 = rx; bottom_w = rw;
#endif
  int header_name_w = (top_w * 2) / 3;

  window_set_background_color(window, GColorBlack);
  window_set_click_config_provider(window, prv_main_click_config_provider);

  s_grid_layer = layer_create(GRect(x0, y0, w, h));
  layer_set_update_proc(s_grid_layer, prv_grid_layer_update_proc);
  layer_add_child(window_layer, s_grid_layer);

  s_top_time_layer = text_layer_create(GRect(top_x0, y0, header_name_w, 30));
  s_top_left_layer = text_layer_create(GRect(top_x0 + header_name_w, y0, top_w - header_name_w, 30));
  s_top_right_layer = text_layer_create(GRect(top_x0, top2_y, top_w / 2, 26));
  s_top_stats_right_layer = text_layer_create(GRect(top_x0 + (top_w / 2), top2_y, top_w - (top_w / 2), 26));

  s_mid_left_icon_layer = bitmap_layer_create(GRect(mid_x0 + (mid_w / 4) - 12, mid_icon_y, 24, 24));
  s_mid_left_value_layer = text_layer_create(GRect(mid_x0, mid_value_y, mid_w / 2, 36));
  s_mid_center_icon_layer = bitmap_layer_create(GRect(mid_x0 + (mid_w / 2) - 12, mid_icon_y, 24, 24));
  s_mid_center_value_layer = text_layer_create(GRect(mid_x0 + (mid_w / 2) - 24, mid_center_value_y, 48, 30));
  s_mid_right_icon_layer = bitmap_layer_create(GRect(mid_x0 + ((mid_w * 3) / 4) - 12, mid_icon_y, 24, 24));
  s_mid_right_value_layer = text_layer_create(GRect(mid_x0 + (mid_w / 2), mid_value_y, mid_w - (mid_w / 2), 36));

  s_bottom_left_icon_layer = bitmap_layer_create(GRect(bottom_x0 + (bottom_w / 4) - 12, bottom_icon_y, 24, 24));
  s_bottom_left_value_layer = text_layer_create(GRect(bottom_x0, bottom_value_y, bottom_w / 2, 28));
  s_bottom_left_secondary_layer = text_layer_create(GRect(bottom_x0, bottom_secondary_y, bottom_w / 2, 28));
  s_bottom_right_icon_layer = bitmap_layer_create(GRect(bottom_x0 + ((bottom_w * 3) / 4) - 12, bottom_icon_y, 24, 24));
  s_bottom_right_value_layer = text_layer_create(GRect(bottom_x0 + (bottom_w / 2), bottom_value_y, bottom_w - (bottom_w / 2), 28));
  s_bottom_right_secondary_layer = text_layer_create(GRect(bottom_x0 + (bottom_w / 2), bottom_secondary_y, bottom_w - (bottom_w / 2), 28));

  s_runner_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_RUNNER);
  s_heart_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_HEART);
  s_timer_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_TIMER);
  s_steps_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_STEPS);
  s_fire_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_FIRE);
  bitmap_layer_set_bitmap(s_mid_left_icon_layer, s_runner_icon);
  bitmap_layer_set_bitmap(s_mid_center_icon_layer, PBL_IF_ROUND_ELSE(s_steps_icon, s_heart_icon));
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
  prv_set_text_style(s_top_right_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentLeft, GColorWhite);
  prv_set_text_style(s_top_stats_right_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentRight, GColorWhite);
  prv_set_text_style(s_mid_left_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_mid_center_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_mid_right_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_bottom_left_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_bottom_left_secondary_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28), GTextAlignmentCenter, GColorLightGray);
  prv_set_text_style(s_bottom_right_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentCenter, GColorWhite);
  prv_set_text_style(s_bottom_right_secondary_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28), GTextAlignmentCenter, GColorLightGray);

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

  // Blue pause icon — full-width layer centred over the bottom three rows; hidden until paused
  s_paused_icon_layer = layer_create(GRect(x0, y0 + 55, w, 110));
  layer_set_update_proc(s_paused_icon_layer, prv_paused_icon_layer_update_proc);
  layer_set_hidden(s_paused_icon_layer, true);
  layer_add_child(window_layer, s_paused_icon_layer);
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
  layer_destroy(s_paused_icon_layer);
  s_paused_icon_layer = NULL;
}

static void prv_schedule_checkin_wakeup(void) {
  WakeupId id = wakeup_schedule(time(NULL) + RUCK_CHECKIN_INTERVAL_S, 0, true);
  if (id < 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "wakeup_schedule failed: %d", (int)id);
  }
}

static void prv_init(void) {
  AppLaunchReason launch_reason_val = launch_reason();
  wakeup_cancel_all();

  prv_load_settings();
  if (persist_exists(APP_STATE_SCHEMA_VERSION_PERSIST_KEY)) {
    int32_t stored_version = persist_read_int(APP_STATE_SCHEMA_VERSION_PERSIST_KEY);
    if (stored_version != APP_STATE_SCHEMA_VERSION) {
      s_lifetime_distance_m = 0;
      s_lifetime_calories = 0;
      s_last_activity_distance_m = 0;
      s_last_activity_calories = 0;
      s_last_activity_pace_sec = 0;
      s_last_activity_duration_sec = 0;
      s_last_activity_timestamp = 0;
      persist_write_int(LIFETIME_DISTANCE_M_PERSIST_KEY, s_lifetime_distance_m);
      persist_write_int(LIFETIME_CALORIES_PERSIST_KEY, s_lifetime_calories);
      persist_write_int(LAST_ACTIVITY_DISTANCE_M_PERSIST_KEY, s_last_activity_distance_m);
      persist_write_int(LAST_ACTIVITY_CALORIES_PERSIST_KEY, s_last_activity_calories);
      persist_write_int(LAST_ACTIVITY_PACE_SEC_PERSIST_KEY, s_last_activity_pace_sec);
      persist_write_int(LAST_ACTIVITY_DURATION_SEC_PERSIST_KEY, s_last_activity_duration_sec);
      persist_write_int(LAST_ACTIVITY_TIMESTAMP_PERSIST_KEY, s_last_activity_timestamp);
    }
  } else {
    persist_write_int(APP_STATE_SCHEMA_VERSION_PERSIST_KEY, APP_STATE_SCHEMA_VERSION);
  }

  if (persist_exists(LIFETIME_DISTANCE_M_PERSIST_KEY)) {
    s_lifetime_distance_m = persist_read_int(LIFETIME_DISTANCE_M_PERSIST_KEY);
  }
  if (persist_exists(LIFETIME_CALORIES_PERSIST_KEY)) {
    s_lifetime_calories = persist_read_int(LIFETIME_CALORIES_PERSIST_KEY);
  }
  if (persist_exists(LAST_ACTIVITY_DISTANCE_M_PERSIST_KEY)) {
    s_last_activity_distance_m = persist_read_int(LAST_ACTIVITY_DISTANCE_M_PERSIST_KEY);
  }
  if (persist_exists(LAST_ACTIVITY_CALORIES_PERSIST_KEY)) {
    s_last_activity_calories = persist_read_int(LAST_ACTIVITY_CALORIES_PERSIST_KEY);
  }
  if (persist_exists(LAST_ACTIVITY_PACE_SEC_PERSIST_KEY)) {
    s_last_activity_pace_sec = persist_read_int(LAST_ACTIVITY_PACE_SEC_PERSIST_KEY);
  }
  if (persist_exists(LAST_ACTIVITY_DURATION_SEC_PERSIST_KEY)) {
    s_last_activity_duration_sec = persist_read_int(LAST_ACTIVITY_DURATION_SEC_PERSIST_KEY);
  }
  if (persist_exists(LAST_ACTIVITY_TIMESTAMP_PERSIST_KEY)) {
    s_last_activity_timestamp = persist_read_int(LAST_ACTIVITY_TIMESTAMP_PERSIST_KEY);
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

  s_ruck_prompt_window = window_create();
  window_set_window_handlers(s_ruck_prompt_window, (WindowHandlers) {
    .load = prv_ruck_prompt_window_load,
    .unload = prv_ruck_prompt_window_unload,
  });

  s_status_window = window_create();
  window_set_background_color(s_status_window, GColorBlack);
  window_set_window_handlers(s_status_window, (WindowHandlers) {
    .load = prv_status_window_load,
    .unload = prv_status_window_unload,
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
  prv_ensure_health_subscription(now);

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_register_inbox_dropped(prv_inbox_dropped_handler);
  app_message_register_outbox_failed(prv_outbox_failed_handler);
  app_message_open(1024, 512);
  APP_LOG(APP_LOG_LEVEL_INFO, "App initialized, waiting for config updates");

  window_stack_push(s_window, false);
  window_stack_push(s_profile_window, true);

  // If the app was killed mid-session (e.g. by a notification), jump straight
  // to the restore prompt rather than leaving the user on the profile screen.
  // A wakeup relaunch gets the "still rucking?" check-in prompt instead of
  // the restore prompt so the user can end the ruck without fully resuming it.
  if (prv_has_resumable_session_for_profile(prv_active_profile_index())) {
    s_ruck_prompt_mode = (launch_reason_val == APP_LAUNCH_WAKEUP) ?
        RUCK_PROMPT_MODE_CHECKIN : RUCK_PROMPT_MODE_RESTORE;
    window_stack_push(s_ruck_prompt_window, true);
    if (launch_reason_val == APP_LAUNCH_WAKEUP) {
      vibes_double_pulse();
    }
  }
}

static void prv_deinit(void) {
  if (s_session_active) {
    prv_save_in_progress_session();
    prv_schedule_checkin_wakeup();
  } else {
    prv_commit_session_totals("deinit");
  }
  tick_timer_service_unsubscribe();
  if (s_health_subscribed) {
    health_service_events_unsubscribe();
    s_health_subscribed = false;
  }
  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
    s_status_timer = NULL;
  }
  window_destroy(s_status_window);
  window_destroy(s_ruck_prompt_window);
  window_destroy(s_profile_window);
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
