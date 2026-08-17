#include "wifi_config_portal.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#if !defined(ESP_PLATFORM)

WiFiConfigPortal & WiFiConfigPortal::instance() {
    static WiFiConfigPortal g;
    return g;
}

const char * WiFiConfigPortal::apSsid() const { return ap_ssid_; }

void WiFiConfigPortal::refreshApSsid() {
    std::snprintf(ap_ssid_, sizeof(ap_ssid_), "ESP32-0000");
}

bool WiFiConfigPortal::start(std::function<void()> successCb) {
    on_success_ = std::move(successCb);
    refreshApSsid();
    std::printf("[WiFiPortal] host stub: SoftAP/HTTP 不可用，仅展示配网说明 ssid=%s\n", ap_ssid_);
    ap_up_ = true;
    return true;
}

void WiFiConfigPortal::stop() {
    ap_up_ = false;
    on_success_ = {};
}

bool WiFiConfigPortal::isRunning() const { return ap_up_; }
bool WiFiConfigPortal::startAp() { return false; }
bool WiFiConfigPortal::startHttp() { return false; }
void WiFiConfigPortal::startDns() {}
void WiFiConfigPortal::stopDns() {}

#else

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char * TAG = "WiFiPortal";

static std::string url_decode(const std::string & str) {
    std::string out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            out += ' ';
        } else if (str[i] == '%' && i + 2 < str.size()) {
            char hex[3] = {str[i + 1], str[i + 2], '\0'};
            char * end = nullptr;
            long v = std::strtol(hex, &end, 16);
            if (end && *end == '\0' && v >= 0 && v <= 255) {
                out += static_cast<char>(v);
                i += 2;
            } else {
                out += str[i];
            }
        } else {
            out += str[i];
        }
    }
    return out;
}

