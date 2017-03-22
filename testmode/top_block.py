#!/usr/bin/env python2

import sys, time

for i in range(100):
    print(hex(int(time.time()*100+i)))
    sys.stdout.flush()
    time.sleep(0.2)
