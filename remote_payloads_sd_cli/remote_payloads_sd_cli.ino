/*---------------------------------------------------------//
*项目名称：remote_payloads_sd_cli
*程序名称：remote_payloads_sd_cli.ino
*作者：wangqinghua
*日期：2021年6月15日
*特别说明：
*  接线：sd卡驱动<---->NodeMCU
*       VCC----------Vin(5V驱动)
*       GND----------GND
*       MISO---------D6
*       MOSI---------D7
*       SCK----------D5
*       CS-----------D8(可修改，如下`SD.begin(D8)`直接替换即可)
*----------------------------------------------------------*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <SPI.h>
#include <SD.h>
#include <Hash.h>

//-----config-start-----//
//#define TIME; // remove comment if you want to know date of each file
String username = "wangqinghua"; 
String password = "4dcc4173d80a2817206e196a38f0dbf7850188ff"; // sha1 of password "hacker"
String hidenDir = "wangqinghua/"; //url including hidenDir is forbidden to browser but accessible to CLI
//-----config-end-----//
ESP8266WiFiMulti wifier; // 建立ESP8266WiFiMulti对象,对象名称是 'wifier'
ESP8266WebServer shell(80); 
String cookieNow = ""; 
String cookieThen = ""; 
WiFiClient wifiClient; 
HTTPClient httpClient; 
std::unique_ptr<BearSSL::WiFiClientSecure>wifiClientSSL(new BearSSL::WiFiClientSecure);
HTTPClient httpsClient; 
const char *fingerprint; 
const char* headerKeys[] = {"cookie", "User-Agent"}; //设置需要收集的请求头信息
File sdFile; 

void setup() {
  wifier.addAP("ssid_1", "pass_1"); 
  wifier.addAP("ssid_1", "pass_2"); 
  while (wifier.run() != WL_CONNECTED) { delay(1000); }
  SD.begin(D8); 
  //启动shell
  shell.on("/cookie", handleCookie); 
  shell.on("/up", HTTP_POST, respondOK, handleFileUpload); 
  shell.on("/logs", handleLogs); 
  shell.on("/list", handleList); 
  shell.on("/cmd", handleCmd); 
  shell.onNotFound(handler); 
  shell.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0])); 
  shell.begin(); 
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
  if (shell.header("cookie") != cookieNow) {
    shell.send(403); return false; 
  }
  String filePath = shell.arg("filePath"); 
  HTTPUpload& upload = shell.upload(); 
  if(upload.status == UPLOAD_FILE_START){                     // 如果上传状态为UPLOAD_FILE_START                          // 建立字符串变量用于存放上传文件名
    if(!filePath.startsWith("/")) filePath = "/" + filePath;  // 为上传文件名前加上"/"
    SD.remove(filePath); 
    sdFile = SD.open(filePath, FILE_WRITE);            // 在SD中建立文件用于写入用户上传的文件数据
  }
  else if(upload.status == UPLOAD_FILE_WRITE){          // 如果上传状态为UPLOAD_FILE_WRITE      
    if(sdFile)
      sdFile.write(upload.buf, upload.currentSize); // 向SD文件写入浏览器发来的文件数据
  }
  else if(upload.status == UPLOAD_FILE_END){            // 如果上传状态为UPLOAD_FILE_END 
    if(sdFile) {                                    // 如果文件成功建立
      sdFile.close();                               // 将文件关闭
      shell.send(200, "text/plain", "upload successfully!"); 
    } else {                                              // 如果文件未能成功建立通过串口监视器输出报错信息
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
  File logFile = SD.open("/payloads/logs", FILE_WRITE); 
  logFile.println(logs); 
  logFile.close(); 
  if (toUrl) {
    shell.send(200, "text/html", "<script>window.location.replace(\"" + toUrl + "\")</script>"); 
  } else {
    shell.send(200); 
  }
}

// need to be modified to fit your SDcard
#ifdef TIME
void deviation(struct tm * tmstruct, int* year, int* month, int* day, int* hour, int* minute, int* second) {
  *year = (tmstruct->tm_year) + 1960; 
  *month = (tmstruct->tm_mon + 8) % 12; 
  if ((*month <= 7 && (*month % 2 == 1)) or (*month >= 8 && (*month % 2 == 0))) {
    *day = (tmstruct->tm_mday + 23) % 31; 
  } else {
    *day = (tmstruct->tm_mday + 22) % 30; 
  }
  *hour = (tmstruct->tm_hour + 4) % 24; 
  *minute = (tmstruct->tm_min + 53) % 60; 
  *second = tmstruct->tm_sec; 
}
#endif

void printDirectory(File dir, String dirPath, int numTabs, String* result, bool html) {
  while (true) {
    File entry =  dir.openNextFile();
    if (!entry) {
      break;
    }
    *result += (html) ? "<tr><td>|" : "|"; 
    for (uint8_t i = 0; i < numTabs; i++) {
      *result += "--";
    }
    *result += (html) ? "<a href=\"" + dirPath + entry.name() + "\">" + entry.name() + "</a>" : entry.name();
    if (entry.isDirectory()) {
      *result += "/</td></tr>";
      printDirectory(entry, dirPath + entry.name() + "/", numTabs + 1, result, html);
    } else {
      // files have sizes, directories do not
      *result += "</td><td>"; 
      *result += entry.size(); 
      *result += "</td>"; 
      #ifdef TIME
      time_t cr = entry.getCreationTime();
      time_t lw = entry.getLastWrite();
      char str[80]; 
      struct tm * tmstruct = localtime(&cr);
      int year, month, day, hour, minute, second; 
      deviation(tmstruct, &year, &month, &day, &hour, &minute, &second); 
      sprintf(str, "<td>%d-%02d-%02d %02d:%02d:%02d</td>", year, month, day, hour, minute, second);
      *result += str; 
      tmstruct = localtime(&lw);
      deviation(tmstruct, &year, &month, &day, &hour, &minute, &second); 
      sprintf(str, "<td>%d-%02d-%02d %02d:%02d:%02d</td>", year, month, day, hour, minute, second);
      *result += str; 
      #endif
      *result += "</tr>"; 
    }
    if (!html) {
      (*result).replace("<tr>", ""); 
      (*result).replace("</tr>", "\n"); 
      (*result).replace("<td>", "\t\t"); 
      (*result).replace("</td>", ""); 
    }
    entry.close();
  }
}

void handleList() {
  File root = SD.open("/"); 
  String dirList = "<html><head><meta charset=\"UTF-8\"><title>remote payloads</title></head>";
  dirList += "<body><center><h1>ESP8266 SD Remote Payloads</h1><hr/><pre><table><tr><th>FilePath</th><th>Size/byte</th>"; 
  #ifdef TIME
  dirList += "<th>creationTime</th><th>lastModifiedTime</th>"; 
  #endif
  dirList += "</tr>"; 
  printDirectory(root, "/", 0, &dirList, true); 
  dirList += "</table></pre></center></body></html>"; 
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

void _ls(String dirPath) {
  File dir = SD.open(dirPath); 
  String result = ""; 
  printDirectory(dir, dirPath, 0, &result, false); 
  shell.send(200, "text/plain", result.substring(0, result.length() - 1)); 
}

void _cat(String filePath) {
  File dataFile = SD.open(filePath, FILE_READ);
  String data = "";
  for (int i = 0; i < dataFile.size(); i++) {
    data += (char)dataFile.read();
  }
  dataFile.close();
  shell.send(200, "text/plain", data); 
}

void rmDir(String filePath) {
  File dir = SD.open(filePath); 
  File entry; 
  while (entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      rmDir(filePath + entry.name() + "/"); 
    } else {
      SD.remove(filePath + entry.name()); 
    }
  }
  SD.rmdir(filePath); 
}

void _rm(String filePath) {
  if (filePath.endsWith("/")) {
    rmDir(filePath); 
  } else SD.remove(filePath);
  shell.send(200); 
}

void mvFile(String oFilePath, String dFilePath) {
  String data = ""; 
  File oDataFile = SD.open(oFilePath, FILE_READ); 
  for (int i = 0; i < oDataFile.size(); i++) {
    data += (char)oDataFile.read(); 
  }
  oDataFile.close(); 
  File dDataFile = SD.open(dFilePath, FILE_WRITE); 
  dDataFile.print(data); 
  dDataFile.close(); 
  SD.remove(oFilePath); 
}

void mvDir(String oFilePath, String dFilePath) {
  File dir = SD.open(oFilePath); 
  File entry; 
  while (entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      mvDir(oFilePath + entry.name() + "/", dFilePath + entry.name() + "/"); 
    } else {
      mvFile(oFilePath + entry.name(), dFilePath + entry.name()); 
    }
  }
  SD.rmdir(oFilePath); 
}

bool _mv(String oFilePath, String dFilePath) {
  if (oFilePath.endsWith("/") && dFilePath.endsWith("/")) {
    mvDir(oFilePath, dFilePath); 
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
    File dataFile = SD.open(filePath, FILE_WRITE); 
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
    File dataFile = SD.open(filePath, FILE_WRITE); 
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
void handler() {
  // 获取用户请求网址信息
  String webAddress = shell.uri();
  // 通过shellFileRead函数处处理用户访问
  bool fileReadOK = handleFileRead(webAddress);
  // 如果在SD无法找到用户访问的资源，则回复404 (Not Found)
  if (!fileReadOK){                                                 
    shell.send(404); 
  }
}

bool handleFileRead(String path) {
  String contentType = getContentType(path);  // 获取文件类型
  if (SD.exists(path)) {                     // 如果访问的文件可以在SD中找到
    File file = SD.open(path, FILE_READ);          // 则尝试打开该文件
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