static bool nvs_save_wifi(const std::string & ssid, const std::string & pass) {
    if (ssid.empty()) return false;
    nvs_handle_t h = 0;
    if (nvs_open("wifi_config", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t e1 = nvs_set_str(h, "wifi_ssid", ssid.c_str());
    esp_err_t e2 = nvs_set_str(h, "wifi_password", pass.c_str());
    esp_err_t e3 = nvs_commit(h);
    nvs_close(h);
    return e1 == ESP_OK && e2 == ESP_OK && e3 == ESP_OK;
}

static const char * HTML_PAGE = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi配网</title>
<style>
body{font-family:Arial,sans-serif;max-width:400px;margin:40px auto;padding:16px;background:#f5f5f5}
.box{background:#fff;padding:24px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,.08)}
h1{text-align:center;color:#333;font-size:22px}
label{display:block;margin:12px 0 6px;color:#555;font-weight:bold}
input{width:100%;padding:12px;border:1px solid #ddd;border-radius:6px;box-sizing:border-box;font-size:16px}
button{width:100%;padding:12px;margin-top:12px;border:none;border-radius:6px;font-size:16px;color:#fff;background:#4CAF50}
.scan{background:#2196F3}
.msg{margin-top:16px;padding:12px;border-radius:6px;display:none;text-align:center}
.ok{background:#d4edda;color:#155724}
.err{background:#f8d7da;color:#721c24}
.list{margin-top:12px;max-height:220px;overflow:auto;border:1px solid #ddd;border-radius:6px;display:none}
.item{padding:10px;border-bottom:1px solid #eee;cursor:pointer}
.item:active{background:#e8f5e9}
</style>
</head>
<body>
<div class="box">
<h1>WiFi配网</h1>
<form id="f">
<label>WiFi名称 (SSID)</label>
<input id="ssid" name="ssid" required>
<label>WiFi密码</label>
<input id="password" name="password" type="password">
<button type="button" class="scan" id="scanBtn">扫描WiFi网络</button>
<div class="list" id="list"></div>
<button type="submit">连接</button>
</form>
<div class="msg" id="msg"></div>
</div>
<script>
function show(ok,t){var m=document.getElementById('msg');m.className='msg '+(ok?'ok':'err');m.style.display='block';m.textContent=t}
document.getElementById('scanBtn').onclick=function(){
  var b=this,l=document.getElementById('list');
  b.disabled=true;b.textContent='扫描中...';l.style.display='block';l.innerHTML='<div class="item">扫描中…</div>';
  fetch('/scan').then(r=>r.json()).then(function(d){
    b.disabled=false;b.textContent='重新扫描';l.innerHTML='';
    (d.networks||[]).forEach(function(n){
      var div=document.createElement('div');div.className='item';
      div.textContent=(n.encrypted?'🔒 ':'')+n.ssid+'  ('+n.rssi+' dBm)';
      div.onclick=function(){document.getElementById('ssid').value=n.ssid};
      l.appendChild(div);
    });
    if(!d.networks||!d.networks.length) l.innerHTML='<div class="item">未找到网络</div>';
  }).catch(function(){b.disabled=false;b.textContent='重新扫描';show(false,'扫描失败')});
};
document.getElementById('f').onsubmit=function(e){
  e.preventDefault();
  var ssid=document.getElementById('ssid').value;
  var password=document.getElementById('password').value;
  fetch('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(password)})
  .then(r=>r.text()).then(function(t){
    if(t==='OK'){show(true,'配网成功！正在同步时间…');var c=3;var iv=setInterval(function(){
      c--; if(c>0) show(true,'配网成功！请查看设备 ('+c+')'); else {show(true,'可断开热点，设备将连家用WiFi');clearInterval(iv)}
    },1000)} else show(false,'失败: '+t);
  }).catch(function(err){show(false,'失败: '+err)});
};
window.onload=function(){document.getElementById('scanBtn').click()};
</script>
</body>
</html>
)HTML";

WiFiConfigPortal & WiFiConfigPortal::instance() {
    static WiFiConfigPortal g;
    return g;
}

const char * WiFiConfigPortal::apSsid() const { return ap_ssid_; }

void WiFiConfigPortal::refreshApSsid() {
    uint8_t mac[6] = {};
    /* SoftAP MAC 后 2 字节 → 4 位十六进制设备 id */
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) != ESP_OK) {
        (void)esp_read_mac(mac, ESP_MAC_WIFI_STA);
    }
    std::snprintf(ap_ssid_, sizeof(ap_ssid_), "ESP32-%02X%02X", mac[4], mac[5]);
}

bool WiFiConfigPortal::startAp() {
    refreshApSsid();

    esp_err_t ev = esp_event_loop_create_default();
    if (ev != ESP_OK && ev != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(ev));
        return false;
    }
    esp_netif_init();

    esp_netif_t * ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "create AP netif failed");
        return false;
    }

    esp_netif_ip_info_t ip_info{};
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dns_info_t dns{};
    dns.ip.u_addr.ip4.addr = ip_info.ip.addr;
    dns.ip.type = IPADDR_TYPE_V4;
    esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns);
    esp_netif_dhcps_start(ap_netif);

    wifi_mode_t mode = WIFI_MODE_NULL;
    bool wifi_inited = (esp_wifi_get_mode(&mode) == ESP_OK);
    if (!wifi_inited) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_err_t ie = esp_wifi_init(&cfg);
        if (ie != ESP_OK && ie != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "wifi init: %s", esp_err_to_name(ie));
            return false;
        }
        wifi_inited = true;
        mode = WIFI_MODE_NULL;
    }

    wifi_mode_t target = WIFI_MODE_AP;
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
        target = WIFI_MODE_APSTA;
    }
    esp_err_t me = esp_wifi_set_mode(target);
    if (me != ESP_OK) {
        ESP_LOGE(TAG, "set_mode: %s", esp_err_to_name(me));
        return false;
    }

    wifi_config_t wcfg{};
    std::strncpy((char *)wcfg.ap.ssid, ap_ssid_, sizeof(wcfg.ap.ssid) - 1);
    wcfg.ap.ssid_len = (uint8_t)std::strlen(ap_ssid_);
    wcfg.ap.channel = 1;
    wcfg.ap.authmode = WIFI_AUTH_OPEN;
    wcfg.ap.max_connection = 4;
    esp_err_t ce = esp_wifi_set_config(WIFI_IF_AP, &wcfg);
    if (ce != ESP_OK) {
        ESP_LOGE(TAG, "set_config AP: %s", esp_err_to_name(ce));
        return false;
    }

    esp_err_t st = esp_wifi_start();
    if (st != ESP_OK && st != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "wifi start: %s", esp_err_to_name(st));
        return false;
    }

    ap_up_ = true;
    ESP_LOGI(TAG, "SoftAP up ssid=%s ip=%s", ap_ssid_, kApIp);
    return true;
}

static esp_err_t root_handler(httpd_req_t * req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static std::function<void()> g_success_cb;

static esp_err_t config_handler_impl(httpd_req_t * req) {
    char content[256]{};
    int len = req->content_len;
    if (len <= 0 || len >= (int)sizeof(content)) len = (int)sizeof(content) - 1;
    int n = httpd_req_recv(req, content, len);
    if (n <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv");
        return ESP_FAIL;
    }
    content[n] = 0;

    std::string ssid, pass;
    char * save = nullptr;
    for (char * tok = strtok_r(content, "&", &save); tok; tok = strtok_r(nullptr, "&", &save)) {
        if (std::strncmp(tok, "ssid=", 5) == 0) ssid = url_decode(tok + 5);
        else if (std::strncmp(tok, "password=", 9) == 0) pass = url_decode(tok + 9);
    }

    ESP_LOGI(TAG, "config ssid=%s pass_len=%d", ssid.c_str(), (int)pass.size());
    if (!nvs_save_wifi(ssid, pass)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs");
        return ESP_FAIL;
    }

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    /* 不重启：回调里关门户 → 强制 NTP → 回主页刷新时钟 */
    auto * heap_cb = new std::function<void()>(g_success_cb);
    xTaskCreate(
        [](void * p) {
            auto * fn = static_cast<std::function<void()> *>(p);
            vTaskDelay(pdMS_TO_TICKS(200));
            if (fn && *fn) (*fn)();
            delete fn;
            vTaskDelete(nullptr);
        },
        "wifi_prov_ok",
        4096,
        heap_cb,
        5,
        nullptr);

    return ESP_OK;
}

static esp_err_t scan_handler(httpd_req_t * req) {
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }

    wifi_scan_config_t scfg{};
    scfg.show_hidden = false;
    scfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    esp_err_t err = esp_wifi_scan_start(&scfg, true);
    std::string json = "{\"networks\":[";
    if (err == ESP_OK) {
        uint16_t ap_num = 0;
        esp_wifi_scan_get_ap_num(&ap_num);
        if (ap_num > 30) ap_num = 30;
        wifi_ap_record_t * rec =
            (wifi_ap_record_t *)std::calloc(ap_num ? ap_num : 1, sizeof(wifi_ap_record_t));
        if (rec) {
            uint16_t n = ap_num;
            if (esp_wifi_scan_get_ap_records(&n, rec) == ESP_OK) {
                for (uint16_t i = 0; i < n; ++i) {
                    if (i) json += ',';
                    char ssid[33]{};
                    std::memcpy(ssid, rec[i].ssid, 32);
                    /* naive JSON escape for quotes */
                    std::string s = ssid;
                    for (size_t p = 0; (p = s.find('"', p)) != std::string::npos; p += 2) s.replace(p, 1, "\\\"");
                    json += "{\"ssid\":\"" + s + "\",\"rssi\":" + std::to_string(rec[i].rssi) +
                            ",\"encrypted\":" +
                            (rec[i].authmode != WIFI_AUTH_OPEN ? "true" : "false") + "}";
                }
            }
            std::free(rec);
        }
    }
    json += "]}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());
    return ESP_OK;
}

bool WiFiConfigPortal::startHttp() {
    if (http_) return true;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 8;
    cfg.lru_purge_enable = true;

    httpd_handle_t server = nullptr;
    esp_err_t ret = ESP_FAIL;
    for (int i = 0; i < 3; ++i) {
        ret = httpd_start(&server, &cfg);
        if (ret == ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(ret));
        return false;
    }

    httpd_uri_t root{.uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = nullptr};
    httpd_uri_t conf{.uri = "/config", .method = HTTP_POST, .handler = config_handler_impl, .user_ctx = nullptr};
    httpd_uri_t scan{.uri = "/scan", .method = HTTP_GET, .handler = scan_handler, .user_ctx = nullptr};
    httpd_uri_t all{.uri = "/*", .method = HTTP_GET, .handler = root_handler, .user_ctx = nullptr};
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &conf);
    httpd_register_uri_handler(server, &scan);
    httpd_register_uri_handler(server, &all);
    http_ = server;
    ESP_LOGI(TAG, "HTTP portal on :80 → http://%s/", kApIp);
    return true;
}

static void dns_task(void * arg) {
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        vTaskDelete(nullptr);
        return;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        vTaskDelete(nullptr);
        return;
    }
    uint8_t buf[512];
    while (true) {
        sockaddr_in src{};
        socklen_t slen = sizeof(src);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr *)&src, &slen);
        if (n < 12) continue;
        /* minimal DNS response → 192.168.4.1 */
        uint8_t resp[512];
        if (n > 500) continue;
        std::memcpy(resp, buf, n);
        resp[2] = 0x81;
        resp[3] = 0x80;
        resp[7] = 1; /* ANCOUNT = 1 */
        int len = n;
        resp[len++] = 0xc0;
        resp[len++] = 0x0c;
        resp[len++] = 0x00;
        resp[len++] = 0x01;
        resp[len++] = 0x00;
        resp[len++] = 0x01;
        resp[len++] = 0x00;
        resp[len++] = 0x00;
        resp[len++] = 0x00;
        resp[len++] = 0x3c;
        resp[len++] = 0x00;
        resp[len++] = 0x04;
        resp[len++] = 192;
        resp[len++] = 168;
        resp[len++] = 4;
        resp[len++] = 1;
        sendto(sock, resp, len, 0, (sockaddr *)&src, slen);
    }
}

