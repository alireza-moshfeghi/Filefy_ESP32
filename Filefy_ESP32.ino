#include <WiFi.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

DNSServer dnsServer;
AsyncWebServer server(80);

struct Session {
  String token;
  String account;
  IPAddress ip;
  uint32_t expire;
};

Session session[5];

String generateToken() {
  const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  String token = "";
  srand(esp_random());
  for (int i = 0; i < 12; i++) {
    token += chars[rand() % strlen(chars)];
  }
  return token;
}

void saveToken(const String& token, String account, AsyncWebServerRequest *request) {
  IPAddress clientIP = request->client()->remoteIP();
  uint32_t now = millis();
  
  for (int i = 0; i < 5; i++) {
    if (session[i].token == "" || session[i].expire < now) {
      session[i].token = token;
      session[i].account = account;
      session[i].ip = clientIP;
      session[i].expire = now + (24UL * 60UL * 60UL * 1000UL);
      return;
    }
  }
  
  for (int i = 0; i < 5; i++) {
    if (session[i].ip == clientIP) {
      session[i].token = token;
      session[i].account = account;
      session[i].expire = now + (24UL * 60UL * 60UL * 1000UL);
      return;
    }
  }
  
  session[0].token = token;
  session[0].account = account;
  session[0].ip = clientIP;
  session[0].expire = now + (24UL * 60UL * 60UL * 1000UL);
}

String isTokenValid(const String& token, AsyncWebServerRequest *request) {
  IPAddress clientIP = request->client()->remoteIP();
  uint32_t now = millis();
  for (int i = 0; i < 5; i++) {
    if (session[i].token == token &&
        session[i].ip == clientIP &&
        session[i].expire > now) {
      return session[i].account;
    }
  }
  return "invalid";
}

