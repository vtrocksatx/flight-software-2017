
import os, threading, subprocess



class TopBlockThread(threading.Thread):

    def run(self):
        self.p = subprocess.Popen("./top_block.py",stdout=subprocess.PIPE)
        while 1:
            line = self.p.stdout.readline()
            if not line:
                break # process exited
            print("ADS-B RX {}".format(line.strip()[1:-1]))

    def stop(self):
        self.p.terminate()

while 1:
    cmd = raw_input("PLD>> ").strip()
    if cmd == "start":
        t = TopBlockThread()
        t.start()
    elif cmd == "stop":
        t.stop()
        t.join()
    else:
        print("unrecognized command: '{}'".format(cmd))

