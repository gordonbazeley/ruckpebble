#include <pebble.h>

#ifndef MESSAGE_KEY_sim_steps_enabled
#define MESSAGE_KEY_sim_steps_enabled 8
#define MESSAGE_KEY_sim_steps_spm 9
#endif

typedef struct {
  int32_t weight_value;       // tenths
  int32_t weight_unit;        // 0=kg, 1=lb
  int32_t ruck_weight_value;  // tenths
  int32_t ruck_weight_unit;   // 0=kg, 1=lb
  int32_t stride_value;       // tenths
  int32_t stride_unit;        // 0=cm, 1=in
  int32_t terrain_factor;     // hundredths
  int32_t grade_percent;      // tenths
  int32_t sim_steps_enabled;  // 0/1
  int32_t sim_steps_spm;      // steps/min
} Settings;

enum {
  SETTINGS_PERSIST_KEY = 1
};

static const Settings SETTINGS_DEFAULTS = {
  .weight_value = 800,
  .weight_unit = 0,
  .ruck_weight_value = 300,
  .ruck_weight_unit = 1,
  .stride_value = 780,
  .stride_unit = 0,
  .terrain_factor = 100,
  .grade_percent = 0,
  .sim_steps_enabled = 1,
  .sim_steps_spm = 122
  /* .sim_steps_spm = 488 */
};

static Window *s_window;
static TextLayer *s_distance_layer;
static TextLayer *s_pace_layer;
static TextLayer *s_steps_layer;
static TextLayer *s_calories_layer;
static TextLayer *s_total_layer;

static Settings s_settings;
static time_t s_start_time;
static bool s_health_available = false;
static time_t s_day_start;
static int32_t s_steps_baseline = 0;
static int32_t s_last_steps = 0;
static time_t s_last_time = 0;
static int64_t s_speed_mmps = 0;

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

