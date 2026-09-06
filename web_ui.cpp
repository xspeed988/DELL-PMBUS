#include "web_ui.h"
#include "oled.h"
#include <WiFi.h>

extern float g_pout_max;
extern float g_vout_min;

#define HIST_LEN   600
#define HIST_EVERY   2   // fast ticks per sample; 2 × 20 ms = 40 ms = 10 Hz
struct HistSample { float pout; float iout; float vout; };
extern HistSample g_hist[];
extern uint16_t   g_hist_head;
extern uint16_t   g_hist_count;

// ─────────────────────────────────────────────────────────────────────────────
// JSON helpers
// ─────────────────────────────────────────────────────────────────────────────
static void jf(String &s, const char *key, float val, int dec = 3, bool last = false) {
    char tmp[24];
    dtostrf(val, 1, dec, tmp);
    s += "\""; s += key; s += "\":"; s += tmp;
    if (!last) s += ",";
}
static void ji(String &s, const char *key, long val, bool last = false) {
    s += "\""; s += key; s += "\":"; s += val;
    if (!last) s += ",";
}
static void js(String &s, const char *key, const String &val, bool last = false) {
    s += "\""; s += key; s += "\":\""; s += val; s += "\"";
    if (!last) s += ",";
}

// ─────────────────────────────────────────────────────────────────────────────
// /data
// ─────────────────────────────────────────────────────────────────────────────
void handle_data() {
    String j = "{";
    j += "\"valid\":"; j += g_data.valid ? "true" : "false"; j += ",";

    js(j, "mfr_id",       g_mfr.id);
    js(j, "mfr_model",    g_mfr.model);
    js(j, "mfr_revision", g_mfr.revision);
    js(j, "mfr_location", g_mfr.location);
    js(j, "mfr_date",     g_mfr.date);
    js(j, "mfr_serial",   g_mfr.serial);

    jf(j, "vin",   g_data.V_in,  2);
    jf(j, "iin",   g_data.I_in,  3);
    jf(j, "pin",   g_data.W_in,  1);
    jf(j, "vout",  g_data.V_out, 3);
    jf(j, "iout",  g_data.I_out, 3);
    jf(j, "pout",  g_data.W_out, 1);
    jf(j, "t1",    g_data.T1,    1);
    jf(j, "t2",    g_data.T2,    1);
    jf(j, "t3",    g_data.T3,    1);
    jf(j, "fan",   g_data.fan,   0);
    jf(j, "fan_cmd", g_data.fan_cmd, 0);
    jf(j, "iout_max", g_iout_max, 3);
    jf(j, "pout_max", g_pout_max, 1);
    jf(j, "vout_min", g_vout_min > 1e8f ? 0.0f : g_vout_min, 3);

    ji(j, "status_byte",  g_data.status_byte);
    ji(j, "status_word",  g_data.status_word);
    ji(j, "status_vout",  g_data.status_vout);
    ji(j, "status_iout",  g_data.status_iout);
    ji(j, "status_input", g_data.status_input);
    ji(j, "status_temp",  g_data.status_temp);
    ji(j, "status_cml",   g_data.status_cml);

    jf(j, "vout_command",  g_limits.vout_command,  3);
    jf(j, "vout_max",      g_limits.vout_max,      3);
    jf(j, "vout_ov_fault", g_limits.vout_ov_fault, 3);
    jf(j, "vout_ov_warn",  g_limits.vout_ov_warn,  3);
    jf(j, "vout_uv_fault", g_limits.vout_uv_fault, 3);
    jf(j, "iout_oc_fault", g_limits.iout_oc_fault, 2);
    jf(j, "iout_oc_warn",  g_limits.iout_oc_warn,  2);

    jf(j, "mfr_vin_min",      g_limits.mfr_vin_min,      1);
    jf(j, "mfr_vin_max",      g_limits.mfr_vin_max,      1);
    jf(j, "mfr_iin_max",      g_limits.mfr_iin_max,      2);
    jf(j, "mfr_pin_max",      g_limits.mfr_pin_max,      1);
    jf(j, "mfr_vout_min",     g_limits.mfr_vout_min,     3);
    jf(j, "mfr_vout_max",     g_limits.mfr_vout_max,     3);
    jf(j, "mfr_iout_max",     g_limits.mfr_iout_max,     2);
    jf(j, "mfr_pout_max",     g_limits.mfr_pout_max,     1);
    jf(j, "mfr_tambient_min", g_limits.mfr_tambient_min, 1);
    jf(j, "mfr_tambient_max", g_limits.mfr_tambient_max, 1);

    j += "\"oled\":[";
    for (int i = 0; i < OLED_ROWS; i++) {
        j += "\""; j += FIELD_NAMES[oled_fields[i]]; j += "\"";
        if (i < OLED_ROWS - 1) j += ",";
    }
    j += "]}";

    server.send(200, "application/json", j);
}

