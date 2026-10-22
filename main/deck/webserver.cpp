#include "webserver.h"

#include <cstring>

#include "cJSON.h"
#include "deck_config.h"
#include "esp_coexist.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "webserver";
constexpr char kApSsid[] = "StickyDeck-Config";

httpd_handle_t s_server = nullptr;
bool s_running = false;
bool s_wifi_setup = false;
char s_ap_password[64] = "stickydeck";

const char kIndexHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>StickyDeck config</title>
<style>
body{font-family:system-ui,sans-serif;max-width:780px;margin:16px auto;padding:0 12px;color:#111}
h1{font-size:20px}
.card{border:1px solid #ccc;border-radius:6px;padding:10px;margin:10px 0;background:#fafafa}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:6px 0}
label{font-size:13px;color:#444;display:inline-flex;align-items:center;gap:3px}
input[type=text],input[type=number],select{padding:6px;border:1px solid #999;border-radius:4px;font-size:14px}
input[type=text]{min-width:110px}
button{padding:8px 14px;border:1px solid #888;border-radius:4px;background:#f4f4f4;cursor:pointer;font-size:14px}
button.danger{color:#a00}
button.primary{background:#e8f0fe}
h2{font-size:16px;margin:4px 0}
#status{font-weight:bold;margin-left:8px}
.hint{color:#555;font-size:12px;margin-top:10px;line-height:1.5}
</style></head><body>
<h1>StickyDeck config</h1>
<div id="app"></div>
<br>
<button class="primary" onclick="save()">Save</button>
<button onclick="reboot()">Save &amp; reboot</button>
<span id="status"></span>
<div class="hint">Key names: single chars, ENTER, SPACE, ESC, TAB, F1-F12, UP, DOWN, LEFT, RIGHT, HOME, END, PAGEUP, PAGEDOWN.</div>
<script>
const ICONS=["","annotate","arrow","circle","whiteboard","text","zoom"];
const MEDIA=["MUTE","VOLUME_UP","VOLUME_DOWN","PLAY","PAUSE","PLAY_PAUSE","NEXT","PREV","STOP"];
const ACTIONS=["keys","text","media","page","wifi"];
let cfg=null;

function esc(s){return String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");}
function opts(list,sel){return list.map(function(o){return '<option value="'+esc(o)+'"'+(o===sel?' selected':'')+'>'+esc(o||'(none)')+'</option>';}).join('');}

function normalize(c){
  c.name=c.name||"StickyDeck";
  c.wifi=(c.wifi!==false);
  c.beep=!!c.beep;
  c.sleep=parseInt(c.sleep)||0;
  c.ap_password=c.ap_password||"stickydeck";
  c.pages=c.pages||[];
  c.pages.forEach(function(p){
    p.cols=p.cols||4; p.rows=p.rows||2; p.buttons=p.buttons||[]; p.lines=p.lines||[];
    p.buttons.forEach(function(b){
      b.icon=b.icon||""; b.action=b.action||{type:"keys"};
      if(b.action.type==="keys"){b.action.mods=b.action.mods||[];b.action.keys=b.action.keys||[];}
    });
  });
  return c;
}

function pageIds(sel){return cfg.pages.map(function(p){return '<option value="'+esc(p.id)+'"'+(p.id===sel?' selected':'')+'>'+esc(p.id)+'</option>';}).join('');}

function actionFields(p,b){
  var a=cfg.pages[p].buttons[b].action, h="";
  if(a.type==="keys"){
    var mods=["CTRL","SHIFT","ALT","GUI"].map(function(m){
      return '<label><input type="checkbox" data-m="'+m+'"'+(a.mods.indexOf(m)>=0?' checked':'')+' onchange="toggleMod2('+p+','+b+',this)">'+m+'</label>';
    }).join(' ');
    h+='<div class="row">Mods: '+mods+' &nbsp; Keys: <input type="text" value="'+esc((a.keys||[]).join(','))+'" oninput="setKeys('+p+','+b+',this.value)"></div>';
  } else if(a.type==="text"){
    h+='<div class="row">Text: <input type="text" value="'+esc(a.text||'')+'" oninput="cfg.pages['+p+'].buttons['+b+'].action.text=this.value" style="flex:1"></div>';
  } else if(a.type==="media"){
    h+='<div class="row">Key: <select onchange="cfg.pages['+p+'].buttons['+b+'].action.key=this.value">'+opts(MEDIA,a.key||"MUTE")+'</select></div>';
  } else if(a.type==="page"){
    h+='<div class="row">Target page: <select onchange="cfg.pages['+p+'].buttons['+b+'].action.target=this.value">'+pageIds(a.target)+'</select></div>';
  } else if(a.type==="wifi"){
    h+='<div class="row">Toggles the Wi-Fi config AP on/off.</div>';
  }
  return h;
}

function toggleMod2(p,b,el){var a=cfg.pages[p].buttons[b].action;a.mods=a.mods||[];var m=el.getAttribute('data-m');if(el.checked){if(a.mods.indexOf(m)<0)a.mods.push(m);}else{a.mods=a.mods.filter(function(x){return x!==m;});}}
function setKeys(p,b,v){cfg.pages[p].buttons[b].action.keys=v.split(',').map(function(s){return s.trim();}).filter(Boolean);}
function setLines(p,v){cfg.pages[p].lines=v.split('\n').map(function(s){return s.replace(/\r/g,'');});}
function changeActionType(p,b,t){cfg.pages[p].buttons[b].action={type:t,mods:[],keys:[]};render();}
function addPage(){cfg.pages.push({id:"page"+cfg.pages.length,title:"Page "+cfg.pages.length,cols:4,rows:2,buttons:[]});render();}
function removePage(i){cfg.pages.splice(i,1);render();}
function addButton(p){cfg.pages[p].buttons.push({label:"Btn",icon:"",action:{type:"keys",mods:[],keys:[]}});render();}
function removeButton(p,b){cfg.pages[p].buttons.splice(b,1);render();}

function render(){
  var app=document.getElementById('app'), h="";
  h+='<div class="card"><h2>Settings</h2>';
  h+='<div class="row"><label>Name</label><input type="text" value="'+esc(cfg.name)+'" oninput="cfg.name=this.value"></div>';
  h+='<div class="row">'
    +'<label><input type="checkbox"'+(cfg.wifi?' checked':'')+' onchange="cfg.wifi=this.checked"> Wi-Fi AP at boot</label>'
    +'<label><input type="checkbox"'+(cfg.beep?' checked':'')+' onchange="cfg.beep=this.checked"> Beep</label>'
    +'<label>Sleep (min) <input type="number" min="0" value="'+cfg.sleep+'" oninput="cfg.sleep=parseInt(this.value)||0" style="width:60px"></label>'
    +'</div>';
  h+='<div class="row"><label>AP password</label><input type="text" value="'+esc(cfg.ap_password)+'" oninput="cfg.ap_password=this.value"></div>';
  h+='</div>';

  cfg.pages.forEach(function(pg,p){
    h+='<div class="card"><h2>Page '+(p+1)+'</h2><div class="row">';
    h+='<label>Title</label><input type="text" value="'+esc(pg.title)+'" oninput="cfg.pages['+p+'].title=this.value">';
    h+='<label>ID</label><input type="text" value="'+esc(pg.id)+'" oninput="cfg.pages['+p+'].id=this.value">';
    h+='<label>Cols</label><input type="number" min="1" max="8" value="'+pg.cols+'" oninput="cfg.pages['+p+'].cols=parseInt(this.value)||1" style="width:52px">';
    h+='<label>Rows</label><input type="number" min="1" max="8" value="'+pg.rows+'" oninput="cfg.pages['+p+'].rows=parseInt(this.value)||1" style="width:52px">';
    h+='<button class="danger" onclick="removePage('+p+')">✕</button></div>';
    h+='<div class="row"><label>Info lines (one per row)</label><textarea rows="3" style="flex:1" oninput="setLines('+p+',this.value)">'+esc((pg.lines||[]).join('\n'))+'</textarea></div>';
    pg.buttons.forEach(function(bt,bidx){
      h+='<div class="card"><div class="row">';
      h+='<label>Label</label><input type="text" value="'+esc(bt.label)+'" oninput="cfg.pages['+p+'].buttons['+bidx+'].label=this.value">';
      h+='<label>Icon</label><select onchange="cfg.pages['+p+'].buttons['+bidx+'].icon=this.value">'+opts(ICONS,bt.icon||'')+'</select>';
      h+='<label>Action</label><select onchange="changeActionType('+p+','+bidx+',this.value)">'+opts(ACTIONS,bt.action.type)+'</select>';
      h+='<button class="danger" onclick="removeButton('+p+','+bidx+')">✕</button></div>';
      h+=actionFields(p,bidx);
      h+='</div>';
    });
    h+='<button onclick="addButton('+p+')">+ Add button</button></div>';
  });
  h+='<br><button class="primary" onclick="addPage()">+ Add page</button>';
  app.innerHTML=h;
}

function setstatus(s,c){var e=document.getElementById('status');e.textContent=s;e.style.color=c||'#111';}
async function save(){var r=await fetch('/api/save',{method:'POST',body:JSON.stringify(cfg)});setstatus(r.ok?'Saved ✓':'Save failed ('+r.status+')',r.ok?'#080':'#c00');}
async function reboot(){var r=await fetch('/api/save',{method:'POST',body:JSON.stringify(cfg)});if(!r.ok){setstatus('Save failed','#c00');return;}await fetch('/api/reboot',{method:'POST'});setstatus('Rebooting…','#c00');}
async function load(){try{var r=await fetch('/api/config');cfg=normalize(JSON.parse(await r.text()));}catch(e){cfg=normalize({});}render();}
load();
</script></body></html>)HTML";

esp_err_t handle_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_get_config(httpd_req_t *req)
{
    static char buffer[8192] = {};
    const esp_err_t err = deck_config_json(buffer, sizeof(buffer));
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "config read failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_save(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 8192) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad length");
        return ESP_FAIL;
    }

    char *body = static_cast<char *>(malloc(req->content_len + 1));
    if (body == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
        return ESP_FAIL;
    }

    const int received = httpd_req_recv(req, body, req->content_len);
    body[received > 0 ? received : 0] = '\0';

    esp_err_t result = ESP_FAIL;
    if (received > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root == nullptr) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        } else {
            cJSON_Delete(root);
            result = deck_config_save_nvs(body);
            if (result != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                    "save failed");
            } else {
                ESP_LOGI(kTag, "config saved (%d bytes)", received);
                httpd_resp_sendstr(req, "OK");
            }
        }
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
    }

    free(body);
    return result == ESP_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t handle_reboot(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "OK");
    ESP_LOGI(kTag, "rebooting to apply config");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id,
                        void *event_data)
{
    if (base != WIFI_EVENT) {
        return;
    }
    switch (id) {
    case WIFI_EVENT_AP_START: {
        uint8_t channel = 0;
        esp_wifi_get_channel(&channel, nullptr);
        ESP_LOGI(kTag, "WIFI_EVENT_AP_START on channel %u", channel);
        break;
    }
    case WIFI_EVENT_AP_STOP:
        ESP_LOGI(kTag, "WIFI_EVENT_AP_STOP");
        break;
    case WIFI_EVENT_AP_STACONNECTED: {
        const auto *ev = static_cast<wifi_event_ap_staconnected_t *>(event_data);
        ESP_LOGI(kTag, "station connected " MACSTR, MAC2STR(ev->mac));
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
        const auto *ev = static_cast<wifi_event_ap_stadisconnected_t *>(event_data);
        ESP_LOGI(kTag, "station disconnected " MACSTR, MAC2STR(ev->mac));
        break;
    }
    default:
        break;
    }
}

esp_err_t wifi_ap_setup()
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    esp_netif_create_default_wifi_ap();

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &wifi_event_handler, nullptr);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        return err;
    }
    /* Max TX power (20 dBm) so the tiny antenna reaches further. */
    esp_wifi_set_max_tx_power(80);
    /* Prefer Wi-Fi in BLE/Wi-Fi coexistence so the AP beacon gets airtime
     * even while BLE is advertising. */
    esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t ap = {};
    std::strncpy(reinterpret_cast<char *>(ap.ap.ssid), kApSsid,
                 sizeof(ap.ap.ssid) - 1);
    ap.ap.ssid_len = std::strlen(kApSsid);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    std::strncpy(reinterpret_cast<char *>(ap.ap.password), s_ap_password,
                 sizeof(ap.ap.password) - 1);
    err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t start_httpd()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        return err;
    }

    httpd_uri_t uri = {};
    uri.uri = "/";
    uri.method = HTTP_GET;
    uri.handler = handle_index;
    httpd_register_uri_handler(s_server, &uri);

    uri.uri = "/api/config";
    uri.method = HTTP_GET;
    uri.handler = handle_get_config;
    httpd_register_uri_handler(s_server, &uri);

    uri.uri = "/api/save";
    uri.method = HTTP_POST;
    uri.handler = handle_save;
    httpd_register_uri_handler(s_server, &uri);

    uri.uri = "/api/reboot";
    uri.method = HTTP_POST;
    uri.handler = handle_reboot;
    httpd_register_uri_handler(s_server, &uri);

    ESP_LOGI(kTag, "HTTP server started at http://192.168.4.1/");
    return ESP_OK;
}

}  // namespace

