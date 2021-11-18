#! /usr/bin/env python3
# -*- coding:utf-8 -*-
#----------------------------------------------------------#
#项目名称：remote_payloads
#程序名称：remote_payloads.py
#作者：wangqinghua
#日期：2021年6月7日
#----------------------------------------------------------#

import os, sys
import getopt
import getpass
import requests
import hashlib

url = ""
username = ""
password = ""
cookie = ""
headers = {}
def main():
    global url
    global username
    global password
    global cookie
    global headers
    # 获取参数
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hr:u:p:')
    except getopt.GetoptError as error:
        print(str(error))
        usage()
        sys.exit(2)
    h_ = False
    for key, value in opts:
        if key == '-h':
            h_ = True
        elif key == '-r':
            url = value
        elif key == '-u':
            username = value
        elif key == '-p':
            password = value
    if (h_):
        usage()
        sys.exit(0)
    if (url == ""):
        url = input("url[http://ip_or_domain:port]: ")
    _ = authentication()
    if (_):
        # 进入shell
        while(True):
            shell()
    else:
        print("fail to authenticate!")
        sys.exit(0)

def usage():
    print("""
usage: remote_payloads_cli.py [-h][-r <url:post>][-u <username>][-p <password>]
description:
    remotely connect to your esp8266 standby with a fake shell.
    """)

def authentication():
    global url
    global username
    global password
    global cookie
    global headers
    if (username == ""):
        username = input("username: ")
    getCookie = requests.post(url=url + "/cookie", data={'username': username})
    if (getCookie.status_code == 200):
        cookie = getCookie.text
    else:
        return False
    if (password == ""):
        password = getpass.getpass("password: ")
    authen = hashlib.sha1((hashlib.sha1(password.encode()).hexdigest() + cookie).encode()).hexdigest()
    checkCookie = requests.post(url=url + "/cookie", data={'cookie': cookie, 'authen': authen})
    if (checkCookie.status_code == 200):
        headers['cookie'] = cookie
        return True
    else:
        return False
      
def shell():
    global url
    global username
    global cookie
    global headers
    nack = "\033[1;32m" + username + "@" + url[url.index('://') + 3:] + " /$ \033[0m"
    cmd = input(nack).split()
    if (cmd):
        if cmd[0] == 'exit':
            response = requests.post(url + "/cmd", headers=headers, data={'cmd': 'exit'})
            print(response.text)
            sys.exit(0)
        try:
            if cmd[0] == 'help':
                help_()
            elif cmd[0] == 'ls':
                ls_(cmd[1])
            elif cmd[0] == 'up':
                up_(cmd[1], cmd[2])
            elif cmd[0] == 'down':
                down_(cmd[1], cmd[2])
            elif cmd[0] == 'cat':
                cat_(cmd[1])
            elif cmd[0] == 'rm':
                rm_(cmd[1])
            elif cmd[0] == 'mv':
                mv_(cmd[1], cmd[2])
            elif cmd[0] == 'wget':
                if (cmd[1].startswith("https://")):
                    fingerprint = input("sha1 fingerprint: ")
                else:
                    fingerprint = ""
                if (len(cmd) > 2):
                    if (cmd[2] == '-o'):
                        wget_(cmd[1], cmd[3], fingerprint)
                    else:
                        print("Invalid Command!")
                else:
                    wget_(cmd[1], "/" + cmd[1].split('/')[-1], fingerprint)
            elif cmd[0] == 'clear':
                os.system('clear')
            else:
                print("Invalid Command!")
        except BaseException as error:
            print(error)

def help_():
    print("""
Command         Description                     Example
help            show helpful information        help
ls              list dir                        ls /payloads
up              upload local file to esp8266    up ~/Desktop/something.js /payloads/other.js
down            download file from esp8266      down /payloads/something.js ~/Desktop/other.js
cat             show content of a file          cat /payloads/logs
rm              remove file or dir              (file)rm /payloads/logs
                                                (dir)rm /payloads/
mv              move file or dir                (file)mv /payloads/logs /logs
                                                (dir)mv /upload/ /payloads/new/
wget            send a HTTP/HTTPS request and\  wget http://www.baidu.com/robots.txt [-o /tmp/baidu_robots.txt]
                save the payload to file\
                default path is '/'+fileName
clear           clear shell                     clear
exit            log off and exit                exit
    """)

def ls_(dirPath):
    global url
    global headers
    response = requests.post(url + "/cmd", headers=headers, data={'cmd': 'ls', 'dir': dirPath})
    print(response.text)

def up_(localFilePath, filePath):
    global url
    global headers
    data = {'filePath': filePath}
    files = {'file': open(localFilePath, 'rb')}
    response = requests.post(url + "/up", headers=headers, data=data, files=files)
    print(response.text)

def down_(filePath, localFilePath):
    global url
    global headers
    response = requests.get(url + filePath, headers=headers)
    with open(localFilePath, 'wb') as fp:
        fp.write(response.content)
    print("done!")

def cat_(filePath):
    global url
    global headers
    response = requests.post(url + "/cmd", headers=headers, data={'cmd': 'cat', 'filePath': filePath})
    print(response.text)

def rm_(filePath):
    global url
    global headers
    response = requests.post(url + "/cmd", headers=headers, data={'cmd': 'rm', 'filePath': filePath})
    if (response.text):
        print(response.text)

def mv_(oFilePath, dFilePath):
    global url
    global headers
    response = requests.post(url + "/cmd", headers=headers, data={'cmd': 'mv', 'oFilePath': oFilePath, 'dFilePath': dFilePath})
    if (response.text):
        print(response.text)
    
def wget_(httpUrl, filePath, fingerprint):
    global url
    global headers
    response = requests.post(url + "/cmd", headers=headers, data={'cmd': 'wget', 'httpUrl': httpUrl, 'filePath': filePath, 'fingerprint': fingerprint})
    print(response.text)

if __name__ == '__main__':
    main()
