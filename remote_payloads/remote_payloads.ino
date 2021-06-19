
//----------------------------------------------------------//
//项目名称：remote_payloads
//程序名称：remote_payloads.ino
//作者：wangqinghua
//日期：2021年6月7日
//----------------------------------------------------------//


#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <FS.h>

ESP8266WiFiMulti wifier; // 建立ESP8266WiFiMulti对象,对象名称是 'wifier'
ESP8266WebServer standby(80); // 建立网络服务器对象standby，该对象用于响应HTTP请求。监听端口（80）
File fsUploadFile; // 建立文件对象用于闪存文件上传

void setup() {
  //打开串口
  Serial.begin(9600);
  //联网
  wifier.addAP("wifi_1_name", "wifi_1_password");
  wifier.addAP("wifi_2_name", "wifi_2_password");
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
  //启动standby
  standby.on("/upload.html", HTTP_POST, respondOK, handleFileUpload); 
  standby.on("/list", handleList); 
  standby.on("/delete", handleDelete); 
  standby.on("/logs", handleLogs);
  standby.onNotFound(handler);
  standby.begin();
  Serial.println("stand by!");
}

void loop() {
  standby.handleClient();
}

// 处理上传文件函数
bool handleFileUpload() {
  HTTPUpload& upload = standby.upload(); 
  String filename = upload.filename; // 建立字符串变量用于存放上传文件名
  if (filename.indexOf("..") != -1) {
    String result = "illegal chr \"..\" in " + filename + "<p><a href=\"javascript:history.go(-1)\">back</a></p>"; 
    standby.send(200, "text/html", result); 
    Serial.println(result); 
    return false; 
  }
  if (!filename.startsWith("/")) filename = "/" + filename; // 为上传文件名前加上"/"
  filename = "/payloads" + filename; // 为上传文件名前加上"/payloads"
  Serial.println("File Name: " + filename); 
  if (upload.status == UPLOAD_FILE_START) {
    fsUploadFile = SPIFFS.open(filename, "w"); // 在SPIFFS中建立文件用于写入用户上传的文件数据
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize); // 向SPIFFS文件写入浏览器发来的文件数据
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      // Serial.println("Size: " + upload.totalSize); 
      fsUploadFile.close(); 
      String result = filename + " uploaded successfully!"; 
      result += "<p><a href=\"javascript:history.go(-1)\">back</a></p>"; 
      standby.send(200, "text/html", result); 
    } else {
      Serial.println("File upload failed"); 
      standby.send(500, "text/plain", "500: fail to upload or create file"); 
    }
  }
  return true; 
}

void respondOK() {
  standby.send(200); 
}

void handleList() {
  Dir payloads = SPIFFS.openDir("/payloads"); 
  String payloadList = "/payloads<br/>"; 
  while (payloads.next()) {
    payloadList += payloads.fileName() + "<br/>"; 
  }
  payloadList += "<p><a href=\"javascript:history.go(-1)\">back</a></p>"; 
  standby.send(200, "text/html", payloadList); 
}

bool handleDelete() {
  String filename = standby.arg("filename"); 
  if (filename.indexOf("..") != -1) {
    String result = "illegal chr \"..\" in " + filename + "<p><a href=\"javascript:history.go(-1)\">back</a></p>"; 
    standby.send(200, "text/html", result); 
    Serial.println(result); 
    return false; 
  }
  filename = "/payloads/" + filename; 
  String result = ""; 
  if (SPIFFS.remove(filename)) {
    result += filename + " deleted!"; 
  } else {
    result += "fail to delete " + filename; 
  }
  result += "<p><a href=\"javascript:history.go(-1)\">back</a></p>"; 
  standby.send(200, "text/html", result); 
  Serial.println(result); 
  return true; 
}

void handleLogs() {
  String toUrl = shell.arg("toUrl"); 
  String logs = standby.hostHeader() + "\n"; 
  for (int i = 0; i < standby.headers(); i++) {
    logs += standby.headerName(i) + ": " + standby.header(i) + "\n"; 
  }
  for (int i = 0; i < standby.args(); i++) {
    logs += standby.argName(i) + ": " + standby.arg(i) + "\n"; 
  }
  logs += "\n\n"; 
  File logFile = SPIFFS.open("/payloads/logs", "a"); 
  logFile.println(logs); 
  logFile.close(); 
  if (toUrl) {
    shell.send(200, "text/html", "<script>window.location.replace(\"" + toUrl + "\")</script>"); 
  } else {
    shell.send(200); 
  }
}

// 处理用户浏览器的HTTP访问
void handler() {
  // 获取用户请求网址信息
  String webAddress = standby.uri();
  Serial.print('webAddress= ');
  Serial.println(webAddress);
  // 通过handleFileRead函数处处理用户访问
  bool fileReadOK = handleFileRead(webAddress);
  // 如果在SPIFFS无法找到用户访问的资源，则回复404 (Not Found)
  if (!fileReadOK){                                                 
    standby.send(404); 
  }
}

bool handleFileRead(String path) {
  String contentType = getContentType(path);  // 获取文件类型
  if (SPIFFS.exists(path)) {                     // 如果访问的文件可以在SPIFFS中找到
    File file = SPIFFS.open(path, "r");          // 则尝试打开该文件
    standby.streamFile(file, contentType);// 并且将该文件返回给浏览器
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
