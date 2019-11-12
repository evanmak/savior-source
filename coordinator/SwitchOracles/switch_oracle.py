from random_oracle import *
from saturate_oracle import *
from saturate_driller_oracle import *
import ConfigParser

class SwitchOracles:
    def __init__(self, config, proj_dir):
        """
        This is the switch oracle interface, it chooses when to invoke explorers 
        based on certain runtime fuzzer stats. 

        currently we support random, saturate and driller_saturate oracles
        """
        self.config = config
        self.proj_dir = proj_dir
        self.get_oracle_config()

    def __repr__(self):
        return "current switch oracle: %s"%str(self.get_current_oracle())

    def get_current_oracle(self):
        return self.oracle_pool[self.current_oracle_idx]

    def next_oracle(self, algo='RR'):
        """TODO: there could be other ways of shceduling the oracle heuristics"""
        if algo == 'RR':
            self.current_oracle_idx = (self.current_oracle_idx + 1) % len(self.oracle_pool)

    def get_oracle_config(self):
        config = ConfigParser.ConfigParser()
        config.read(self.config)
        if "switch oracle" not in config.sections():
            oracle_info("Missing switch oracle section in config file %s"%self.config)
            sys.exit(-1)

        self.sync_dir = config.get("moriarty", "sync_dir").replace("@target", self.proj_dir)
        self.num_of_fuzzers = int(config.get("afl", "slave_num"))+1
        pool = list(config.get("switch oracle", "strategy").split(":"))

        self.oracle_pool = []
        for oracle in pool:
            if oracle == 'saturate':
                self.oracle_pool.append(self.get_saturate(self.config, self.proj_dir,
                                                          self.num_of_fuzzers,
                                                          self.sync_dir))
            if oracle == 'driller_saturate':
                self.oracle_pool.append(self.get_saturate_driller(self.config, self.proj_dir,
                                                          self.num_of_fuzzers,
                                                          self.sync_dir))
            else:
                self.oracle_pool.append(self.get_random(self.config, self.proj_dir))
        oracle_info("Using oracle pool: (%s)"%",".join([str(oracle) for oracle in self.oracle_pool]))
        self.current_oracle_idx = 0

    def time_to_invoke_explorer(self):
        cur_oracle = self.get_current_oracle()
        return cur_oracle.time_to_invoke_se()

    def time_to_shutdown_explorer(self, se):
        cur_oracle = self.get_current_oracle()
        return cur_oracle.time_to_shutdown_se(se)

    def get_random(self, config, target):
        """random_oracle specific initialization wrapper code"""
        return RandomOracle(config, target)

    def get_saturate(self, config, target, num_fuzzer, sync_dir):
        """saturate_oracle specific initialization wrapper code"""
        return SaturateOracle(config, target, num_fuzzer, sync_dir)


    def get_saturate_driller(self, config, target, num_fuzzer, sync_dir):
        """Driller's saturate_oracle: using pending_fav as metric"""
        return SaturateDrillerOracle(config, target, num_fuzzer, sync_dir)

    def terminate_callback(self):
        """called when SIGINT and SIGTERM"""
        pass

    def periodic_callback(self):
        """place holder"""
        pass
