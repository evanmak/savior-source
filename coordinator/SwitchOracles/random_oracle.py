import time
import math
import random
import threading

def oracle_info(s):
    print "[Oracle-Info] {0}".format(s)
class RandomOracle:
    def __init__(self, config, target):
        """
        The switching strategy is RANDOM:
            randomly select a period of time to run SE
        """
        self.fuzz_timeout = random.randrange(50,60) # in seconds
        self.se_timeout = random.randrange(150,250)*50 # in seconds
        self._se_running = False
        self._invoke_se = False
        self._terminate_se = False
        self.se_info_collect_period = 10
        self.fuzz_info_collect_period = 10
        self._start_time = math.ceil(time.time())

        print "Fuzzer run ", self.fuzz_timeout, " secs"
        print "SE run ", self.se_timeout, " secs"

    def __repr__(self):
        return "Switch Oracle: Random"

    def activate_se(self):
        if not self._se_running:
            time_out = self.fuzz_timeout - (math.ceil(time.time()) - self._start_time)
            oracle_info("{0} secs away from activating se".\
                        format(time_out))
            if math.ceil(time.time()) - self._start_time >= self.fuzz_timeout:
                self._start_time = math.ceil(time.time())
                self.fuzz_timeout = random.randrange(1,3)  # in seconds
                self._invoke_se = True
                self._se_running = True
                self._terminate_se = False
        else:
            #force the signal pulse to stay down
            self._invoke_se = False

    def deactivate_se(self, se):
        if self._se_running:
            time_out = self.se_timeout - (math.ceil(time.time()) - self._start_time)
            oracle_info("{0} secs away from terminating se".\
                        format(time_out))
            if (math.ceil(time.time()) - self._start_time >= self.se_timeout) or \
               se.is_explorers_alive() == False: #some times se terminates early:
                self._start_time = math.ceil(time.time())
                self.se_timeout = random.randrange(150,250)*50 # in seconds
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
        if True:
            self.activate_se()


    def collect_se_stats(self, se):
        """Different Oracles should have different impl"""
        if self._se_running:
            oracle_info("collecting se stats")
        if True:
            self.deactivate_se(se)

    def time_to_invoke_se(self):
        self.collect_fuzzer_stats()
        return self._invoke_se

    def time_to_shutdown_se(self, se):
        self.collect_se_stats(se)
        return self._terminate_se
