#! /usr/bin/env python3
# -*- coding:utf-8 -*-
#----------------------------------------------------------#
#项目名称：remote_payloads_cli
#程序名称：remote_payloads_cli.py
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
proxy = {'http': '127.0.0.1:8080'}
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
shell commands:
    Command         Description                     Example
    help            show helpful information        help
    ls              list dir                        ls /payloads
    up              upload local file to esp8266    up ~/Desktop/something.js /payloads/other.js
    cat             show content of a file          cat /payloads/logs
    rm              remove file or dir              (file)rm /payloads/logs
                                                    (dir)rm /payloads/
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
    authen = hashlib.sha1((password + cookie).encode()).hexdigest()
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
    nack = username + "@" + url + " /$ "
    cmd = input(nack).split()
    if (cmd):
        print(cmd)
        if cmd[0] == 'exit':
            requests.post(url + "/cmd", headers=headers, data={'cmd': 'exit'})
            sys.exit(0)
        try:
            if cmd[0] == 'help':
                help_()
            elif cmd[0] == 'ls':
                ls_(cmd[1])
            elif cmd[0] == 'up':
                up_(cmd[1], cmd[2])
            elif cmd[0] == 'cat':
                cat_(cmd[1])
            elif cmd[0] == 'rm':
                rm_(cmd[1])
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
cat             show content of a file          cat /payloads/logs
rm              remove file or dir              (file)rm /payloads/logs
                                                (dir)rm /payloads/
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

def cat_(filePath):
    global url
    global headers
    response = requests.post(url + "/cmd", headers=headers, data={'cmd': 'cat', 'filePath': filePath})
    print(response.text)

def rm_(filePath):
    global url
    global headers
    response = requests.post(url + "/cmd", headers=headers, data={'cmd': 'rm', 'filePath': filePath})
    print(response.text)

if __name__ == '__main__':
    main()
