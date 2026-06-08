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

  function lbTenthsToKgTenths(valueTenths) {
    return Math.round((parseInt(valueTenths, 10) || 0) * 0.453592);
  }

  function kgTenthsToLbTenths(valueTenths) {
    return Math.round((parseInt(valueTenths, 10) || 0) / 0.453592);
  }

  function inchTenthsToCmTenths(valueTenths) {
    return Math.round((parseInt(valueTenths, 10) || 0) * 2.54);
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

  function terrainOptionsHtml() {
    return '' +
      '<option value="road">Road (1.0)</option>' +
      '<option value="gravel">Gravel (1.2)</option>' +
      '<option value="mixed">Mixed (1.3)</option>' +
      '<option value="sand">Sand (1.5)</option>' +
      '<option value="snow">Snow (1.5)</option>';
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

  function buildTabbedConfigHtml(s) {
    var dataJson = JSON.stringify(s).replace(/</g, '\\u003c');
    var defaultsJson = JSON.stringify(defaults).replace(/</g, '\\u003c');
    return `<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Ruck Settings</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link href="https://fonts.googleapis.com/css2?family=Stardos+Stencil:wght@700&display=swap" rel="stylesheet">
  <style>
    *,*::before,*::after{box-sizing:border-box}
    html,body{overflow-x:hidden;max-width:100%}
    body{margin:0;font:13px/1.4 Helvetica,Arial,sans-serif;background:#f3f4f6;color:#1f2430}
    .app{min-height:100vh;display:flex;flex-direction:column}
    .top,.tabs,.main{width:100%}
    .top{padding:14px 14px 8px}
    .brand{display:flex;align-items:center;gap:10px;padding:10px 0 14px}
    .badge{width:56px;height:56px;border-radius:16px;display:grid;place-items:center;flex:0 0 auto}
    .badge svg{width:100%;height:100%}
    .title{font:700 38px/1 'Stardos Stencil',Impact,'Arial Narrow Bold',sans-serif;letter-spacing:.04em}
    .tabs{display:flex;border-bottom:1px solid #dde1e7;background:rgba(255,255,255,.35)}
    .tab-btn{appearance:none;border:0;background:none;flex:1;padding:14px 8px 13px;font:700 12px/1.1 Helvetica,Arial,sans-serif;letter-spacing:.12em;color:#7b818f}
    .tab-btn.active{color:#4f8ed6}
    .tab-btn.active span{position:relative}
    .tab-btn.active span:after{content:"";position:absolute;left:0;right:0;bottom:-15px;height:4px;border-radius:4px 4px 0 0;background:#4f8ed6}
    .main{flex:1;padding:0 14px 88px}
    .tab{display:none;padding-top:14px}
    .tab.active{display:block}
    .section{display:flex;align-items:center;justify-content:space-between;gap:10px;margin:4px 2px 10px}
    .section-title{font:800 18px/1.1 Helvetica,Arial,sans-serif;letter-spacing:.14em;text-transform:uppercase}
    .section-chip{font:700 11px/1 Helvetica,Arial,sans-serif;letter-spacing:.18em;text-transform:uppercase;color:#767d88;flex:0 0 auto}
    .subhead{font:800 16px/1.1 Helvetica,Arial,sans-serif;letter-spacing:.14em;text-transform:uppercase;color:#4f8ed6;display:flex;align-items:center;gap:8px;margin:10px 0 8px}
    .subhead svg{width:16px;height:16px;fill:#4f8ed6}
    .card{background:#fff;border:1px solid #dde1e7;border-radius:22px;padding:12px 12px 14px;box-shadow:0 1px 0 rgba(0,0,0,.03);margin-bottom:14px}
    .field-label{display:block;font:800 11px/1.1 Helvetica,Arial,sans-serif;letter-spacing:.12em;text-transform:uppercase;color:#727887;margin:10px 0 7px}
    .row{display:grid;grid-template-columns:1fr;gap:12px}
    .row>*{min-width:0}
    input,select,button{font:inherit}
    input,select{width:100%;min-width:0;height:56px;border:2px solid #e1e5eb;border-radius:16px;background:#fff;padding:0 14px;color:#252b39;outline:none;box-sizing:border-box;font:700 17px/1.1 Helvetica,Arial,sans-serif}
    .field-with-unit{display:flex;align-items:stretch;border:2px solid #e1e5eb;border-radius:16px;overflow:hidden;background:#fff}
    .field-with-unit:focus-within{border-color:#4f8ed6}
    .field-with-unit input,.field-with-unit select{flex:1;min-width:0;border:none;border-radius:0;background:transparent;padding:0 14px}
    .field-with-unit .unit{display:flex;align-items:center;padding:0 14px 0 12px;border-left:2px solid #e1e5eb;font:800 14px/1 Helvetica,Arial,sans-serif;letter-spacing:.06em;color:#6f7480;text-transform:uppercase;white-space:nowrap;flex-shrink:0}
    .profiles{display:grid;grid-template-columns:1fr;gap:14px}
    .profile{border-radius:20px;border:1px solid #dde1e7;background:#fff;padding:16px 14px 14px;box-shadow:0 1px 0 rgba(0,0,0,.03)}
    .profile-top{display:flex;align-items:center;justify-content:space-between;gap:12px}
    .profile-name{min-width:0;display:flex;align-items:center;gap:10px;font:800 20px/1.1 Helvetica,Arial,sans-serif;letter-spacing:-.01em}
    .profile-name span:last-child{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
    .swatch{width:18px;height:18px;border-radius:6px;flex:0 0 auto}
    .edit-btn,.done-btn,.clear-btn,.save-btn{appearance:none;border:0;border-radius:16px;font:800 15px/1 Helvetica,Arial,sans-serif;letter-spacing:.12em;text-transform:uppercase}
    .edit-btn{width:48px;height:48px;background:#f3f6fb;border:2px solid #e2e6ed;color:#6b7280;display:grid;place-items:center;border-radius:14px;flex:0 0 auto}
    .edit-btn svg{width:22px;height:22px;fill:none;stroke:#6b7280;stroke-width:2}
    .summary-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin-top:14px}
    .summary-k{font:800 11px/1 Helvetica,Arial,sans-serif;letter-spacing:.14em;text-transform:uppercase;color:#727887;display:flex;align-items:center;gap:5px}
    .summary-v{font:700 17px/1.1 Helvetica,Arial,sans-serif;font-variant-numeric:tabular-nums}
    .summary-v small,.kcal small,.history-v small{font:800 11px/1 Helvetica,Arial,sans-serif;letter-spacing:.06em;text-transform:uppercase;color:#7a808b}
    .profile-kcal-badge{font:700 14px/1 Helvetica,Arial,sans-serif;color:#4f8ed6;background:#eaf2fb;border-radius:10px;padding:5px 9px;white-space:nowrap;flex-shrink:0}
    .profile-kcal-badge small{font:700 11px/1 Helvetica,Arial,sans-serif;letter-spacing:.04em;text-transform:uppercase}
    .profile.editing{border-color:#4f8ed6;border-width:2px}
    .profile.editing .edit-btn{display:none}
    .inline-editor{display:none;padding-top:14px;border-top:1px solid #e8eaf0;margin-top:14px}
    .inline-editor.open{display:block}
    .editor-actions{display:flex;justify-content:flex-end;align-items:center;margin-top:14px;gap:10px}
    .done-btn svg,.save-btn svg{width:18px;height:18px;fill:none;stroke:currentColor;stroke-width:2}
    .done-btn{background:#4f8ed6;color:#fff;padding:12px 20px;border-radius:16px;display:flex;align-items:center;justify-content:center;gap:7px;appearance:none;border:0;font:800 15px/1 Helvetica,Arial,sans-serif;letter-spacing:.1em;text-transform:uppercase}
    .save-bar{position:fixed;left:0;right:0;bottom:0;padding:10px 14px 12px;background:linear-gradient(to top, rgba(243,244,246,1) 76%, rgba(243,244,246,0));}
    .save-btn{width:100%;height:62px;border-radius:18px;background:#4f8ed6;color:#fff;display:flex;align-items:center;justify-content:center;gap:10px;font-size:16px;appearance:none;border:0;font:800 16px/1 Helvetica,Arial,sans-serif;letter-spacing:.12em;text-transform:uppercase;cursor:pointer}
    .chart-card{border:1px solid #dde1e7;border-radius:22px;background:#fff;padding:14px}
    .pace-row{margin-bottom:12px}
    .legend-row{display:flex;align-items:center;gap:10px;margin:12px 0}
    .legend-dot{width:16px;height:16px;border-radius:6px;flex:0 0 auto}
    .legend-name{width:88px;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font:800 14px/1.1 Helvetica,Arial,sans-serif}
    .bar{flex:1;background:#edf0f5;border-radius:12px;height:28px;position:relative;overflow:hidden}
    .fill{height:100%;border-radius:12px}
    .kcal{width:78px;text-align:right;font:800 19px/1 Helvetica,Arial,sans-serif;font-variant-numeric:tabular-nums}
    .history-card{border:1px solid #dde1e7;border-radius:22px;background:#fff;padding:14px 14px 6px;margin-bottom:14px}
    .history-title{font:800 12px/1 Helvetica,Arial,sans-serif;letter-spacing:.14em;text-transform:uppercase;color:#727887;margin-bottom:10px}
    .history-row{display:flex;justify-content:space-between;align-items:baseline;padding:14px 2px;border-bottom:2px solid #e3e6ec;gap:12px}
    .history-row:last-child{border-bottom:0}
    .history-k{font-size:14px;color:#727887;min-width:0}
    .history-v{font:800 18px/1 Helvetica,Arial,sans-serif;font-variant-numeric:tabular-nums;text-align:right;min-width:0}
    @media (min-width: 560px){
      .top{padding:18px 18px 10px}
      .brand{gap:12px;padding:12px 0 18px}
      .badge{width:64px;height:64px;border-radius:18px}
      .title{font-size:44px}
      .main{padding-left:18px;padding-right:18px}
      .row{grid-template-columns:repeat(2,minmax(0,1fr))}
      .profile-name{font-size:22px}
      .summary-v{font-size:19px}
      .kcal{font-size:21px;width:86px}
      .history-v{font-size:20px}
      .legend-name{width:96px;font-size:15px}
    }
    @media (min-width: 900px){
      .app{align-items:center}
      .top,.tabs,.main{max-width:1120px}
      .top,.main{padding-left:24px;padding-right:24px}
      .top{padding-top:22px}
      .brand{padding-bottom:22px}
      .badge{width:72px;height:72px}
      .title{font-size:52px}
      .tab-btn{font-size:13px;padding-top:16px;padding-bottom:15px}
      .tab-btn.active span:after{bottom:-16px}
      .main{padding-bottom:104px}
      .section-title{font-size:20px}
      .subhead{font-size:18px}
      .card,.profile,.editor,.chart-card,.history-card{border-radius:28px}
      .profiles{gap:16px}
      .profile{padding:18px 16px 16px}
      .profile-name{font-size:24px}
      .summary-grid{gap:10px}
      .summary-v{font-size:20px}
      .preview{font-size:16px}
      .preview span{font-size:17px}
      .save-bar{padding-left:24px;padding-right:24px}
      .save-btn{height:70px;font-size:18px}
      .history-k{font-size:16px}
      .history-v{font-size:21px}
    }
  </style>
</head>
<body>
  <div class="app">
    <div class="top">
      <div class="brand"><div class="badge"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 144 144"><rect x="0" y="0" width="144" height="144" rx="26" fill="#11161a"/><rect x="34" y="18" width="76" height="108" rx="12" fill="none" stroke="#ecf1f4" stroke-width="4"/><path d="M55 18 Q72 4 89 18" fill="none" stroke="#ecf1f4" stroke-width="4" stroke-linecap="round"/><path d="M36 65 L108 51" fill="none" stroke="#ecf1f4" stroke-width="3" stroke-linecap="round"/><rect x="60" y="28" width="24" height="11" rx="2" fill="none" stroke="#ecf1f4" stroke-width="2"/></svg></div><div class="title">RuckPebble</div></div>
    </div>
    <div class="tabs">
      <button class="tab-btn active" data-tab="profiles"><span>PROFILES</span></button>
      <button class="tab-btn" data-tab="calories"><span>CALORIES</span></button>
      <button class="tab-btn" data-tab="history"><span>HISTORY</span></button>
    </div>
    <div class="main">
      <section class="tab active" id="tab-profiles">
        <div class="section"><div class="subhead"><svg viewBox="0 0 16 16"><circle cx="8" cy="3.5" r="2" fill="#4f8ed6"/><path d="M8 6v4.5M5.5 8l2.5-1.5 2.5 1.5M6 14l2-3.5M10 14l-2-3.5" stroke="#4f8ed6" stroke-width="1.5" stroke-linecap="round" fill="none"/></svg>ABOUT YOU</div></div>
        <div class="row"><div><div class="field-label">Units</div><select id="measurement_unit"><option value="0">Metric</option><option value="1">Imperial</option></select></div><div></div></div>
        <div class="row">
          <div><div class="field-label" id="body_weight_label_text">Body weight</div><div class="field-with-unit"><input id="weight_value" type="number" step="0.1"><div class="unit" id="body_weight_unit_suffix">kg</div></div></div>
          <div><div class="field-label" id="stride_length_label_text">Stride length</div><div class="field-with-unit"><input id="stride_length_value" type="number" step="0.1"><div class="unit" id="stride_length_unit_suffix">cm</div></div></div>
        </div>
        <div class="section" style="margin-top:26px"><div class="subhead"><svg viewBox="0 0 16 16"><path d="M2 3h5l1 2h6v8H2zM3 4v7h10V6H7.4L6.4 4z"/></svg>PROFILES</div></div>
        <div id="profile_list" class="profiles"></div>
      </section>
      <section class="tab" id="tab-calories">
        <div class="section"><div class="subhead"><svg viewBox="0 0 16 16"><path d="M9 1c-.5 2.5.5 4 1.5 5.5C11.5 8 12 9 11 11c-.5 1-1.5 2-3 2.5 1-1 1.2-2.5.5-3.5-.5 1.5-1.5 2-2 2.5.5-1.5-.5-3-1-4C5 10 4.5 11.5 5 13c-1-1-1.5-2.5-1-4.5.5-2 2-3.5 2-5.5C7 4.5 7.5 6 8 7c0-2 .5-4 1-6z"/></svg>CALORIES / HOUR</div></div>
        <div class="chart-card"><div class="pace-row"><div class="row"><div><div class="field-label" style="margin-top:0">Pace</div><div class="field-with-unit"><select id="chart_speed"><option value="10">10:00</option><option value="12" selected>12:00</option><option value="15">15:00</option><option value="20">20:00</option></select><div class="unit" id="pace_unit">/km</div></div></div><div></div></div></div><div id="calorie_chart" style="margin-top:14px"></div></div>
      </section>
      <section class="tab" id="tab-history">
        <div class="section"><div class="subhead"><svg viewBox="0 0 16 16"><path d="M8 1a7 7 0 1 0 .01 14.01A7 7 0 0 0 8 1zm0 1.2A5.8 5.8 0 1 1 2.2 8 5.8 5.8 0 0 1 8 2.2zm-.5 2v4.1l3.1 1.9.6-1-2.7-1.6V4.2z"/></svg>HISTORY</div><div class="section-chip">READ ONLY</div></div>
        <div class="history-title">LIFETIME</div>
        <div class="history-card"><div class="history-row"><div class="history-k">Distance</div><div class="history-v" id="lifetime_distance_km_total">--</div></div><div class="history-row"><div class="history-k">Calories</div><div class="history-v" id="lifetime_calories_total">--</div></div></div>
        <div class="history-title">LAST RUCK</div>
        <div class="history-card"><div class="history-row"><div class="history-k">Date</div><div class="history-v" id="last_activity_datetime">--</div></div><div class="history-row"><div class="history-k">Distance</div><div class="history-v" id="last_activity_distance_km">--</div></div><div class="history-row"><div class="history-k">Avg pace</div><div class="history-v" id="last_activity_pace">--</div></div><div class="history-row"><div class="history-k">Duration</div><div class="history-v" id="last_activity_duration">--</div></div><div class="history-row"><div class="history-k">Calories</div><div class="history-v" id="last_activity_calories_display">--</div></div></div>
      </section>
    </div>
    <div class="save-bar"><button id="save" class="save-btn" type="button"><svg viewBox="0 0 16 16"><path d="M3 2h7l3 3v9H3zM5 3v3h5V3zM5 9v4h6V9z"/></svg>SAVE CHANGES</button></div>
  </div>
  <script>
    var INITIAL_STATE = ${dataJson};
    var DEFAULTS = ${defaultsJson};
    function $(id){return document.getElementById(id);}
    function clone(o){return JSON.parse(JSON.stringify(o));}
    function toInt(v){v=parseInt(v,10);return isNaN(v)?0:v;}
    function kgTenthsToLbTenths(v){return Math.round((toInt(v)||0)/0.453592);}
    function lbTenthsToKgTenths(v){return Math.round((toInt(v)||0)*0.453592);}
    function cmTenthsToInTenths(v){return Math.round((toInt(v)||0)/2.54);}
    function inTenthsToCmTenths(v){return Math.round((toInt(v)||0)*2.54);}
    function terrainFactor(type){if(type==="road")return 100;if(type==="gravel")return 120;if(type==="mixed")return 130;return 150;}
    function terrainLabel(type,factor){if(type==="road"||type==="gravel"||type==="mixed"||type==="sand"||type==="snow"){return type.charAt(0).toUpperCase()+type.slice(1);}if(factor<=110){return "Road";}if(factor<=125){return "Gravel";}if(factor<=140){return "Mixed";}return "Sand";}
    var state = clone(INITIAL_STATE);
    var defaults = clone(DEFAULTS);
    var activeTab = "profiles";
    function profileFromState(index){var n=index+1;return {name:state["profile"+n+"_name"]||"",ruck_weight_value:toInt(state["profile"+n+"_ruck_weight_value"]),terrain_type:state["profile"+n+"_terrain_type"]||"road",terrain_factor:toInt(state["profile"+n+"_terrain_factor"])||terrainFactor(state["profile"+n+"_terrain_type"]||"road"),grade_percent:toInt(state["profile"+n+"_grade_percent"])};}
    var draftProfiles = [profileFromState(0), profileFromState(1), profileFromState(2)];
    function unit(){return toInt($("measurement_unit").value);}
    function isImperial(){return unit()===1;}
    function sharedWeightKg(){var v=parseFloat($("weight_value").value)||0;return isImperial()?v*0.453592:v;}
    function profileKcal(p){var w=sharedWeightKg();if(w<=0)return 0;var l=(toInt(p.ruck_weight_value)/10)*0.453592;var pace=parseFloat($("chart_speed").value)||12;var sp=60/pace;var grade=toInt(p.grade_percent);var factor=toInt(p.terrain_factor)||terrainFactor(p.terrain_type);var V=sp/3.6,G=grade/1000,T=factor/100,tot=w+l,ratio=l/w,inner=1.5*V*V+0.35*V*G,mult=(1+Math.sqrt(0.3*V*V)/7+Math.pow(V*ratio,2)/4)*1.1,k=(1.5*w+2*tot*ratio*ratio+T*tot*inner*mult)*3600/4184;var walk=(3.5+0.1*(sp*1000/60))*w*60/200;if(l>0&&k<walk)k=walk+walk*(l/w);return Math.round(k);}
    var PACE_KM_VALUES=[10,12,15,20];var PACE_KM_LABELS=["10:00","12:00","15:00","20:00"];
    function paceKmToMiLabel(minKm){var minMi=minKm*1.60934;var m=Math.floor(minMi);var s=Math.round((minMi-m)*60);return m+":"+(s<10?"0":"")+s;}
    function setSharedLabels(){var imp=isImperial();$("body_weight_label_text").textContent="Body weight";$("stride_length_label_text").textContent="Stride length";$("body_weight_unit_suffix").textContent=imp?"lb":"kg";$("stride_length_unit_suffix").textContent=imp?"in":"cm";var sel=$("chart_speed");var cur=sel.value;for(var i=0;i<sel.options.length;i++){sel.options[i].text=imp?paceKmToMiLabel(PACE_KM_VALUES[i]):PACE_KM_LABELS[i];}sel.value=cur;$("pace_unit").textContent=imp?"/mi":"/km";for(var j=0;j<3;j++){var u=$("ie_weight_unit_"+j);if(u)u.textContent=imp?"lb":"kg";}}
    function convertSharedValues(prev,next){if(prev===next)return;var w=parseFloat($("weight_value").value)||0;var s=parseFloat($("stride_length_value").value)||0;if(next==="1"){$("weight_value").value=(w*2.2046226218).toFixed(1);$("stride_length_value").value=(s*0.3937007874).toFixed(1);}else{$("weight_value").value=(w/2.2046226218).toFixed(1);$("stride_length_value").value=(s/0.3937007874).toFixed(1);}for(var i=0;i<3;i++){var wEl=$("ie_weight_"+i);if(!wEl)continue;var wv=parseFloat(wEl.value)||0;if(next==="1"){wEl.value=(wv*2.2046226218).toFixed(1);}else{wEl.value=(wv/2.2046226218).toFixed(1);}}}
    function renderTabs(){["profiles","calories","history"].forEach(function(name){var active=name===activeTab;$("tab-"+name).classList.toggle("active",active);var btn=document.querySelector('.tab-btn[data-tab="'+name+'"]');if(btn){btn.classList.toggle("active",active);}});}
    function profileDisplayName(draft){var imp=isImperial();var w=Math.round((imp?kgTenthsToLbTenths(draft.ruck_weight_value):draft.ruck_weight_value/10));var t=terrainLabel(draft.terrain_type,draft.terrain_factor);var unit=imp?"lb":"kg";return draft.name||(( w>0?w:"--")+unit+", "+t.toLowerCase());}
    function renderProfileSummary(idx){var draft=draftProfiles[idx];var imp=isImperial();var w=Math.round(imp?kgTenthsToLbTenths(draft.ruck_weight_value)/10:draft.ruck_weight_value/10);var wUnit=imp?"lb":"kg";var t=terrainLabel(draft.terrain_type,draft.terrain_factor);var g=Math.round(toInt(draft.grade_percent)/10);return '<div class="summary-grid" id="profile_summary_'+idx+'"><div><div class="summary-k">Weight</div><div class="summary-v">'+w+' <small>'+wUnit+'</small></div></div><div><div class="summary-k">Terrain</div><div class="summary-v">'+t+' <small>× '+(toInt(draft.terrain_factor)/100).toFixed(1)+'</small></div></div><div><div class="summary-k">Grade</div><div class="summary-v">'+g+' <small>%</small></div></div></div>';}
    function renderProfileList(){var colors=["#6a8d3f","#d8943d","#b1764d"];var html="";var imp=isImperial();var terrainOpts='<option value="road">Road × 1.0</option><option value="gravel">Gravel × 1.2</option><option value="mixed">Mixed × 1.3</option><option value="sand">Sand × 1.5</option><option value="snow">Snow × 1.5</option>';for(var i=0;i<3;i++){var d=draftProfiles[i];var wDisp=(imp?(kgTenthsToLbTenths(d.ruck_weight_value)/10):(d.ruck_weight_value/10)).toFixed(1);var gDisp=Math.round(toInt(d.grade_percent)/10);var kcal=profileKcal(d);html+='<div class="profile" id="profile_card_'+i+'"><div class="profile-top"><div class="profile-name"><span class="swatch" style="background:'+colors[i]+'"></span><span id="profile_display_name_'+i+'">'+profileDisplayName(d)+'</span></div><div style="display:flex;align-items:center;gap:8px"><div class="profile-kcal-badge" id="profile_kcal_'+i+'">'+kcal+' <small>kcal/h</small></div><button type="button" class="edit-btn" data-edit="'+i+'"><svg viewBox="0 0 16 16"><path d="M3 11.5V13h1.5l6.8-6.8-1.5-1.5zM12.3 4.7l-1.9-1.9 1-1a1.2 1.2 0 0 1 1.7 0l.2.2a1.2 1.2 0 0 1 0 1.7z"/></svg></button></div></div>'+renderProfileSummary(i)+'<div class="inline-editor" id="profile_editor_'+i+'"><div class="field-label" style="margin-top:0">Profile name</div><input id="ie_name_'+i+'" type="text" maxlength="32" value="'+(d.name||"").replace(/"/g,"&quot;")+'"><div class="row"><div><div class="field-label">Pack weight</div><div class="field-with-unit"><input id="ie_weight_'+i+'" type="number" step="0.1" value="'+wDisp+'"><div class="unit" id="ie_weight_unit_'+i+'">'+(imp?"lb":"kg")+'</div></div></div><div><div class="field-label">Grade</div><div class="field-with-unit"><input id="ie_grade_'+i+'" type="number" step="1" value="'+gDisp+'"><div class="unit">%</div></div></div></div><div class="field-label">Terrain</div><select id="ie_terrain_'+i+'">'+terrainOpts.replace('value="'+d.terrain_type+'"','value="'+d.terrain_type+'" selected')+'</select><div class="editor-actions"><button type="button" class="done-btn" data-done="'+i+'"><svg viewBox="0 0 16 16"><path d="M6.5 11.5 3.8 8.8l1.4-1.4 1.3 1.3 4.3-4.3 1.4 1.4z"/></svg>Done</button></div></div></div>';}$("profile_list").innerHTML=html;for(var j=0;j<3;j++){(function(idx){var editBtn=$("profile_card_"+idx).querySelector("[data-edit]");editBtn.onclick=function(){toggleProfileEditor(idx);};var doneBtn=$("profile_card_"+idx).querySelector("[data-done]");doneBtn.onclick=function(){closeProfileEditor(idx);};(function(){var t=null;function s(){if(t)clearTimeout(t);t=setTimeout(function(){syncProfile(idx);},0);}["ie_name_","ie_weight_","ie_grade_","ie_terrain_"].forEach(function(pfx){var el=$(pfx+idx);if(!el)return;["input","change","keyup","pointerup"].forEach(function(ev){el.addEventListener(ev,s);});});})();})(j);}}
    function toggleProfileEditor(idx){var card=$("profile_card_"+idx);var editor=$("profile_editor_"+idx);var isOpen=editor.classList.contains("open");if(isOpen){closeProfileEditor(idx);}else{editor.classList.add("open");card.classList.add("editing");var sumEl=$("profile_summary_"+idx);if(sumEl)sumEl.style.display="none";}}
    function closeProfileEditor(idx){var card=$("profile_card_"+idx);var editor=$("profile_editor_"+idx);editor.classList.remove("open");card.classList.remove("editing");syncProfile(idx);var nameEl=$("profile_display_name_"+idx);if(nameEl)nameEl.textContent=profileDisplayName(draftProfiles[idx]);var sumEl=$("profile_summary_"+idx);if(sumEl){sumEl.outerHTML=renderProfileSummary(idx);}}
    function syncProfile(idx){var d=draftProfiles[idx];d.name=($("ie_name_"+idx).value||"").trim().slice(0,32);var wv=parseFloat($("ie_weight_"+idx).value)||0;d.ruck_weight_value=isImperial()?lbTenthsToKgTenths(Math.round(wv*10)):Math.round(wv*10);d.grade_percent=(toInt($("ie_grade_"+idx).value)||0)*10;d.terrain_type=$("ie_terrain_"+idx).value;d.terrain_factor=terrainFactor(d.terrain_type);var kcalEl=$("profile_kcal_"+idx);if(kcalEl)kcalEl.innerHTML=profileKcal(d)+' <small>kcal/h</small>';}
    function updateAllProfileKcal(){for(var i=0;i<3;i++){var el=$("profile_kcal_"+i);if(el)el.innerHTML=profileKcal(draftProfiles[i])+' <small>kcal/h</small>';}}
    function drawCalories(){var pace=parseFloat($("chart_speed").value)||12;var sp=60/pace;var w=sharedWeightKg();function walkKcalHr(wKg,spKmh){var sm=spKmh*1000/60;return(3.5+0.1*sm)*wKg*60/200;}var bars=[{name:"Walk",kcal:Math.round(walkKcalHr(w,sp)),color:"#8f95a1"}];for(var i=0;i<3;i++){bars.push({name:draftProfiles[i].name||("P"+(i+1)),kcal:profileKcal(draftProfiles[i]),color:["#6a8d3f","#d8943d","#b1764d"][i]});}var max=0;for(var j=0;j<bars.length;j++){if(bars[j].kcal>max)max=bars[j].kcal;}var html="";for(var k=0;k<bars.length;k++){var b=bars[k];var pct=max>0?(b.kcal/max*100):0;html+='<div class="legend-row"><div class="legend-dot" style="background:'+b.color+'"></div><div class="legend-name">'+b.name+'</div><div class="bar"><div class="fill" style="width:'+pct.toFixed(1)+'%;background:'+b.color+'"></div></div><div class="kcal">'+b.kcal+' <small>kcal</small></div></div>';}$("calorie_chart").innerHTML=html;updateAllProfileKcal();}
    function renderHistory(){var imp=isImperial();function fmtDist(meters){var m=toInt(meters);if(imp){var mi=(m/1609.344);return mi.toFixed(2)+' <small>mi</small>';}return (Math.round(m/10)/100).toFixed(2)+' <small>km</small>';}function fmtPace(secPerKm){if(!secPerKm)return '--';if(imp){var spm=Math.round(secPerKm*1.60934);return Math.floor(spm/60)+":"+("0"+(spm%60)).slice(-2)+' <small>/mi</small>';}return Math.floor(secPerKm/60)+":"+("0"+(secPerKm%60)).slice(-2)+' <small>/km</small>';}$("lifetime_distance_km_total").innerHTML=fmtDist(state.lifetime_distance_m_total);$("lifetime_calories_total").textContent=toInt(state.lifetime_calories_total);var ts=toInt(state.last_activity_timestamp);if(ts>0){var d=new Date(ts*1000);var mo=["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"][d.getMonth()];$("last_activity_datetime").textContent=mo+" "+d.getDate()+" · "+("0"+d.getHours()).slice(-2)+":"+("0"+d.getMinutes()).slice(-2);}else{$("last_activity_datetime").textContent="--";}$("last_activity_distance_km").innerHTML=fmtDist(state.last_activity_distance_m);$("last_activity_pace").innerHTML=fmtPace(toInt(state.last_activity_pace_sec));var dur=toInt(state.last_activity_duration_sec);$("last_activity_duration").textContent=dur>0?Math.floor(dur/3600)+":"+("0"+Math.floor((dur%3600)/60)).slice(-2)+":"+("0"+(dur%60)).slice(-2):'--';$("last_activity_calories_display").textContent=toInt(state.last_activity_calories);}
    function showTab(name){activeTab=name;renderTabs();if(name==="calories"){drawCalories();}if(name==="history"){renderHistory();}}
    function saveAll(){var u=unit();var wv=parseFloat($("weight_value").value)||0;var sv=parseFloat($("stride_length_value").value)||0;var out={measurement_unit:u,weight_unit:u,stride_length_unit:u,weight_value:u===1?lbTenthsToKgTenths(Math.round(wv*10)):Math.round(wv*10),stride_length_value:u===1?inTenthsToCmTenths(Math.round(sv*10)):Math.round(sv*10)};for(var i=0;i<3;i++){out["profile"+(i+1)+"_ruck_weight_value"]=draftProfiles[i].ruck_weight_value;out["profile"+(i+1)+"_terrain_type"]=draftProfiles[i].terrain_type;out["profile"+(i+1)+"_terrain_factor"]=draftProfiles[i].terrain_factor;out["profile"+(i+1)+"_grade_percent"]=draftProfiles[i].grade_percent;out["profile"+(i+1)+"_name"]=draftProfiles[i].name;}var payload=encodeURIComponent(JSON.stringify(out));var ret=(location.search.match(/[?&]return_to=([^&]*)/)||[])[1];location.href=ret?decodeURIComponent(ret)+payload:"pebblejs://close#"+payload;}
    function initSharedFields(){var u=toInt(state.measurement_unit);$("measurement_unit").value=String(u);$("measurement_unit").dataset.unit=String(u);$("weight_value").value=(u===1?(kgTenthsToLbTenths(state.weight_value)/10):(toInt(state.weight_value)/10)).toFixed(1);$("stride_length_value").value=(u===1?(cmTenthsToInTenths(state.stride_length_value)/10):(toInt(state.stride_length_value)/10)).toFixed(u===1?1:0);setSharedLabels();}
    initSharedFields();renderProfileList();renderHistory();drawCalories();renderTabs();
    $("measurement_unit").addEventListener("change",function(){var prev=this.dataset.unit||this.value;convertSharedValues(prev,this.value);this.dataset.unit=this.value;setSharedLabels();drawCalories();renderHistory();});
    $("weight_value").addEventListener("input",function(){updateAllProfileKcal();drawCalories();});
    $("stride_length_value").addEventListener("input",drawCalories);
    $("chart_speed").addEventListener("change",drawCalories);
    $("save").addEventListener("click",saveAll);
    Array.prototype.forEach.call(document.querySelectorAll(".tab-btn"),function(btn){btn.addEventListener("click",function(){showTab(this.getAttribute("data-tab"));});});
  </script>
</body>
</html>`;
  }

  function openConfig(settingsSnapshot) {
    var s = normalizeSettings(settingsSnapshot || s_latestSettingsSnapshot || loadSettings());
    Pebble.openURL('data:text/html;charset=utf-8;base64,' + btoa(unescape(encodeURIComponent(buildTabbedConfigHtml(s)))));
    return;
    var compactHtml = '' +
      '<!doctype html><html><head><meta charset="utf-8">' +
      '<meta name="viewport" content="width=device-width,initial-scale=1">' +
      '<title>Ruck Settings</title>' +
      '<style>body{font:14px/1.4 Helvetica,Arial,sans-serif;margin:12px;background:#f5f5f5;color:#111}fieldset{margin:10px 0;padding:10px;border:1px solid #ddd;border-radius:8px;background:#fff}label{display:block;margin:8px 0 4px;font-weight:600}.il{display:flex;align-items:center;gap:6px}.il img{width:14px;height:14px;display:inline-block}input,select,button{width:100%;box-sizing:border-box;padding:8px;font-size:14px}button{margin-top:10px}h1{font-size:20px;margin:0 0 8px}.chart{margin:10px 0 0}.bar{display:flex;align-items:center;margin:0 0 6px}.lab{width:62px;font-size:12px;color:#555;flex:0 0 auto;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}.trk{flex:1;background:#eee;border-radius:4px;height:22px;position:relative}.fill{height:100%;border-radius:4px}.val{position:absolute;right:6px;top:3px;font-size:12px;font-weight:600;color:#333}.stat-row{display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #eee}.stat-row:last-child{border-bottom:none}.stat-label{font-size:13px;color:#555}.stat-value{font-size:14px;font-weight:600;color:#111}</style>' +
      '</head><body><h1>Ruck Settings</h1>' +
      '<fieldset><legend>Estimated calories per hour</legend><label>Pace</label><select id="chart_speed"><option value="20">20 min/km</option><option value="15">15 min/km</option><option value="12" selected>12 min/km</option><option value="10">10 min/km</option></select><div id="calorie_chart" class="chart"></div></fieldset>' +
      '<fieldset><legend>Shared</legend>' +
      '<label>Units</label><select id="measurement_unit"><option value="0">Metric</option><option value="1">Imperial</option></select>' +
      '<label><span id="body_weight_label_text">Body weight (kg)</span></label><div class="row"><div><input type="number" id="weight_value" step="0.1"></div><div style="display:none"><select id="weight_unit"><option value="0">kg</option><option value="1">lb</option></select></div></div>' +
      '<label><span id="stride_length_label_text">Stride length (cm)</span></label><div class="row"><div><input type="number" id="stride_length_value" step="0.1"></div><div style="display:none"><select id="stride_length_unit"><option value="0">cm</option><option value="1">in</option></select></div></div>' +
      '</fieldset>' +
      '<fieldset><legend>Profile 1</legend>' +
      '<label>Name</label><input id="p1_name" type="text" maxlength="32">' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M5 5h6l1 9H4l1-9zm1-3h4v2H6V2zm1 1v1h2V3H7z"/></svg>') + '" alt="">Ruck weight (lb)</label><input id="p1_ruck_weight_value" type="number" step="0.1">' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M1 12l3-6 3 4 2-3 6 5v2H1v-2zm8-6l2-4 4 7-4-3H9z"/></svg>') + '" alt="">Terrain</label><select id="p1_terrain_type">' + terrainOptionsHtml() + '</select>' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M2 12h12v2H2v-2zm1-2l8-8 2 2-8 8H3v-2z"/></svg>') + '" alt="">Grade (%)</label><input id="p1_grade_percent" type="number" step="1">' +
      '</fieldset>' +
      '<fieldset><legend>Profile 2</legend>' +
      '<label>Name</label><input id="p2_name" type="text" maxlength="32">' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M5 5h6l1 9H4l1-9zm1-3h4v2H6V2zm1 1v1h2V3H7z"/></svg>') + '" alt="">Ruck weight (lb)</label><input id="p2_ruck_weight_value" type="number" step="0.1">' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M1 12l3-6 3 4 2-3 6 5v2H1v-2zm8-6l2-4 4 7-4-3H9z"/></svg>') + '" alt="">Terrain</label><select id="p2_terrain_type">' + terrainOptionsHtml() + '</select>' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M2 12h12v2H2v-2zm1-2l8-8 2 2-8 8H3v-2z"/></svg>') + '" alt="">Grade (%)</label><input id="p2_grade_percent" type="number" step="1">' +
      '</fieldset>' +
      '<fieldset><legend>Profile 3</legend>' +
      '<label>Name</label><input id="p3_name" type="text" maxlength="32">' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M5 5h6l1 9H4l1-9zm1-3h4v2H6V2zm1 1v1h2V3H7z"/></svg>') + '" alt="">Ruck weight (lb)</label><input id="p3_ruck_weight_value" type="number" step="0.1">' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M1 12l3-6 3 4 2-3 6 5v2H1v-2zm8-6l2-4 4 7-4-3H9z"/></svg>') + '" alt="">Terrain</label><select id="p3_terrain_type">' + terrainOptionsHtml() + '</select>' +
      '<label class="il"><img src="data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path fill="#111" d="M2 12h12v2H2v-2zm1-2l8-8 2 2-8 8H3v-2z"/></svg>') + '" alt="">Grade (%)</label><input id="p3_grade_percent" type="number" step="1">' +
      '</fieldset>' +
      '<fieldset><legend>Tracked Totals</legend>' +
      '<div class="stat-row"><span class="stat-label">Lifetime distance (km)</span><span class="stat-value" id="lifetime_distance_km_total">--</span></div>' +
      '<div class="stat-row"><span class="stat-label">Lifetime calories</span><span class="stat-value" id="lifetime_calories_total">--</span></div>' +
      '</fieldset>' +
      '<fieldset><legend>Last Activity</legend>' +
      '<div class="stat-row"><span class="stat-label">Date / Time</span><span class="stat-value" id="last_activity_datetime">--</span></div>' +
      '<div class="stat-row"><span class="stat-label">Distance (km)</span><span class="stat-value" id="last_activity_distance_km">--</span></div>' +
      '<div class="stat-row"><span class="stat-label">Pace</span><span class="stat-value" id="last_activity_pace">--</span></div>' +
      '<div class="stat-row"><span class="stat-label">Calories</span><span class="stat-value" id="last_activity_calories_display">--</span></div>' +
      '</fieldset>' +
      '<button id="save" type="button">Save</button>' +
      '<button id="reset_defaults" type="button">Reset</button>' +
      '<script>' +
      'function $(i){return document.getElementById(i)}function ce(t,a,c){var e=document.createElement(t);if(a)for(var k in a)e.setAttribute(k,a[k]);if(c!==undefined)e.textContent=c;return e}' +
      'function tf(t){return t==="road"?100:t==="gravel"?120:t==="mixed"?130:150}' +
      'function tr(t,f){return t==="road"||t==="gravel"||t==="mixed"||t==="sand"||t==="snow"?t:f<=110?"road":f<=125?"gravel":f<=140?"mixed":"sand"}' +
      'var drawTimer=0;function q(){clearTimeout(drawTimer);drawTimer=setTimeout(draw,0)}' +
      'function unitFactor(){return $("measurement_unit").value==="1"?1:0;}' +
      'function bodyWeightKg(){var v=parseFloat($("weight_value").value)||0;return $("measurement_unit").value==="1"?v*0.453592:v;}' +
      'function strideLengthCm(){var v=parseFloat($("stride_length_value").value)||0;return $("measurement_unit").value==="1"?v*2.54:v;}' +
      'function updateUnitLabels(){var imperial=$("measurement_unit").value==="1";$("body_weight_label_text").textContent=imperial?"Body weight (lb)":"Body weight (kg)";$("stride_length_label_text").textContent=imperial?"Stride length (in)":"Stride length (cm)";$("weight_unit").value=imperial?"1":"0";$("stride_length_unit").value=imperial?"1":"0";}' +
      'function convertDisplayedValues(prevUnit,nextUnit){if(prevUnit===nextUnit)return;var weight=parseFloat($("weight_value").value)||0;var stride=parseFloat($("stride_length_value").value)||0;if(nextUnit==="1"){$("weight_value").value=(weight*2.2046226218).toFixed(1);$("stride_length_value").value=(stride*0.3937007874).toFixed(1);}else{$("weight_value").value=(weight/2.2046226218).toFixed(1);$("stride_length_value").value=(stride/0.3937007874).toFixed(1);}}' +
      'function draw(){var p=parseFloat($("chart_speed").value)||12,w=bodyWeightKg(),sp=60/p;function kg(i){return((parseFloat($(i).value)||0)*0.453592);}function gr(i){return(parseInt($(i).value,10)||0)*10}function tf2(i){return tf($(i).value)}function wk(wk,sp){var sm=sp*1000/60;return(3.5+0.1*sm)*wk*60/200}function rk(wk,lk,sp,t,g){if(wk<=0)return 0;var V=sp/3.6,G=g/1000,T=t/100,tot=wk+lk,ratio=lk/wk,inner=1.5*V*V+0.35*V*G,mult=(1+Math.sqrt(0.3*V*V)/7+Math.pow(V*ratio,2)/4)*1.1,k=(1.5*wk+2*tot*ratio*ratio+T*tot*inner*mult)*3600/4184,walk=wk?wk:0;return lk>0&&k<walk?walk+walk*(lk/wk):k}var b=[["Walk",Math.round(wk(w,sp)),"#888"],[$("p1_name").value||"P1",Math.round(rk(w,kg("p1_ruck_weight_value"),sp,tf2("p1_terrain_type"),gr("p1_grade_percent"))),"#e45545"],[$("p2_name").value||"P2",Math.round(rk(w,kg("p2_ruck_weight_value"),sp,tf2("p2_terrain_type"),gr("p2_grade_percent"))),"#4a90d9"],[$("p3_name").value||"P3",Math.round(rk(w,kg("p3_ruck_weight_value"),sp,tf2("p3_terrain_type"),gr("p3_grade_percent"))),"#5cb85c"]],m=0;for(var i=0;i<b.length;i++)if(b[i][1]>m)m=b[i][1];var el=$("calorie_chart");el.innerHTML="";for(var j=0;j<b.length;j++){var row=ce("div",{class:"bar"}),lab=ce("div",{class:"lab"},b[j][0]),trk=ce("div",{class:"trk"}),fillEl=ce("div",{class:"fill"}),val=ce("span",{class:"val"},b[j][1]+" kcal");fillEl.style.width=(m?b[j][1]/m*100:0).toFixed(1)+"%";fillEl.style.background=b[j][2];trk.appendChild(fillEl);trk.appendChild(val);row.appendChild(lab);row.appendChild(trk);el.appendChild(row)}}' +
      'function fill(c){function kg2lb(v){return Math.round((parseInt(v,10)||0)*1000000/453592)}function cm2in(v){return Math.round((parseInt(v,10)||0)/2.54)}var unit=String(parseInt(c.measurement_unit,10)||0);$("measurement_unit").value=unit;$("measurement_unit").dataset.unit=unit;updateUnitLabels();$("weight_value").value=(unit==="1"?kg2lb(c.weight_value)/10:parseInt(c.weight_value,10)/10).toFixed(1);$("stride_length_value").value=(unit==="1"?cm2in(c.stride_length_value)/10:parseInt(c.stride_length_value,10)/10).toFixed(1);$("p1_name").value=c.profile1_name||"";$("p1_ruck_weight_value").value=(kg2lb(c.profile1_ruck_weight_value)/10).toFixed(1);$("p1_terrain_type").value=tr(c.profile1_terrain_type,c.profile1_terrain_factor);$("p1_grade_percent").value=Math.round(c.profile1_grade_percent/10);$("p2_name").value=c.profile2_name||"";$("p2_ruck_weight_value").value=(kg2lb(c.profile2_ruck_weight_value)/10).toFixed(1);$("p2_terrain_type").value=tr(c.profile2_terrain_type,c.profile2_terrain_factor);$("p2_grade_percent").value=Math.round(c.profile2_grade_percent/10);$("p3_name").value=c.profile3_name||"";$("p3_ruck_weight_value").value=(kg2lb(c.profile3_ruck_weight_value)/10).toFixed(1);$("p3_terrain_type").value=tr(c.profile3_terrain_type,c.profile3_terrain_factor);$("p3_grade_percent").value=Math.round(c.profile3_grade_percent/10);$("weight_unit").value=unit;$("stride_length_unit").value=unit;draw()}' +
      'function fmtNum(v){return String(parseInt(v,10)||0)}function fmtKm(m){var n=parseInt(m,10)||0;var km100=Math.round(n/10);return String(Math.floor(km100/100))+"."+("0"+(km100%100)).slice(-2)}' +
      'var s=' + JSON.stringify(s) + ';var d=' + JSON.stringify(defaults) + ';fill(s);' +
      '$("measurement_unit").addEventListener("change",function(){var prev=this.dataset.unit||this.value;convertDisplayedValues(prev,this.value);this.dataset.unit=this.value;updateUnitLabels();q();});' +
      '["chart_speed","weight_value","stride_length_value","p1_ruck_weight_value","p1_terrain_type","p1_grade_percent","p1_name","p2_ruck_weight_value","p2_terrain_type","p2_grade_percent","p2_name","p3_ruck_weight_value","p3_terrain_type","p3_grade_percent","p3_name"].forEach(function(id){var e=$(id);if(e){e.addEventListener("input",q);e.addEventListener("change",q)}});' +
      '$("lifetime_distance_km_total").textContent=fmtKm(s.lifetime_distance_m_total);$("lifetime_calories_total").textContent=fmtNum(s.lifetime_calories_total);if((s.last_activity_timestamp||0)>0){var _d=new Date(s.last_activity_timestamp*1000);var _day=_d.getDate();var _sfx=["th","st","nd","rd"];var _v=_day%100;var _ord=_sfx[(_v-20)%10]||_sfx[_v]||_sfx[0];var _mo=["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"][_d.getMonth()];$("last_activity_datetime").textContent=_day+_ord+" "+_mo+" "+("0"+_d.getHours()).slice(-2)+":"+("0"+_d.getMinutes()).slice(-2);}else{$("last_activity_datetime").textContent="--";}$("last_activity_distance_km").textContent=fmtKm(s.last_activity_distance_m||0);$("last_activity_pace").textContent=(s.last_activity_pace_sec>0)?Math.floor(s.last_activity_pace_sec/60)+":"+(("0"+(s.last_activity_pace_sec%60)).slice(-2))+" / km":"--";$("last_activity_calories_display").textContent=fmtNum(s.last_activity_calories);' +
      '$("save").onclick=function(){function lb2kg(v){return Math.round((parseInt(v,10)||0)*453592/1000000)}function in2cm(v){return Math.round((parseInt(v,10)||0)*254/100)}var unit=parseInt($("measurement_unit").value,10)||0;var o={measurement_unit:unit,weight_value:unit===1?lb2kg(Math.round(parseFloat($("weight_value").value||0)*10)):Math.round(parseFloat($("weight_value").value||0)*10),weight_unit:unit,stride_length_value:unit===1?in2cm(Math.round(parseFloat($("stride_length_value").value||0)*10)):Math.round(parseFloat($("stride_length_value").value||0)*10),stride_length_unit:unit,profile1_ruck_weight_value:lb2kg(Math.round(parseFloat($("p1_ruck_weight_value").value||0)*10)),profile1_terrain_type:$("p1_terrain_type").value,profile1_terrain_factor:tf($("p1_terrain_type").value),profile1_grade_percent:(parseInt($("p1_grade_percent").value,10)||0)*10,profile1_name:($("p1_name").value||"").trim().slice(0,32),profile2_ruck_weight_value:lb2kg(Math.round(parseFloat($("p2_ruck_weight_value").value||0)*10)),profile2_terrain_type:$("p2_terrain_type").value,profile2_terrain_factor:tf($("p2_terrain_type").value),profile2_grade_percent:(parseInt($("p2_grade_percent").value,10)||0)*10,profile2_name:($("p2_name").value||"").trim().slice(0,32),profile3_ruck_weight_value:lb2kg(Math.round(parseFloat($("p3_ruck_weight_value").value||0)*10)),profile3_terrain_type:$("p3_terrain_type").value,profile3_terrain_factor:tf($("p3_terrain_type").value),profile3_grade_percent:(parseInt($("p3_grade_percent").value,10)||0)*10,profile3_name:($("p3_name").value||"").trim().slice(0,32)};var p=encodeURIComponent(JSON.stringify(o)),r=(location.search.match(/[?&]return_to=([^&]*)/)||[])[1];location.href=r?decodeURIComponent(r)+p:"pebblejs://close#"+p};' +
      '$("reset_defaults").onclick=function(){fill(d);$("save").click()};' +
      '</script></body></html>';
    Pebble.openURL('data:text/html,' + encodeURIComponent(compactHtml));
    return;
    var terrainOptions = terrainOptionsHtml();
    function svgIcon(body) {
      return 'data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">' + body + '</svg>');
    }
    var weightIcon = svgIcon('<path fill="#111" d="M5 5h6l1 9H4l1-9zm1-3h4v2H6V2zm1 1v1h2V3H7z"/>');
    var terrainIcon = svgIcon('<path fill="#111" d="M1 12l3-6 3 4 2-3 6 5v2H1v-2zm8-6l2-4 4 7-4-3H9z"/>');
    var gradeIcon = svgIcon('<path fill="#111" d="M2 12h12v2H2v-2zm1-2l8-8 2 2-8 8H3v-2z"/>');
    var html = '' +
      '<!doctype html><html><head><meta charset="utf-8">' +
      '<meta name="viewport" content="width=device-width,initial-scale=1">' +
      '<title>Ruck Settings</title>' +
      '<style>' +
      'body{font-family:Helvetica,Arial,sans-serif;margin:16px;background:#f5f5f5;color:#111;}' +
      'h1{font-size:20px;margin:0 0 12px;}h2{font-size:16px;margin:18px 0 8px;}' +
      'label{display:block;margin:10px 0 4px;font-weight:600;}' +
      '.icon-label{display:flex;align-items:center;gap:6px;}' +
      '.icon-label img{width:14px;height:14px;display:inline-block;}' +
      'input,select{width:100%;padding:8px;font-size:14px;box-sizing:border-box;}' +
      '.row{display:flex;gap:8px;}.row>div{flex:1;}' +
      '.card{background:#fff;border-radius:8px;padding:12px;margin-top:10px;}' +
      '.actions{display:flex;gap:8px;}' +
      '.actions button{margin-top:16px;padding:11px;font-size:16px;color:#fff;border:0;border-radius:6px;}' +
      '#save{flex:2;background:#111;}' +
      '#reset_defaults{flex:1;background:#666;}' +
      '.stat-row{display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #f0f0f0;}' +
      '.stat-row:last-child{border-bottom:none;}' +
      '.stat-label{font-size:13px;color:#555;}' +
      '.stat-value{font-size:14px;font-weight:600;color:#111;}' +
      '</style></head><body>' +
      '<h1>Ruck Settings</h1>' +

      '<div class="card"><h2>Estimated calories per hour</h2>' +
      '<label>Pace</label>' +
      '<select id="chart_speed" onchange="redrawChart()">' +
      '<option value="20">20 min/km</option>' +
      '<option value="15">15 min/km</option>' +
      '<option value="12" selected>12 min/km</option>' +
      '<option value="10">10 min/km</option>' +
      '</select>' +
      '<div id="calorie_chart" style="margin-top:14px;"></div>' +
      '</div>' +

      '<div class="card"><h2>Shared</h2>' +
      '<label>Body weight</label>' +
      '<div class="row"><div><input type="number" id="weight_value" step="0.1"></div>' +
      '<div><select id="weight_unit"><option value="0">kg</option><option value="1">lb</option></select></div></div>' +
      '<label>Ruck weight unit</label>' +
      '<select id="ruck_weight_unit"><option value="0">kg</option><option value="1">lb</option></select>' +
      '<label>Stride length</label>' +
      '<div class="row"><div><input type="number" id="stride_length_value" step="0.1"></div>' +
      '<div><select id="stride_length_unit"><option value="0">cm</option><option value="1">in</option></select></div></div>' +
      '</div>' +

      '<div class="card"><h2>Profile 1</h2>' +
      '<label>Profile name (optional)</label><input type="text" id="p1_name" maxlength="32">' +
      '<label id="p1_ruck_weight_label" class="icon-label"><img src="' + weightIcon + '" alt=""><span>Ruck weight (kg)</span></label><input type="number" id="p1_ruck_weight_value" step="0.1">' +
      '<label class="icon-label"><img src="' + terrainIcon + '" alt="">Terrain</label><select id="p1_terrain_type">' + terrainOptions + '</select>' +
      '<label class="icon-label"><img src="' + gradeIcon + '" alt="">Grade (%)</label><input type="number" id="p1_grade_percent" step="1">' +
      '</div>' +

      '<div class="card"><h2>Profile 2</h2>' +
      '<label>Profile name (optional)</label><input type="text" id="p2_name" maxlength="32">' +
      '<label id="p2_ruck_weight_label" class="icon-label"><img src="' + weightIcon + '" alt=""><span>Ruck weight (kg)</span></label><input type="number" id="p2_ruck_weight_value" step="0.1">' +
      '<label class="icon-label"><img src="' + terrainIcon + '" alt="">Terrain</label><select id="p2_terrain_type">' + terrainOptions + '</select>' +
      '<label class="icon-label"><img src="' + gradeIcon + '" alt="">Grade (%)</label><input type="number" id="p2_grade_percent" step="1">' +
      '</div>' +

      '<div class="card"><h2>Profile 3</h2>' +
      '<label>Profile name (optional)</label><input type="text" id="p3_name" maxlength="32">' +
      '<label id="p3_ruck_weight_label" class="icon-label"><img src="' + weightIcon + '" alt=""><span>Ruck weight (kg)</span></label><input type="number" id="p3_ruck_weight_value" step="0.1">' +
      '<label class="icon-label"><img src="' + terrainIcon + '" alt="">Terrain</label><select id="p3_terrain_type">' + terrainOptions + '</select>' +
      '<label class="icon-label"><img src="' + gradeIcon + '" alt="">Grade (%)</label><input type="number" id="p3_grade_percent" step="1">' +
      '</div>' +

      '<div class="card"><h2>Tracked Totals</h2>' +
      '<div class="stat-row"><span class="stat-label">Lifetime distance (km)</span><span class="stat-value" id="lifetime_distance_km_total">--</span></div>' +
      '<div class="stat-row"><span class="stat-label">Lifetime calories</span><span class="stat-value" id="lifetime_calories_total">--</span></div>' +
      '</div>' +

      '<div class="card"><h2>Last Activity</h2>' +
      '<div class="stat-row"><span class="stat-label">Date / Time</span><span class="stat-value" id="last_activity_datetime">--</span></div>' +
      '<div class="stat-row"><span class="stat-label">Distance (km)</span><span class="stat-value" id="last_activity_distance_km">--</span></div>' +
      '<div class="stat-row"><span class="stat-label">Pace</span><span class="stat-value" id="last_activity_pace">--</span></div>' +
      '<div class="stat-row"><span class="stat-label">Calories</span><span class="stat-value" id="last_activity_calories_display">--</span></div>' +
      '</div>' +


      '<div class="actions">' +
      '<button id="save" type="button">Save</button>' +
      '<button id="reset_defaults" type="button">Reset</button>' +
      '</div>' +
      '<script>' +
      'function $(id){return document.getElementById(id);}' +
      'function terrainTypeFromSettingsInner(type,factor){' +
      'if(type==="road"||type==="gravel"||type==="mixed"||type==="sand"||type==="snow"){return type;}' +
      'if(factor<=110){return "road";}' +
      'if(factor<=125){return "gravel";}' +
      'if(factor<=140){return "mixed";}' +
      'return "sand";}' +
      'function updateRuckWeightLabels(){' +
      'var unit=($("ruck_weight_unit").value==="1")?"lb":"kg";' +
      '$("p1_ruck_weight_label").getElementsByTagName("span")[0].textContent="Ruck weight ("+unit+")";' +
      '$("p2_ruck_weight_label").getElementsByTagName("span")[0].textContent="Ruck weight ("+unit+")";' +
      '$("p3_ruck_weight_label").getElementsByTagName("span")[0].textContent="Ruck weight ("+unit+")";' +
      '}' +
      'function copy(o){var r={};for(var k in o){if(Object.prototype.hasOwnProperty.call(o,k)){r[k]=o[k];}}return r;}' +
      'function terrainFactorFromType(t){' +
      'if(t==="road"){return 100;}' +
      'if(t==="gravel"){return 120;}' +
      'if(t==="mixed"){return 130;}' +
      'if(t==="sand"){return 150;}' +
      'if(t==="snow"){return 150;}' +
      'return 130;}' +
      'function walkKcalHr(wKg,spKmh,gradePct){' +
      'var sm=spKmh*1000/60;' +
      'var vo2=3.5+0.1*sm+1.8*sm*(gradePct/100);' +
      'return vo2*wKg*60/200;}' +
      'function ruckKcalHr(wKg,lKg,spKmh,t100,gradeTenths){' +
      'if(wKg<=0)return 0;' +
      'var V=spKmh/3.6,G=gradeTenths/1000,T=t100/100;' +
      'var tot=wKg+lKg,ratio=lKg/wKg;' +
      'var t1=1.5*wKg,t2=2*tot*ratio*ratio;' +
      'var inner=1.5*V*V+0.35*V*G;' +
      'var mult=(1+Math.sqrt(0.3*V*V)/7+Math.pow(V*ratio,2)/4)*1.1;' +
      'var kcal=(t1+t2+T*tot*inner*mult)*3600/4184;' +
      'var wk=walkKcalHr(wKg,spKmh,gradeTenths/10);' +
      'if(lKg>0&&kcal<wk)kcal=wk+wk*(lKg/wKg);' +
      'return kcal;}' +
      'function redrawChart(){' +
      'var paceMinKm=parseFloat($("chart_speed").value)||12;' +
      'var sp=60/paceMinKm;' +
      'var wv=parseFloat($("weight_value").value)||0;' +
      'var wkg=$("weight_unit").value==="1"?wv*0.453592:wv;' +
      'var ru=$("ruck_weight_unit").value;' +
      'function kg(id){var v=parseFloat($(id).value)||0;return ru==="1"?v*0.453592:v;}' +
      'function gr(id){return(parseInt($(id).value,10)||0)*10;}' +
      'function tf(id){return terrainFactorFromType($(id).value);}' +
      'var wk=Math.round(walkKcalHr(wkg,sp,0));' +
      'var bars=[' +
      '{label:"Walk",kcal:wk,col:"#888888"},' +
      '{label:$("p1_name").value||"P1",kcal:Math.round(ruckKcalHr(wkg,kg("p1_ruck_weight_value"),sp,tf("p1_terrain_type"),gr("p1_grade_percent"))),col:"#e45545"},' +
      '{label:$("p2_name").value||"P2",kcal:Math.round(ruckKcalHr(wkg,kg("p2_ruck_weight_value"),sp,tf("p2_terrain_type"),gr("p2_grade_percent"))),col:"#4a90d9"},' +
      '{label:$("p3_name").value||"P3",kcal:Math.round(ruckKcalHr(wkg,kg("p3_ruck_weight_value"),sp,tf("p3_terrain_type"),gr("p3_grade_percent"))),col:"#5cb85c"}' +
      '];' +
      'var mx=Math.max.apply(null,bars.map(function(b){return b.kcal;}));' +
      'var html=bars.map(function(b){' +
      'var pct=mx>0?(b.kcal/mx*100):0;' +
      'return"<div style=\'display:flex;align-items:center;margin-bottom:8px;\'>"' +
      '+"<div style=\'width:80px;font-size:12px;color:#555;flex-shrink:0;overflow:hidden;white-space:nowrap;text-overflow:ellipsis;\'>"+b.label+"</div>"' +
      '+"<div style=\'flex:1;background:#f0f0f0;border-radius:4px;height:24px;position:relative;\'>"' +
      '+"<div style=\'background:"+b.col+";width:"+pct.toFixed(1)+"%;height:100%;border-radius:4px;\'></div>"' +
      '+"<span style=\'position:absolute;right:6px;top:4px;font-size:13px;font-weight:600;color:#333;\'>"+b.kcal+" kcal</span>"' +
      '+"</div></div>";' +
      '}).join("");' +
      '$("calorie_chart").innerHTML=html;}' +
      'function queryParam(name){' +
      'var m=RegExp("[?&]"+name+"=([^&]*)").exec(location.search);' +
      'return m?decodeURIComponent(m[1]):"";' +
      '}' +
      'function formatNumber(n){return String(parseInt(n,10)||0);}' +
      'function formatKmFromMeters(m){' +
      'var n=parseInt(m,10)||0;' +
      'var km100=Math.round(n/10);' +
      'return String(Math.floor(km100/100))+"."+("0"+(km100%100)).slice(-2);' +
      '}' +
      'var s=' + JSON.stringify(s) + ';' +
      'var d=' + JSON.stringify(defaults) + ';' +
      'function applyToForm(cfg){' +
      '$("weight_value").value=(cfg.weight_value/10).toFixed(1);' +
      '$("weight_unit").value=cfg.weight_unit;' +
      '$("ruck_weight_unit").value=cfg.ruck_weight_unit;' +
      '$("stride_length_value").value=(cfg.stride_length_value/10).toFixed(1);' +
      '$("stride_length_unit").value=cfg.stride_length_unit;' +
      '$("p1_ruck_weight_value").value=(cfg.profile1_ruck_weight_value/10).toFixed(1);' +
      '$("p1_terrain_type").value=terrainTypeFromSettingsInner(cfg.profile1_terrain_type,cfg.profile1_terrain_factor);' +
      '$("p1_grade_percent").value=Math.round(cfg.profile1_grade_percent/10);' +
      '$("p1_name").value=cfg.profile1_name||"";' +
      '$("p2_ruck_weight_value").value=(cfg.profile2_ruck_weight_value/10).toFixed(1);' +
      '$("p2_terrain_type").value=terrainTypeFromSettingsInner(cfg.profile2_terrain_type,cfg.profile2_terrain_factor);' +
      '$("p2_grade_percent").value=Math.round(cfg.profile2_grade_percent/10);' +
      '$("p2_name").value=cfg.profile2_name||"";' +
      '$("p3_ruck_weight_value").value=(cfg.profile3_ruck_weight_value/10).toFixed(1);' +
      '$("p3_terrain_type").value=terrainTypeFromSettingsInner(cfg.profile3_terrain_type,cfg.profile3_terrain_factor);' +
      '$("p3_grade_percent").value=Math.round(cfg.profile3_grade_percent/10);' +
      '$("p3_name").value=cfg.profile3_name||"";' +
      '$("lifetime_distance_km_total").textContent=formatKmFromMeters(cfg.lifetime_distance_m_total);' +
      '$("lifetime_calories_total").textContent=formatNumber(cfg.lifetime_calories_total);' +
      'var ts=parseInt(cfg.last_activity_timestamp,10)||0;' +
      'if(ts>0){var _d=new Date(ts*1000);var _day=_d.getDate();var _sfx=["th","st","nd","rd"];var _v=_day%100;var _ord=_sfx[(_v-20)%10]||_sfx[_v]||_sfx[0];var _mo=["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"][_d.getMonth()];$("last_activity_datetime").textContent=_day+_ord+" "+_mo+" "+("0"+_d.getHours()).slice(-2)+":"+("0"+_d.getMinutes()).slice(-2);}else{$("last_activity_datetime").textContent="--";}' +
      '$("last_activity_distance_km").textContent=formatKmFromMeters(cfg.last_activity_distance_m||0);' +
      'var ps=parseInt(cfg.last_activity_pace_sec,10)||0;' +
      '$("last_activity_pace").textContent=ps>0?Math.floor(ps/60)+":"+(("0"+(ps%60)).slice(-2))+" / km":"--";' +
      '$("last_activity_calories_display").textContent=formatNumber(cfg.last_activity_calories||0);' +
      'updateRuckWeightLabels();' +
      '}' +
      'applyToForm(s);' +
      '$("ruck_weight_unit").addEventListener("change",updateRuckWeightLabels);' +
      '["chart_speed","weight_value","weight_unit","ruck_weight_unit",' +
      '"p1_ruck_weight_value","p1_terrain_type","p1_grade_percent","p1_name",' +
      '"p2_ruck_weight_value","p2_terrain_type","p2_grade_percent","p2_name",' +
      '"p3_ruck_weight_value","p3_terrain_type","p3_grade_percent","p3_name"' +
      '].forEach(function(id){var el=$(id);if(el){el.addEventListener("change",redrawChart);el.addEventListener("input",redrawChart);}});' +
      'redrawChart();' +
      '$("reset_defaults").addEventListener("click",function(){' +
      's=copy(d);' +
      'applyToForm(s);' +
      '$("save").click();' +
      '});' +

      'document.getElementById("save").addEventListener("click",function(){' +
      'var out={' +
      'weight_value: hiddenWeightValue,' +
      'stride_length_value: hiddenStrideLengthValue,' +

      'profile1_ruck_weight_value: Math.round(parseFloat($("p1_ruck_weight_value").value||0)*10),' +
      'profile1_terrain_type: $("p1_terrain_type").value,' +
      'profile1_terrain_factor: terrainFactorFromType($("p1_terrain_type").value),' +
      'profile1_grade_percent: (parseInt($("p1_grade_percent").value,10)||0)*10,' +
      'profile1_name: ($("p1_name").value||"").trim().slice(0,32),' +

      'profile2_ruck_weight_value: Math.round(parseFloat($("p2_ruck_weight_value").value||0)*10),' +
      'profile2_terrain_type: $("p2_terrain_type").value,' +
      'profile2_terrain_factor: terrainFactorFromType($("p2_terrain_type").value),' +
      'profile2_grade_percent: (parseInt($("p2_grade_percent").value,10)||0)*10,' +
      'profile2_name: ($("p2_name").value||"").trim().slice(0,32),' +

      'profile3_ruck_weight_value: Math.round(parseFloat($("p3_ruck_weight_value").value||0)*10),' +
      'profile3_terrain_type: $("p3_terrain_type").value,' +
      'profile3_terrain_factor: terrainFactorFromType($("p3_terrain_type").value),' +
      'profile3_grade_percent: (parseInt($("p3_grade_percent").value,10)||0)*10,' +
      'profile3_name: ($("p3_name").value||"").trim().slice(0,32)' +
      '};' +
      'var payload=encodeURIComponent(JSON.stringify(out));' +
      'var ret=queryParam("return_to");' +
      'if(ret){document.location=ret+payload;}' +
      'else{document.location="pebblejs://close#"+payload;}' +
      '});' +
      '</script>' +
      '</body></html>';

    // base64 encoding works reliably across both WebKit (Safari) and Chromium (Brave/Chrome).
    // The unescape(encodeURIComponent()) step makes btoa() safe for any Unicode characters.
    Pebble.openURL('data:text/html;charset=utf-8;base64,' + btoa(unescape(encodeURIComponent(html))));
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