void handle_history() {
    String j;
    j.reserve(60000);

    uint16_t n = g_hist_count;
    uint16_t start = (n < HIST_LEN) ? 0 : g_hist_head;

    j += "{\"n\":"; j += n;
    j += ",\"ms\":"; j += (HIST_EVERY * 20);
    j += ",\"pout\":[";
    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx = (start + i) % HIST_LEN;
        char tmp[12]; dtostrf(g_hist[idx].pout, 1, 1, tmp);
        j += tmp;
        if (i < n - 1) j += ",";
    }
    j += "],\"iout\":[";
    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx = (start + i) % HIST_LEN;
        char tmp[12]; dtostrf(g_hist[idx].iout, 1, 2, tmp);
        j += tmp;
        if (i < n - 1) j += ",";
    }
    j += "],\"vout\":[";
    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx = (start + i) % HIST_LEN;
        char tmp[12]; dtostrf(g_hist[idx].vout, 1, 3, tmp);
        j += tmp;
        if (i < n - 1) j += ",";
    }
    j += "]}";

    server.send(200, "application/json", j);
}

// ─────────────────────────────────────────────────────────────────────────────
// /reset_max
// ─────────────────────────────────────────────────────────────────────────────
void handle_reset_max() {
    g_iout_max = 0.0f;
    g_pout_max = 0.0f;
    g_vout_min = 1e9f;   // reset sentinel
    server.send(200, "application/json", "{\"ok\":true}");
}

// ─────────────────────────────────────────────────────────────────────────────
// /oled  (POST)
// ─────────────────────────────────────────────────────────────────────────────
void handle_oled_cfg() {
    bool changed = false;
    for (int i = 0; i < OLED_ROWS; i++) {
        String key = "row" + String(i);
        if (server.hasArg(key)) {
            FieldID fid = name_to_field(server.arg(key));
            if (fid < F_COUNT) { oled_fields[i] = fid; changed = true; }
        }
    }
    server.send(200, "application/json", changed ? "{\"ok\":true}" : "{\"ok\":false}");
}

