#!/usr/bin/env python
import random
import sys
from utils import bcolors
from operator import itemgetter

def oracle_info(s):
    print bcolors.HEADER+"[Edge-Oracle-Info]"+bcolors.ENDC, "{0}".format(s)

class RandomOracle:
    def __init__(self):
        # self.count = 0
        pass
    def __repr__(self):
        return "random"

    def get_result(self, raw_data, max_results, edge_threshold=0.8):
        total_execs = float(raw_data['0']['inputs'])
        stats = []
        for e, raw in raw_data.iteritems():
            stat = {} 
            stat['edge_id'] = e
            stat['random'] = random.randint(1, sys.maxint) 
            stats.append(stat)
        stats = sorted(stats, key=itemgetter('random'), reverse = True)
        result = {}
        for stat in stats:
            edge_id = stat['edge_id']
            score = stat['random']
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
            elif score >= edge_threshold * result[input_file]['score']:
                result[input_file]['interesting_edges'].append(edge_id)
        return result
