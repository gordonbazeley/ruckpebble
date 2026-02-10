/* Simple settings page for Pebble app. */
(function() {
  var SETTINGS_KEY = 'ruck_settings_v1';

  var defaults = {
    weight_value: 800, // 80.0
    weight_unit: 0, // 0=kg, 1=lb
    ruck_weight_value: 300, // 30.0
    ruck_weight_unit: 1,
    stride_length_value: 780, // 78.0
    stride_length_unit: 0, // 0=cm, 1=in
    terrain_factor: 100, // 1.00
    grade_percent: 0, // 0.0
    sim_steps_enabled: 0,
    sim_steps_spm: 122
  };

  function loadSettings() {
    var raw = localStorage.getItem(SETTINGS_KEY);
    if (!raw) {
      return defaults;
    }
    try {
      var parsed = JSON.parse(raw);
      return Object.assign({}, defaults, parsed);
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
      'h1{font-size:18px;margin:0 0 12px;}' +
      'label{display:block;margin:12px 0 4px;font-weight:600;}' +
      'input,select{width:100%;padding:8px;font-size:14px;}' +
      '.row{display:flex;gap:8px;}' +
      '.row > div{flex:1;}' +
      'button{margin-top:16px;width:100%;padding:10px;font-size:16px;background:#111;color:#fff;border:0;border-radius:4px;}' +
      '</style></head><body>' +
      '<h1>Ruck Settings</h1>' +
      '<label>Body weight</label>' +
      '<div class="row"><div><input type="number" id="weight_value" step="0.1"></div>' +
      '<div><select id="weight_unit"><option value="0">kg</option><option value="1">lb</option></select></div></div>' +
      '<label>Ruck weight</label>' +
      '<div class="row"><div><input type="number" id="ruck_weight_value" step="0.1"></div>' +
      '<div><select id="ruck_weight_unit"><option value="0">kg</option><option value="1">lb</option></select></div></div>' +
      '<label>Stride length</label>' +
      '<div class="row"><div><input type="number" id="stride_length_value" step="0.1"></div>' +
      '<div><select id="stride_length_unit"><option value="0">cm</option><option value="1">in</option></select></div></div>' +
      '<label>Terrain factor (μ)</label>' +
      '<input type="number" id="terrain_factor" step="0.01">' +
      '<label>Grade (%)</label>' +
      '<input type="number" id="grade_percent" step="0.1">' +
      '<label>Simulated steps (emulator)</label>' +
      '<div class="row"><div><select id="sim_steps_enabled"><option value="0">Off</option><option value="1">On</option></select></div>' +
      '<div><input type="number" id="sim_steps_spm" step="1"></div></div>' +
      '<button id="save">Save</button>' +
      '<script>' +
      'function $(id){return document.getElementById(id);}' +
      'var s=' + JSON.stringify(s) + ';' +
      '$("weight_value").value=(s.weight_value/10).toFixed(1);' +
      '$("weight_unit").value=s.weight_unit;' +
      '$("ruck_weight_value").value=(s.ruck_weight_value/10).toFixed(1);' +
      '$("ruck_weight_unit").value=s.ruck_weight_unit;' +
      '$("stride_length_value").value=(s.stride_length_value/10).toFixed(1);' +
      '$("stride_length_unit").value=s.stride_length_unit;' +
      '$("terrain_factor").value=(s.terrain_factor/100).toFixed(2);' +
      '$("grade_percent").value=(s.grade_percent/10).toFixed(1);' +
      '$("sim_steps_enabled").value=s.sim_steps_enabled;' +
      '$("sim_steps_spm").value=s.sim_steps_spm;' +
      'document.getElementById("save").addEventListener("click",function(){' +
      'var out={' +
      'weight_value: Math.round(parseFloat($("weight_value").value||0)*10),' +
      'weight_unit: parseInt($("weight_unit").value,10),' +
      'ruck_weight_value: Math.round(parseFloat($("ruck_weight_value").value||0)*10),' +
      'ruck_weight_unit: parseInt($("ruck_weight_unit").value,10),' +
      'stride_length_value: Math.round(parseFloat($("stride_length_value").value||0)*10),' +
      'stride_length_unit: parseInt($("stride_length_unit").value,10),' +
      'terrain_factor: Math.round(parseFloat($("terrain_factor").value||0)*100),' +
      'grade_percent: Math.round(parseFloat($("grade_percent").value||0)*10),' +
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
