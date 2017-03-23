#!/usr/bin/env python2

import time
from socket import socket, AF_INET, SOCK_STREAM, SOCK_DGRAM, timeout
from threading import Thread

class DataListener(Thread):
    def run(self):
        self.s = socket(AF_INET, SOCK_DGRAM)
        self.s.bind(("localhost", 1337))
        self.s.settimeout(1)
        self._exit = False
        while not self._exit:
            try:
                d = self.s.recvfrom(252)
            except timeout:
                continue
            print(d[0])
        self.s.close()

    def stop(self):
        self._exit = True

def drivetest():
    s = socket(AF_INET, SOCK_STREAM)
    s.connect(("", 2600))
    s.send("testmode\n")
    time.sleep(3)
    s.send("adsb\n")
    time.sleep(3)
    s.send("testmode\n")
    time.sleep(3)
    s.send("exit\n")
    s.close()

if __name__ == "__main__":
    t = DataListener()
    t.start()
    try:
        drivetest()
    finally:
        t.stop()
        t.join()
