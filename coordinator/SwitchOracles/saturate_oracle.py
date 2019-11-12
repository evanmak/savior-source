import time
import math
import random
import threading

def oracle_info(s):
    print "[Oracle-Info] {0}".format(s)
class SaturateOracle:
    def __init__(self, config, target, num_fuzzer, sync_dir):
        """
        The switching strategy is SATURATE:
            invoke SE when we detect fuzzer is saturated
        """
        self.se_timeout = random.randrange(200,220)*20 # in seconds
        self._se_running = False
        self._invoke_se = False
        self._terminate_se = False
        self._start_time = math.ceil(time.time())
        self.num_fuzzer = num_fuzzer
        self.sync_dir = sync_dir


    def __repr__(self):
        return "Switch Oracle: Saturate"

    def activate_se(self):
        self._invoke_se = True
        self._se_running = True
        self._terminate_se = False

    def deactivate_se(self, se):
        if self._se_running:
            time_out = self.se_timeout - (math.ceil(time.time()) - self._start_time)
            oracle_info("{0} secs away from terminating se".\
                        format(time_out))
            if (math.ceil(time.time()) - self._start_time >= self.se_timeout) or\
               se.is_explorers_alive() == False: #some times se terminates early:
                self._start_time = math.ceil(time.time())
                self.se_timeout = random.randrange(100,120)*20 # in seconds
                self._invoke_se = False
                self._se_running = False
                self._terminate_se = True
                oracle_info("Next time SE will run {0} secs".format(self.se_timeout))
        else:
            #force the signal pulse to stay down
            self._terminate_se = False


    def collect_fuzzer_stats(self):
        """Different Oracles should have different impl"""
        if not self._se_running:
            oracle_info("collecting fuzzer stats")

            #if over half of the fuzzers do not see new path for 10 min
            time_threashold = 300
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
                            if data[0] == "start_time":
                                start_time = int(data[1])
                            if data[0] == "last_update":
                                cur_time = int(data[1])
                            if data[0] == "last_path":
                                last_path = int(data[1])
                        if last_path != 0:
                            if cur_time -last_path >= time_threashold:
                                num_saturated += 1
                        else:
                            if cur_time - start_time >= time_threashold:
                                num_saturated += 1
                    stat_f.close()
                except Exception:
                    pass
            if num_saturated > self.num_fuzzer/2:
                self.activate_se()
        else:
            #force the signal pulse to stay down
            self._invoke_se = False


    def collect_se_stats(self, se):
        """Different Oracles should have different impl"""
        if self._se_running:
            oracle_info("collecting se stats")
        if True:
            self.deactivate_se(se)

    def time_to_invoke_se(self):
        self.collect_fuzzer_stats()
        # print "invoke se:{0} terminate se:{1} se_running:{2} se_timeout:{3}".format(self._invoke_se, self._terminate_se, self._se_running, self.se_timeout)
        return self._invoke_se

    def time_to_shutdown_se(self, se):
        self.collect_se_stats(se)
        return self._terminate_se
