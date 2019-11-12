#!/usr/bin/env python
import csv
import sys
import utils
import ConfigParser
import os
from utils import bcolors

from klee_sym_explorer import *
from klee_conc_explorer import *

class KleeExplorers:
    def __init__(self, config, proj_dir):
        self.se_factory = list()
        self.config = config
        self.proj_dir = proj_dir
        self.get_klee_configs()

    def get_klee_configs(self):
        config = ConfigParser.ConfigParser()
        config.read(self.config)
        if "klee sym_explorer" in config.sections():
            print "Initialize symbolic explorer"
            self.se_factory.append(SymExplorer(self.config, self.proj_dir))
        if "klee conc_explorer" in config.sections():
            print "Initialize concolic explorer"
            self.se_factory.append(ConcExplorer(self.config, self.proj_dir))
        self.se_num = int(config.get("moriarty", "max_explorer_instance"))
        assert len(self.se_factory) > 0, "[Error] can't get klee explorers"

        try:
            self.fuzzer_cov_file = config.get("auxiliary info", "cov_edge_file").replace("@target", self.proj_dir)
        except NoOptionError:
            self.fuzzer_cov_file = os.path.join(self.proj_dir,".afl_coverage_combination")
        utils.rmfile_force(self.fuzzer_cov_file)

    def get_se_size(self):
        return len(self.se_factory)

    def get_heuristics(self):
        searcher_heuristic = []
        for _ in self.se_factory:
            searcher_heuristic.extend(_.get_search_heuristics())
        return list(set(searcher_heuristic))

    def get_fuzzer_cov_file(self):
        return self.fuzzer_cov_file


    def is_explorers_alive(self):
        alive = False
        for _ in self.se_factory:
            alive = alive or _.alive()
        return alive


    def run(self, input_list, cov_file_list, batch_run_input_num):
        counter = 0
        for _ in self.se_factory:
            explorer_base = self.se_num * counter
            for i in xrange(self.se_num):
                input_base = batch_run_input_num * i
                _.run(input_list[input_base : input_base + batch_run_input_num], cov_file_list[explorer_base : explorer_base + self.se_num][i])
            counter = counter + 1
    def stop(self):
        for _ in self.se_factory:
            _.stop()


    def terminate_callback(self):
        """called when SIGINT and SIGTERM"""
        for _ in self.se_factory:
            _.terminate_callback()

    def periodic_callback(self):
        """called every 1 hour"""
        for _ in self.se_factory:
            _.periodic_callback()
