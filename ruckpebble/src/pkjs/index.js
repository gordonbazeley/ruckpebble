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
    ruck_weight_unit: 1,
    stride_length_value: 780,
    stride_length_unit: 0,

    profile1_ruck_weight_value: 300,
    profile1_terrain_factor: 100,
    profile1_terrain_type: 'road',
    profile1_grade_percent: 0,
    profile1_name: '30lb, road',

    profile2_ruck_weight_value: 300,
    profile2_terrain_factor: 100,
    profile2_terrain_type: 'road',
    profile2_grade_percent: 0,
    profile2_name: '30lb, hilly',

    profile3_ruck_weight_value: 150,
    profile3_terrain_factor: 100,
    profile3_terrain_type: 'road',
    profile3_grade_percent: 0,
    profile3_name: '15lb, road',
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
      weight_unit:               normalized.weight_unit,
      ruck_weight_unit:          normalized.ruck_weight_unit,
      stride_length_value:       normalized.stride_length_value,
      stride_length_unit:        normalized.stride_length_unit,
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

  function openConfig(settingsSnapshot) {
    var s = normalizeSettings(settingsSnapshot || s_latestSettingsSnapshot || loadSettings());
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
      'weight_value: Math.round(parseFloat($("weight_value").value||0)*10),' +
      'weight_unit: parseInt($("weight_unit").value,10),' +
      'ruck_weight_unit: parseInt($("ruck_weight_unit").value,10),' +
      'stride_length_value: Math.round(parseFloat($("stride_length_value").value||0)*10),' +
      'stride_length_unit: parseInt($("stride_length_unit").value,10),' +

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

    Pebble.openURL('data:text/html,' + encodeURIComponent(html));
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

    var layout = { type: 'genericPin', title: 'Ruck complete' };
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
