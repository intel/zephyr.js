# !usr/bin/python
# coding:utf-8

import time
import socket
import SocketServer

def main():
    host = "2001:db8::2"
    port = 9876
    print "Creating socket server on " + host + " port " + str(port) + "..."

    addr = (host, port)
    Timeout = 80
    bufSize = 1024

    mySocket = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
    mySocket.settimeout(Timeout)
    mySocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    mySocket.bind(addr)
    mySocket.listen(5)

    connection, clientaddr = mySocket.accept()
    print "Connected from: ", clientaddr

    while 1:
        try :
            Data = connection.recv(bufSize)
            Data = Data.strip()
            print "Got data: ", Data

            time.sleep(2)

            if Data == "close":
                connection.close()
                print "close socket"

                break
            else:
                connection.sendall(Data)
                print "Send data: ", Data
        except KeyboardInterrupt :
            print "exit server"
            break
        except :
            print "time out"
            continue

if __name__ == "__main__" :
    main()