void setup() {
  LittleFS.begin(true);

  File ap = LittleFS.open("/configs/ap.txt", "r");
  File maxcon = LittleFS.open("/configs/maxcon.txt", "r");
  WiFi.softAP(ap.readString(), "", 1, false, maxcon.readString().toInt());
  ap.close();
  maxcon.close();

  WiFi.hostname("Filefy-ESP");
  WiFi.setAutoReconnect(true);

  File dhcpFile = LittleFS.open("/configs/dhcp.txt", "r");
  String dhcpValue = dhcpFile.readString();
  dhcpValue.trim();
  dhcpFile.close();

  File ssidFile = LittleFS.open("/configs/ssid.txt", "r");
  String ssid = ssidFile.readString();
  ssid.trim();
  ssidFile.close();

  File keyFile = LittleFS.open("/configs/key.txt", "r");
  String key = keyFile.readString();
  key.trim();
  keyFile.close();

  if (dhcpValue == "true" || dhcpValue == "1") {
    WiFi.begin(ssid, key);
  } else {
    File ipFile = LittleFS.open("/configs/ip.txt", "r");
    IPAddress ip;
    ip.fromString(ipFile.readString());
    ipFile.close();

    File gatewayFile = LittleFS.open("/configs/gateway.txt", "r");
    IPAddress gateway;
    gateway.fromString(gatewayFile.readString());
    gatewayFile.close();

    File subnetFile = LittleFS.open("/configs/subnet.txt", "r");
    IPAddress subnet;
    subnet.fromString(subnetFile.readString());
    subnetFile.close();

    File dnsFile = LittleFS.open("/configs/dns.txt", "r");
    IPAddress dns;
    dns.fromString(dnsFile.readString());
    dnsFile.close();

    WiFi.config(ip, gateway, subnet, dns);
    WiFi.begin(ssid, key);
  }

  auto readConfig = [](String path) -> String {
    File file = LittleFS.open(path, "r");
    if (!file) return "";
    String content = file.readString();
    content.trim();
    file.close();
    return content;
  };

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/language.htm", "text/html");
  });

  server.on("/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    String result = "{\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/fa/upload/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/upload.htm", "text/html");
  });

  server.on("/fa/upload/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/fa/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File allow = LittleFS.open("/fa/allow.txt", "r");
    result += "\"allow\":\"" + allow.readString() + "\",";
    allow.close();
    File limit = LittleFS.open("/fa/limit.txt", "r");
    result += "\"limit\":\"" + limit.readString() + "\",";
    limit.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
    File host = LittleFS.open("/configs/host.txt", "r");
    result += "\"host\":\"" + host.readString() + "\"}";
    host.close();
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/fa/download/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/download.htm", "text/html");
  });

  server.on("/fa/download/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("code", true)) {
      if (request->getParam("code", true)->value() == "") {
        request->send(403, "application/json", "{\"status\":\"code\"}");
      }
      else if (LittleFS.exists("/files/" + request->getParam("code", true)->value() + ".txt")) {
        File url = LittleFS.open("/files/" + request->getParam("code", true)->value() + ".txt", "r");
        File host = LittleFS.open("/configs/host.txt", "r");
        request->send(200, "application/json", "{\"download_url\":\"http://" + host.readString() + "/downloads/" + url.readString() + "\"}");
        url.close();
        host.close();
      }
      else {
        request->send(403, "application/json", "{\"status\":\"no_exists\"}");
      }
    }
    else {
      File name = LittleFS.open("/fa/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
      File host = LittleFS.open("/configs/host.txt", "r");
      result += "\"host\":\"" + host.readString() + "\"}";
      host.close();
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/fa/files/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/files.htm", "text/html");
  });

  server.on("/fa/files/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin" || isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "delete" && request->hasParam("code", true)) {
            String fileName = "/files/" + request->getParam("code", true)->value() + ".txt";
            if (fileName == "/files/null.txt") {
              request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Cannot delete null.txt\"}");
              return;
            }
            if (LittleFS.exists(fileName)) {
              LittleFS.remove(fileName);
              request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
              request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"File not found\"}");
            }
          }
          else if (action == "add" && request->hasParam("code", true) && request->hasParam("name", true)) {
            String code = request->getParam("code", true)->value();
            String name = request->getParam("name", true)->value();
            if (name == "") {
              request->send(200, "application/json", "{\"status\":\"name\"}");
            }
            else if (code == "") {
              request->send(200, "application/json", "{\"status\":\"code\"}");
            }
            else {
              String fileName = "/files/" + code + ".txt";
              if (LittleFS.exists(fileName)) {
                request->send(200, "application/json", "{\"status\":\"exists\"}");
              }
              else {
                File file = LittleFS.open(fileName, "w");
                file.print(name);
                file.close();
                request->send(200, "application/json", "{\"status\":\"ok\"}");
              }
            }
          }
        }
        else {
          int index = 1;
          File name = LittleFS.open("/fa/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          result += "\"files\":[";
          File dir = LittleFS.open("/files");
          while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;
            if (!entry.isDirectory()) {
              String text = entry.readString();
              String fileName = entry.name();
              int lastSlash = fileName.lastIndexOf('/');
              if (lastSlash != -1) {
                fileName = fileName.substring(lastSlash + 1);
              }
              if (fileName.equals("null.txt")) {
                entry.close();
                continue;
              }
              int dotIndex = fileName.lastIndexOf('.');
              if (dotIndex != -1) {
                fileName = fileName.substring(0, dotIndex);
              }
              result += "{\"id\":\"" + String(index++) + "\",";
              result += "\"code\":\"" + fileName + "\",";
              result += "\"name\":\"" + text + "\"},";
            }
            entry.close();
          }
          dir.close();
          if (result.endsWith(",")) {
            result.remove(result.length() - 1);
          }
          result += "],";
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
          File host = LittleFS.open("/configs/host.txt", "r");
          result += "\"host\":\"" + host.readString() + "\"}";
          host.close();
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/fa/login/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/login.htm", "text/html");
  });

  server.on("/fa/login/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        request->send(200, "application/json", "{\"status\":\"admin\"}");
      }
      else if (isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        request->send(200, "application/json", "{\"status\":\"user\"}");
      }
      else {
        request->send(200, "application/json", "{\"status\":\"invalid\"}");
      }
    }
    else if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String username = request->getParam("username", true)->value();
      String password = request->getParam("password", true)->value();
      File user = LittleFS.open("/configs/username.txt", "r");
      File pass = LittleFS.open("/configs/password.txt", "r");
      if (username == "" || password == "") {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
      }
      else if (username == user.readString() && password == pass.readString()) {
        String token = generateToken();
        saveToken(token, "admin", request);
        request->send(200, "application/json", "{\"status\":\"ok\", \"token\":\"" + token + "\"}");
      }
      else {
        request->send(401, "application/json", "{\"status\":\"error\",\"message\":\"Invalid credentials\"}");
      }
      user.close();
      pass.close();
    }
    else {
      File name = LittleFS.open("/fa/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/fa/home/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/home.htm", "text/html");
  });

  server.on("/fa/home/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/fa/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/fa/logout/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/logout.htm", "text/html");
  });

  server.on("/fa/logout/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin" || isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        for (int i = 0; i < 5; i++) {
          if (request->getParam("token", true)->value() == session[i].token) {
            session[i].token = "";
            session[i].ip = IPAddress();
            session[i].expire = 0;
          }
        }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      }
      else {
        request->send(200, "application/json", "{\"status\":\"invalid\"}");
      }
    }
    else {
      File name = LittleFS.open("/fa/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/fa/modules/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/modules.htm", "text/html");
  });

  server.on("/fa/modules/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            if (request->getParam("ap", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"ap\"}");
            }
            else if (request->getParam("maxcon", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"maxcon\"}");
            }
            else if (request->getParam("host", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"host\"}");
            }
            else if (request->getParam("networkType", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"networkType\"}");
            }
            else if (request->getParam("ip", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"ip\"}");
            }
            else if (request->getParam("gateway", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"gateway\"}");
            }
            else if (request->getParam("subnet", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"subnet\"}");
            }
            else if (request->getParam("dns", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"dns\"}");
            }
            else {
              File ap = LittleFS.open("/configs/ap.txt", "w");
              ap.print(request->getParam("ap", true)->value());
              ap.close();
              File maxcon = LittleFS.open("/configs/maxcon.txt", "w");
              maxcon.print(request->getParam("maxcon", true)->value());
              maxcon.close();
              File host = LittleFS.open("/configs/host.txt", "w");
              host.print(request->getParam("host", true)->value());
              host.close();

              File ssidFile = LittleFS.open("/configs/ssid.txt", "w");
              ssidFile.print(request->getParam("ssid", true)->value());
              ssidFile.close();

              File keyFile = LittleFS.open("/configs/key.txt", "w");
              keyFile.print(request->getParam("key", true)->value());
              keyFile.close();

              File dhcpFile = LittleFS.open("/configs/dhcp.txt", "w");
              if (request->getParam("networkType", true)->value() == "dhcp")
                dhcpFile.print("true");
              else
                dhcpFile.print("false");
              dhcpFile.close();

              File ipFile = LittleFS.open("/configs/ip.txt", "w");
              ipFile.print(request->getParam("ip", true)->value());
              ipFile.close();

              File gatewayFile = LittleFS.open("/configs/gateway.txt", "w");
              gatewayFile.print(request->getParam("gateway", true)->value());
              gatewayFile.close();

              File subnetFile = LittleFS.open("/configs/subnet.txt", "w");
              subnetFile.print(request->getParam("subnet", true)->value());
              subnetFile.close();

              File dnsFile = LittleFS.open("/configs/dns.txt", "w");
              dnsFile.print(request->getParam("dns", true)->value());
              dnsFile.close();

              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
          }
        }
        else {
          File name = LittleFS.open("/fa/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          File ap = LittleFS.open("/configs/ap.txt", "r");
          result += "\"ap\":\"" + ap.readString() + "\",";
          ap.close();
          File maxcon = LittleFS.open("/configs/maxcon.txt", "r");
          result += "\"maxcon\":\"" + maxcon.readString() + "\",";
          maxcon.close();
          File host = LittleFS.open("/configs/host.txt", "r");
          result += "\"host\":\"" + host.readString() + "\",";
          host.close();
          result += "\"ssid\":\"" + readConfig("/configs/ssid.txt") + "\",";
          result += "\"key\":\"" + readConfig("/configs/key.txt") + "\",";

          File dhcpFile = LittleFS.open("/configs/dhcp.txt", "r");
          String dhcpVal = dhcpFile.readString();
          dhcpVal.trim();
          dhcpFile.close();

          if (dhcpVal == "true") {
            result += "\"networkType\":\"dhcp\",";
            result += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
            result += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
            result += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
            result += "\"dns\":\"" + WiFi.dnsIP().toString() + "\",";
          }
          else {
            result += "\"networkType\":\"manual\",";
            result += "\"ip\":\"" + readConfig("/configs/ip.txt") + "\",";
            result += "\"gateway\":\"" + readConfig("/configs/gateway.txt") + "\",";
            result += "\"subnet\":\"" + readConfig("/configs/subnet.txt") + "\",";
            result += "\"dns\":\"" + readConfig("/configs/dns.txt") + "\",";
          }
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/fa/settings/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/settings.htm", "text/html");
  });

  server.on("/fa/settings/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            if (request->getParam("name", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"name\"}");
            }
            else if (request->getParam("bgcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"bgcolor\"}");
            }
            else if (request->getParam("fgcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"fgcolor\"}");
            }
            else if (request->getParam("navcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"navcolor\"}");
            }
            else if (request->getParam("pricolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"pricolor\"}");
            }
            else if (request->getParam("seccolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"seccolor\"}");
            }
            else {
              if (LittleFS.exists("/fa/name.txt")) {
                File file = LittleFS.open("/fa/name.txt", "w");
                file.print(request->getParam("name", true)->value());
                file.close();
              }
              if (LittleFS.exists("/fa/allow.txt")) {
                File file = LittleFS.open("/fa/allow.txt", "w");
                file.print(request->getParam("allow", true)->value());
                file.close();
              }
              if (LittleFS.exists("/fa/limit.txt")) {
                File file = LittleFS.open("/fa/limit.txt", "w");
                file.print(request->getParam("limit", true)->value());
                file.close();
              }

              File navcolorFile = LittleFS.open("/configs/navcolor.txt", "w");
              navcolorFile.print(request->getParam("navcolor", true)->value());
              navcolorFile.close();

              File bgcolorFile = LittleFS.open("/configs/bgcolor.txt", "w");
              bgcolorFile.print(request->getParam("bgcolor", true)->value());
              bgcolorFile.close();

              File fgcolorFile = LittleFS.open("/configs/fgcolor.txt", "w");
              fgcolorFile.print(request->getParam("fgcolor", true)->value());
              fgcolorFile.close();

              File pricolorFile = LittleFS.open("/configs/pricolor.txt", "w");
              pricolorFile.print(request->getParam("pricolor", true)->value());
              pricolorFile.close();

              File seccolorFile = LittleFS.open("/configs/seccolor.txt", "w");
              seccolorFile.print(request->getParam("seccolor", true)->value());
              seccolorFile.close();

              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
          }
        }
        else {
          File name = LittleFS.open("/fa/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          File allow = LittleFS.open("/fa/allow.txt", "r");
          result += "\"allow\":\"" + allow.readString() + "\",";
          allow.close();
          File limit = LittleFS.open("/fa/limit.txt", "r");
          result += "\"limit\":\"" + limit.readString() + "\",";
          limit.close();
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/fa/account/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/account.htm", "text/html");
  });

  server.on("/fa/account/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            File user = LittleFS.open("/configs/username.txt", "r");
            File pass = LittleFS.open("/configs/password.txt", "r");
            if (request->getParam("username", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"username\"}");
            }
            else if (request->getParam("username", true)->value() != user.readString()) {
              request->send(200, "application/json", "{\"status\":\"wrongUsername\"}");
            }
            else if (request->getParam("currentPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"currentPassword\"}");
            }
            else if (request->getParam("newPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"newPassword\"}");
            }
            else if (request->getParam("confirmPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"confirmPassword\"}");
            }
            else if (request->getParam("currentPassword", true)->value() != pass.readString()) {
              request->send(200, "application/json", "{\"status\":\"wrongPassword\"}");
            }
            else if (request->getParam("newPassword", true)->value() != request->getParam("confirmPassword", true)->value()) {
              request->send(200, "application/json", "{\"status\":\"noMatch\"}");
            }
            else if (request->getParam("newPassword", true)->value().length() < 8) {
              request->send(200, "application/json", "{\"status\":\"shortPassword\"}");
            }
            else {
              File userFile = LittleFS.open("/configs/username.txt", "r");
              if (request->getParam("username", true)->value() == userFile.readString()) {
                File passFile = LittleFS.open("/configs/password.txt", "w");
                passFile.print(request->getParam("newPassword", true)->value());
                passFile.close();
              }
              userFile.close();
              for (int i = 0; i < 5; i++) {
                session[i].token = "";
                session[i].ip = IPAddress();
                session[i].expire = 0;
              }
              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
            user.close();
            pass.close();
          }
        }
        else {
          File name = LittleFS.open("/fa/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/fa/about/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/about.htm", "text/html");
  });

  server.on("/fa/about/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/fa/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File info = LittleFS.open("/fa/info.txt", "r");
    result += "\"info\":\"" + info.readString() + "\",";
    info.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/fa/contact/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/fa/contact.htm", "text/html");
  });

  server.on("/fa/contact/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/fa/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File address = LittleFS.open("/fa/address.txt", "r");
    result += "\"address\":\"" + address.readString() + "\",";
    address.close();
    File phone = LittleFS.open("/fa/phone.txt", "r");
    result += "\"phone\":\"" + phone.readString() + "\",";
    phone.close();
    File email = LittleFS.open("/fa/email.txt", "r");
    result += "\"email\":\"" + email.readString() + "\",";
    email.close();
    File instagram = LittleFS.open("/fa/instagram.txt", "r");
    result += "\"instagram\":\"" + instagram.readString() + "\",";
    instagram.close();
    File telegram = LittleFS.open("/fa/telegram.txt", "r");
    result += "\"telegram\":\"" + telegram.readString() + "\",";
    telegram.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/en/upload/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/upload.htm", "text/html");
  });

  server.on("/en/upload/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/en/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File allow = LittleFS.open("/en/allow.txt", "r");
    result += "\"allow\":\"" + allow.readString() + "\",";
    allow.close();
    File limit = LittleFS.open("/en/limit.txt", "r");
    result += "\"limit\":\"" + limit.readString() + "\",";
    limit.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
    File host = LittleFS.open("/configs/host.txt", "r");
    result += "\"host\":\"" + host.readString() + "\"}";
    host.close();
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/en/download/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/download.htm", "text/html");
  });

  server.on("/en/download/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("code", true)) {
      if (request->getParam("code", true)->value() == "") {
        request->send(403, "application/json", "{\"status\":\"code\"}");
      }
      else if (LittleFS.exists("/files/" + request->getParam("code", true)->value() + ".txt")) {
        File url = LittleFS.open("/files/" + request->getParam("code", true)->value() + ".txt", "r");
        File host = LittleFS.open("/configs/host.txt", "r");
        request->send(200, "application/json", "{\"download_url\":\"http://" + host.readString() + "/downloads/" + url.readString() + "\"}");
        url.close();
        host.close();
      }
      else {
        request->send(403, "application/json", "{\"status\":\"no_exists\"}");
      }
    }
    else {
      File name = LittleFS.open("/en/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
      File host = LittleFS.open("/configs/host.txt", "r");
      result += "\"host\":\"" + host.readString() + "\"}";
      host.close();
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/en/files/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/files.htm", "text/html");
  });

  server.on("/en/files/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin" || isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "delete" && request->hasParam("code", true)) {
            String fileName = "/files/" + request->getParam("code", true)->value() + ".txt";
            if (fileName == "/files/null.txt") {
              request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Cannot delete null.txt\"}");
              return;
            }
            if (LittleFS.exists(fileName)) {
              LittleFS.remove(fileName);
              request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
              request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"File not found\"}");
            }
          }
          else if (action == "add" && request->hasParam("code", true) && request->hasParam("name", true)) {
            String code = request->getParam("code", true)->value();
            String name = request->getParam("name", true)->value();
            if (name == "") {
              request->send(200, "application/json", "{\"status\":\"name\"}");
            }
            else if (code == "") {
              request->send(200, "application/json", "{\"status\":\"code\"}");
            }
            else {
              String fileName = "/files/" + code + ".txt";
              if (LittleFS.exists(fileName)) {
                request->send(200, "application/json", "{\"status\":\"exists\"}");
              }
              else {
                File file = LittleFS.open(fileName, "w");
                file.print(name);
                file.close();
                request->send(200, "application/json", "{\"status\":\"ok\"}");
              }
            }
          }
        }
        else {
          int index = 1;
          File name = LittleFS.open("/en/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          result += "\"files\":[";
          File dir = LittleFS.open("/files");
          while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;
            if (!entry.isDirectory()) {
              String text = entry.readString();
              String fileName = entry.name();
              int lastSlash = fileName.lastIndexOf('/');
              if (lastSlash != -1) {
                fileName = fileName.substring(lastSlash + 1);
              }
              if (fileName.equals("null.txt")) {
                entry.close();
                continue;
              }
              int dotIndex = fileName.lastIndexOf('.');
              if (dotIndex != -1) {
                fileName = fileName.substring(0, dotIndex);
              }
              result += "{\"id\":\"" + String(index++) + "\",";
              result += "\"code\":\"" + fileName + "\",";
              result += "\"name\":\"" + text + "\"},";
            }
            entry.close();
          }
          dir.close();
          if (result.endsWith(",")) {
            result.remove(result.length() - 1);
          }
          result += "],";
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
          File host = LittleFS.open("/configs/host.txt", "r");
          result += "\"host\":\"" + host.readString() + "\"}";
          host.close();
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/en/login/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/login.htm", "text/html");
  });

  server.on("/en/login/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        request->send(200, "application/json", "{\"status\":\"admin\"}");
      }
      else if (isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        request->send(200, "application/json", "{\"status\":\"user\"}");
      }
      else {
        request->send(200, "application/json", "{\"status\":\"invalid\"}");
      }
    }
    else if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String username = request->getParam("username", true)->value();
      String password = request->getParam("password", true)->value();
      File user = LittleFS.open("/configs/username.txt", "r");
      File pass = LittleFS.open("/configs/password.txt", "r");
      if (username == "" || password == "") {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
      }
      else if (username == user.readString() && password == pass.readString()) {
        String token = generateToken();
        saveToken(token, "admin", request);
        request->send(200, "application/json", "{\"status\":\"ok\", \"token\":\"" + token + "\"}");
      }
      else {
        request->send(401, "application/json", "{\"status\":\"error\",\"message\":\"Invalid credentials\"}");
      }
      user.close();
      pass.close();
    }
    else {
      File name = LittleFS.open("/en/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/en/home/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/home.htm", "text/html");
  });

  server.on("/en/home/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/en/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/en/logout/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/logout.htm", "text/html");
  });

  server.on("/en/logout/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin" || isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        for (int i = 0; i < 5; i++) {
          if (request->getParam("token", true)->value() == session[i].token) {
            session[i].token = "";
            session[i].ip = IPAddress();
            session[i].expire = 0;
          }
        }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      }
      else {
        request->send(200, "application/json", "{\"status\":\"invalid\"}");
      }
    }
    else {
      File name = LittleFS.open("/en/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/en/modules/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/modules.htm", "text/html");
  });

  server.on("/en/modules/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            if (request->getParam("ap", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"ap\"}");
            }
            else if (request->getParam("maxcon", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"maxcon\"}");
            }
            else if (request->getParam("host", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"host\"}");
            }
            else if (request->getParam("networkType", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"networkType\"}");
            }
            else if (request->getParam("ip", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"ip\"}");
            }
            else if (request->getParam("gateway", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"gateway\"}");
            }
            else if (request->getParam("subnet", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"subnet\"}");
            }
            else if (request->getParam("dns", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"dns\"}");
            }
            else {
              File ap = LittleFS.open("/configs/ap.txt", "w");
              ap.print(request->getParam("ap", true)->value());
              ap.close();
              File maxcon = LittleFS.open("/configs/maxcon.txt", "w");
              maxcon.print(request->getParam("maxcon", true)->value());
              maxcon.close();
              File host = LittleFS.open("/configs/host.txt", "w");
              host.print(request->getParam("host", true)->value());
              host.close();

              File ssidFile = LittleFS.open("/configs/ssid.txt", "w");
              ssidFile.print(request->getParam("ssid", true)->value());
              ssidFile.close();

              File keyFile = LittleFS.open("/configs/key.txt", "w");
              keyFile.print(request->getParam("key", true)->value());
              keyFile.close();

              File dhcpFile = LittleFS.open("/configs/dhcp.txt", "w");
              if (request->getParam("networkType", true)->value() == "dhcp")
                dhcpFile.print("true");
              else
                dhcpFile.print("false");
              dhcpFile.close();

              File ipFile = LittleFS.open("/configs/ip.txt", "w");
              ipFile.print(request->getParam("ip", true)->value());
              ipFile.close();

              File gatewayFile = LittleFS.open("/configs/gateway.txt", "w");
              gatewayFile.print(request->getParam("gateway", true)->value());
              gatewayFile.close();

              File subnetFile = LittleFS.open("/configs/subnet.txt", "w");
              subnetFile.print(request->getParam("subnet", true)->value());
              subnetFile.close();

              File dnsFile = LittleFS.open("/configs/dns.txt", "w");
              dnsFile.print(request->getParam("dns", true)->value());
              dnsFile.close();

              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
          }
        }
        else {
          File name = LittleFS.open("/en/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          File ap = LittleFS.open("/configs/ap.txt", "r");
          result += "\"ap\":\"" + ap.readString() + "\",";
          ap.close();
          File maxcon = LittleFS.open("/configs/maxcon.txt", "r");
          result += "\"maxcon\":\"" + maxcon.readString() + "\",";
          maxcon.close();
          File host = LittleFS.open("/configs/host.txt", "r");
          result += "\"host\":\"" + host.readString() + "\",";
          host.close();
          result += "\"ssid\":\"" + readConfig("/configs/ssid.txt") + "\",";
          result += "\"key\":\"" + readConfig("/configs/key.txt") + "\",";

          File dhcpFile = LittleFS.open("/configs/dhcp.txt", "r");
          String dhcpVal = dhcpFile.readString();
          dhcpVal.trim();
          dhcpFile.close();

          if (dhcpVal == "true") {
            result += "\"networkType\":\"dhcp\",";
            result += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
            result += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
            result += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
            result += "\"dns\":\"" + WiFi.dnsIP().toString() + "\",";
          }
          else {
            result += "\"networkType\":\"manual\",";
            result += "\"ip\":\"" + readConfig("/configs/ip.txt") + "\",";
            result += "\"gateway\":\"" + readConfig("/configs/gateway.txt") + "\",";
            result += "\"subnet\":\"" + readConfig("/configs/subnet.txt") + "\",";
            result += "\"dns\":\"" + readConfig("/configs/dns.txt") + "\",";
          }
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/en/settings/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/settings.htm", "text/html");
  });

  server.on("/en/settings/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            if (request->getParam("name", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"name\"}");
            }
            else if (request->getParam("bgcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"bgcolor\"}");
            }
            else if (request->getParam("fgcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"fgcolor\"}");
            }
            else if (request->getParam("navcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"navcolor\"}");
            }
            else if (request->getParam("pricolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"pricolor\"}");
            }
            else if (request->getParam("seccolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"seccolor\"}");
            }
            else {
              if (LittleFS.exists("/en/name.txt")) {
                File file = LittleFS.open("/en/name.txt", "w");
                file.print(request->getParam("name", true)->value());
                file.close();
              }
              if (LittleFS.exists("/en/allow.txt")) {
                File file = LittleFS.open("/en/allow.txt", "w");
                file.print(request->getParam("allow", true)->value());
                file.close();
              }
              if (LittleFS.exists("/en/limit.txt")) {
                File file = LittleFS.open("/en/limit.txt", "w");
                file.print(request->getParam("limit", true)->value());
                file.close();
              }

              File navcolorFile = LittleFS.open("/configs/navcolor.txt", "w");
              navcolorFile.print(request->getParam("navcolor", true)->value());
              navcolorFile.close();

              File bgcolorFile = LittleFS.open("/configs/bgcolor.txt", "w");
              bgcolorFile.print(request->getParam("bgcolor", true)->value());
              bgcolorFile.close();

              File fgcolorFile = LittleFS.open("/configs/fgcolor.txt", "w");
              fgcolorFile.print(request->getParam("fgcolor", true)->value());
              fgcolorFile.close();

              File pricolorFile = LittleFS.open("/configs/pricolor.txt", "w");
              pricolorFile.print(request->getParam("pricolor", true)->value());
              pricolorFile.close();

              File seccolorFile = LittleFS.open("/configs/seccolor.txt", "w");
              seccolorFile.print(request->getParam("seccolor", true)->value());
              seccolorFile.close();

              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
          }
        }
        else {
          File name = LittleFS.open("/en/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          File allow = LittleFS.open("/en/allow.txt", "r");
          result += "\"allow\":\"" + allow.readString() + "\",";
          allow.close();
          File limit = LittleFS.open("/en/limit.txt", "r");
          result += "\"limit\":\"" + limit.readString() + "\",";
          limit.close();
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/en/account/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/account.htm", "text/html");
  });

  server.on("/en/account/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            File user = LittleFS.open("/configs/username.txt", "r");
            File pass = LittleFS.open("/configs/password.txt", "r");
            if (request->getParam("username", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"username\"}");
            }
            else if (request->getParam("username", true)->value() != user.readString()) {
              request->send(200, "application/json", "{\"status\":\"wrongUsername\"}");
            }
            else if (request->getParam("currentPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"currentPassword\"}");
            }
            else if (request->getParam("newPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"newPassword\"}");
            }
            else if (request->getParam("confirmPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"confirmPassword\"}");
            }
            else if (request->getParam("currentPassword", true)->value() != pass.readString()) {
              request->send(200, "application/json", "{\"status\":\"wrongPassword\"}");
            }
            else if (request->getParam("newPassword", true)->value() != request->getParam("confirmPassword", true)->value()) {
              request->send(200, "application/json", "{\"status\":\"noMatch\"}");
            }
            else if (request->getParam("newPassword", true)->value().length() < 8) {
              request->send(200, "application/json", "{\"status\":\"shortPassword\"}");
            }
            else {
              File userFile = LittleFS.open("/configs/username.txt", "r");
              if (request->getParam("username", true)->value() == userFile.readString()) {
                File passFile = LittleFS.open("/configs/password.txt", "w");
                passFile.print(request->getParam("newPassword", true)->value());
                passFile.close();
              }
              userFile.close();
              for (int i = 0; i < 5; i++) {
                session[i].token = "";
                session[i].ip = IPAddress();
                session[i].expire = 0;
              }
              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
            user.close();
            pass.close();
          }
        }
        else {
          File name = LittleFS.open("/en/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/en/about/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/about.htm", "text/html");
  });

  server.on("/en/about/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/en/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File info = LittleFS.open("/en/info.txt", "r");
    result += "\"info\":\"" + info.readString() + "\",";
    info.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/en/contact/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/en/contact.htm", "text/html");
  });

  server.on("/en/contact/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/en/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File address = LittleFS.open("/en/address.txt", "r");
    result += "\"address\":\"" + address.readString() + "\",";
    address.close();
    File phone = LittleFS.open("/en/phone.txt", "r");
    result += "\"phone\":\"" + phone.readString() + "\",";
    phone.close();
    File email = LittleFS.open("/en/email.txt", "r");
    result += "\"email\":\"" + email.readString() + "\",";
    email.close();
    File instagram = LittleFS.open("/en/instagram.txt", "r");
    result += "\"instagram\":\"" + instagram.readString() + "\",";
    instagram.close();
    File telegram = LittleFS.open("/en/telegram.txt", "r");
    result += "\"telegram\":\"" + telegram.readString() + "\",";
    telegram.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/ar/upload/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/upload.htm", "text/html");
  });

  server.on("/ar/upload/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/ar/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File allow = LittleFS.open("/ar/allow.txt", "r");
    result += "\"allow\":\"" + allow.readString() + "\",";
    allow.close();
    File limit = LittleFS.open("/ar/limit.txt", "r");
    result += "\"limit\":\"" + limit.readString() + "\",";
    limit.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
    File host = LittleFS.open("/configs/host.txt", "r");
    result += "\"host\":\"" + host.readString() + "\"}";
    host.close();
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/ar/download/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/download.htm", "text/html");
  });

  server.on("/ar/download/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("code", true)) {
      if (request->getParam("code", true)->value() == "") {
        request->send(403, "application/json", "{\"status\":\"code\"}");
      }
      else if (LittleFS.exists("/files/" + request->getParam("code", true)->value() + ".txt")) {
        File url = LittleFS.open("/files/" + request->getParam("code", true)->value() + ".txt", "r");
        File host = LittleFS.open("/configs/host.txt", "r");
        request->send(200, "application/json", "{\"download_url\":\"http://" + host.readString() + "/downloads/" + url.readString() + "\"}");
        url.close();
        host.close();
      }
      else {
        request->send(403, "application/json", "{\"status\":\"no_exists\"}");
      }
    }
    else {
      File name = LittleFS.open("/ar/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
      File host = LittleFS.open("/configs/host.txt", "r");
      result += "\"host\":\"" + host.readString() + "\"}";
      host.close();
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/ar/files/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/files.htm", "text/html");
  });

  server.on("/ar/files/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin" || isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "delete" && request->hasParam("code", true)) {
            String fileName = "/files/" + request->getParam("code", true)->value() + ".txt";
            if (fileName == "/files/null.txt") {
              request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Cannot delete null.txt\"}");
              return;
            }
            if (LittleFS.exists(fileName)) {
              LittleFS.remove(fileName);
              request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
              request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"File not found\"}");
            }
          }
          else if (action == "add" && request->hasParam("code", true) && request->hasParam("name", true)) {
            String code = request->getParam("code", true)->value();
            String name = request->getParam("name", true)->value();
            if (name == "") {
              request->send(200, "application/json", "{\"status\":\"name\"}");
            }
            else if (code == "") {
              request->send(200, "application/json", "{\"status\":\"code\"}");
            }
            else {
              String fileName = "/files/" + code + ".txt";
              if (LittleFS.exists(fileName)) {
                request->send(200, "application/json", "{\"status\":\"exists\"}");
              }
              else {
                File file = LittleFS.open(fileName, "w");
                file.print(name);
                file.close();
                request->send(200, "application/json", "{\"status\":\"ok\"}");
              }
            }
          }
        }
        else {
          int index = 1;
          File name = LittleFS.open("/ar/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          result += "\"files\":[";
          File dir = LittleFS.open("/files");
          while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;
            if (!entry.isDirectory()) {
              String text = entry.readString();
              String fileName = entry.name();
              int lastSlash = fileName.lastIndexOf('/');
              if (lastSlash != -1) {
                fileName = fileName.substring(lastSlash + 1);
              }
              if (fileName.equals("null.txt")) {
                entry.close();
                continue;
              }
              int dotIndex = fileName.lastIndexOf('.');
              if (dotIndex != -1) {
                fileName = fileName.substring(0, dotIndex);
              }
              result += "{\"id\":\"" + String(index++) + "\",";
              result += "\"code\":\"" + fileName + "\",";
              result += "\"name\":\"" + text + "\"},";
            }
            entry.close();
          }
          dir.close();
          if (result.endsWith(",")) {
            result.remove(result.length() - 1);
          }
          result += "],";
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\",";
          File host = LittleFS.open("/configs/host.txt", "r");
          result += "\"host\":\"" + host.readString() + "\"}";
          host.close();
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/ar/login/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/login.htm", "text/html");
  });

  server.on("/ar/login/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        request->send(200, "application/json", "{\"status\":\"admin\"}");
      }
      else if (isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        request->send(200, "application/json", "{\"status\":\"user\"}");
      }
      else {
        request->send(200, "application/json", "{\"status\":\"invalid\"}");
      }
    }
    else if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String username = request->getParam("username", true)->value();
      String password = request->getParam("password", true)->value();
      File user = LittleFS.open("/configs/username.txt", "r");
      File pass = LittleFS.open("/configs/password.txt", "r");
      if (username == "" || password == "") {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
      }
      else if (username == user.readString() && password == pass.readString()) {
        String token = generateToken();
        saveToken(token, "admin", request);
        request->send(200, "application/json", "{\"status\":\"ok\", \"token\":\"" + token + "\"}");
      }
      else {
        request->send(401, "application/json", "{\"status\":\"error\",\"message\":\"Invalid credentials\"}");
      }
      user.close();
      pass.close();
    }
    else {
      File name = LittleFS.open("/ar/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/ar/home/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/home.htm", "text/html");
  });

  server.on("/ar/home/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/ar/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/ar/logout/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/logout.htm", "text/html");
  });

  server.on("/ar/logout/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin" || isTokenValid(request->getParam("token", true)->value(), request) == "user") {
        for (int i = 0; i < 5; i++) {
          if (request->getParam("token", true)->value() == session[i].token) {
            session[i].token = "";
            session[i].ip = IPAddress();
            session[i].expire = 0;
          }
        }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      }
      else {
        request->send(200, "application/json", "{\"status\":\"invalid\"}");
      }
    }
    else {
      File name = LittleFS.open("/ar/name.txt", "r");
      String result = "{\"name\":\"" + name.readString() + "\",";
      name.close();
      result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
      result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
      result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
      result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
      result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
      request->send(200, "application/json; charset=utf-8", result);
    }
  });

  server.on("/ar/modules/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/modules.htm", "text/html");
  });

  server.on("/ar/modules/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            if (request->getParam("ap", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"ap\"}");
            }
            else if (request->getParam("maxcon", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"maxcon\"}");
            }
            else if (request->getParam("host", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"host\"}");
            }
            else if (request->getParam("networkType", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"networkType\"}");
            }
            else if (request->getParam("ip", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"ip\"}");
            }
            else if (request->getParam("gateway", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"gateway\"}");
            }
            else if (request->getParam("subnet", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"subnet\"}");
            }
            else if (request->getParam("dns", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"dns\"}");
            }
            else {
              File ap = LittleFS.open("/configs/ap.txt", "w");
              ap.print(request->getParam("ap", true)->value());
              ap.close();
              File maxcon = LittleFS.open("/configs/maxcon.txt", "w");
              maxcon.print(request->getParam("maxcon", true)->value());
              maxcon.close();
              File host = LittleFS.open("/configs/host.txt", "w");
              host.print(request->getParam("host", true)->value());
              host.close();

              File ssidFile = LittleFS.open("/configs/ssid.txt", "w");
              ssidFile.print(request->getParam("ssid", true)->value());
              ssidFile.close();

              File keyFile = LittleFS.open("/configs/key.txt", "w");
              keyFile.print(request->getParam("key", true)->value());
              keyFile.close();

              File dhcpFile = LittleFS.open("/configs/dhcp.txt", "w");
              if (request->getParam("networkType", true)->value() == "dhcp")
                dhcpFile.print("true");
              else
                dhcpFile.print("false");
              dhcpFile.close();

              File ipFile = LittleFS.open("/configs/ip.txt", "w");
              ipFile.print(request->getParam("ip", true)->value());
              ipFile.close();

              File gatewayFile = LittleFS.open("/configs/gateway.txt", "w");
              gatewayFile.print(request->getParam("gateway", true)->value());
              gatewayFile.close();

              File subnetFile = LittleFS.open("/configs/subnet.txt", "w");
              subnetFile.print(request->getParam("subnet", true)->value());
              subnetFile.close();

              File dnsFile = LittleFS.open("/configs/dns.txt", "w");
              dnsFile.print(request->getParam("dns", true)->value());
              dnsFile.close();

              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
          }
        }
        else {
          File name = LittleFS.open("/ar/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          File ap = LittleFS.open("/configs/ap.txt", "r");
          result += "\"ap\":\"" + ap.readString() + "\",";
          ap.close();
          File maxcon = LittleFS.open("/configs/maxcon.txt", "r");
          result += "\"maxcon\":\"" + maxcon.readString() + "\",";
          maxcon.close();
          File host = LittleFS.open("/configs/host.txt", "r");
          result += "\"host\":\"" + host.readString() + "\",";
          host.close();
          result += "\"ssid\":\"" + readConfig("/configs/ssid.txt") + "\",";
          result += "\"key\":\"" + readConfig("/configs/key.txt") + "\",";

          File dhcpFile = LittleFS.open("/configs/dhcp.txt", "r");
          String dhcpVal = dhcpFile.readString();
          dhcpVal.trim();
          dhcpFile.close();

          if (dhcpVal == "true") {
            result += "\"networkType\":\"dhcp\",";
            result += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
            result += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
            result += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
            result += "\"dns\":\"" + WiFi.dnsIP().toString() + "\",";
          }
          else {
            result += "\"networkType\":\"manual\",";
            result += "\"ip\":\"" + readConfig("/configs/ip.txt") + "\",";
            result += "\"gateway\":\"" + readConfig("/configs/gateway.txt") + "\",";
            result += "\"subnet\":\"" + readConfig("/configs/subnet.txt") + "\",";
            result += "\"dns\":\"" + readConfig("/configs/dns.txt") + "\",";
          }
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/ar/settings/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/settings.htm", "text/html");
  });

  server.on("/ar/settings/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            if (request->getParam("name", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"name\"}");
            }
            else if (request->getParam("bgcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"bgcolor\"}");
            }
            else if (request->getParam("fgcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"fgcolor\"}");
            }
            else if (request->getParam("navcolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"navcolor\"}");
            }
            else if (request->getParam("pricolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"pricolor\"}");
            }
            else if (request->getParam("seccolor", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"seccolor\"}");
            }
            else {
              if (LittleFS.exists("/ar/name.txt")) {
                File file = LittleFS.open("/ar/name.txt", "w");
                file.print(request->getParam("name", true)->value());
                file.close();
              }
              if (LittleFS.exists("/ar/allow.txt")) {
                File file = LittleFS.open("/ar/allow.txt", "w");
                file.print(request->getParam("allow", true)->value());
                file.close();
              }
              if (LittleFS.exists("/ar/limit.txt")) {
                File file = LittleFS.open("/ar/limit.txt", "w");
                file.print(request->getParam("limit", true)->value());
                file.close();
              }

              File navcolorFile = LittleFS.open("/configs/navcolor.txt", "w");
              navcolorFile.print(request->getParam("navcolor", true)->value());
              navcolorFile.close();

              File bgcolorFile = LittleFS.open("/configs/bgcolor.txt", "w");
              bgcolorFile.print(request->getParam("bgcolor", true)->value());
              bgcolorFile.close();

              File fgcolorFile = LittleFS.open("/configs/fgcolor.txt", "w");
              fgcolorFile.print(request->getParam("fgcolor", true)->value());
              fgcolorFile.close();

              File pricolorFile = LittleFS.open("/configs/pricolor.txt", "w");
              pricolorFile.print(request->getParam("pricolor", true)->value());
              pricolorFile.close();

              File seccolorFile = LittleFS.open("/configs/seccolor.txt", "w");
              seccolorFile.print(request->getParam("seccolor", true)->value());
              seccolorFile.close();

              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
          }
        }
        else {
          File name = LittleFS.open("/ar/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          File allow = LittleFS.open("/ar/allow.txt", "r");
          result += "\"allow\":\"" + allow.readString() + "\",";
          allow.close();
          File limit = LittleFS.open("/ar/limit.txt", "r");
          result += "\"limit\":\"" + limit.readString() + "\",";
          limit.close();
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/ar/account/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/account.htm", "text/html");
  });

  server.on("/ar/account/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    if (request->hasParam("token", true)) {
      if (isTokenValid(request->getParam("token", true)->value(), request) == "admin") {
        if (request->hasParam("action", true)) {
          String action = request->getParam("action", true)->value();
          if (action == "set") {
            File user = LittleFS.open("/configs/username.txt", "r");
            File pass = LittleFS.open("/configs/password.txt", "r");
            if (request->getParam("username", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"username\"}");
            }
            else if (request->getParam("username", true)->value() != user.readString()) {
              request->send(200, "application/json", "{\"status\":\"wrongUsername\"}");
            }
            else if (request->getParam("currentPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"currentPassword\"}");
            }
            else if (request->getParam("newPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"newPassword\"}");
            }
            else if (request->getParam("confirmPassword", true)->value() == "") {
              request->send(200, "application/json", "{\"status\":\"confirmPassword\"}");
            }
            else if (request->getParam("currentPassword", true)->value() != pass.readString()) {
              request->send(200, "application/json", "{\"status\":\"wrongPassword\"}");
            }
            else if (request->getParam("newPassword", true)->value() != request->getParam("confirmPassword", true)->value()) {
              request->send(200, "application/json", "{\"status\":\"noMatch\"}");
            }
            else if (request->getParam("newPassword", true)->value().length() < 8) {
              request->send(200, "application/json", "{\"status\":\"shortPassword\"}");
            }
            else {
              File userFile = LittleFS.open("/configs/username.txt", "r");
              if (request->getParam("username", true)->value() == userFile.readString()) {
                File passFile = LittleFS.open("/configs/password.txt", "w");
                passFile.print(request->getParam("newPassword", true)->value());
                passFile.close();
              }
              userFile.close();
              for (int i = 0; i < 5; i++) {
                session[i].token = "";
                session[i].ip = IPAddress();
                session[i].expire = 0;
              }
              request->send(200, "application/json", "{\"status\":\"ok\"}");
            }
            user.close();
            pass.close();
          }
        }
        else {
          File name = LittleFS.open("/ar/name.txt", "r");
          String result = "{\"name\":\"" + name.readString() + "\",";
          name.close();
          result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
          result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
          result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
          result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
          result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
          request->send(200, "application/json; charset=utf-8", result);
        }
      }
    }
  });

  server.on("/ar/about/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/about.htm", "text/html");
  });

  server.on("/ar/about/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/ar/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File info = LittleFS.open("/ar/info.txt", "r");
    result += "\"info\":\"" + info.readString() + "\",";
    info.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.on("/ar/contact/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/ar/contact.htm", "text/html");
  });

  server.on("/ar/contact/", HTTP_POST, [readConfig](AsyncWebServerRequest * request) {
    File name = LittleFS.open("/ar/name.txt", "r");
    String result = "{\"name\":\"" + name.readString() + "\",";
    name.close();
    File address = LittleFS.open("/ar/address.txt", "r");
    result += "\"address\":\"" + address.readString() + "\",";
    address.close();
    File phone = LittleFS.open("/ar/phone.txt", "r");
    result += "\"phone\":\"" + phone.readString() + "\",";
    phone.close();
    File email = LittleFS.open("/ar/email.txt", "r");
    result += "\"email\":\"" + email.readString() + "\",";
    email.close();
    File instagram = LittleFS.open("/ar/instagram.txt", "r");
    result += "\"instagram\":\"" + instagram.readString() + "\",";
    instagram.close();
    File telegram = LittleFS.open("/ar/telegram.txt", "r");
    result += "\"telegram\":\"" + telegram.readString() + "\",";
    telegram.close();
    result += "\"navcolor\":\"" + readConfig("/configs/navcolor.txt") + "\",";
    result += "\"bgcolor\":\"" + readConfig("/configs/bgcolor.txt") + "\",";
    result += "\"fgcolor\":\"" + readConfig("/configs/fgcolor.txt") + "\",";
    result += "\"pricolor\":\"" + readConfig("/configs/pricolor.txt") + "\",";
    result += "\"seccolor\":\"" + readConfig("/configs/seccolor.txt") + "\"}";
    request->send(200, "application/json; charset=utf-8", result);
  });

  server.begin();
  dnsServer.start(53, "*", WiFi.softAPIP());
}

void loop() {
  dnsServer.processNextRequest();
  delay(250);
}