// ─────────────────────────────────────────────────────────────────────────────
// /  (dashboard)
// ─────────────────────────────────────────────────────────────────────────────
void handle_root() {
    String fieldOpts = "";
    for (uint8_t i = 0; i < F_COUNT; i++) {
        fieldOpts += "\"" + String(FIELD_NAMES[i]) + "\"";
        if (i < F_COUNT - 1) fieldOpts += ",";
    }

    String html = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PSU Monitor</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@600;800&display=swap');
  :root{
    --bg:#080b0e;--surf:#0e1218;--surf2:#131920;--brd:#1c2430;
    --acc:#00d4ff;--acc2:#0099bb;--ok:#4ade80;--warn:#f87171;--mut:#3d4f63;
    --txt:#b8c5d3;--lbl:#526272;
    --pout-clr:#00d4ff;--iout-clr:#f59e0b;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--txt);font-family:'Share Tech Mono',monospace;min-height:100vh}

  /* ── top bar ── */
  .topbar{display:flex;align-items:center;gap:16px;padding:12px 20px;background:var(--surf);border-bottom:1px solid var(--brd)}
  .topbar h1{font-family:'Orbitron',sans-serif;font-size:.9rem;font-weight:800;color:var(--acc);letter-spacing:.14em}
  .topbar .sub{font-size:.62rem;color:var(--mut);margin-top:2px}
  #dot{width:9px;height:9px;border-radius:50%;background:var(--mut);margin-left:auto;flex-shrink:0;transition:.3s}
  #dot.ok{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  #dot.bad{background:var(--warn);box-shadow:0 0 8px var(--warn)}

  /* ── main 3-col grid ── */
  .main{padding:16px;display:grid;gap:12px;grid-template-columns:repeat(3,1fr)}
  @media(max-width:700px){.main{grid-template-columns:1fr}}
  .full{grid-column:1/-1}

  /* ── cards ── */
  .card{background:var(--surf);border:1px solid var(--brd);border-radius:3px;overflow:hidden}
  .ct{font-family:'Orbitron',sans-serif;font-size:.56rem;font-weight:600;letter-spacing:.15em;text-transform:uppercase;
      color:var(--lbl);padding:7px 14px;border-bottom:1px solid var(--brd);display:flex;align-items:center;gap:8px}
  .ct-dot{width:5px;height:5px;border-radius:50%;background:var(--acc2);flex-shrink:0}
  .cb{padding:8px 14px}

  /* ── big metric rows (main cards) ── */
  .mrow{display:flex;justify-content:space-between;align-items:baseline;padding:7px 0;border-bottom:1px solid var(--brd)}
  .mrow:last-child{border-bottom:none}
  .ml{color:var(--lbl);font-size:.75rem}
  .mv{font-family:'Orbitron',sans-serif;font-size:1.15rem;font-weight:700;color:var(--acc)}
  .mv.ok{color:var(--ok)}.mv.bad{color:var(--warn)}.mv.mut{color:var(--mut)}

  /* ── MFR info bar ── */
  .mfr-bar{
    display:flex;flex-wrap:wrap;gap:0;
    background:var(--surf);border:1px solid var(--brd);border-radius:3px;overflow:hidden;
    margin:0 16px;
  }
  .mfr-cell{
    flex:1;min-width:120px;padding:8px 14px;border-right:1px solid var(--brd);
  }
  .mfr-cell:last-child{border-right:none}
  .mfr-lbl{font-size:.58rem;color:var(--lbl);text-transform:uppercase;letter-spacing:.1em;margin-bottom:3px}
  .mfr-val{font-family:'Orbitron',sans-serif;font-size:.78rem;font-weight:600;color:var(--acc);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}

  /* ── secondary grid ── */
  .secondary{padding:12px 16px 16px;display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(200px,1fr))}

  /* ── small rows (secondary cards) ── */
  .row{display:flex;justify-content:space-between;align-items:baseline;padding:4px 0;border-bottom:1px solid var(--brd)}
  .row:last-child{border-bottom:none}
  .lbl{color:var(--lbl);font-size:.7rem}
  .val{font-family:'Orbitron',sans-serif;font-size:.78rem;font-weight:600;color:var(--acc)}
  .val.ok{color:var(--ok)}.val.bad{color:var(--warn)}.val.mut{color:var(--mut)}

  /* ── status 2-col ── */
  .statgrid{display:grid;grid-template-columns:1fr 1fr;gap:0}

  /* ── OLED config ── */
  .oled-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:8px}
  .osel label{font-size:.62rem;color:var(--lbl);display:block;margin-bottom:3px}
  .osel select{background:#0a0f14;color:var(--acc);border:1px solid var(--brd);border-radius:2px;
    padding:4px 6px;font-family:'Share Tech Mono',monospace;font-size:.73rem;width:100%}

  /* ── buttons ── */
  .btn{display:inline-flex;align-items:center;background:transparent;border:1px solid var(--acc2);color:var(--acc);
    font-family:'Orbitron',sans-serif;font-size:.58rem;font-weight:600;letter-spacing:.1em;
    padding:6px 16px;border-radius:2px;cursor:pointer;transition:.2s}
  .btn:hover{background:var(--acc2);color:#000}
  .btn-sm{border-color:var(--mut);color:var(--mut);font-size:.56rem;padding:2px 8px;margin-left:6px}
  .btn-sm:hover{border-color:var(--warn);color:var(--warn);background:transparent}
  #fb{font-size:.63rem;color:var(--ok);opacity:0;transition:.3s;margin-left:10px}
  #fb.show{opacity:1}

  .sec-hdr{padding:14px 16px 4px;font-family:'Orbitron',sans-serif;font-size:.56rem;
    font-weight:600;letter-spacing:.15em;text-transform:uppercase;color:var(--mut)}

  footer{padding:10px 20px;font-size:.58rem;color:var(--mut);border-top:1px solid var(--brd);
    display:flex;justify-content:space-between;margin-top:4px}

  /* ── power graph ── */
  .graph-wrap{position:relative;width:100%;height:180px}
  #pgraph{display:block;width:100%;height:100%}
  .graph-legend{display:flex;gap:18px;padding:6px 14px 8px;font-size:.62rem}
  .leg-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:5px;vertical-align:middle}
  .leg-pout .leg-dot{background:var(--pout-clr)}
  .leg-iout .leg-dot{background:var(--iout-clr)}
  .graph-axis-l{position:absolute;top:6px;left:4px;font-size:.55rem;color:var(--pout-clr);opacity:.7;pointer-events:none}
  .graph-axis-r{position:absolute;top:6px;right:4px;font-size:.55rem;color:var(--iout-clr);opacity:.7;pointer-events:none}
  .graph-axis-b{position:absolute;bottom:2px;right:14px;font-size:.55rem;color:var(--lbl);pointer-events:none}
