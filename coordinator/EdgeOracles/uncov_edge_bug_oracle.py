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
from ConfigParser import NoOptionError

def oracle_info(s):
    print bcolors.HEADER+"[Edge-Oracle-Info]"+bcolors.ENDC, "{0}".format(s)

class UncovEdgeBugOracle:
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
        self.target_prog = target_bin
        self.target_dir = os.path.dirname(os.path.abspath(self.target_prog).split()[0])

        self.get_oracle_config()
        self.load_bb2dom_file()#static collection
        self.load_pair_edges_file()#static collection

    def __repr__(self):
        return "uncov-edge-bug"

    def get_oracle_config(self):
        config = ConfigParser.ConfigParser()
        config.read(self.config)
        try:
            self.bb_to_dom_file = config.get("auxiliary info", "code_reach_map").replace("@target", self.target_dir)
            self.pair_edge_file = config.get("auxiliary info", "pair_edge_file").replace("@target", self.target_dir)
        except Exception:
            utils.error_msg("code_reach_map|pair_edge not found in %s"%self.target_dir)
            sys.exit(-1)

        try:
            self.bug_edge_file = config.get("auxiliary info", "bug_edge_file").replace("@target", self.target_dir)
        except NoOptionError:
            self.bug_edge_file = os.path.join(self.target_dir, ".savior_sanitize_combination")

        try:
            self.bp_weight = int(config.get("edge oracle", "bug_potential_weight"))
            self.cp_weight = int(config.get("edge oracle", "code_potential_weight"))
        except Exception:
            self.bp_weight = 50
            self.cp_weight = 50

    def load_bb2dom_file(self):
        try:
            self.bb_dom_map = dict()
            with open(self.bb_to_dom_file) as b2d_file:
                reader = csv.DictReader(b2d_file, delimiter=',')
                for row in reader:
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


    def load_bug_edges_file(self):
        bug_edge_map = {}
        try:
            with open(self.bug_edge_file) as csv_file:
                reader = csv.DictReader(csv_file, delimiter='\t')
                for row in reader:
                    if bug_edge_map.has_key(row['first_seen']):
                        bug_edge_map[row['first_seen']].append(row['edge_id'])
                    else:
                        bug_edge_map[row['first_seen']] = [row['edge_id']]
        except IOError:
            oracle_info("[Warning] Can't open %s" % self.bug_edge_file)

        return bug_edge_map

    def get_result(self, raw_data, max_results):
        #raw_data {e: {'first_seen':str}}

        stats = []
        bug_edge_map = self.load_bug_edges_file()
        for e, raw in raw_data.iteritems():
            cov_edge = e
            seed = raw['first_seen']
            if e == 0:
                continue
            stat  = raw.copy()
            stat['score'] = 0
            neighbours = self.get_pair_if_any(cov_edge)
            for ne in neighbours:
                if not raw_data.has_key(ne):
                    neighbour_bbl_id = str(((int(self.edge_to_parentBB[ne]) >> 1) ^ int(ne)) & 0xfffff)
                    try:
                        stat['score'] += float(self.bb_dom_map[neighbour_bbl_id])  * self.cp_weight
                    except KeyError:
                        stat['score'] += self.cp_weight

                    if not stat.has_key('interesting_block'):
                        stat['interesting_block'] = self.edge_to_parentBB[ne]

            if bug_edge_map.has_key(seed):
                stat['score'] += float(len(bug_edge_map[seed])) * self.bp_weight
                if not stat.has_key('interesting_edges'):
                    stat['interesting_edges'] = []
                stat['interesting_edges'].extend(bug_edge_map[seed])

            if stat['score'] > 0 or random.randrange(1,100) > 70:
                if stat['first_seen'] != '-':
                    stats.append(stat)

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
