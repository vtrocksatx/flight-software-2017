#!/usr/bin/env python2

# this manages the currently running flow graph
# future: receive commands from network

# source ~/prefix/setup_env.sh && cd ~/rocksat/ && python2 manager.py
# nc -ulp 1337

import os
from threading import Thread
from datareporter import TCPDataReporter, UDPDataReporter
from subprocess import Popen, PIPE

def main():
    f = FlowGraphManager()
    while 1:
        try:
            f.processinput()
        except:
            f.destroy()
            raise


class FlowGraphManager():
    def __init__(self):
        self.topblock = None
        self.r = UDPDataReporter(("localhost", 1337))

    def processinput(self):
        cmd = raw_input("PLD>> ").strip().lower()
        self.r.checkfailure()
        if cmd in TopBlockThread.mode_handlers.keys():
            self.start(cmd)
        elif cmd == "stop":
            self.stop()
        elif cmd in ["quit", "exit"]:
            self.destroy()
            exit(0)
        else:
            print("unrecognized command: '{}'".format(cmd))

    def start(self, mode="adsb"):
        self.stop()
        self.topblock = TopBlockThread(mode, self.r)
        self.topblock.start()

    def stop(self):
        if not self.topblock:
            return
        self.topblock.stop()
        self.topblock.join()
        self.topblock = None

    def destroy(self):
        self.stop()
        self.r.stop()


class TopBlockThread(Thread):
    def _process_adsb(self, data):
        return "ADS-B RX {}".format(data.strip()[1:-1])
    def _process_ais(self, data):
        raise NotImplementedError("no AIS data formatter yet")
    mode_handlers = {
            "adsb":     _process_adsb,
            "ais":      _process_ais,
            "testmode": lambda data: data.strip(),
    }

    def __init__(self, mode, r):
        super(TopBlockThread, self).__init__()
        self.mode = mode
        self.r = r
        if self.mode not in self.mode_handlers.keys():
            raise ValueError("TopBlockThread mode {} not supported, try {}".format(
                    self.mode, self.mode_handlers))

    def run(self):
        self.p = Popen("{}/top_block.py".format(self.mode), stdout=PIPE)
        try:
            while 1:
                line = self.p.stdout.readline()
                if not line: # process finished
                    break
                msg = self.mode_handlers[self.mode](line)
                print(msg)
                self.r.send(msg)
        finally:
            self.stop()

    def stop(self):
        self.p.terminate()


if __name__ == "__main__":
    main()

