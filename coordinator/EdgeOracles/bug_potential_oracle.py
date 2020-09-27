#!/usr/bin/env python
import math
import utils
import sys
import os
import csv
import random
import ConfigParser
from utils import bcolors
from operator import itemgetter
import itertools

def oracle_info(s):
    print bcolors.HEADER+"[Edge-Oracle-Info]"+bcolors.ENDC, "{0}".format(s)

class BugPotentialOracle:
    """ This oracle calculate per-seed potential to reach bugs"""
    def __init__(self, config, target_bin):
        #map[edge]={'bbl','dom_num'}, singleton, constantly updating
        self.edge_dom_map = dict()
        #map['BBID']={'DOMNUM'}, singleton, load once
        self.bb_dom_map = None
        #map['edge id']={}
        self.edge_pair_map = dict()
        #map['edge_id] = {'parent_basicblock_id'}
        self.edge_to_parentBB = {}

        self.config = config
        self.target_prog_cmd = target_bin
        self.target_dir = os.path.dirname(os.path.abspath(self.target_prog_cmd).split()[0])

        self.get_oracle_config()
        self.load_bb2dom_file()#static collection
        self.load_pair_edges_file()#static collection

        self.fuzzer_input_dir = self.get_fuzzer_queue_dir(config, target_bin)
        oracle_info("Using fuzzer input dir %s" % self.fuzzer_input_dir)
        self.input_to_edges_cache = {} #cache of seed to edges
        self.input_to_score_cache = {} #cache of seed to score
        self.covered_fuzzer_edges = set() #edges obtained by replaying savior-afl binary

    def __repr__(self):
        return "bug-potential"

    def get_fuzzer_queue_dir(self, raw_config, target_bin):
        config = ConfigParser.ConfigParser()
        config.read(raw_config)
        target_dir = os.path.dirname(os.path.abspath(target_bin).split()[0])
        self.input_mode = config.get("moriarty", "inputtype")
        sync_dir = config.get("moriarty", "sync_dir").replace("@target", target_dir)

        # TODO: read seed from all queues
        # prefer to use slave queue
        fuzzer_dir = os.path.join(sync_dir, "slave_000001", "queue")
        return fuzzer_dir

##### PR: filename mismatch
    def read_queue(self):
        return [utils.from_afl_name_to_simple(f) for f in os.listdir(self.fuzzer_input_dir) if os.path.isfile(os.path.join(self.fuzzer_input_dir, f))]
