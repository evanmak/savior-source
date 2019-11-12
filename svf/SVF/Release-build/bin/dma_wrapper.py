#!/usr/bin/env python2
import sys
import os
import subprocess

def error(err_msg):
    print err_msg
    sys.exit(-1)

def run():
    found_dma = False
    dma_arg_list = []
    out_file = ""
    bb_score = ""
    final_bc_file = ""
    src_bc = ""
    dma_dir = ""
    dma_abs_path = ""
    bug_inst_flag = "-savior-label-only"
    for i in xrange(len(sys.argv)):
        if not found_dma:
            if 'dma_wrapper.py' in sys.argv[i]:
                found_dma = True
                dma_dir = os.path.dirname(os.path.abspath(sys.argv[i]))
                dma_abs_path = os.path.join(dma_dir, os.path.basename("./dma"))
                if not os.path.exists(dma_abs_path):
                    found_dma = False
                dma_arg_list.append(dma_abs_path)
                continue
        if sys.argv[i] == "-o":
            bugcov_idx = i+1
        if ".bc" in sys.argv[i]:
            src_bc = sys.argv[i]
            final_bc_file = src_bc.split('.')[0]+".dma.bc"
        dma_arg_list.append(sys.argv[i])

    arg_list_cov = list(dma_arg_list)
    arg_list_cov[bugcov_idx] = arg_list_cov[bugcov_idx]+".cov"
    arg_list_bug = list(dma_arg_list)
    arg_list_bug[bugcov_idx] = arg_list_bug[bugcov_idx]+".bug"
    arg_list_bug.append(bug_inst_flag)
    bb_score = arg_list_bug[bugcov_idx]

    if not found_dma:
        error("can't find dma bin")
    print" ".join(arg_list_cov)
    subprocess.call(arg_list_cov)
    print" ".join(arg_list_bug)
    subprocess.call(arg_list_bug)


    if os.path.exists(src_bc) and os.path.exists(bb_score):
        opt_arg_list = ["opt", "-load" , os.path.join(dma_dir,os.path.basename("./libInsertBugPass.so")), "-InsertBug", "-i", bb_score, src_bc, "-o", final_bc_file]
        print opt_arg_list
        #call opt insert bug pass
        subprocess.call(opt_arg_list)
    else:
        print "src_bc or bb_score not exist"
        sys.exit(-1)


if __name__ == "__main__":
    # parser = argparse.ArgumentParser()
    # parser.add_argument('-b', '--bin', required=True,
    #                     help="Absolute path of the SVF dma binary")
    # args = parser.parse_args()
    run()