</style>
</head>
<body>

<div class="topbar">
  <div>
    <h1>PSU Monitor</h1>
    <div class="sub">Dell D1100e &mdash; PMBus over I²C</div>
  </div>
  <div id="dot"></div>
</div>

<!-- ── primary 3-col ── -->
<div class="main">

  <div class="card">
    <div class="ct"><div class="ct-dot"></div>Input</div>
    <div class="cb">
      <div class="mrow"><span class="ml">Vin</span><span class="mv" id="v-vin">--</span></div>
      <div class="mrow"><span class="ml">Iin</span><span class="mv" id="v-iin">--</span></div>
      <div class="mrow"><span class="ml">Pin</span><span class="mv" id="v-pin">--</span></div>
    </div>
  </div>

  <div class="card">
    <div class="ct"><div class="ct-dot"></div>Output</div>
    <div class="cb">
      <div class="mrow"><span class="ml">Vout</span><span class="mv" id="v-vout">--</span></div>
      <div class="mrow"><span class="ml">Iout</span><span class="mv" id="v-iout">--</span></div>
      <div class="mrow"><span class="ml">Pout</span><span class="mv" id="v-pout">--</span></div>
      <div class="mrow"><span class="ml">Efficiency</span><span class="mv" id="v-eff">--</span></div>
      <div class="mrow">
        <span class="ml">Iout peak</span>
        <span><span class="mv" id="v-ipeak">--</span><button class="btn btn-sm" onclick="resetMax()">RST</button></span>
      </div>
      <div class="mrow">
        <span class="ml">Pout peak</span>
        <span><span class="mv" id="v-ppeak">--</span></span>
      </div>
      <div class="mrow">
        <span class="ml">Vout min</span>
        <span><span class="mv" id="v-vmin">--</span></span>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="ct"><div class="ct-dot"></div>Thermal &amp; Fan</div>
    <div class="cb">
      <div class="mrow"><span class="ml">T1</span><span class="mv" id="v-t1">--</span></div>
      <div class="mrow"><span class="ml">T2</span><span class="mv" id="v-t2">--</span></div>
      <div class="mrow"><span class="ml">T3</span><span class="mv" id="v-t3">--</span></div>
      <div class="mrow"><span class="ml">Fan</span><span class="mv" id="v-fan">--</span></div>
    </div>
  </div>

</div>

<!-- ── MFR info bar ── -->
<div class="mfr-bar">
  <div class="mfr-cell"><div class="mfr-lbl">Model</div><div class="mfr-val" id="m-model">--</div></div>
  <div class="mfr-cell"><div class="mfr-lbl">Serial</div><div class="mfr-val" id="m-serial">--</div></div>
  <div class="mfr-cell"><div class="mfr-lbl">Location</div><div class="mfr-val" id="m-loc">--</div></div>
  <div class="mfr-cell"><div class="mfr-lbl">Date</div><div class="mfr-val" id="m-date">--</div></div>
  <div class="mfr-cell"><div class="mfr-lbl">Revision</div><div class="mfr-val" id="m-rev">--</div></div>
  <div class="mfr-cell"><div class="mfr-lbl">ID</div><div class="mfr-val" id="m-id">--</div></div>
</div>

