/* Settings page for Pebble app with shared settings + 3 profiles. */
(function() {
  var SETTINGS_KEY = 'ruck_settings_v2';

  var defaults = {
    weight_value: 800,
    weight_unit: 0,
    ruck_weight_unit: 0,
    stride_length_value: 780,
    stride_length_unit: 0,

    profile1_ruck_weight_value: 136,
    profile1_terrain_factor: 200,
    profile1_grade_percent: 0,
    profile1_name: '',

    profile2_ruck_weight_value: 80,
    profile2_terrain_factor: 100,
    profile2_grade_percent: 0,
    profile2_name: '',

    profile3_ruck_weight_value: 120,
    profile3_terrain_factor: 150,
    profile3_grade_percent: 0,
    profile3_name: '',

    sim_steps_enabled: 0,
    sim_steps_spm: 122
  };

  function loadSettings() {
    var raw = localStorage.getItem(SETTINGS_KEY);
    if (!raw) {
      return defaults;
    }
    try {
      return Object.assign({}, defaults, JSON.parse(raw));
    } catch (e) {
      return defaults;
    }
  }

  function saveSettings(settings) {
    localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
  }

  function openConfig() {
    var s = loadSettings();
    var html = '' +
      '<!doctype html><html><head><meta charset="utf-8">' +
      '<meta name="viewport" content="width=device-width,initial-scale=1">' +
      '<title>Ruck Settings</title>' +
      '<style>' +
      'body{font-family:Helvetica,Arial,sans-serif;margin:16px;background:#f5f5f5;color:#111;}' +
      'h1{font-size:20px;margin:0 0 12px;}h2{font-size:16px;margin:18px 0 8px;}' +
      'label{display:block;margin:10px 0 4px;font-weight:600;}' +
      'input,select{width:100%;padding:8px;font-size:14px;box-sizing:border-box;}' +
      '.row{display:flex;gap:8px;}.row>div{flex:1;}' +
      '.card{background:#fff;border-radius:8px;padding:12px;margin-top:10px;}' +
      'button{margin-top:16px;width:100%;padding:11px;font-size:16px;background:#111;color:#fff;border:0;border-radius:6px;}' +
      '</style></head><body>' +
      '<h1>Ruck Settings</h1>' +

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
      '<label>Profile name (optional)</label><input type="text" id="p1_name" maxlength="20">' +
      '<label>Ruck weight</label><input type="number" id="p1_ruck_weight_value" step="0.1">' +
      '<label>Terrain factor (μ)</label><input type="number" id="p1_terrain_factor" step="0.01">' +
      '<label>Grade (%)</label><input type="number" id="p1_grade_percent" step="0.1">' +
      '</div>' +

      '<div class="card"><h2>Profile 2</h2>' +
      '<label>Profile name (optional)</label><input type="text" id="p2_name" maxlength="20">' +
      '<label>Ruck weight</label><input type="number" id="p2_ruck_weight_value" step="0.1">' +
      '<label>Terrain factor (μ)</label><input type="number" id="p2_terrain_factor" step="0.01">' +
      '<label>Grade (%)</label><input type="number" id="p2_grade_percent" step="0.1">' +
      '</div>' +

      '<div class="card"><h2>Profile 3</h2>' +
      '<label>Profile name (optional)</label><input type="text" id="p3_name" maxlength="20">' +
      '<label>Ruck weight</label><input type="number" id="p3_ruck_weight_value" step="0.1">' +
      '<label>Terrain factor (μ)</label><input type="number" id="p3_terrain_factor" step="0.01">' +
      '<label>Grade (%)</label><input type="number" id="p3_grade_percent" step="0.1">' +
      '</div>' +

      '<div class="card"><h2>Emulator</h2>' +
      '<label>Simulated steps</label>' +
      '<div class="row"><div><select id="sim_steps_enabled"><option value="0">Off</option><option value="1">On</option></select></div>' +
      '<div><input type="number" id="sim_steps_spm" step="1"></div></div>' +
      '</div>' +

      '<button id="save">Save</button>' +
      '<script>' +
      'function $(id){return document.getElementById(id);}' +
      'var s=' + JSON.stringify(s) + ';' +
      '$("weight_value").value=(s.weight_value/10).toFixed(1);' +
      '$("weight_unit").value=s.weight_unit;' +
      '$("ruck_weight_unit").value=s.ruck_weight_unit;' +
      '$("stride_length_value").value=(s.stride_length_value/10).toFixed(1);' +
      '$("stride_length_unit").value=s.stride_length_unit;' +

      '$("p1_ruck_weight_value").value=(s.profile1_ruck_weight_value/10).toFixed(1);' +
      '$("p1_terrain_factor").value=(s.profile1_terrain_factor/100).toFixed(2);' +
      '$("p1_grade_percent").value=(s.profile1_grade_percent/10).toFixed(1);' +
      '$("p1_name").value=s.profile1_name||"";' +

      '$("p2_ruck_weight_value").value=(s.profile2_ruck_weight_value/10).toFixed(1);' +
      '$("p2_terrain_factor").value=(s.profile2_terrain_factor/100).toFixed(2);' +
      '$("p2_grade_percent").value=(s.profile2_grade_percent/10).toFixed(1);' +
      '$("p2_name").value=s.profile2_name||"";' +

      '$("p3_ruck_weight_value").value=(s.profile3_ruck_weight_value/10).toFixed(1);' +
      '$("p3_terrain_factor").value=(s.profile3_terrain_factor/100).toFixed(2);' +
      '$("p3_grade_percent").value=(s.profile3_grade_percent/10).toFixed(1);' +
      '$("p3_name").value=s.profile3_name||"";' +

      '$("sim_steps_enabled").value=s.sim_steps_enabled;' +
      '$("sim_steps_spm").value=s.sim_steps_spm;' +

      'document.getElementById("save").addEventListener("click",function(){' +
      'var out={' +
      'weight_value: Math.round(parseFloat($("weight_value").value||0)*10),' +
      'weight_unit: parseInt($("weight_unit").value,10),' +
      'ruck_weight_unit: parseInt($("ruck_weight_unit").value,10),' +
      'stride_length_value: Math.round(parseFloat($("stride_length_value").value||0)*10),' +
      'stride_length_unit: parseInt($("stride_length_unit").value,10),' +

      'profile1_ruck_weight_value: Math.round(parseFloat($("p1_ruck_weight_value").value||0)*10),' +
      'profile1_terrain_factor: Math.round(parseFloat($("p1_terrain_factor").value||0)*100),' +
      'profile1_grade_percent: Math.round(parseFloat($("p1_grade_percent").value||0)*10),' +
      'profile1_name: ($("p1_name").value||"").trim().slice(0,20),' +

      'profile2_ruck_weight_value: Math.round(parseFloat($("p2_ruck_weight_value").value||0)*10),' +
      'profile2_terrain_factor: Math.round(parseFloat($("p2_terrain_factor").value||0)*100),' +
      'profile2_grade_percent: Math.round(parseFloat($("p2_grade_percent").value||0)*10),' +
      'profile2_name: ($("p2_name").value||"").trim().slice(0,20),' +

      'profile3_ruck_weight_value: Math.round(parseFloat($("p3_ruck_weight_value").value||0)*10),' +
      'profile3_terrain_factor: Math.round(parseFloat($("p3_terrain_factor").value||0)*100),' +
      'profile3_grade_percent: Math.round(parseFloat($("p3_grade_percent").value||0)*10),' +
      'profile3_name: ($("p3_name").value||"").trim().slice(0,20),' +

      'sim_steps_enabled: parseInt($("sim_steps_enabled").value,10),' +
      'sim_steps_spm: parseInt($("sim_steps_spm").value,10)' +
      '};' +
      'var payload=encodeURIComponent(JSON.stringify(out));' +
      'document.location="pebblejs://close#"+payload;' +
      '});' +
      '</script>' +
      '</body></html>';

    Pebble.openURL('data:text/html,' + encodeURIComponent(html));
  }

  Pebble.addEventListener('showConfiguration', function() {
    openConfig();
  });

  Pebble.addEventListener('webviewclosed', function(e) {
    if (!e || !e.response) {
      return;
    }
    var data = e.response;
    var settings;
    try {
      settings = JSON.parse(decodeURIComponent(data));
    } catch (err) {
      return;
    }
    saveSettings(settings);
    Pebble.sendAppMessage(settings);
  });
})();
