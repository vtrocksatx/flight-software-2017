# this manages the currently running flow graph
# future: receive commands from network

# source ~/prefix/setup_env.sh && cd ~/rocksat/ && python2 manager.py
# while true; do nc -lp 1337; done

import os, threading, subprocess
from socket import socket, SOCK_STREAM, AF_INET
from Queue import Queue

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
        self.r = DataReporter()
        self.r.start()
        self.r.loaded.acquire()
        if self.r.failure:
            raise IOError("FlowGraphManager couldn't initialize DataReporter")
        self.topblock = None

    def processinput(self):
        cmd = raw_input("PLD>> ").strip()
        self.r.checkfailure()
        if cmd == "start":
            self.start()
        elif cmd == "stop":
            self.stop()
        elif cmd in ["quit", "exit"]:
            self.destroy()
            exit(0)
        else:
            print("unrecognized command: '{}'".format(cmd))

    def start(self):
        self.stop()
        self.topblock = TopBlockThread(self.r) # TODO add adsb/ais switch
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

class DataReporter(threading.Thread):
    class _StopReporter(): pass # reporter shutdown token class

    def __init__(self):
        super(DataReporter, self).__init__()
        self.target = ("localhost", 1337)
        self.loaded = threading.Semaphore(0)
        self.failure = False

    def run(self):
        self.q = Queue()
        try:
            self.s = socket(AF_INET, SOCK_STREAM)
            self.s.connect(self.target)
            print("DataReporter connected to {0}:{1}".format(*self.target))
        except:
            print("DataReporter failed connecting to {0}:{1}\n"
                    "HELPFUL HINT: try nc -lp {1}".format(*self.target))
            self.failure = True
            raise
        finally:
            self.loaded.release()
        while 1:
            msg = self.q.get()
            if self.failure or isinstance(msg, self._StopReporter):
                self.s.close()
                break
            self._checkmsg(msg)
            try:
                self.s.send(msg + "\n")
            except:
                self.failure = True
                raise

    def stop(self):
        self.q.put(self._StopReporter())

    def send(self, msg):
        self.checkfailure()
        self._checkmsg(msg)
        self.q.put(msg)

    def checkfailure(self):
        if self.failure:
            raise IOError("DataReporter failure has occurred")

    def _checkmsg(self, msg):
        if not isinstance(msg, str):
            raise ValueError("only send strings")
        if "\n" in msg:
            raise NotImplementedError("crappy encapsulation in use; no newlines please")

class TopBlockThread(threading.Thread):

    def __init__(self, r):
        super(TopBlockThread, self).__init__()
        self.r = r

    def run(self):
        self.p = subprocess.Popen("./top_block.py",stdout=subprocess.PIPE)
        try:
            while 1:
                line = self.p.stdout.readline()
                if not line: # process finished
                    break
                msg = "ADS-B RX {}".format(line.strip()[1:-1])
                self.r.send(msg)
        finally:
            self.stop()

    def stop(self):
        self.p.terminate()

if __name__ == "__main__":
    main()

