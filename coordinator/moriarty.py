import re
import sys
import os
import signal
import argparse
import ConfigParser
import Fuzzers
import SwitchOracles
import EdgeOracles
import SEs
import utils
import shutil
from utils import bcolors

def moriarty_info(s):
    print bcolors.HEADER+"[Coordinator-Info]"+bcolors.ENDC," {0}".format(s)

class Moriarty(object):
    """
    This is 'the' coordinator that can flexibly choose:
       1. fuzzer
       2. symbolic executor
       3. switch oracle with different switching strategies
       4. edge oracle with different edge/seed selecting strategies
    """
    def __init__(self, proj_dir, config):
        self.proj_dir = proj_dir
        self.config = config
        self.epoch = 10 #num of ticks to poke switch oracle
        self.cov_file_list = []
        self.san_file_list = []
        self.explorer_cov_file_list = []
        self.explored_tests = []#record tests explored before

        self.fuzzer = Fuzzers.get_afl(config, proj_dir)
        self.se_factory = SEs.get_explorer_factory(config, proj_dir)#SEs stands for Symbolic Engines
        self.get_moriarty_config()
        self.switch_oracle = SwitchOracles.get_switch_oracle(config, self.proj_dir)
        self.edge_oracle = EdgeOracles.get_edge_oracle(config, self.target_bin, self.proj_dir)


        moriarty_info("Using Switch_Oracle[{0}], Edge_Oracle[{1}]".format(
            self.switch_oracle.get_current_oracle(),self.edge_oracle.get_current_oracle()))

        utils.expand_stack_limit()

    def clean_up_last_session(self):
        try:
            os.unlink(self.fuzzer.get_fuzzer_cov_file())
        except Exception:
            pass

    def get_moriarty_config(self):
        config = ConfigParser.ConfigParser()
        config.read(self.config)
        if "moriarty" not in config.sections() or "afl" not in config.sections():
            moriarty_info("Config file read error")
            sys.exit()
        try:
            self.compile_script =  config.get("moriarty", "compile_script")
        except Exception:
            self.compile_script = None
        self.target_bin =  config.get("moriarty", "target_bin").replace("@target",self.proj_dir)
        self.target_bc =  config.get("moriarty", "target_bc").replace("@target", self.proj_dir)
        self.cov_file_base = config.get("moriarty", "sync_dir").replace("@target", self.proj_dir)
        self.switch_heuristic = config.get("switch oracle","strategy")
        self.max_allow_se_num = int(config.get("moriarty", "max_explorer_instance"))
        self.num_of_fuzzers = int(config.get("afl", "slave_num"))+1
        self.num_of_explorers = self.se_factory.get_se_size() * self.max_allow_se_num
        self.is_sym_explorer_activated = True if config.has_option("sym_explorer", "bin") else False
        self.is_conc_explorer_activated = True if config.has_option("conc_explorer", "bin") else False

        try:
            self.batch_run_seed_num = int(config.get("moriarty", "batch_run_input_num"))
        except Exception:
            self.batch_run_seed_num = 1
        moriarty_info("Number of explorers: {0}, each batch run {1} inputs".format(self.num_of_explorers, self.batch_run_seed_num))

        try:
            self.only_count_se_cov = True if "true" in config.get("moriarty","only_count_se_cov") else False
        except Exception:
            self.only_count_se_cov = True 

        try:
            self.log_explored_seed_dir = config.get("moriarty", "explored_seed_dir").replace("@target", self.proj_dir)
            utils.mkdir_force(self.log_explored_seed_dir)
            moriarty_info("Saving explored inputs to {0}".format(self.log_explored_seed_dir))
        except Exception:
            moriarty_info("Will not save explored inputs")
            self.log_explored_seed_dir = None

        for explorer_id in xrange(self.num_of_explorers):
            cov = self.cov_file_base
            cov_suffix="/coverage.csv"
            cov = cov + "/.tmp_se_"+str(explorer_id)+".cov"
            self.explorer_cov_file_list.append(cov)

        for slave_id in xrange(self.num_of_fuzzers):
            #constructing coverage file list
            #constructing sanitizer_edge file list
            cov_suffix="/coverage.csv"
            san_suffix="/edge_sanitizer.csv"
            cov = self.cov_file_base
            san = self.cov_file_base
            if slave_id == 0:
                cov = cov + "/master" + cov_suffix
                san = san + "/master" + san_suffix
            else:
                cov = cov + "/slave_"+str(slave_id).zfill(6)+cov_suffix
                san = san + "/slave_"+str(slave_id).zfill(6)+san_suffix
            self.cov_file_list.append(cov)
            self.san_file_list.append(san)

        self.clean_up_last_session();

        #collecting the recommended edges for reasoning
        try:
            self.loc_map = self.proj_dir + '/locmap.csv'
            self.find_loc_script = config.get("afl","root")+"/find_source_loc.sh"
            self.recommend_edge_log = config.get("moriarty","recommend_edge_log").replace("@target", self.proj_dir)
        except Exception:
            self.recommend_edge_log = None
            self.loc_map = None
            self.find_loc_script = None

    def poke_switch_oracle(self):
        if self.switch_oracle.time_to_invoke_explorer():
            if "AFLUnCovSearcher" in self.se_factory.get_heuristics():
                if not self.only_count_se_cov:
                    if not utils.merge_coverage_files(self.cov_file_list, self.fuzzer.get_fuzzer_cov_file()):
                        moriarty_info("can't merge the fuzzer cov files, using the old one")
                if not utils.append_merge_coverage_files(self.explorer_cov_file_list, self.fuzzer.get_fuzzer_cov_file()):
                    moriarty_info("can't append the merged se cov files, using the old one")

            if "SANGuidedSearcher" in self.se_factory.get_heuristics():
                if not utils.merge_sanitizer_files(self.san_file_list, self.fuzzer.get_fuzzer_san_file()):
                    moriarty_info("can't merge the fuzzer sanitizer_edge files, using the old one")

            input_edgeId_list = self.edge_oracle.find_edges_for_se(self.cov_file_list, [self.fuzzer.get_fuzzer_cov_file()], max_inputs = -1, explored_cases=self.explored_tests)
            num = self.remove_explored(input_edgeId_list, self.explored_tests)
            # print "total seeds num:", len(input_edgeId_list)

            # TODO: assuming max_allow_se_num is always 1 for now.
            deduplicated_list = input_edgeId_list[0:min(self.max_allow_se_num * self.batch_run_seed_num, len(input_edgeId_list))]

            if self.log_explored_seed_dir is not None:
                utils.save_inputs(deduplicated_list, self.log_explored_seed_dir)
            # print "len explored tests:", len(self.explored_tests) 

            assert len(deduplicated_list) <= self.max_allow_se_num * self.batch_run_seed_num
            if len(deduplicated_list) > 0:
                utils.log_recommend_edges(deduplicated_list, self.recommend_edge_log,\
                                          self.loc_map, self.find_loc_script, \
                                          self.target_bin, str(self.edge_oracle.get_current_oracle()))
                self.explored_tests.extend(deduplicated_list)
                self.se_factory.run(deduplicated_list, self.explorer_cov_file_list, self.batch_run_seed_num)
                # print "len explored tests:", len(self.explored_tests) 

                # read file by lines -> list/set

                # remove items from self.explored_testsi - list/set.
            else:
                moriarty_info("No meaningful seeds found......")
                print "explored seeds:"
                print self.explored_tests
                #bonus: switch edge oracle and start over
                self.explored_tests = []
                self.edge_oracle.next_oracle()
                moriarty_info("Switching to [{0}] heuristic".format(str(self.edge_oracle.get_current_oracle())))

                #add a snag for better marking
                if self.recommend_edge_log is not None:
                    utils.add_file_snag(self.recommend_edge_log)


        if self.switch_oracle.time_to_shutdown_explorer(self.se_factory):
            self.se_factory.stop()

    def remove_explored(self, all_id_list, explored_id_list):
        """
        this function remove the already explored seeds from the all_id_list.
        record the never-seen seeds in the explored_id_list
        and return the number of removed items
        """
        r_ctr = 0
        new_list = []
        to_remove = []
        for ent in all_id_list:
            not_seen = True
            cur_seed = ent.values()[0]
            cur_edges = ent.values()[2]

            #to remove
            # if (os.path.getsize(cur_seed) < 384):
            #     r_ctr += 1
            #     not_seen = False


            for i in explored_id_list:#check the global explored list and already-seen new_list
                if self.is_sym_explorer_activated:
                    #symbolic executor
                    if (len(set(cur_seed)-set(i.values()[0])) == 0 \
                        and len(set(cur_edges)-set(i.values()[2])) == 0):
                        #seen before
                        r_ctr += 1
                        not_seen = False
                        break
                else:
                    #concolic executor
                    if (cur_seed == i.values()[0]):
                        #seen before
                        r_ctr += 1
                        not_seen = False
                        break
            if not_seen:
                new_list.append(ent)
            else:
                to_remove.append(ent)

        for ent in to_remove:
            all_id_list.remove(ent)
        return r_ctr

    def start(self):
        """
        main logic: query switch oracle on configured time period
        for SE or CE
            the oracle will tell: #Note: Latter we can easily get a more fine-grained control
              1. time_to_invoke_explorer()
              2. time_to_shutdown_explorer()
        for fuzzer
            we keep it running
        """
        utils.compile_target(self.proj_dir, self.compile_script);
        self.fuzzer.run()
        signal.signal(signal.SIGINT, utils.signal_ignore)

        #drive oracle to take action, this callback is synchronized
        utils.loop_every(self.epoch, self.poke_switch_oracle)

    def terminate_callback(self, signal, frame):
        self.switch_oracle.terminate_callback()
        self.edge_oracle.terminate_callback()
        self.fuzzer.terminate_callback()
        self.se_factory.terminate_callback()
        moriarty_info("Professor Moriarty terminated, have a nice day : )")
        os._exit(0)

    # BEAWARE race condition
    def periodic_callback(self, signal, frame):
        self.switch_oracle.periodic_callback()
        # self.edge_oracle.periodic_callback()
        # self.fuzzer.periodic_callback()
        # self.se_factory.periodic_callback()

def init(target, config):
    moriarty = Moriarty(target, config)

    #register signal handlers
    signal.signal(signal.SIGALRM, moriarty.periodic_callback)
    signal.alarm(3600)#trigger every hour 
    signal.signal(signal.SIGINT, moriarty.terminate_callback)
    signal.signal(signal.SIGTERM, moriarty.terminate_callback)

    #start moriarty
    moriarty.start()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--target', required=True,
                        help="Absolute path of the target project directory")
    parser.add_argument('-c', '--config', required=True,
                        help="The moriarty configuration file")
    args = parser.parse_args()
    init(args.target, args.config);





