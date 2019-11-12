#!/usr/bin/env python
import csv
import sys
import utils
import ConfigParser
from utils import bcolors

from sequential_oracle import *
from random_oracle import *
from random_sanitizer_oracle import *
from uncov_edge_bug_oracle import *
from bug_potential_oracle import *
from avg_bug_potential_oracle import *


class EdgeOracles:
    def __init__(self, config, target_bin, prog_dir):
        """
        This is the edge oracle interface, it collect the raw data and let different oracles
        process it.

        currently we support random, rareness, tfidf and dom-tfidf oracles
        """
        self.config = config
        self.target_prog=target_bin
        self.prog_dir = prog_dir
        self.get_oracle_config()

    def __repr__(self):
        return "current edge oracle: %s"%str(self.get_current_oracle())

    def get_current_oracle(self):
        return self.oracle_pool[self.current_oracle_idx]

    def get_oracle_config(self):
        config = ConfigParser.ConfigParser()
        config.read(self.config)
        if "edge oracle" not in config.sections():
            oracle_info("Missing edge oracle section in config file %s"%self.config)
            sys.exit(-1)
        pool = list(config.get("edge oracle", "heuristics").split(":"))
        self.oracle_pool = []
        for oracle in pool:
            #obey the order specified by user
            if oracle == 'sequential':
                self.oracle_pool.append(SequentialOracle(self.config, self.target_prog))
            if oracle == 'random':
                self.oracle_pool.append(RandomOracle())
            if oracle == 'san-guided':
                self.oracle_pool.append(RandomSanitizerOracle(self.config, self.prog_dir))
            if oracle == 'uncov-edge-bug':
                self.oracle_pool.append(UncovEdgeBugOracle(self.config, self.target_prog))
            if oracle == 'bug-potential':
                self.oracle_pool.append(BugPotentialOracle(self.config, self.target_prog))
            if oracle == 'avg-bug-potential':
                self.oracle_pool.append(AvgBugPotentialOracle(self.config, self.target_prog))

        oracle_info("Using oracle pool: (%s)"%",".join([str(oracle) for oracle in self.oracle_pool]))
        self.current_oracle_idx = 0

    def next_oracle(self, algo='RR'):
        """TODO: there could be other ways of shceduling the oracle heuristics"""
        if algo == 'RR':
            self.current_oracle_idx = (self.current_oracle_idx + 1) % len(self.oracle_pool)

    def get_raw_data(self, raw_data_files):

        for f in raw_data_files:
            if not os.path.exists(f):
                return {}

        def update_raw(raw, csv_file_name, debug=False):
            with open(csv_file_name) as csv_file:
                reader = csv.DictReader(csv_file, delimiter='\t')
                counter =0
                for row in reader:
                    e = row['edge_id']
                    if debug and counter < 50:
                        #print the top 50 rows
                        print row, e
                        counter+=1
                    if e not in raw:
                        raw[e] = {
                            'inputs': row['inputs'],
                            'seeds': row['seeds'],
                            'first_seen': row['first_seen']
                        }
                    else:
                        try:
                            r = row['inputs']
                            s = row['seeds']
                            raw[e]['inputs'] += r
                            raw[e]['seeds'] += s
                        except TypeError:
                            print r
                            print s
                            print raw[e]

        raw_data = dict()

        # Make a list out of argument if it is not already a list
        if type(raw_data_files) is str or type(raw_data_files) is unicode:
            raw_data_files = [raw_data_files]

        # Read raw data from each CSV file and aggregate
        for file_name in raw_data_files:
            update_raw(raw_data, file_name)
        try:
            #this is for system resilience, we don't want moriarty crash when coverage file is malformed
            tmp = raw_data['0']
        except KeyError:
            utils.error_msg('None of the following files contain valid row of total exec number')
            utils.error_msg('{0} '.format(raw_data_files))
            return None

        return raw_data

    def get_se_coverage_data(self, data_files):
        cov_map = {}
        for file_name in data_files:
            try:
                with open(file_name, 'r') as f:
                    for edge in f.readlines():
                        cov_map[edge.strip()] = 0 #Done care about the value
            except IOError:
                oracle_info("{0} is not available yet".format(file_name))
                continue
        return cov_map

    def find_edges_for_se(self, raw_data_files, se_cov_files = [], max_inputs = 3, heuristic = None, explored_cases = []):
        """
        return value format:
            [{input:str , score:int, interesting edges:[str,]}]
        """

        oracle_no_afl_dep = ["bug-potential", "avg-bug-potential", "san-guided"]
        cur_oracle = self.get_current_oracle()
        if cur_oracle.__repr__() in oracle_no_afl_dep:
            # should get edge coverage from savior-afl-binary to avoid mismatch
            raw = self.get_se_coverage_data(se_cov_files)
            oracle_info("Using oracle {0}, only counting se cov #{1}".format(cur_oracle, len(raw.keys())))
        else:
            raw = self.get_raw_data(raw_data_files)
            if raw is None:
                return []
        if cur_oracle.__repr__() == "avg-bug-potential":
            result = cur_oracle.get_result(raw, max_inputs, explored_cases)
        else:
            result = cur_oracle.get_result(raw, max_inputs)
        return result.values() if result is not None else []



    def show_all_scores(self, raw_data, log_file, rank_heu='dom-tfidf'):
        """
        TODO:
        This method is only for debugging, to let us reason about the recommended edges
        based on different oracles
        """
        pass

    def terminate_callback(self):
        """called when SIGINT and SIGTERM"""
        pass

    def periodic_callback(self):
        """place holder"""
        pass
