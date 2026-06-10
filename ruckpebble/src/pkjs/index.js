/* Settings page for Pebble app with shared settings + 3 profiles. */
(function() {
  var SETTINGS_KEY = 'ruck_settings_v2';
  var KEY_INSERT_TIMELINE_PIN = 10020;
  var KEY_REQUEST_LIFETIME_TOTALS = 10022;
  var KEY_LIFETIME_DISTANCE_M_TOTAL = 10023;
  var KEY_LIFETIME_CALORIES_TOTAL = 10024;
  var KEY_LAST_ACTIVITY_DISTANCE_M = 10025;
  var KEY_LAST_ACTIVITY_CALORIES = 10026;
  var KEY_LAST_ACTIVITY_PACE_SEC = 10027;
  var KEY_LAST_ACTIVITY_DURATION_SEC = 10028;
  var KEY_LAST_ACTIVITY_TIMESTAMP = 10029;

  var defaults = {
    weight_value: 800,
    weight_unit: 0,
    stride_length_value: 780,
    stride_length_unit: 0,
    measurement_unit: 0,
    ruck_weight_unit: 0,

    profile1_ruck_weight_value: 136,
    profile1_terrain_factor: 100,
    profile1_terrain_type: 'road',
    profile1_grade_percent: 0,
    profile1_name: '30lb, road',

    profile2_ruck_weight_value: 68,
    profile2_terrain_factor: 100,
    profile2_terrain_type: 'road',
    profile2_grade_percent: 0,
    profile2_name: '15lb, road',

    profile3_ruck_weight_value: 136,
    profile3_terrain_factor: 120,
    profile3_terrain_type: 'gravel',
    profile3_grade_percent: 20,
    profile3_name: '30lb, trail',
    lifetime_distance_m_total: 0,
    lifetime_calories_total: 0,
    last_activity_distance_m: 0,
    last_activity_calories: 0,
    last_activity_pace_sec: 0,
    last_activity_duration_sec: 0,
    last_activity_timestamp: 0
  };
  var s_waitingLifetimeCallback = null;
  var s_latestSettingsSnapshot = null;

  function assignObjects() {
    var out = {};
    for (var i = 0; i < arguments.length; i += 1) {
      var src = arguments[i] || {};
      for (var key in src) {
        if (hasOwn(src, key)) {
          out[key] = src[key];
        }
      }
    }
    return out;
  }

  function loadSettings() {
    var raw = localStorage.getItem(SETTINGS_KEY);
    if (!raw) {
      return defaults;
    }
    try {
      return assignObjects(defaults, JSON.parse(raw));
    } catch (e) {
      return defaults;
    }
  }

  function saveSettings(settings) {
    localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
  }


  function hasOwn(obj, key) {
    return Object.prototype.hasOwnProperty.call(obj, key);
  }

  function hasAny(obj, keys) {
    for (var i = 0; i < keys.length; i += 1) {
      if (hasOwn(obj, keys[i])) {
        return true;
      }
    }
    return false;
  }

  function toInt(value, fallback) {
    var n = parseInt(value, 10);
    if (isNaN(n)) {
      return fallback;
    }
    return n;
  }

  function readIntFromPayload(payload, nameKey, numericKey, fallback) {
    if (hasOwn(payload, nameKey)) {
      return toInt(payload[nameKey], fallback);
    }
    if (hasOwn(payload, String(numericKey))) {
      return toInt(payload[String(numericKey)], fallback);
    }
    return fallback;
  }

  function normalizeSettings(settings) {
    var out = assignObjects(defaults, settings || {});
    out.profile1_terrain_type = terrainTypeFromSettings(out.profile1_terrain_type, out.profile1_terrain_factor);
    out.profile2_terrain_type = terrainTypeFromSettings(out.profile2_terrain_type, out.profile2_terrain_factor);
    out.profile3_terrain_type = terrainTypeFromSettings(out.profile3_terrain_type, out.profile3_terrain_factor);
    out.profile1_terrain_factor = terrainFactorFromType(out.profile1_terrain_type);
    out.profile2_terrain_factor = terrainFactorFromType(out.profile2_terrain_type);
    out.profile3_terrain_factor = terrainFactorFromType(out.profile3_terrain_type);
    out.weight_value = parseInt(out.weight_value, 10) || 0;
    out.stride_length_value = parseInt(out.stride_length_value, 10) || 0;
    var measurementUnit = parseInt(out.measurement_unit, 10);
    if (isNaN(measurementUnit)) {
      measurementUnit = parseInt(out.weight_unit, 10);
    }
    if (isNaN(measurementUnit)) {
      measurementUnit = parseInt(out.stride_length_unit, 10);
    }
    if (isNaN(measurementUnit)) {
      measurementUnit = 0;
    }
    out.measurement_unit = measurementUnit;
    out.profile1_ruck_weight_value = parseInt(out.profile1_ruck_weight_value, 10) || 0;
    out.profile2_ruck_weight_value = parseInt(out.profile2_ruck_weight_value, 10) || 0;
    out.profile3_ruck_weight_value = parseInt(out.profile3_ruck_weight_value, 10) || 0;
    out.ruck_weight_unit = parseInt(out.ruck_weight_unit, 10) || 0;
    out.weight_unit = out.measurement_unit;
    out.stride_length_unit = out.measurement_unit;
    out.lifetime_distance_m_total = parseInt(out.lifetime_distance_m_total, 10) || 0;
    out.lifetime_calories_total = parseInt(out.lifetime_calories_total, 10) || 0;
    out.last_activity_distance_m = parseInt(out.last_activity_distance_m, 10) || 0;
    out.last_activity_calories = parseInt(out.last_activity_calories, 10) || 0;
    out.last_activity_pace_sec = parseInt(out.last_activity_pace_sec, 10) || 0;
    out.last_activity_duration_sec = parseInt(out.last_activity_duration_sec, 10) || 0;
    out.last_activity_timestamp = parseInt(out.last_activity_timestamp, 10) || 0;
    return out;
  }

  function syncSettingsToWatch(settings) {
    var normalized = normalizeSettings(settings);
    s_latestSettingsSnapshot = normalized;
    saveSettings(normalized);
    // Strip read-only stats — the watch owns those values and never reads them
    // from phone→watch messages.
    var watchMsg = {
      weight_value:              normalized.weight_value,
      measurement_unit:          normalized.measurement_unit,
      weight_unit:               0,
      stride_length_value:       normalized.stride_length_value,
      stride_length_unit:        0,
      profile1_ruck_weight_value: normalized.profile1_ruck_weight_value,
      profile1_terrain_factor:   normalized.profile1_terrain_factor,
      profile1_terrain_type:     normalized.profile1_terrain_type,
      profile1_grade_percent:    normalized.profile1_grade_percent,
      profile1_name:             normalized.profile1_name,
      profile2_ruck_weight_value: normalized.profile2_ruck_weight_value,
      profile2_terrain_factor:   normalized.profile2_terrain_factor,
      profile2_terrain_type:     normalized.profile2_terrain_type,
      profile2_grade_percent:    normalized.profile2_grade_percent,
      profile2_name:             normalized.profile2_name,
      profile3_ruck_weight_value: normalized.profile3_ruck_weight_value,
      profile3_terrain_factor:   normalized.profile3_terrain_factor,
      profile3_terrain_type:     normalized.profile3_terrain_type,
      profile3_grade_percent:    normalized.profile3_grade_percent,
      profile3_name:             normalized.profile3_name,
    };
    Pebble.sendAppMessage(watchMsg, function() {
      console.log('send settings success');
    }, function(err) {
      console.log('send settings failed:', JSON.stringify(err));
    });
  }

  function terrainFactorFromType(type) {
    switch (type) {
      case 'road': return 100;
      case 'gravel': return 120;
      case 'mixed': return 130;
      case 'sand': return 150;
      case 'snow': return 150;
      default: return 130;
    }
  }

  function terrainTypeFromSettings(type, factor) {
    if (type === 'road' || type === 'gravel' || type === 'mixed' || type === 'sand' || type === 'snow') {
      return type;
    }
    if (factor <= 110) {
      return 'road';
    }
    if (factor <= 125) {
      return 'gravel';
    }
    if (factor <= 140) {
      return 'mixed';
    }
    return 'sand';
  }

  function requestLifetimeTotals(onComplete) {
    var done = false;
    var timeoutId = null;
    var attempts = 0;
    var maxAttempts = 3;
    var attemptTimeoutMs = 1200;
    function finish() {
      if (done) {
        return;
      }
      done = true;
      if (timeoutId) {
        clearTimeout(timeoutId);
        timeoutId = null;
      }
      s_waitingLifetimeCallback = null;
      if (onComplete) {
        onComplete();
      }
    }
    function scheduleAttempt() {
      if (done) {
        return;
      }
      attempts += 1;
      var msg = {};
      msg.request_lifetime_totals = 1;
      msg[String(KEY_REQUEST_LIFETIME_TOTALS)] = 1;
      Pebble.sendAppMessage(msg, function() {
        // Wait for appmessage response. Retry timer handles missed responses.
      }, function() {
        if (attempts >= maxAttempts) {
          finish();
          return;
        }
      });
      timeoutId = setTimeout(function() {
        if (done) {
          return;
        }
        if (attempts >= maxAttempts) {
          finish();
          return;
        }
        scheduleAttempt();
      }, attemptTimeoutMs);
    }
    s_waitingLifetimeCallback = finish;
    scheduleAttempt();
  }


  function openConfig(settingsSnapshot) {
    var s = normalizeSettings(settingsSnapshot || s_latestSettingsSnapshot || loadSettings());
    Pebble.openURL('https://gordonbazeley.github.io/ruckpebble/config.html?data=' + encodeURIComponent(JSON.stringify(s)));
  }
  Pebble.addEventListener('showConfiguration', function() {
    console.log('showConfiguration event');
    openConfig(s_latestSettingsSnapshot);
  });

  Pebble.addEventListener('ready', function() {
    console.log('ready: syncing settings to watch');
    s_latestSettingsSnapshot = normalizeSettings(loadSettings());
    syncSettingsToWatch(s_latestSettingsSnapshot);
    requestLifetimeTotals();
  });

  function buildTimelinePin(activity) {
    var ts = activity.last_activity_timestamp;
    var pinId = 'ruck-' + ts;
    var pinTime = new Date((Date.now() + 120000)).toISOString();

    var distKm = (activity.last_activity_distance_m / 1000).toFixed(2);
    var cal = activity.last_activity_calories;
    var paceSec = activity.last_activity_pace_sec;
    var durationSec = parseInt(activity.last_activity_duration_sec, 10) || 0;
    var bodyLines = [];
    bodyLines.push(distKm + ' km' + (cal > 0 ? ' \u00b7 ' + cal + ' kcal' : ''));
    if (paceSec > 0) {
      bodyLines.push('Pace: ' + Math.floor(paceSec / 60) + ':' + ('0' + (paceSec % 60)).slice(-2) + ' / km');
    }
    if (durationSec > 0) {
      var durationMin = Math.max(1, Math.round(durationSec / 60));
      bodyLines.push(durationMin + (durationMin === 1 ? ' minute' : ' minutes'));
    }

    var titleMin = Math.max(1, Math.round(durationSec / 60));
    var layout = { type: 'genericPin', title: 'Ruck: ' + titleMin + (titleMin === 1 ? ' minute' : ' minutes') };
    if (bodyLines.length) { layout.body = bodyLines.join('\n'); }
    return { id: pinId, time: pinTime, layout: layout };
  }

  function insertTimelinePin(activity) {
    function sendTimelineStatus(text) {
      Pebble.sendAppMessage({ timeline_status_text: text }, function() {
        console.log('timeline status sent:', text);
      }, function(err) {
        console.log('timeline status send failed:', text, JSON.stringify(err));
      });
    }

    var pin = buildTimelinePin(activity);
    console.log('timeline pin payload:', JSON.stringify(pin));

    Pebble.getTimelineToken(function(token) {
      var xhr = new XMLHttpRequest();
      xhr.open('PUT', 'https://timeline-api.getpebble.com/v1/user/pins/' + pin.id, true);
      xhr.setRequestHeader('Content-Type', 'application/json');
      xhr.setRequestHeader('X-User-Token', token);
      xhr.onload = function() {
        console.log('timeline pin response:', xhr.status, xhr.responseText);
        if (xhr.status >= 200 && xhr.status < 300) {
          sendTimelineStatus('Ruck saved to timeline and phone settings');
        } else {
          sendTimelineStatus('Timeline HTTP ' + xhr.status);
        }
      };
      xhr.onerror = function() {
        console.log('timeline pin request error');
        sendTimelineStatus('Timeline request failed');
      };
      xhr.send(JSON.stringify(pin));
    }, function(err) {
      console.log('getTimelineToken failed:', err);
      console.log('timeline debug pin retained locally:', JSON.stringify(pin));
      sendTimelineStatus('Pin cached locally');
    });
  }

  Pebble.addEventListener('appmessage', function(e) {
    var payload = (e && e.payload) ? e.payload : {};
    console.log('appmessage payload:', JSON.stringify(payload));
    if (hasAny(payload, [
      'insert_timeline_pin',
      'lifetime_distance_m_total', 'lifetime_calories_total',
      'last_activity_distance_m', 'last_activity_calories',
      'last_activity_pace_sec', 'last_activity_duration_sec', 'last_activity_timestamp',
      String(KEY_INSERT_TIMELINE_PIN),
      String(KEY_LIFETIME_DISTANCE_M_TOTAL), String(KEY_LIFETIME_CALORIES_TOTAL),
      String(KEY_LAST_ACTIVITY_DISTANCE_M), String(KEY_LAST_ACTIVITY_CALORIES),
      String(KEY_LAST_ACTIVITY_PACE_SEC), String(KEY_LAST_ACTIVITY_DURATION_SEC),
      String(KEY_LAST_ACTIVITY_TIMESTAMP)
    ])) {
      var s = loadSettings();
      var prevTimestamp = s.last_activity_timestamp || 0;
      var insertTimelinePinRequested = readIntFromPayload(payload, 'insert_timeline_pin', KEY_INSERT_TIMELINE_PIN, 0) === 1;
      s.lifetime_distance_m_total = readIntFromPayload(payload, 'lifetime_distance_m_total', KEY_LIFETIME_DISTANCE_M_TOTAL, s.lifetime_distance_m_total || 0);
      s.lifetime_calories_total = readIntFromPayload(payload, 'lifetime_calories_total', KEY_LIFETIME_CALORIES_TOTAL, s.lifetime_calories_total || 0);
      s.last_activity_distance_m = readIntFromPayload(payload, 'last_activity_distance_m', KEY_LAST_ACTIVITY_DISTANCE_M, s.last_activity_distance_m || 0);
      s.last_activity_calories = readIntFromPayload(payload, 'last_activity_calories', KEY_LAST_ACTIVITY_CALORIES, s.last_activity_calories || 0);
      s.last_activity_pace_sec = readIntFromPayload(payload, 'last_activity_pace_sec', KEY_LAST_ACTIVITY_PACE_SEC, s.last_activity_pace_sec || 0);
      s.last_activity_duration_sec = readIntFromPayload(payload, 'last_activity_duration_sec', KEY_LAST_ACTIVITY_DURATION_SEC, s.last_activity_duration_sec || 0);
      s.last_activity_timestamp = readIntFromPayload(payload, 'last_activity_timestamp', KEY_LAST_ACTIVITY_TIMESTAMP, s.last_activity_timestamp || 0);
      var isNewSave = s.last_activity_timestamp > prevTimestamp && s.last_activity_timestamp > 0;
      var shouldInsertTimelinePin = (insertTimelinePinRequested || isNewSave) && s.last_activity_timestamp > 0;
      s = normalizeSettings(s);
      s_latestSettingsSnapshot = s;
      saveSettings(s);
      console.log('saved totals snapshot:', JSON.stringify({
        lifetime_distance_m_total: s.lifetime_distance_m_total,
        lifetime_calories_total: s.lifetime_calories_total,
        last_activity_distance_m: s.last_activity_distance_m,
        last_activity_calories: s.last_activity_calories,
        last_activity_pace_sec: s.last_activity_pace_sec,
        last_activity_duration_sec: s.last_activity_duration_sec,
        last_activity_timestamp: s.last_activity_timestamp
      }));
      if (shouldInsertTimelinePin) {
        console.log('timeline pin requested, inserting timeline pin');
        insertTimelinePin(s);
      }
      if (s_waitingLifetimeCallback) {
        s_waitingLifetimeCallback();
      }
    }
  });

  Pebble.addEventListener('webviewclosed', function(e) {
    if (!e || !e.response) {
      console.log('webviewclosed: no response payload');
      return;
    }
    var data = e.response;
    var settings;
    try {
      settings = JSON.parse(decodeURIComponent(data));
    } catch (err) {
      try {
        // Some toolchains already hand us decoded JSON.
        settings = JSON.parse(data);
      } catch (fallbackErr) {
        console.log('config parse failed:', String(err), String(fallbackErr));
        return;
      }
    }
    console.log('config parsed, sending to watch');
    // Never allow config form submit to clobber live totals/last-activity snapshots.
    var current = normalizeSettings(loadSettings());
    var incoming = normalizeSettings(settings);
    var merged = assignObjects(incoming);
    merged.lifetime_distance_m_total = Math.max(current.lifetime_distance_m_total, incoming.lifetime_distance_m_total);
    merged.lifetime_calories_total = Math.max(current.lifetime_calories_total, incoming.lifetime_calories_total);
    if (incoming.last_activity_timestamp >= current.last_activity_timestamp) {
      merged.last_activity_distance_m = incoming.last_activity_distance_m;
      merged.last_activity_calories = incoming.last_activity_calories;
      merged.last_activity_pace_sec = incoming.last_activity_pace_sec;
      merged.last_activity_duration_sec = incoming.last_activity_duration_sec;
      merged.last_activity_timestamp = incoming.last_activity_timestamp;
    } else {
      merged.last_activity_distance_m = current.last_activity_distance_m;
      merged.last_activity_calories = current.last_activity_calories;
      merged.last_activity_pace_sec = current.last_activity_pace_sec;
      merged.last_activity_duration_sec = current.last_activity_duration_sec;
      merged.last_activity_timestamp = current.last_activity_timestamp;
    }
    s_latestSettingsSnapshot = normalizeSettings(merged);
    syncSettingsToWatch(s_latestSettingsSnapshot);
  });
})();
