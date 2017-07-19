# !usr/bin/python
# coding:utf-8

import time
import socket

def main():
    print "Socket client creat successful"

    host = "192.0.2.1"
    port = 9876
    bufSize = 1024
    addr = (host, port)
    Timeout = 300

    mySocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    mySocket.settimeout(Timeout)
    mySocket.connect(addr)

    while 1:
        try :
            Data = mySocket.recv(bufSize)
            Data = Data.strip()
            print "Got data: ", Data

            time.sleep(2)

            if Data == "close":
                mySocket.close()
                print "close socket"

                break
            else:
                mySocket.sendall(Data)
                print "Send data: ", Data
        except :
            print "time out"
            continue

if __name__ == "__main__" :
    main()