<!-- ── Power graph ── -->
<div style="padding:12px 16px 0">
  <div class="card full">
    <div class="ct">
      <div class="ct-dot"></div>Power History (60 s, 10 Hz)
      <span style="margin-left:auto;font-size:.55rem;color:var(--mut)" id="graph-age"></span>
    </div>
    <div class="graph-wrap">
      <canvas id="pgraph"></canvas>
      <span class="graph-axis-l" id="ax-pout-max"></span>
      <span class="graph-axis-r" id="ax-iout-max"></span>
      <span class="graph-axis-b">← older · newer →</span>
    </div>
    <div class="graph-legend">
      <span class="leg-pout"><span class="leg-dot"></span>Pout (W)</span>
      <span class="leg-iout"><span class="leg-dot"></span>Iout (A)</span>
    </div>
  </div>
</div>

<!-- ── secondary data ── -->
<div class="sec-hdr">Diagnostics &amp; Limits</div>
<div class="secondary">

  <div class="card">
    <div class="ct"><div class="ct-dot"></div>Status</div>
    <div class="cb statgrid">
      <div class="row"><span class="lbl">BYTE</span><span class="val" id="st-byte">--</span></div>
      <div class="row"><span class="lbl">WORD</span><span class="val" id="st-word">--</span></div>
      <div class="row"><span class="lbl">VOUT</span><span class="val" id="st-vout">--</span></div>
      <div class="row"><span class="lbl">IOUT</span><span class="val" id="st-iout">--</span></div>
      <div class="row"><span class="lbl">INPUT</span><span class="val" id="st-input">--</span></div>
      <div class="row"><span class="lbl">TEMP</span><span class="val" id="st-temp">--</span></div>
      <div class="row"><span class="lbl">CML</span><span class="val" id="st-cml">--</span></div>
    </div>
  </div>

  <div class="card">
    <div class="ct"><div class="ct-dot"></div>Protection Limits</div>
    <div class="cb">
      <div class="row"><span class="lbl">Vout cmd</span><span class="val mut" id="l-vcmd">--</span></div>
      <div class="row"><span class="lbl">Vout max</span><span class="val mut" id="l-vmax">--</span></div>
      <div class="row"><span class="lbl">Vout OV fault</span><span class="val mut" id="l-vovf">--</span></div>
      <div class="row"><span class="lbl">Vout OV warn</span><span class="val mut" id="l-vovw">--</span></div>
      <div class="row"><span class="lbl">Vout UV fault</span><span class="val mut" id="l-vuvf">--</span></div>
      <div class="row"><span class="lbl">Iout OC fault</span><span class="val mut" id="l-iocf">--</span></div>
      <div class="row"><span class="lbl">Iout OC warn</span><span class="val mut" id="l-iocw">--</span></div>
    </div>
  </div>

  <div class="card">
    <div class="ct"><div class="ct-dot"></div>MFR Rated Specs</div>
    <div class="cb">
      <div class="row"><span class="lbl">Vin</span><span class="val mut" id="r-vin">--</span></div>
      <div class="row"><span class="lbl">Iin max</span><span class="val mut" id="r-iinmax">--</span></div>
      <div class="row"><span class="lbl">Pin max</span><span class="val mut" id="r-pinmax">--</span></div>
      <div class="row"><span class="lbl">Vout</span><span class="val mut" id="r-vout">--</span></div>
      <div class="row"><span class="lbl">Iout max</span><span class="val mut" id="r-ioutmax">--</span></div>
      <div class="row"><span class="lbl">Pout max</span><span class="val mut" id="r-poutmax">--</span></div>
      <div class="row"><span class="lbl">Tamb</span><span class="val mut" id="r-tamb">--</span></div>
    </div>
  </div>

  <div class="card full">
    <div class="ct"><div class="ct-dot"></div>OLED Config</div>
    <div class="cb">
      <div class="oled-grid" id="oled-grid"></div>
      <div style="display:flex;align-items:center;margin-top:12px">
        <button class="btn" onclick="applyOled()">Apply to OLED</button>
        <span id="fb">&#10003; Applied</span>
      </div>
    </div>
  </div>

</div>

<footer>
  <span>auto-refresh 1 s</span>
  <span>)rawhtml";
    html += WiFi.localIP().toString();
    html += R"rawhtml(</span>
</footer>