void deck_webserver_set_ap_password(const char *password)
{
    if (password == nullptr || password[0] == '\0') {
        return;
    }
    std::strncpy(s_ap_password, password, sizeof(s_ap_password) - 1);
    s_ap_password[sizeof(s_ap_password) - 1] = '\0';
}

esp_err_t deck_webserver_start()
{
    if (s_running) {
        return ESP_OK;
    }

    esp_err_t err = ESP_OK;
    if (!s_wifi_setup) {
        err = wifi_ap_setup();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "wifi_ap_setup failed: %s", esp_err_to_name(err));
            return err;
        }
        s_wifi_setup = true;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return err;
    }

    err = start_httpd();
    if (err != ESP_OK) {
        esp_wifi_stop();
        return err;
    }
    s_running = true;
    ESP_LOGI(kTag, "Wi-Fi AP '%s' started", kApSsid);
    return ESP_OK;
}

esp_err_t deck_webserver_stop()
{
    if (!s_running) {
        return ESP_OK;
    }

    if (s_server != nullptr) {
        httpd_stop(s_server);
        s_server = nullptr;
    }
    esp_wifi_stop();
    s_running = false;
    ESP_LOGI(kTag, "Wi-Fi AP + HTTP server stopped");
    return ESP_OK;
}

esp_err_t deck_webserver_toggle()
{
    return s_running ? deck_webserver_stop() : deck_webserver_start();
}

bool deck_webserver_is_running()
{
    return s_running;
}
