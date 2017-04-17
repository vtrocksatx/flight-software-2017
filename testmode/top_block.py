#!/usr/bin/env python2

import sys, time

i = 0
while 1:
    pkt = hex(int(time.time()*10+i))[-4:]
    if int(time.time()*16) % 2 != 1:
        pkt = pkt.rjust(56/8, "s")
    else:
        pkt = pkt.rjust(112/8, "l")
    print(pkt.encode("hex"))
    sys.stdout.flush()
    time.sleep(0.1)
    i += 1
