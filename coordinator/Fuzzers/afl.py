import os
import sys
import ConfigParser
import subprocess
import multiprocessing
import utils
import logging
import signal
import shutil
import time
from ConfigParser import NoOptionError

def get_afl(config, target):
    """afl fuzzer specific initialization code"""
    return AFL(config, target)

def fuzzer_info(s):
    print utils.bcolors.HEADER+"[AFL-Info]"+utils.bcolors.ENDC," {0}".format(s)

class AFL:
    # ktest file to list of RARE branchid
    input_to_rareid_list = {};
    # ktest file to list of td-idf branchid
    input_to_tfidfid_list = {};

    def __init__(self, config, target):
        self.test_id_candidates = {}
        self.config = config
        self.target = target
        self.get_afl_config()

    def __repr__(self):
        return "Fuzzer: AFL"

    def get_fuzzer_cov_file(self):
        return self.combine_cov_file


    def get_fuzzer_san_file(self):
        return self.combine_san_file

    def get_afl_config(self):
        config = ConfigParser.ConfigParser()
        config.read(self.config)
        if "afl" not in config.sections():
            print "Config file read error"
            sys.exit()

        if "auxiliary info" not in config.sections():
            print "Config file read error"
            sys.exit()

        self.root = config.get("afl", "root")
        self.in_dir = config.get("afl", "in_dir").replace("@target", self.target)
        self.out_dir = config.get("moriarty", "sync_dir").replace("@target", self.target)
        self.input_type = config.get("moriarty", "inputtype")
        self.slave_num = int(config.get("afl", "slave_num"))
        self.target_bin = config.get("moriarty", "target_bin").replace("@target", self.target)

        try:
            self.dictionary = config.get("afl", "use_dict")
        except Exception:
            self.dictionary = None

        try:
            self.use_ui = True if config.get("afl", "use_ui") == '1' else False
        except Exception:
            self.use_ui = False

        try:
            self.combine_cov_file = config.get("auxiliary info", "cov_edge_file").replace("@target", self.target)
        except NoOptionError:
            self.combine_cov_file = os.path.join(self.target,".afl_coverage_combination")

        try:
            self.combine_san_file = config.get("auxiliary info", "bug_edge_file").replace("@target", self.target)
        except NoOptionError:
            self.combine_san_file = os.path.join(self.target,".savior_sanitizer_combination")
        
        if self.dictionary is not None:
            self.dictionary = self.dictionary.replace("@target", self.target)

        #place holders for supporting configurable ASAN options
        self.fuzzer_user = "yaohui.c"
        self.use_asan = False

    def run(self):
        # run afl in master-slave mode

        processes = []

        logger = multiprocessing.log_to_stderr()
        logger.setLevel(logging.INFO)
        os.environ['HYBRID_FUZZING'] = '1'
        for slave in xrange(self.slave_num + 1):
            afl_args = []
            if self.use_asan:
                afl_args.append("sudo ")
                afl_args.append(os.path.join(self.root+"/experimental/asan_cgroups", "limit_memory.sh"))
                afl_args.append("-u "+self.fuzzer_user)
                afl_args.append(os.path.join(self.root, "afl-fuzz"))
                afl_args.append("-m none")
                afl_args.append("-t 5000+")
            else:
                # afl_args.append("env ")
                # afl_args.append(" AFL_NO_UI=1")
                # afl_args.append(" AFL_PRELOAD="+self.ROOT+"/libdislocator/libdislocator.so ")
                os.environ['AFL_PRELOAD'] = self.root+"/libdislocator/libdislocator.so"


                afl_args.append(os.path.join(self.root, "afl-fuzz"))
                afl_args.append("-t 100+")
		afl_args.append("-m none")


            # if not (os.path.exists(self.out_dir) and os.listdir(self.out_dir)):
            afl_args.append("-i"+self.in_dir)
            if self.dictionary is not None:
                afl_args.append("-x"+self.dictionary)
            # else:
            #     #we resume previous fuzzing work
            #     afl_args.append("-i-")

            afl_args.append("-o"+self.out_dir)

            if slave == 0:
                afl_args.append("-M"+"master")
            else:
                afl_args.append("-S "+"slave_"+str(slave).zfill(6))

            if self.input_type == "symfile":
                self.target_bin = self.target_bin.replace("INPUT_FILE", "@@")
            afl_args.append(self.target_bin)


            if self.use_ui:
                args = ["gnome-terminal","--geometry=80x25" ,"--command="+" ".join(afl_args)]
                kw = {}#place holder
            else:
                os.environ['AFL_NO_UI'] = '1'
                args = afl_args
                kw = {'no_output':True, 'use_shell':True}
            try_num = 10000
            while try_num > 0:
                p = multiprocessing.Process(target=utils.exec_async, args=[args], kwargs=kw)
                print "starting Fuzzer:", " ".join(args)
                p.start()
                time.sleep(3)
                try_num -= 1
                if p.is_alive():
                    break
            print " ".join(afl_args)


    def terminate_callback(self):
        """called when SIGINT and SIGTERM"""
        # self.cleanup_synced_klee_instances(self.out_dir)
        pass

    def periodic_callback(self):
        """place holder"""
        pass




    """REMOVE all klee_instances under sync_dir"""
    def cleanup_synced_klee_instances(self, sync_dir):
        #to avoid potential race condition, this should only be done when
        #fuzzers stop all together
        fuzzer_info("Clearing remaing klee_instances folders")
        SENAME="klee_instance"
        try:
            for _f in os.listdir(sync_dir):
                if SENAME in sync_dir+'/'+_f:
                    shutil.rmtree(sync_dir+'/'+_f)
        except OSError:
            return

        #remove AFL's SE .synced dir
        for _f in os.listdir(sync_dir):
            sdp = sync_dir+"/"+_f+"/.synced"
            if ('master' in sdp or 'slave' in sdp) and os.path.exists(sdp):
                for _p in os.listdir(sdp):
                    if SENAME in sdp+"/"+_p:
                        os.remove(sdp+"/"+_p)
        fuzzer_info("Done clearing remaing klee_instances folders")

