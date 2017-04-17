#!/usr/bin/env python2

# this manages the currently running flow graph
# future: receive commands from network

# nc -nlup1337
# nc -lp2600 | ./payload.py
# ./mockcommand.py

import os, socket, time, struct
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
        #self.r = UDPDataReporter(("10.101.10.1", 1337))
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
    def _pad_msg(self, data):
        return data.ljust(self.r.max_msg_len, "\0")
    def _process_adsb(self, data):
        msg = data.strip().split()[0].decode("hex")
        try:
            self.adsb_part.add(msg)
        except self.adsb_part.PacketFullError:
            # packetize the queued messages
            ret = str(self.adsb_part)
            # add the next message to the next packet
            self.adsb_part = ADSBPacker()
            self.adsb_part.add(msg)
            return ret
    def _process_ais(self, data):
        raise NotImplementedError("no AIS data formatter yet")
    def _process_test(self, data):
        return data.strip()


    mode_handlers = {
            "adsb":     _process_adsb,
            "ais":      _process_ais,
            "testmode": _process_adsb,
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
        self.adsb_part = ADSBPacker()
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
                if msg:
                    self.r.send(self._pad_msg(msg))
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


class ADSBPacker(object):

    class PacketFullError(ValueError): pass

    def __init__(self):
        self.len = 0
        self.msgs = []

    # refer to icd, ADSB section
    def __str__(self):
        count = len(self.msgs)
        if count > 32: # 6 bits
            return "\0ERROR packet too large"
        lengths = 0
        bit = 1
        for i in range(count):
            l = len(self.msgs[i])*8
            if l == 56:
                # 0, do nothing
                pass
            elif l == 112:
                # 1, add to bitmask
                lengths += bit
            else:
                raise ValueError("bad packet length {} (data {})".format(l, repr(self.msgs[i])))
            bit *= 2
        ret = struct.pack("!BI", count, lengths) + "".join(self.msgs)
        return ret

    def add(self, msg):
        if (1 + 4 + self.len + len(msg)) > 252:
            raise self.PacketFullError("packet is full!")
        self.msgs.append(msg)
        self.len += len(msg)


if __name__ == "__main__":
    main()