static int64_t prv_grade_q(void) {
  return (int64_t)s_settings.grade_percent * 100;
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
  int64_t mu_q = s_settings.terrain_factor; // scale 1e2
  int64_t term3 = (total * inner_q * mu_q) / (100 * 1000000);

  return term1 + term2 + term3;
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
  time_t now = time(NULL);
  int64_t elapsed_s = (int64_t)(now - s_start_time);
  if (elapsed_s < 1) {
    elapsed_s = 1;
  }

  int32_t steps = 0;
  if (s_settings.sim_steps_enabled) {
    steps = (int32_t)((elapsed_s * (int64_t)s_settings.sim_steps_spm) / 60);
  } else if (s_health_available) {
    int32_t steps_total = (int32_t)health_service_sum(HealthMetricStepCount, s_day_start, now);
    steps = steps_total - s_steps_baseline;
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
  if (delta_s >= 5) {
    int32_t delta_steps = steps - s_last_steps;
    if (delta_steps < 0) {
      delta_steps = 0;
    }
    speed_mmps = (int64_t)delta_steps * stride_mm / delta_s;
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

  int64_t weight_kg1000 = prv_weight_to_kg1000(s_settings.weight_value, s_settings.weight_unit);
  int64_t load_kg1000 = prv_weight_to_kg1000(s_settings.ruck_weight_value, s_settings.ruck_weight_unit);
  int64_t metabolic_mw = prv_pandolf_metabolic_mw(weight_kg1000, load_kg1000, speed_mmps);
  int64_t kcal_per_hour = (metabolic_mw * 3600) / 4184 / 1000;
  int64_t kcal_total = (kcal_per_hour * elapsed_s) / 3600;

  static char distance_buf[32];
  static char pace_buf[32];
  static char steps_buf[32];
  static char cal_buf[32];
  static char total_buf[32];

  int64_t dist_int = distance_x100 / 100;
  int64_t dist_frac = distance_x100 % 100;
  if (dist_frac < 0) {
    dist_frac = -dist_frac;
  }
  snprintf(distance_buf, sizeof(distance_buf), "Dist: %ld.%02ld %s",
           (long)dist_int, (long)dist_frac, distance_unit_label);
  if (pace_sec > 0) {
    int pace_min = (int)(pace_sec / 60);
    int pace_rem = (int)(pace_sec % 60);
    snprintf(pace_buf, sizeof(pace_buf), "Pace: %d:%02d /%s",
             pace_min, pace_rem, distance_unit_label);
  } else {
    snprintf(pace_buf, sizeof(pace_buf), "Pace: --");
  }

  if (s_settings.sim_steps_enabled) {
    snprintf(steps_buf, sizeof(steps_buf), "Steps: %ld (sim)", (long)steps);
  } else if (s_health_available) {
    snprintf(steps_buf, sizeof(steps_buf), "Steps: %ld", (long)steps);
  } else {
    snprintf(steps_buf, sizeof(steps_buf), "Steps: N/A");
  }
  snprintf(cal_buf, sizeof(cal_buf), "Cal/h: %ld", (long)kcal_per_hour);
  snprintf(total_buf, sizeof(total_buf), "Total: %ld", (long)kcal_total);

  text_layer_set_text(s_distance_layer, distance_buf);
  text_layer_set_text(s_pace_layer, pace_buf);
  text_layer_set_text(s_steps_layer, steps_buf);
  text_layer_set_text(s_calories_layer, cal_buf);
  text_layer_set_text(s_total_layer, total_buf);
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
  t = dict_find(iter, MESSAGE_KEY_ruck_weight_value);
  if (t) {
    s_settings.ruck_weight_value = t->value->int32;
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
  t = dict_find(iter, MESSAGE_KEY_terrain_factor);
  if (t) {
    s_settings.terrain_factor = t->value->int32;
  }
  t = dict_find(iter, MESSAGE_KEY_grade_percent);
  if (t) {
    s_settings.grade_percent = t->value->int32;
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
  prv_update_display();
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_distance_layer = text_layer_create(GRect(4, 6, bounds.size.w - 8, 22));
  s_pace_layer = text_layer_create(GRect(4, 28, bounds.size.w - 8, 22));
  s_steps_layer = text_layer_create(GRect(4, 50, bounds.size.w - 8, 22));
  s_calories_layer = text_layer_create(GRect(4, 72, bounds.size.w - 8, 22));
  s_total_layer = text_layer_create(GRect(4, 94, bounds.size.w - 8, 22));

  text_layer_set_font(s_distance_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_font(s_pace_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_font(s_steps_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_font(s_calories_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_font(s_total_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  text_layer_set_text_alignment(s_distance_layer, GTextAlignmentLeft);
  text_layer_set_text_alignment(s_pace_layer, GTextAlignmentLeft);
  text_layer_set_text_alignment(s_steps_layer, GTextAlignmentLeft);
  text_layer_set_text_alignment(s_calories_layer, GTextAlignmentLeft);
  text_layer_set_text_alignment(s_total_layer, GTextAlignmentLeft);

  layer_add_child(window_layer, text_layer_get_layer(s_distance_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_pace_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_steps_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_calories_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_total_layer));
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_distance_layer);
  text_layer_destroy(s_pace_layer);
  text_layer_destroy(s_steps_layer);
  text_layer_destroy(s_calories_layer);
  text_layer_destroy(s_total_layer);
}

static void prv_init(void) {
  prv_load_settings();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  s_start_time = time(NULL);
  struct tm *start_tm = localtime(&s_start_time);
  start_tm->tm_hour = 0;
  start_tm->tm_min = 0;
  start_tm->tm_sec = 0;
  s_day_start = mktime(start_tm);
  HealthServiceAccessibilityMask access = health_service_metric_accessible(HealthMetricStepCount, s_start_time, s_start_time);
  s_health_available = (access & HealthServiceAccessibilityMaskAvailable);
  if (s_health_available) {
    s_steps_baseline = (int32_t)health_service_sum(HealthMetricStepCount, s_day_start, s_start_time);
    health_service_events_subscribe(prv_health_handler, NULL);
  }

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(256, 64);

  window_stack_push(s_window, true);
  prv_update_display();
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  if (s_health_available) {
    health_service_events_unsubscribe();
  }
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