<script>
const FIELDS=[)rawhtml";
    html += fieldOpts;
    html += R"rawhtml(];
let oledSynced = false;

// ── OLED selects ──────────────────────────────────────────────────────────────
const og = document.getElementById("oled-grid");
for (let i = 0; i < 6; i++) {
  const w = document.createElement("div"); w.className = "osel";
  const l = document.createElement("label"); l.textContent = "Row " + (i + 1);
  const s = document.createElement("select"); s.id = "oled-row-" + i;
  FIELDS.forEach(f => { const o = document.createElement("option"); o.value = f; o.textContent = f; s.appendChild(o); });
  w.appendChild(l); w.appendChild(s); og.appendChild(w);
}

// ── Graph ─────────────────────────────────────────────────────────────────────
const canvas = document.getElementById("pgraph");
const ctx    = canvas.getContext("2d");
const CSS_POUT = getComputedStyle(document.documentElement).getPropertyValue("--pout-clr").trim();
const CSS_IOUT = getComputedStyle(document.documentElement).getPropertyValue("--iout-clr").trim();
const CSS_BRD  = getComputedStyle(document.documentElement).getPropertyValue("--brd").trim();
const CSS_MUT  = getComputedStyle(document.documentElement).getPropertyValue("--mut").trim();

function resizeCanvas() {
  const rect = canvas.parentElement.getBoundingClientRect();
  canvas.width  = rect.width  * devicePixelRatio;
  canvas.height = rect.height * devicePixelRatio;
  canvas.style.width  = rect.width  + "px";
  canvas.style.height = rect.height + "px";
}
resizeCanvas();
window.addEventListener("resize", () => { resizeCanvas(); drawGraph(lastHist); });

let lastHist = null;

function drawGraph(h) {
  if (!h || h.n === 0) return;
  lastHist = h;

  const W = canvas.width, H = canvas.height;
  const PAD_L = 42, PAD_R = 42, PAD_T = 16, PAD_B = 22;
  const plotW = W - PAD_L - PAD_R;
  const plotH = H - PAD_T - PAD_B;
  const n = h.n;

  ctx.clearRect(0, 0, W, H);

  // ── axis ranges ──────────────────────────────────────────────────────────
  const pMax = Math.max(...h.pout, 10);
  const pTop = Math.ceil(pMax / 100) * 100;   // round to next 100 W
  const iMax = Math.max(...h.iout, 1);
  const iTop = Math.ceil(iMax / 10) * 10;      // round to next 10 A

  function xOf(i) { return PAD_L + (i / (n - 1 || 1)) * plotW; }
  function yOfP(v) { return PAD_T + plotH * (1 - v / pTop); }
  function yOfI(v) { return PAD_T + plotH * (1 - v / iTop); }

  // ── grid lines (4 horizontals) ────────────────────────────────────────────
  ctx.strokeStyle = CSS_BRD;
  ctx.lineWidth   = 1 * devicePixelRatio;
  for (let g = 0; g <= 4; g++) {
    const y = PAD_T + (g / 4) * plotH;
    ctx.beginPath(); ctx.moveTo(PAD_L, y); ctx.lineTo(W - PAD_R, y); ctx.stroke();
  }

  // ── Pout fill + line ──────────────────────────────────────────────────────
  ctx.beginPath();
  ctx.moveTo(xOf(0), yOfP(h.pout[0]));
  for (let i = 1; i < n; i++) ctx.lineTo(xOf(i), yOfP(h.pout[i]));
  ctx.lineTo(xOf(n - 1), PAD_T + plotH);
  ctx.lineTo(xOf(0),     PAD_T + plotH);
  ctx.closePath();
  const grad = ctx.createLinearGradient(0, PAD_T, 0, PAD_T + plotH);
  grad.addColorStop(0,   "rgba(0,212,255,0.22)");
  grad.addColorStop(1,   "rgba(0,212,255,0.01)");
  ctx.fillStyle = grad;
  ctx.fill();

  ctx.beginPath();
  ctx.moveTo(xOf(0), yOfP(h.pout[0]));
  for (let i = 1; i < n; i++) ctx.lineTo(xOf(i), yOfP(h.pout[i]));
  ctx.strokeStyle = CSS_POUT;
  ctx.lineWidth   = 1.5 * devicePixelRatio;
  ctx.stroke();

  // ── Iout line ─────────────────────────────────────────────────────────────
  ctx.beginPath();
  ctx.moveTo(xOf(0), yOfI(h.iout[0]));
  for (let i = 1; i < n; i++) ctx.lineTo(xOf(i), yOfI(h.iout[i]));
  ctx.strokeStyle = CSS_IOUT;
  ctx.lineWidth   = 1.5 * devicePixelRatio;
  ctx.setLineDash([4 * devicePixelRatio, 3 * devicePixelRatio]);
  ctx.stroke();
  ctx.setLineDash([]);

  // ── Y axis labels (left = Pout, right = Iout) ─────────────────────────────
  ctx.font      = `${10 * devicePixelRatio}px 'Share Tech Mono', monospace`;
  ctx.textAlign = "right";
  ctx.fillStyle = CSS_POUT;
  for (let g = 0; g <= 4; g++) {
    const v = pTop * (1 - g / 4);
    const y = PAD_T + (g / 4) * plotH + 4 * devicePixelRatio;
    ctx.fillText(v.toFixed(0) + "W", PAD_L - 4 * devicePixelRatio, y);
  }
  ctx.textAlign = "left";
  ctx.fillStyle = CSS_IOUT;
  for (let g = 0; g <= 4; g++) {
    const v = iTop * (1 - g / 4);
    const y = PAD_T + (g / 4) * plotH + 4 * devicePixelRatio;
    ctx.fillText(v.toFixed(0) + "A", W - PAD_R + 4 * devicePixelRatio, y);
  }

  // ── X axis tick marks (time) ───────────────────────────────────────────────
  ctx.fillStyle = CSS_MUT;
  ctx.textAlign = "center";
  const windowSec = ((n - 1) * h.ms / 1000).toFixed(0);
  ctx.fillText("now", W - PAD_R, H - 4 * devicePixelRatio);
  ctx.fillText("-" + windowSec + "s", PAD_L,  H - 4 * devicePixelRatio);

  // update axis range labels
  document.getElementById("ax-pout-max").textContent = pTop + " W";
  document.getElementById("ax-iout-max").textContent = iTop + " A";
  document.getElementById("graph-age").textContent   = n + " samples";
}

