import time
import math
import random
import threading
import utils
from utils import bcolors

def oracle_info(s):
    print bcolors.HEADER+"[Switch-Oracle-Info]"+bcolors.ENDC, "{0}".format(s)
class SaturateDrillerOracle:
    def __init__(self, config, target, num_fuzzer, sync_dir):
        """
        The switching strategy is SATURATE(Driller version):
            invoke SE when we detect fuzzer is saturated
        """
        self.se_timeout = random.randrange(20,40)*60 # in seconds
        self._se_running = False
        self._invoke_se = False
        self._terminate_se = False
        self._start_time = math.ceil(time.time())
        self.num_fuzzer = num_fuzzer
        self.sync_dir = sync_dir


    def __repr__(self):
        return "Switch Oracle: Saturate Driller"

    def activate_se(self):
        self._invoke_se = True
        self._se_running = True
        self._terminate_se = False

    def deactivate_se(self):
        self._invoke_se = False
        self._se_running = False
        self._terminate_se = True


    def collect_fuzzer_stats(self):
        def lottery_win():
            """randomly return true"""
            if random.randrange(0,100) > 70:
                return True
            return False

        """Different Oracles should have different impl"""
        if not self._se_running:
            oracle_info("collecting fuzzer stats")

            #if over half of the fuzzers do not see pending fav path
            num_saturated = 0
            for slave_id in xrange(self.num_fuzzer):
                if slave_id == 0:
                    fuzzer = self.sync_dir+"/master"
                else:
                    fuzzer = self.sync_dir+"/slave_"+str(slave_id).zfill(6);
                stat_file=fuzzer+"/fuzzer_stats"
                try:
                    with open(stat_file, 'r') as stat_f:
                        for line in stat_f.readlines():
                            data=[x.strip() for x in line.strip().split(":")]
                            if data[0] == "pending_favs":
                                pending_fav = int(data[1])
                        if pending_fav == 0:
                            num_saturated += 1
                    stat_f.close()
                except Exception:
                    pass
            if num_saturated > self.num_fuzzer/2 or lottery_win():
                self._start_time = math.ceil(time.time())
                self.activate_se()
        else:
            #force the signal pulse to stay down
            self._invoke_se = False


    def collect_se_stats(self, se):
        """Different Oracles should have different impl"""
        if self._se_running:
            oracle_info("collecting se stats")
            time_out = self.se_timeout - (math.ceil(time.time()) - self._start_time)
            oracle_info("{0} secs away from terminating se".\
                        format(time_out))
            if (math.ceil(time.time()) - self._start_time >= self.se_timeout) or \
               se.is_explorers_alive() == False: #some times se terminates early
                self.se_timeout = random.randrange(100,120)*20 # in seconds
                self.deactivate_se()
                oracle_info("Next time SE will run {0} secs".format(self.se_timeout))
        else:
            #force the signal pulse to stay down
            self._terminate_se = False


    def time_to_invoke_se(self):
        self.collect_fuzzer_stats()
        # print "invoke se:{0} terminate se:{1} se_running:{2} se_timeout:{3}".format(self._invoke_se, self._terminate_se, self._se_running, self.se_timeout)
        return self._invoke_se

    def time_to_shutdown_se(self, se):
        self.collect_se_stats(se)
        return self._terminate_se
