#include <pebble.h>

typedef struct {
  int32_t weight_value;       // tenths
  int32_t weight_unit;        // 0=kg, 1=lb
  int32_t ruck_weight_value;  // tenths
  int32_t ruck_weight_unit;   // 0=kg, 1=lb
  int32_t stride_value;       // tenths
  int32_t stride_unit;        // 0=cm, 1=in
  int32_t terrain_factor;     // hundredths
  int32_t grade_percent;      // tenths
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
  .grade_percent = 0
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

static double prv_weight_to_kg(int32_t value_tenths, int32_t unit) {
  double value = (double)value_tenths / 10.0;
  if (unit == 1) {
    return value * 0.453592;
  }
  return value;
}

static double prv_stride_to_m(int32_t value_tenths, int32_t unit) {
  double value = (double)value_tenths / 10.0;
  if (unit == 1) {
    return value * 0.0254;
  }
  return value / 100.0;
}

static double prv_grade_fraction(void) {
  return (double)s_settings.grade_percent / 1000.0;
}

static double prv_terrain_factor(void) {
  return (double)s_settings.terrain_factor / 100.0;
}

static double prv_pandolf_metabolic_rate(double weight_kg, double load_kg, double speed_mps) {
  if (weight_kg <= 0.0) {
    return 0.0;
  }
  double grade = prv_grade_fraction();
  double mu = prv_terrain_factor();
  double total = weight_kg + load_kg;
  double load_ratio = load_kg / weight_kg;
  double term1 = 1.5 * weight_kg;
  double term2 = 2.0 * total * load_ratio * load_ratio;
  double term3 = mu * total * (1.5 * speed_mps * speed_mps + 0.35 * speed_mps * grade);
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
  double elapsed_s = difftime(now, s_start_time);
  if (elapsed_s < 1.0) {
    elapsed_s = 1.0;
  }

  int32_t steps = 0;
  if (s_health_available) {
    steps = (int32_t)health_service_sum(HealthMetricStepCount, s_start_time, now);
  }

  double stride_m = prv_stride_to_m(s_settings.stride_value, s_settings.stride_unit);
  double distance_m = steps * stride_m;
  double speed_mps = distance_m / elapsed_s;

  bool use_imperial = (s_settings.weight_unit == 1);
  double distance_unit = use_imperial ? (distance_m / 1609.344) : (distance_m / 1000.0);
  const char *distance_unit_label = use_imperial ? "mi" : "km";

  double pace_sec = 0.0;
  if (distance_unit > 0.0001) {
    pace_sec = elapsed_s / distance_unit;
  }

  double weight_kg = prv_weight_to_kg(s_settings.weight_value, s_settings.weight_unit);
  double load_kg = prv_weight_to_kg(s_settings.ruck_weight_value, s_settings.ruck_weight_unit);
  double metabolic_watts = prv_pandolf_metabolic_rate(weight_kg, load_kg, speed_mps);
  double kcal_per_hour = metabolic_watts * 3600.0 / 4184.0;
  double kcal_total = kcal_per_hour * (elapsed_s / 3600.0);

  static char distance_buf[32];
  static char pace_buf[32];
  static char steps_buf[32];
  static char cal_buf[32];
  static char total_buf[32];

  snprintf(distance_buf, sizeof(distance_buf), "Dist: %.2f %s", distance_unit, distance_unit_label);
  if (pace_sec > 0.0) {
    int pace_min = (int)(pace_sec / 60.0);
    int pace_rem = (int)pace_sec % 60;
    snprintf(pace_buf, sizeof(pace_buf), "Pace: %d:%02d /%s", pace_min, pace_rem, distance_unit_label);
  } else {
    snprintf(pace_buf, sizeof(pace_buf), "Pace: --");
  }

  if (s_health_available) {
    snprintf(steps_buf, sizeof(steps_buf), "Steps: %ld", (long)steps);
  } else {
    snprintf(steps_buf, sizeof(steps_buf), "Steps: N/A");
  }
  snprintf(cal_buf, sizeof(cal_buf), "Cal/h: %.0f", kcal_per_hour);
  snprintf(total_buf, sizeof(total_buf), "Total: %.0f", kcal_total);

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
  HealthServiceAccessibilityMask access = health_service_metric_accessible(HealthMetricStepCount, s_start_time, s_start_time);
  s_health_available = (access & HealthServiceAccessibilityMaskAvailable);
  if (s_health_available) {
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