void WiFiConfigPortal::startDns() {
    if (dns_task_) return;
    TaskHandle_t th = nullptr;
    xTaskCreate(dns_task, "wifi_dns", 3072, nullptr, 3, &th);
    dns_task_ = th;
}

void WiFiConfigPortal::stopDns() {
    if (dns_task_) {
        vTaskDelete(static_cast<TaskHandle_t>(dns_task_));
        dns_task_ = nullptr;
    }
}

bool WiFiConfigPortal::start(std::function<void()> successCb) {
    on_success_ = std::move(successCb);
    g_success_cb = on_success_;
    if (!startAp()) return false;
    if (!startHttp()) {
        ESP_LOGW(TAG, "HTTP failed; AP still up");
        return false;
    }
    startDns();
    return true;
}

void WiFiConfigPortal::stop() {
    stopDns();
    if (http_) {
        httpd_stop(static_cast<httpd_handle_t>(http_));
        http_ = nullptr;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    /* 关掉 SoftAP，切回 STA，便于随后 NTP 连家用 WiFi */
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) == ESP_OK) {
        esp_wifi_set_mode(WIFI_MODE_STA);
        ESP_LOGI(TAG, "portal stop → WIFI_MODE_STA");
    }
    ap_up_ = false;
    on_success_ = {};
    g_success_cb = {};
}

bool WiFiConfigPortal::isRunning() const { return http_ != nullptr || ap_up_; }

#endif /* ESP_PLATFORM */
