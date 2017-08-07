# !usr/bin/python
# coding:utf-8

# CMD: python test-tcp4-client-dhcp.py <DHCP Server IP>
import time
import socket
import sys

def main():
    print "Socket client creat successful"

    host = sys.argv[1]
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
