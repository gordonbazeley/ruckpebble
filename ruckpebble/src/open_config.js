#!/usr/bin/env node
// Opens the RuckPebble settings page in Brave by running the built app JS in a mocked
// Pebble/pypkjs environment, capturing the data URI from Pebble.openURL(), and opening
// the HTML as a file:// URL (which works in Chromium-based browsers).

var fs = require('fs');
var path = require('path');
var child_process = require('child_process');

var SETTINGS_KEY = 'ruck_settings_v2';
var APP_UUID = 'e078ce1c-3c93-459f-aa3c-b5390977c663';
var TMP_HTML = '/tmp/ruck_config.html';

// ---- Locate pypkjs localStorage file ----
function findLocalStorageFile() {
  var candidates = [
    path.join(process.env.HOME, '.pebble-dev', APP_UUID, 'localStorage.json'),
    path.join(process.env.HOME, '.pebble-dev', 'pypkjs_localStorage.json'),
    path.join(process.env.HOME, 'Library', 'Application Support', 'Pebble SDK', '4.9.169', 'emery', 'localStorage.json'),
  ];
  for (var i = 0; i < candidates.length; i++) {
    if (fs.existsSync(candidates[i])) return candidates[i];
  }
  // Broader search (capped at 5s)
  try {
    var result = child_process.execSync(
      'find "$HOME" -name "*.json" -exec grep -l "' + SETTINGS_KEY + '" {} \\; 2>/dev/null | head -1',
      { timeout: 5000, env: process.env }
    ).toString().trim();
    if (result) return result;
  } catch (e) {}
  return null;
}

// ---- Build mocked environment ----
var storageData = {};
var lsFile = findLocalStorageFile();
if (lsFile) {
  try {
    storageData = JSON.parse(fs.readFileSync(lsFile, 'utf8'));
    console.log('Loaded settings from', lsFile);
  } catch (e) {
    console.warn('Could not parse localStorage file:', e.message);
  }
} else {
  console.warn('pypkjs localStorage not found — opening with default settings');
}

global.localStorage = {
  getItem: function(k) { return Object.prototype.hasOwnProperty.call(storageData, k) ? storageData[k] : null; },
  setItem: function(k, v) { storageData[k] = v; },
  removeItem: function(k) { delete storageData[k]; },
};

var capturedUrl = null;
var eventHandlers = {};

global.Pebble = {
  openURL: function(url) { capturedUrl = url; },
  sendAppMessage: function() {},
  addEventListener: function(event, handler) { eventHandlers[event] = handler; },
  getTimelineToken: function(_success, fail) { if (fail) fail('not available'); },
};

global.XMLHttpRequest = function() {
  this.open = function() {};
  this.setRequestHeader = function() {};
  this.send = function() {};
  this.onload = null;
  this.onerror = null;
};

// ---- Load and run app JS ----
var appJs = path.resolve(__dirname, '..', 'build', 'pebble-js-app.js');
if (!fs.existsSync(appJs)) {
  console.error('Error: ' + appJs + ' not found. Run pebble build first.');
  process.exit(1);
}

try {
  require(appJs);
} catch (e) {
  console.error('Error loading app JS:', e.message);
  process.exit(1);
}

// Fire ready to populate s_latestSettingsSnapshot from stored settings
if (eventHandlers['ready']) {
  try { eventHandlers['ready'](); } catch (e) {}
}

// Fire showConfiguration to generate and capture the config URL
if (eventHandlers['showConfiguration']) {
  try { eventHandlers['showConfiguration'](); } catch (e) {}
}

if (!capturedUrl) {
  console.error('Error: app JS did not call Pebble.openURL() — could not capture config URL.');
  process.exit(1);
}

// ---- Decode HTML and open in Brave ----
var html;
if (capturedUrl.indexOf('base64,') !== -1) {
  html = Buffer.from(capturedUrl.split('base64,')[1], 'base64').toString('utf8');
} else if (capturedUrl.startsWith('data:text/html,')) {
  html = decodeURIComponent(capturedUrl.slice('data:text/html,'.length));
} else {
  // Non-data URI — open directly
  child_process.execSync('open -a "Brave Browser" ' + JSON.stringify(capturedUrl));
  process.exit(0);
}

fs.writeFileSync(TMP_HTML, html);
child_process.execSync('open -a "Brave Browser" ' + JSON.stringify('file://' + TMP_HTML));
console.log('Config page opened in Brave.');
