import ConfigParser
import multiprocessing
import subprocess
import os
import sys
import utils
import signal
from utils import bcolors


def se_info(s):
    print bcolors.HEADER+"[KleeSym-Info]"+bcolors.ENDC," {0}".format(s)

class SymExplorer:
    def __init__(self, config, target):
        self.jobs = {}
        self.started_jobs = set()
        self.config = config
        self.target = target
        self.get_config()
        utils.mkdir_force(self.seed_dir)
        self.pid_ctr = 0
        se_info("Symbolic Explorer using searcher[{0}]".format(''.join(self.get_search_heuristics())))

    def get_config(self):
        config = ConfigParser.ConfigParser()
        config.read(self.config)
        self.bin = config.get("klee sym_explorer", "bin")
        self.converter = config.get("klee sym_explorer","converter")
        self.seed_dir = config.get("klee sym_explorer", "klee_seed_dir").replace("@target", self.target)
        self.search_heuristics = config.get("klee sym_explorer", "search_heuristic").split(":")
        self.target_bc = config.get("moriarty", "target_bc").replace("@target", self.target).split()[0]
        self.options = config.get("moriarty", "target_bc").replace("@target", self.target).split()[1:]
        self.klee_err_dir = config.get("klee sym_explorer", "error_dir").replace("@target", self.target)
        # print "XXX", self.options
        try:
            self.max_output = config.get("klee sym_explorer", "max_interesting_output")
        except Exception:
            self.max_output = None
        try:
            self.max_mem = config.get("klee conc_explorer", "max_memory")
        except Exception:
            self.max_mem = str(1024*1024*20) # in kbytes
        try:
            self.max_time_per_seed = config.get("klee conc_explorer", "max_time_per_seed")
        except Exception:
            # by default no time limit per seed.
            self.max_time_per_seed = 0

        self.bitmodel = config.get("moriarty", "bitmodel")
        self.input_type = config.get("moriarty", "inputtype")
        self.sync_dir_base = config.get("moriarty", "sync_dir").replace("@target", self.target)
        if "AFLUnCovSearcher" in self.get_search_heuristics():
            try:
                self.fuzzer_cov_file = config.get("auxiliary info", "cov_edge_file").replace("@target", self.target)
            except NoOptionError:
                self.fuzzer_cov_file = os.path.join(self.target,".afl_coverage_combination")
        #handling cxx options
        try:
            self.klee_ctor_stub = True if config.get("klee sym_explorer", "klee_ctor_stub") == '1' else False
        except Exception:
            self.klee_ctor_stub = True
        try:
            self.klee_uclibcxx = True if config.get("klee sym_explorer", "klee_uclibcxx") == '1' else False
        except Exception:
            self.klee_uclibcxx = False



    def get_search_heuristics(self):
        """return a list of search heuristics"""
        return self.search_heuristics

    def __repr__(self):
        return "SE Engine: KLEE Symbolic Explorer"

    def alive(self):
        alive = False
        multiprocessing.active_children()
        for pid in [self.jobs[x]['real_pid'] for x in self.jobs]:
            try:
                os.kill(pid, 0)
                print "sym_explorer pid: {0} is alive".format(pid)
                alive = True
            except Exception:
                print "sym_explorer pid: {0} not alive".format(pid)
        # cmd = 'ps -e | grep klee | wc -l'
        # if 0 == subprocess.Popen(cmd, shell=True).communicate()[0]:
        #     alive = False

        return alive

    def run(self, input_id_map_list, cov_file_list):
        """
        For each input,
            -convert ktest
            -create sync dir
            -build cmd
            -create new process job
        """
        se_info("{0} activated. input list : {1}".format(self, input_id_map_list))
        for input_id_map in input_id_map_list:
            pid = self.get_new_pid()
            input_idx = 0

            #--generate klee seed ktest
            # print input_id_map
            afl_input = utils.from_simple_to_afl_name(input_id_map['input'])
            if not afl_input:
                continue
            klee_seed = self.seed_dir+"/klee_instance_sym_"+str(pid).zfill(6)+".ktest"
            # print "before calling converter"
            # print afl_input
            self.call_converter("a2k", afl_input, klee_seed, self.bitmodel, self.input_type)
            if not os.path.exists(klee_seed):
                print "no seed" + klee_seed
                continue

            #--create sync_dir for new klee instance
            new_sync_dir = self.sync_dir_base+"/klee_instance_sym_"+str(pid).zfill(6)+"/queue"
            utils.mkdir_force(new_sync_dir)

            #--build klee instance cmd
            edge_ids = [x for x in input_id_map['interesting_edges']]
            stdin_len = os.path.getsize(afl_input)
            klee_cmd = self.build_cmd(klee_seed, edge_ids, new_sync_dir, stdin_len, afl_input, cov_file_list[input_idx])
            print ' '.join(klee_cmd)

            #--construct process meta data, add to jobs list
            kw = {'mock_eof':True,'mem_cap': self.max_mem}
            p = multiprocessing.Process(target=utils.exec_async, args=[klee_cmd], kwargs=kw)
            p.daemon = True
            task_st = {}
            task_st['instance'] = p
            task_st['sync_dir'] = new_sync_dir
            task_st['seed'] = klee_seed
            task_st['cmd'] = klee_cmd
            if "AFLUnCovSearcher" in self.get_search_heuristics():
                task_st['afl_cov'] = self.fuzzer_cov_file
            self.jobs[pid] = task_st
            input_idx = input_idx + 1

        for pid, task in self.jobs.iteritems():
            try:
                if pid not in self.started_jobs:
                    task['instance'].start()
                    task['real_pid'] = task['instance'].pid
                    # print "starting klee process: ", task['real_pid']
                    self.started_jobs.add(pid)
                else:
                    se_info("WTF the process {0} is already started".format(pid))
            except Exception:
                pass



    def stop(self):
        """
        Terminate all jobs,
        you could have more fine-grained control by extending this function
        """
        se_info("{0} deactivated".format(self))
        for pid, task in self.jobs.iteritems():
            se_info("Terminting klee instance: {0} {1} real pid:{2}".format(pid, task['instance'], task['real_pid']))
            utils.terminate_proc_tree(task['real_pid'])
        #reset jobs queue
        self.jobs = {}
        # self.started_jobs= set()


    def build_cmd(self, ktest_seed, edge_ids, sync_dir, stdin_len, afl_input, out_cov_file):
        """
        each afl_testcase will have a list of branch ids,
        we use these info to construct the command for
        starting a new klee instance

        by default:
         use klee's own searching algo
         if specified afl_uncov in config, use AFLUnCovSearcher
        """
        cmd = []
        afl_uncov = "--afl-covered-branchid-file="
        klee_out_uncov = "--klee-covered-branchid-outfile="
        # max_output = "-max-interesting-seinfo="
        sync_dir_flag = "--sync-dir="
        stdin_sym_flag = "--sym-stdin"
        file_sym_flag = "--sym-files"
        sanitizer_searcher_flag = "--edge-sanitizer-heuristic"
        if self.klee_uclibcxx == True:
            klee_libc = "--libc=uclibcxx"
        else:
            klee_libc = "--libc=uclibc"

        if self.klee_ctor_stub == True:
            klee_ctor_stub="--disable-inject-ctor-and-dtor=false"
        else:
            klee_ctor_stub="--disable-inject-ctor-and-dtor=true"

        common_prefix = [self.bin,
                         klee_libc,
                         klee_ctor_stub,
                         "--posix-runtime",
                         "--only-replay-seeds",
                         "--symbolic-explorer=true",
                         "--named-seed-matching=true",
                         "--allow-external-sym-calls",
                         "--use-non-intrinsics-memops=false",
                         "--check-overshift=false",
                         "--only-output-states-covering-new=true"
                         ]
        common_prefix.append("--max-memory=0")

        if "AFLUnCovSearcher" in self.get_search_heuristics():
            common_prefix.append(afl_uncov + self.fuzzer_cov_file)
            # common_prefix.append(max_output + self.max_output)
            common_prefix.append(klee_out_uncov + out_cov_file)

        if "SANGuidedSearcher" in self.get_search_heuristics():
            common_prefix.append(sanitizer_searcher_flag)

        cmd.extend(common_prefix);

        #symbolic explorer by default need target edge ids
        for eid in edge_ids:
            cmd.append("--branchid-list=" + eid)

        cmd.append("--seed-out=" + ktest_seed)
        cmd.append(sync_dir_flag + sync_dir)
        cmd.append(self.target_bc)
        cmd.extend(self.options)

        if self.input_type == "stdin":
                cmd.append(stdin_sym_flag)
                cmd.append(str(stdin_len))
        else:
            cmd.append("A")
            cmd.append(file_sym_flag)
            cmd.append("1")
            cmd.append(str(stdin_len))
        return cmd

    def get_new_pid(self):
        self.pid_ctr += 1
        return self.pid_ctr

    def call_converter(self, mode, afl_input, ktest, bitmodel, inputtype):
        """
        SEs directly invoke the converter to
        convert between the afl/klee file formats
        as the SE input format is specific to target SE engine
        """
        args = [];
        args.append(self.converter)
        args.append("--mode="+ mode)
        args.append("--afl-name="+afl_input)
        args.append("--ktest-name="+ktest)
        args.append("--bitmodel="+bitmodel);
        args.append("--inputmode="+inputtype);
        subprocess.Popen(args).wait()

    def terminate_callback(self):
        """called when SIGINT and SIGTERM"""
        se_info("packing klee error cases into [{0}]".format(self.klee_err_dir))
        utils.pack_klee_errors(self.target, self.klee_err_dir)

    def periodic_callback(self):
        """called every 1 hour"""
        pass
