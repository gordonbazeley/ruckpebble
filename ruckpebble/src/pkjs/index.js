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
    profile1_terrain_factor: 100,
    profile1_terrain_type: 'road',
    profile1_grade_percent: 0,
    profile1_name: '30lb, road',

    profile2_ruck_weight_value: 80,
    profile2_terrain_factor: 100,
    profile2_terrain_type: 'gravel',
    profile2_grade_percent: 0,
    profile2_name: '15lb, trail',

    profile3_ruck_weight_value: 136,
    profile3_terrain_factor: 130,
    profile3_terrain_type: 'mixed',
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

  function normalizeSettings(settings) {
    var out = Object.assign({}, defaults, settings || {});
    out.profile1_terrain_type = terrainTypeFromSettings(out.profile1_terrain_type, out.profile1_terrain_factor);
    out.profile2_terrain_type = terrainTypeFromSettings(out.profile2_terrain_type, out.profile2_terrain_factor);
    out.profile3_terrain_type = terrainTypeFromSettings(out.profile3_terrain_type, out.profile3_terrain_factor);
    out.profile1_terrain_factor = terrainFactorFromType(out.profile1_terrain_type);
    out.profile2_terrain_factor = terrainFactorFromType(out.profile2_terrain_type);
    out.profile3_terrain_factor = terrainFactorFromType(out.profile3_terrain_type);
    return out;
  }

  function syncSettingsToWatch(settings) {
    var normalized = normalizeSettings(settings);
    saveSettings(normalized);
    Pebble.sendAppMessage(normalized, function() {
      console.log('initial/send settings success');
    }, function(err) {
      console.log('initial/send settings failed:', JSON.stringify(err));
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

  function openConfig() {
    var s = loadSettings();
    var p1TerrainType = terrainTypeFromSettings(s.profile1_terrain_type, s.profile1_terrain_factor);
    var p2TerrainType = terrainTypeFromSettings(s.profile2_terrain_type, s.profile2_terrain_factor);
    var p3TerrainType = terrainTypeFromSettings(s.profile3_terrain_type, s.profile3_terrain_factor);
    var terrainOptions = terrainOptionsHtml();
    var weightIcon = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAYAAADgdz34AAABYUlEQVR4AeRU4VUDMQgmncARdAPdQCdwBN3AbtJVdAPdoBvoBk6g9IOQ3LWBK32v/dW8EDj44MuRvKzowuOKCJh5w9PYZDubatEfihPTmqaxBleKJEUAkBYvNoxHfWaHCrlh7CyBkAAt+OV/rJjKVIjYHGiXulhGxXyrw1lcAuS9AnsDOZhgEY8p7jbfIsdtmUuAvDdLfrK2u2oFL7APRMr4Qs6ICO4Fi/xP0UsCzFbi2JDmiD2XiKDuaY5ctDnEhwS8WDAfDAnyJSoy2tBAgNvwKCk4tqP9F1yVsi0wkDucw0AAnBJAf0Gy88OAw03yCJ4NfMIf0DvV0TZXv7B6BPqbuH5pAmD1qqKe5kL36RH04ClG+pBbUX1ipgXnN5vqn33jjZJDbm9UqyHa+4M7Caholi546cRj+zSXePakEJ6NPQ8NBOjnD6TIOyO6izp06a7J6P52Fp1lIOiRMxkXJ9gBAAD//+xKIa4AAAAGSURBVAMAmz2PMR1V/7YAAAAASUVORK5CYII=';
    var terrainIcon = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAYAAADgdz34AAABqElEQVR4AdyU4VHDMAyFnzoBbACbwAawAWwAE1AmgA1gA9igbNJuABNEfHZsx65zvVyP/kEn2crTs56T9rTSie2fC7j7mvAhLO53h74mlDNiQ3zUvO4TQXjw1BLiEyELi/TmkxUxIC4xOJRv4kryG/bijQBkp/JCKCTsX8TorjpHbIjXgPikfAX11ghMZTtfWbTrjNnKrg3j+VKmnVjim5mC8Dml+ChwVdYLcHXIPxWnSantiMsMkgfhik+DXGTvBQCP9tg7vUhq0gu09USb22K3tjBzthdojxx4mukWNeNSzi0QcP4ohb8gaYUXCNje/2KBRkVZIFCxj0gXCLTftNaYr7ToAgGrezb5fKVFe4H2AjTsALDRu0rbO5J6gURiLsXpqPQTM3i2YBcq5qkyAtTOxqxdewGXnG7Q8nQkjR6abwfHBt+I9lDhOiMaVAp8cZqYvBeItfQa0rMx5Zg3JtN9KNm4XI1bWEUeUWGPBp+9+L7Ap0xlOtJ4rWTk70R0oHsiO5cw2sbSawbz3ghAuSX2pmOmTjucIkZeLjExpqwRmOC/y04u8AsAAP//3EypAQAAAAZJREFUAwCgdJgxQLeyzwAAAABJRU5ErkJggg==';
    var gradeIcon = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAYAAADgdz34AAAB30lEQVR4AcyUi1XsMAxEpa2AEnY74nXw6IAOgAooATqAEuhk6QAqwNwZO04M4bN8zsHHlmRFmrGlJJv45fH3CUop2/eK8K0bAH4F+B59jPbEPrLRxJcJADoF4z9L806irYemrb5EALjKcmmEiF1mPsrGXyKY2rR1MAEgR+TuG9AF4PfsAz8+WeM6mID0VoK8BfycvcBvOPf2xeH1KN4k4ESlPJV5TLbSStwD/i8YBEBSjhM7ZqGd1yoBSXo7oiaU8HCyrchN7mQRp16cRQ3cfeoGLclvR3psLCehXTCIq72o/LUXPkR1EOK5dgOa5aATR7wtHhyV0XsRHmaxJTEQcCrA5U7V+FoWvtUZ1CMjFOdehIYZLbTz6gSgqCx+EyjHVOPaC4Uu82xnTHF67JWWg+gEeOvXmDGBq4EiDYCSxlpZqBEY5KzMkaUTEK/rotIfDpl7qoCKj3qhmMXy9fq+E3QPBuXatzCRXuM6YOYQ+4oA8FMauFUYZXG5howPN+1oLe4VAWW5bM8OBx+xDTMQ6G9gb+j8of88F5IXtZx2WZTl76SmVYRJdoJSypVh25P5MCteu5LLEmwbremkpSPmnx2vzwkrqftS2Z5Fez+bY9zhbA5xTavfYHL8tP51gmcAAAD//2tJwoIAAAAGSURBVAMAu73qMUTY1OoAAAAASUVORK5CYII=';
    var html = '' +
      '<!doctype html><html><head><meta charset="utf-8">' +
      '<meta name="viewport" content="width=device-width,initial-scale=1">' +
      '<title>Ruck Settings</title>' +
      '<style>' +
      'body{font-family:Helvetica,Arial,sans-serif;margin:16px;background:#f5f5f5;color:#111;}' +
      'h1{font-size:20px;margin:0 0 12px;}h2{font-size:16px;margin:18px 0 8px;}' +
      'label{display:block;margin:10px 0 4px;font-weight:600;}' +
      '.icon-label{display:flex;align-items:center;gap:6px;}' +
      '.icon-label img{width:14px;height:14px;display:inline-block;filter:brightness(0) invert(1);}' +
      '.icon-chip{display:inline-flex;align-items:center;justify-content:center;width:18px;height:18px;border-radius:4px;background:#111;}' +
      'input,select{width:100%;padding:8px;font-size:14px;box-sizing:border-box;}' +
      '.row{display:flex;gap:8px;}.row>div{flex:1;}' +
      '.card{background:#fff;border-radius:8px;padding:12px;margin-top:10px;}' +
      '.actions{display:flex;gap:8px;}' +
      'button{margin-top:16px;width:100%;padding:11px;font-size:16px;background:#111;color:#fff;border:0;border-radius:6px;}' +
      '#reset_defaults{background:#666;}' +
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
      '<label>Profile name (optional)</label><input type="text" id="p1_name" maxlength="32">' +
      '<label id="p1_ruck_weight_label" class="icon-label"><span>Ruck weight (kg)</span><span class="icon-chip"><img src="' + weightIcon + '" alt=""></span></label><input type="number" id="p1_ruck_weight_value" step="0.1">' +
      '<label class="icon-label"><span>Terrain</span><span class="icon-chip"><img src="' + terrainIcon + '" alt=""></span></label><select id="p1_terrain_type">' + terrainOptions + '</select>' +
      '<label class="icon-label"><span>Grade (%)</span><span class="icon-chip"><img src="' + gradeIcon + '" alt=""></span></label><input type="number" id="p1_grade_percent" step="0.1">' +
      '</div>' +

      '<div class="card"><h2>Profile 2</h2>' +
      '<label>Profile name (optional)</label><input type="text" id="p2_name" maxlength="32">' +
      '<label id="p2_ruck_weight_label" class="icon-label"><span>Ruck weight (kg)</span><span class="icon-chip"><img src="' + weightIcon + '" alt=""></span></label><input type="number" id="p2_ruck_weight_value" step="0.1">' +
      '<label class="icon-label"><span>Terrain</span><span class="icon-chip"><img src="' + terrainIcon + '" alt=""></span></label><select id="p2_terrain_type">' + terrainOptions + '</select>' +
      '<label class="icon-label"><span>Grade (%)</span><span class="icon-chip"><img src="' + gradeIcon + '" alt=""></span></label><input type="number" id="p2_grade_percent" step="0.1">' +
      '</div>' +

      '<div class="card"><h2>Profile 3</h2>' +
      '<label>Profile name (optional)</label><input type="text" id="p3_name" maxlength="32">' +
      '<label id="p3_ruck_weight_label" class="icon-label"><span>Ruck weight (kg)</span><span class="icon-chip"><img src="' + weightIcon + '" alt=""></span></label><input type="number" id="p3_ruck_weight_value" step="0.1">' +
      '<label class="icon-label"><span>Terrain</span><span class="icon-chip"><img src="' + terrainIcon + '" alt=""></span></label><select id="p3_terrain_type">' + terrainOptions + '</select>' +
      '<label class="icon-label"><span>Grade (%)</span><span class="icon-chip"><img src="' + gradeIcon + '" alt=""></span></label><input type="number" id="p3_grade_percent" step="0.1">' +
      '</div>' +

      '<div class="actions">' +
      '<button id="reset_defaults" type="button">Reset to defaults</button>' +
      '<button id="save" type="button">Save</button>' +
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
      '$("p1_ruck_weight_label").querySelector("span").textContent="Ruck weight ("+unit+")";' +
      '$("p2_ruck_weight_label").querySelector("span").textContent="Ruck weight ("+unit+")";' +
      '$("p3_ruck_weight_label").querySelector("span").textContent="Ruck weight ("+unit+")";' +
      '}' +
      'function terrainFactorFromType(t){' +
      'if(t==="road"){return 100;}' +
      'if(t==="gravel"){return 120;}' +
      'if(t==="mixed"){return 130;}' +
      'if(t==="sand"){return 150;}' +
      'if(t==="snow"){return 150;}' +
      'return 130;}' +
      'function queryParam(name){' +
      'var m=RegExp("[?&]"+name+"=([^&]*)").exec(location.search);' +
      'return m?decodeURIComponent(m[1]):"";' +
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
      '$("p1_grade_percent").value=(cfg.profile1_grade_percent/10).toFixed(1);' +
      '$("p1_name").value=cfg.profile1_name||"";' +
      '$("p2_ruck_weight_value").value=(cfg.profile2_ruck_weight_value/10).toFixed(1);' +
      '$("p2_terrain_type").value=terrainTypeFromSettingsInner(cfg.profile2_terrain_type,cfg.profile2_terrain_factor);' +
      '$("p2_grade_percent").value=(cfg.profile2_grade_percent/10).toFixed(1);' +
      '$("p2_name").value=cfg.profile2_name||"";' +
      '$("p3_ruck_weight_value").value=(cfg.profile3_ruck_weight_value/10).toFixed(1);' +
      '$("p3_terrain_type").value=terrainTypeFromSettingsInner(cfg.profile3_terrain_type,cfg.profile3_terrain_factor);' +
      '$("p3_grade_percent").value=(cfg.profile3_grade_percent/10).toFixed(1);' +
      '$("p3_name").value=cfg.profile3_name||"";' +
      'updateRuckWeightLabels();' +
      '}' +
      'applyToForm(s);' +
      '$("ruck_weight_unit").addEventListener("change",updateRuckWeightLabels);' +
      '$("reset_defaults").addEventListener("click",function(){' +
      's=Object.assign({},d);' +
      'applyToForm(s);' +
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
      'profile1_grade_percent: Math.round(parseFloat($("p1_grade_percent").value||0)*10),' +
      'profile1_name: ($("p1_name").value||"").trim().slice(0,32),' +

      'profile2_ruck_weight_value: Math.round(parseFloat($("p2_ruck_weight_value").value||0)*10),' +
      'profile2_terrain_type: $("p2_terrain_type").value,' +
      'profile2_terrain_factor: terrainFactorFromType($("p2_terrain_type").value),' +
      'profile2_grade_percent: Math.round(parseFloat($("p2_grade_percent").value||0)*10),' +
      'profile2_name: ($("p2_name").value||"").trim().slice(0,32),' +

      'profile3_ruck_weight_value: Math.round(parseFloat($("p3_ruck_weight_value").value||0)*10),' +
      'profile3_terrain_type: $("p3_terrain_type").value,' +
      'profile3_terrain_factor: terrainFactorFromType($("p3_terrain_type").value),' +
      'profile3_grade_percent: Math.round(parseFloat($("p3_grade_percent").value||0)*10),' +
      'profile3_name: ($("p3_name").value||"").trim().slice(0,32),' +
      'sim_steps_enabled: 0,' +
      'sim_steps_spm: (s.sim_steps_spm||122)' +
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
    openConfig();
  });

  Pebble.addEventListener('ready', function() {
    console.log('ready: syncing settings to watch');
    syncSettingsToWatch(loadSettings());
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
    syncSettingsToWatch(settings);
  });
})();