##### PR: filename mismatch

    def get_oracle_config(self):
        config = ConfigParser.ConfigParser()
        config.read(self.config)
        self.replay_prog_cmd = config.get("moriarty", "target_bin").replace("@target",self.target_dir)
        try:
            self.bb_to_dom_file = config.get("auxiliary info", "bug_reach_map").replace("@target", self.target_dir)
            self.pair_edge_file = config.get("auxiliary info", "pair_edge_file").replace("@target", self.target_dir)
        except Exception:
            utils.error_msg("bug_reach_map|pair_edge files not found in %s"%self.target_dir)
            sys.exit(-1)

    def load_bb2dom_file(self):
        try:
            self.bb_dom_map = dict()
            with open(self.bb_to_dom_file) as b2d_file:
                reader = csv.DictReader(b2d_file, delimiter=',')
                for row in reader:
                    if self.bb_dom_map.has_key(row['BBID']):
                        if self.bb_dom_map[row['BBID']] < row['DOMNUM']:
                            #take the higher one, as dma might have collision
                            self.bb_dom_map[row['BBID']] = row['DOMNUM']
                    else:
                        self.bb_dom_map[row['BBID']] = row['DOMNUM']
            oracle_info('Loading BBL to Domination Map %s'%self.bb_to_dom_file)
        except Exception:
            utils.error_msg("can't load bb_dom_map: %s"%self.bb_to_dom_file)
            sys.exit(-1)


    def get_pair_if_any(self, e):
        ret = []
        self_group = set([e])
        if self.edge_pair_map.has_key(e):
            my_group = self.edge_pair_map[e]
            ret = list(my_group - self_group)
        return ret

    def load_pair_edges_file(self):
        try:
            with open(self.pair_edge_file, "r") as f:
                for line in f:
                    parent_bbid = line.split(':')[0]
                    edges = line.split(':')[1].split()
                    if len(edges) < 2:
                        continue
                    for i in range(len(edges)):
                        #there could be hash collision, causing duplicated edge ids in different code blocks
                        self.edge_pair_map[edges[i]] = set(edges)
                        self.edge_to_parentBB[edges[i]] = parent_bbid

        except:
            utils.error_msg("can't load pair_edge_file: %s"%self.pair_edge_file)
            sys.exit(-1)


    def build_input_to_edges_cache(self, inputs):
        """store input and map to their edges"""
        for seed in inputs:
            if not self.input_to_edges_cache.has_key(seed):
                # run seed can collect edges
                edges = utils.get_edge_cover_by_seed(self.replay_prog_cmd, seed, self.input_mode)
                if len(edges) == 0:
                    continue
                self.input_to_edges_cache[seed] = set(edges)
                self.covered_fuzzer_edges = self.covered_fuzzer_edges | self.input_to_edges_cache[seed]

    def build_input_to_score_cache(self, dummy_all_edges, inputs):
        stats = []
        # update input_to_edges_cache.
        self.build_input_to_edges_cache(inputs)
        for seed,edges in self.input_to_edges_cache.iteritems ():
            # calculate potential of each seed
            stat = {}
            stat['score'] = 0.0
            stat['first_seen'] = seed
            stat['interesting_edges'] = []
            contributing_edge_counter = 0
            for e in edges:
                is_interesting_edge = False
                neighbours = self.get_pair_if_any(e)
                for ne in neighbours:
                    if not ne in self.covered_fuzzer_edges :
                        neighbour_bbl_id = str(((int(self.edge_to_parentBB[ne]) >> 1) ^ int(ne)) & 0xfffff)
                        try:
                            contributing_edge_counter += 1
                            is_interesting_edge = True
                            stat['score'] += float(self.bb_dom_map[neighbour_bbl_id])
                        except KeyError:
                            pass
                        if not stat.has_key('interesting_block'):
                            stat['interesting_block'] = self.edge_to_parentBB[ne]
                if is_interesting_edge:
                    stat['interesting_edges'].append(e)

            # it is not used for now, but maybe later could help with debugging
            self.input_to_score_cache[seed] = stat['score']

            if stat['score'] > 0:
                if stat['first_seen'] != '-':
                    stat['score'] = stat['score']
                    stats.append(stat)
        return stats

    def get_result(self, raw_data, max_results):
        #raw_data {e: {'first_seen':str}}

        seeds = [os.path.join(self.fuzzer_input_dir, x) for x in self.read_queue()]
        oracle_info("read %d seeds" % len(seeds))
        stats = self.build_input_to_score_cache(raw_data, seeds)

        stats = sorted(stats, key=itemgetter('score'), reverse=True)
        # print stats
        result = {}
        for stat in stats:
            # Don't add more results than requested
            if max_results != -1 and len(result) >= max_results:
                break
            try:
                edge_ids = stat['interesting_edges']
            except KeyError:
                edge_ids = []
            try:
                block_id = stat['interesting_block']
            except KeyError:
                block_id = None
            score = stat['score']
            input_file = stat['first_seen']
            if input_file not in result:
                result[input_file] = {
                    'score': score,
                    'interesting_edges': edge_ids,
                    'interesting_blocks': [block_id],
                    'input': input_file
                }
            else :
                result[input_file]['interesting_edges'].extend(edge_ids)
                result[input_file]['interesting_blocks'].append(block_id)
        return result