// ── Data refresh ─────────────────────────────────────────────────────────────
function sv(id, t, cls) {
  const e = document.getElementById(id);
  if (!e) return;
  e.textContent = t;
  e.className = (e.className.split(" ")[0]) + (cls ? " " + cls : "");
}
function h(n, p = 2) { return "0x" + n.toString(16).padStart(p, "0").toUpperCase(); }
function fv(v, u, d = 2) { return v == null ? "--" : v.toFixed(d) + "\u00a0" + u; }
function sc(v) { return v == null ? "mut" : v === 0 ? "ok" : "bad"; }
function tc(t) { return t > 70 ? "bad" : t > 55 ? "" : "ok"; }

function refresh() {
  fetch("/data", { signal: AbortSignal.timeout(2000) })
    .then(r => r.json())
    .then(d => {
      const dot = document.getElementById("dot");
      if (!d.valid) { dot.className = "bad"; return; }
      dot.className = "ok";

      // MFR bar
      document.getElementById("m-model").textContent  = d.mfr_model    || "--";
      document.getElementById("m-serial").textContent = d.mfr_serial   || "--";
      document.getElementById("m-loc").textContent    = d.mfr_location || "--";
      document.getElementById("m-date").textContent   = d.mfr_date     || "--";
      document.getElementById("m-rev").textContent    = d.mfr_revision || "--";
      document.getElementById("m-id").textContent     = d.mfr_id       || "--";

      // Input
      sv("v-vin", fv(d.vin, "V", 1));
      sv("v-iin", fv(d.iin, "A", 2));
      sv("v-pin", fv(d.pin, "W", 1));

      // Output
      sv("v-vout",  fv(d.vout, "V", 3));
      sv("v-iout",  fv(d.iout, "A", 2));
      sv("v-pout",  fv(d.pout, "W", 1));
      const eff = d.pin > 1 ? (d.pout / d.pin * 100).toFixed(1) + "\u00a0%" : "--";
      sv("v-eff",   eff);
      sv("v-ipeak", fv(d.iout_max, "A", 2));
      sv("v-ppeak", fv(d.pout_max, "W", 1));
      // Vout min — only show if valid (> 0.1 V)
      sv("v-vmin",  d.vout_min > 0.1 ? fv(d.vout_min, "V", 3) : "--");

      // Thermal
      sv("v-t1",  fv(d.t1, "\u00b0C", 1), tc(d.t1));
      sv("v-t2",  fv(d.t2, "\u00b0C", 1), tc(d.t2));
      sv("v-t3",  fv(d.t3, "\u00b0C", 1), tc(d.t3));
      sv("v-fan", d.fan != null ? Math.round(d.fan) + "\u00a0RPM" : "--");

      // Status
      sv("st-byte",  h(d.status_byte),    sc(d.status_byte));
      sv("st-word",  h(d.status_word, 4), sc(d.status_word));
      sv("st-vout",  h(d.status_vout),    sc(d.status_vout));
      sv("st-iout",  h(d.status_iout),    sc(d.status_iout));
      sv("st-input", h(d.status_input),   sc(d.status_input));
      sv("st-temp",  h(d.status_temp),    sc(d.status_temp));
      sv("st-cml",   h(d.status_cml),     sc(d.status_cml));

      // Limits
      sv("l-vcmd", fv(d.vout_command,  "V", 3));
      sv("l-vmax", fv(d.vout_max,      "V", 3));
      sv("l-vovf", fv(d.vout_ov_fault, "V", 3));
      sv("l-vovw", fv(d.vout_ov_warn,  "V", 3));
      sv("l-vuvf", fv(d.vout_uv_fault, "V", 3));
      sv("l-iocf", fv(d.iout_oc_fault, "A", 2));
      sv("l-iocw", fv(d.iout_oc_warn,  "A", 2));

      // MFR specs
      const vinRange = (d.mfr_vin_min != null && d.mfr_vin_max != null)
        ? d.mfr_vin_min.toFixed(0) + "\u2013" + d.mfr_vin_max.toFixed(0) + "\u00a0V" : "--";
      sv("r-vin",     vinRange);
      sv("r-iinmax",  fv(d.mfr_iin_max, "A", 2));
      sv("r-pinmax",  fv(d.mfr_pin_max, "W", 1));
      const voutRange = (d.mfr_vout_min != null && d.mfr_vout_max != null)
        ? d.mfr_vout_min.toFixed(2) + "\u2013" + d.mfr_vout_max.toFixed(2) + "\u00a0V" : "--";
      sv("r-vout",    voutRange);
      sv("r-ioutmax", fv(d.mfr_iout_max, "A", 2));
      sv("r-poutmax", fv(d.mfr_pout_max, "W", 1));
      const tambRange = (d.mfr_tambient_min != null && d.mfr_tambient_max != null)
        ? d.mfr_tambient_min.toFixed(0) + "\u2013" + d.mfr_tambient_max.toFixed(0) + "\u00a0\u00b0C" : "--";
      sv("r-tamb", tambRange);

      // OLED selects
      if (!oledSynced && d.oled) {
        d.oled.forEach((f, i) => { const s = document.getElementById("oled-row-" + i); if (s) s.value = f; });
        oledSynced = true;
      }
    })
    .catch(() => document.getElementById("dot").className = "bad");
}

// ── History poll (every 5 s is enough — buffer grows at 1 Hz) ────────────────
function refreshGraph() {
  fetch("/history", { signal: AbortSignal.timeout(3000) })
    .then(r => r.json())
    .then(drawGraph)
    .catch(() => {});
}

function resetMax() { fetch("/reset_max", { method: "POST" }); }

function applyOled() {
  const p = new URLSearchParams();
  for (let i = 0; i < 6; i++) p.append("row" + i, document.getElementById("oled-row-" + i).value);
  fetch("/oled", { method: "POST", body: p }).then(r => r.json()).then(d => {
    if (d.ok) {
      const f = document.getElementById("fb");
      f.classList.add("show");
      setTimeout(() => f.classList.remove("show"), 2000);
      oledSynced = false;
    }
  });
}

refresh();
refreshGraph();
setInterval(refresh, 1000);
setInterval(refreshGraph, 1000);
</script>
</body>
</html>)rawhtml";

    server.send(200, "text/html", html);
}
