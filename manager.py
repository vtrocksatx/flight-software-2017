# this manages the currently running flow graph
# future: receive commands from network

# source ~/prefix/setup_env.sh && cd ~/rocksat/ && python2 manager.py
# while true; do nc -lp 1337; done

import os, threading, subprocess
from socket import socket, SOCK_STREAM, AF_INET
from Queue import Queue

def main():
    r = DataReporter()
    r.start()
    r.loaded.acquire()
    if r.failure:
        exit(-1)
    while 1:
        cmd = raw_input("PLD>> ").strip()
        if cmd == "start":
            t = TopBlockThread(r) # TODO add adsb/ais switch
            t.start()
        elif cmd == "stop":
            t.stop()
            t.join()
        elif cmd in ["quit", "exit"]:
            try:
                r.stop()
                t.stop()
            finally:
                exit(0)
        else:
            print("unrecognized command: '{}'".format(cmd))


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
        except Exception as e:
            print("DataReporter failed connecting to {0}:{1}\n"
                    "HELPFUL HINT: try nc -lp {1}".format(*self.target))
            self.failure = True
            raise e
        finally:
            self.loaded.release()
        while 1:
            msg = self.q.get()
            if isinstance(msg, self._StopReporter):
                self.s.close()
                break
            self._checkmsg(msg)
            self.s.send(self.q.get() + "\n")

    def stop(self):
        self.q.put(self._StopReporter())

    def send(self, msg):
        self._checkmsg(msg)
        self.q.put(msg)

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
        while 1:
            line = self.p.stdout.readline()
            if not line: # process finished
                self.p.terminate()
                break
            msg = "ADS-B RX {}".format(line.strip()[1:-1])
            self.r.send(msg)

    def stop(self):
        self.p.terminate()

if __name__ == "__main__":
    main()

