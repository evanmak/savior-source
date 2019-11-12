#!/usr/bin/env python
import sys
import os
import ConfigParser
from utils import bcolors
from operator import itemgetter

def oracle_info(s):
    print bcolors.HEADER+"[Edge-Oracle-Info]"+bcolors.ENDC, "{0}".format(s)

class SequentialOracle:
    def __init__(self, config, target_bin):
        self.count = 0
        self.fuzzer_input_dir = self.get_fuzzer_queue_dir(config, target_bin)
        oracle_info("Using input dir %s" % self.fuzzer_input_dir)
    def __repr__(self):
        return "Sequential"

    def get_fuzzer_queue_dir(self, raw_config, target_bin):
        config = ConfigParser.ConfigParser()
        config.read(raw_config)
        target_dir = os.path.dirname(os.path.abspath(target_bin).split()[0])
        sync_dir = config.get("moriarty", "sync_dir").replace("@target", target_dir)
        return os.path.join(sync_dir, "master", "queue")


    def read_queue(self):
        return [f for f in os.listdir(self.fuzzer_input_dir) if os.path.isfile(os.path.join(self.fuzzer_input_dir, f))]

    def get_result(self, raw_data, max_results, edge_threshold=0.8):
        stats = []
        seeds = self.read_queue()
        oracle_info("read seeds %d" % len(seeds))
        for input in seeds:
            stat = {}
            stat['edge_id'] = '0'
            stat['sequential'] = self.count
            stat['first_seen'] = os.path.join(self.fuzzer_input_dir, input)
            self.count += 1
            stats.append(stat)
        stats = sorted(stats, key=itemgetter('sequential'), reverse = False)
        result = {}
        for stat in stats:
            edge_id = stat['edge_id']
            score = stat['sequential']
            input_file = stat['first_seen']
            if input_file not in result:
                # Don't add more results than requested
                if max_results != -1 and len(result) >= max_results:
                    break
                result[input_file] = {
                    'score': score,
                    'interesting_edges': [edge_id],
                    'input': input_file
                }
        return result
