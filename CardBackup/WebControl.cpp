#ifndef ICCARD_HOST_TEST
#include "WebControl.h"
#include "Config.h"
#include "WebPage.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_system.h>

namespace {
WebServer server(80);
DNSServer dns;
bool started = false;
String token;

void noCache() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("X-Frame-Options", "DENY");
}

bool authorized() {
  // Only JavaScript served by this device can set this header and read the token.
  // No CORS headers are sent; a normal cross-origin HTML form cannot submit actions.
  return token.length() > 0 && server.header("X-ICCard-Token") == token;
}

void homePage() {
  noCache();
  server.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
}

void state() {
  noCache();
  String json = controlStateJson();
  json.remove(json.length() - 1); // Append network/session fields to the object.
  json += ",\"token\":\"" + token + "\",\"ip\":\"" + WiFi.softAPIP().toString() + "\"}";
  server.send(200, "application/json; charset=utf-8", json);
}

void action() {
  noCache();
  if (!authorized()) {
    server.send(403, "application/json; charset=utf-8", "{\"error\":\"页面已失效，请刷新后重试。\"}");
    return;
  }
  String error;
  const int status = enqueueWebAction(server.arg("action"), server.arg("uid"),
                                     server.arg("confirmation"), error);
  // Error strings are fixed literals from the dispatcher, never request input.
  server.send(status, "application/json; charset=utf-8", status == 202
      ? "{\"accepted\":true}" : "{\"error\":\"" + error + "\"}");
}

void download() {
  noCache();
  if (!authorized()) {
    server.send(403, "application/json; charset=utf-8", "{\"error\":\"请从操作页面下载。\"}");
    return;
  }
  if (controlBusy()) {
    server.send(409, "application/json; charset=utf-8", "{\"error\":\"请等待当前操作结束。\"}");
    return;
  }
  if (!controlHasBackup()) {
    server.send(404, "application/json; charset=utf-8", "{\"error\":\"还没有可下载的备份。\"}");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=iccard-backup.json");
  server.send(200, "application/json; charset=utf-8", backupJson());
}
} // namespace

void beginWebControl() {
  const IPAddress address(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0)) ||
      !WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, false, 4)) {
    Serial.println("ERROR: Wi-Fi hotspot could not start. Serial commands remain available.");
    return;
  }
  char randomToken[33];
  snprintf(randomToken, sizeof(randomToken), "%08lx%08lx%08lx%08lx",
           (unsigned long)esp_random(), (unsigned long)esp_random(),
           (unsigned long)esp_random(), (unsigned long)esp_random());
  token = randomToken;
  const char* headers[] = {"X-ICCard-Token"};
  server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, homePage);
  server.on("/api/state", HTTP_GET, state);
  server.on("/api/action", HTTP_POST, action);
  server.on("/api/backup", HTTP_GET, download);
  server.on("/favicon.ico", HTTP_GET, []() { server.send(204); });
  server.onNotFound([]() {
    noCache();
    if (server.uri().startsWith("/api/") || server.method() != HTTP_GET) {
      server.send(404, "application/json", "{\"error\":\"Not found\"}");
      return;
    }
    // Captive-portal probes and arbitrary HTTP hostnames land on the canonical IP.
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/plain", "Open http://192.168.4.1/");
  });
  dns.start(53, "*", address);
  server.begin();
  started = true;
  Serial.printf("Wi-Fi hotspot: %s | password: %s\nOpen http://192.168.4.1/\n",
                WIFI_AP_SSID, WIFI_AP_PASSWORD);
}

void serviceWeb() {
  if (!started) return;
  dns.processNextRequest();
  server.handleClient();
}
#endif
