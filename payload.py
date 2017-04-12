#!/usr/bin/env python2

# this manages the currently running flow graph
# future: receive commands from network

# nc -nlup1337
# nc -lp2600 | ./payload.py
# ./mockcommand.py

import os, socket, time
from threading import Thread
from datareporter import TCPDataReporter, UDPDataReporter
from subprocess import Popen, PIPE

def main():
    start = time.time()
    t = lambda: time.time()-start
    f = FlowGraphManager()
    while 1:
        # FIXME read input from socket instead
        try:
            f.processinput()
        except socket.timeout:
            raise NotImplementedError("comms failsafe under construction")
        except:
            f.destroy()
            raise


class FlowGraphManager():
    def __init__(self):
        self.topblock = None
        self.r = UDPDataReporter(("10.101.10.1", 1337))

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
    def _pad_msg(self, data):
        return data.ljust(self.r.max_msg_len, "\0")
    def _process_adsb(self, data):
        return self._pad_msg("ADS-B RX {}\n".format(data.strip().split()[0]))
    def _process_ais(self, data):
        raise NotImplementedError("no AIS data formatter yet")
    def _process_test(self, data):
        return self._pad_msg(data.strip())
    mode_handlers = {
            "adsb":     _process_adsb,
            "ais":      _process_ais,
            "testmode": _process_test,
    }
    mode_binaries = {
            "adsb":     os.path.join(os.path.dirname(__file__), "modes_rx"),
            "ais":      "ais/top_block.py",
            "testmode": "testmode/top_block.py",
    }

    def __init__(self, mode, r):
        super(TopBlockThread, self).__init__()
        self.mode = mode
        self.r = r
        self.p = None
        if self.mode not in self.mode_handlers.keys():
            raise ValueError("TopBlockThread mode {} not supported, try {}".format(
                    self.mode, self.mode_handlers))

    def run(self):
        self.p = Popen(self.mode_binaries[self.mode], stdout=PIPE)
        try:
            self._wait_for_load()
            while 1:
                line = self.p.stdout.readline()
                if not line: # process finished
                    break
                msg = self.mode_handlers[self.mode](self, line)
                self.r.send(msg)
        finally:
            self.stop()

    def _wait_for_load(self):
        if self.mode == "adsb":
            trace = ""
            while 1:
                line = self.p.stdout.readline()
                if not line:
                    raise IOError("failed to load {}, output so far:\n{}".format(self.mode, trace))
                if line.startswith("Using Volk machine: "):
                    #next line will be actual data
                    return
                trace += line


    def stop(self):
        if self.p:
            self.p.terminate()
            self.p = None


if __name__ == "__main__":
    main()

