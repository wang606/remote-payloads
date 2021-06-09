//----------------------------------------------------------//
//项目名称：remote_payloads_cli
//程序名称：remote_payloads_cli.ino
//作者：wangqinghua
//日期：2021年6月7日
//----------------------------------------------------------//


#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <Hash.h>

String _username = "wangqinghua"; // 自定义用户名
String _password = "hacker"; // 自定义密码
ESP8266WiFiMulti wifier; // 建立ESP8266WiFiMulti对象,对象名称是 'wifier'
ESP8266WebServer shell(80); // 建立cli服务器对象shell，该对象用于响应cli请求。监听端口（80）
String _cookie = ""; 
String cookie_ = ""; 
File fsUploadFile; 
//设置需要收集的请求头信息
const char* headerKeys[] = {"cookie", "User-Agent"}; 

void setup() {
  //打开串口
  Serial.begin(9600);
  //联网
  wifier.addAP("ssid_1", "pass_1");
  wifier.addAP("ssid_1", "pass_2");
  Serial.print("Connecting to ");
  int i = 0;
  while (wifier.run() != WL_CONNECTED) {
    delay(1000);
    Serial.print('.');
  }
  Serial.print(WiFi.SSID());
  Serial.print(" successfully!\nIP:\t");
  Serial.println(WiFi.localIP());
  //启动文件系统
  bool _ = SPIFFS.begin();
  if (_) {
    Serial.println("open FS successfully!");
  } else {
    Serial.println("can't access to FS!");
  }
  //启动shell
  shell.on("/cookie", handleCookie); 
  shell.on("/up", HTTP_POST, respondOK, handleFileUpload); 
  shell.on("/log", handleLogs); 
  shell.on("/cmd", handleCmd); 
  shell.onNotFound(handler); 
  shell.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));
  shell.begin();
  Serial.println("stand by!");
}

void loop() {
  shell.handleClient();
}

//--------------------cmd-start--------------------//
bool handleCookie() {
  // checkCookie
  if (shell.hasArg("cookie")) {
    // oldCookie
    if (shell.arg("cookie") == _cookie) {
      shell.send(200); 
      return true; 
    }
    // newCookie
    else if (shell.arg("cookie") == cookie_) {
      if (shell.arg("authen") == sha1(_password + cookie_)) {
        _cookie = cookie_; 
        Serial.println("_cookie=" + _cookie); 
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
  if (shell.arg("username") != _username) {
    shell.send(403, "text/plain", "Invalid username!"); 
    return false; 
  }
  int time_ = time(NULL); 
  int rand_ = rand(); 
  cookie_ = sha1(_username + time_ + rand_); 
  shell.send(200, "text/plain", cookie_); 
  return true; 
}

void respondOK() {
  shell.send(200); 
}

bool handleFileUpload() {
  if (!_cookie) {
    shell.send(403, "text/plain", "no cookie!"); return false; 
  }
  String cookie = shell.header("cookie"); 
  Serial.println("cookie_header=" + cookie); 
  if (cookie != _cookie) {
    Serial.println("wrong cookie!"); shell.send(403); return false; 
  }
  String filePath = shell.arg("filePath"); 
  HTTPUpload& upload = shell.upload(); 
  if(upload.status == UPLOAD_FILE_START){                     // 如果上传状态为UPLOAD_FILE_START                          // 建立字符串变量用于存放上传文件名
    if(!filePath.startsWith("/")) filePath = "/" + filePath;  // 为上传文件名前加上"/"
    Serial.println("File Path: " + filePath);                 // 通过串口监视器输出上传文件的名称
    fsUploadFile = SPIFFS.open(filePath, "w");            // 在SPIFFS中建立文件用于写入用户上传的文件数据
  }
  else if(upload.status == UPLOAD_FILE_WRITE){          // 如果上传状态为UPLOAD_FILE_WRITE      
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // 向SPIFFS文件写入浏览器发来的文件数据
  }
  else if(upload.status == UPLOAD_FILE_END){            // 如果上传状态为UPLOAD_FILE_END 
    if(fsUploadFile) {                                    // 如果文件成功建立
      fsUploadFile.close();                               // 将文件关闭
      Serial.println(" Size: "+ upload.totalSize);        // 通过串口监视器输出文件大小
      shell.send(200, "text/plain", "upload successfully!"); 
    } else {                                              // 如果文件未能成功建立
      Serial.println("File upload failed");               // 通过串口监视器输出报错信息
      shell.send(200, "text/plain", "fail to upload!"); // 向浏览器发送相应代码500（服务器错误）
    }    
  }
}

void handleLogs() {
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
  shell.send(200); 
}

bool handleCmd() {
  if (!_cookie) {
    shell.send(403, "text/plain", "no cookie!"); 
    return false; 
  }
  if (shell.header("cookie") != _cookie) {
    shell.send(403, "text/plain", "wrong cookie!"); 
    return false; 
  }
  String cmd = shell.arg("cmd"); 
  if (cmd == "hello") {
    shell.send(200, "text/plain", "hello " + _username + "!"); 
  } else if (cmd == "ls") {
    String dir = shell.arg("dir");
    _ls(dir); 
  } else if (cmd == "cat") {
    String filePath = shell.arg("filePath"); 
    _cat(filePath); 
  } else if (cmd == "rm") {
    String filePath = shell.arg("filePath"); 
    _rm(filePath); 
  } else if (cmd == "exit") {
    _cookie = ""; 
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
  shell.send(200, "text/plain", dirList); 
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
//--------------------cli-end--------------------//

// 处理用户浏览器的HTTP访问
void handler() {
  // 获取用户请求网址信息
  String webAddress = shell.uri();
  Serial.print('webAddress= ');
  Serial.println(webAddress);
  // 通过handleFileRead函数处处理用户访问
  bool fileReadOK = handleFileRead(webAddress);
  // 如果在SPIFFS无法找到用户访问的资源，则回复404 (Not Found)
  if (!fileReadOK){                                                 
    shell.send(404, "text/plain", "404 Not Found"); 
  }
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
