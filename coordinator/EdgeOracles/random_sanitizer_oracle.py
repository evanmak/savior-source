#!/usr/bin/env python
import random
import sys
import csv
from utils import bcolors
from operator import itemgetter
import ConfigParser
import utils
import os
from ConfigParser import NoOptionError

def oracle_info(s):
    print bcolors.HEADER+"[Edge-Oracle-Info]"+bcolors.ENDC, "{0}".format(s)

class RandomSanitizerOracle:
    def __init__(self, config, target):
        self.target = target
        self.config = config
        cfg = ConfigParser.ConfigParser()
        cfg.read(self.config)

        try:
            self.san_edge_file = cfg.get("auxiliary info", "bug_edge_file").replace("@target", self.target)
            utils.rmfile_force(self.san_edge_file)
        except NoOptionError:
            self.san_edge_file = os.path.join(self.target, ".savior_sanitizer_combination")

    def __repr__(self):
        return "san-guided"

    def get_raw_sanitizer_data(self):
        def update_raw(raw, csv_file_name):
            try:
                with open(csv_file_name) as csv_file:
                    reader = csv.DictReader(csv_file, delimiter='\t')
                    for row in reader:
                        e = row['edge_id']
                        if e not in raw:
                            raw[e] = {
                                'first_seen': row['first_seen']
                            }
                        else:
                            oracle_info("saw duplicated edge:{0}".format(e))
            except IOError:
                oracle_info("can't open %s" % csv_file_name)

        raw_data = dict()
        update_raw(raw_data, self.san_edge_file)
        return raw_data

    def get_result(self, raw_cov_data, max_results):
        stats = []
        raw_san_data = self.get_raw_sanitizer_data()
        for e, raw in raw_san_data.iteritems():
            stat = raw.copy()
            stat['edge_id'] = e
            stat['random'] = random.randint(1, sys.maxint)
            stats.append(stat)
        stats = sorted(stats, key=itemgetter('random'), reverse=True)
        result = {}
        for stat in stats:
            edge_id = stat['edge_id']
            score = stat['random']
            input_file = stat['first_seen']
            if input_file not in result:
                # Don't add more results than requested
                if len(result) >= max_results and max_results != -1:
                    break
                result[input_file] = {
                    'score': score,
                    'interesting_edges': [edge_id],
                    'input': input_file
                }
        return result
