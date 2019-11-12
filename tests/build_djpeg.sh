#! /bin/bash

# get source code
wget https://www.ijg.org/files/jpegsrc.v9c.tar.gz
tar zxf jpegsrc.v9c.tar.gz

pushd jpeg-9c

# generate whole program bc
CC=wllvm LLVM_COMPILER=clang CFLAGS="-fsanitize=integer,bounds,shift -g" ./configure  --enable-shared=no --enable-static=yes
LLVM_COMPILER=clang make -j$(nproc)

# extract bc
extract-bc djpeg

#set up fuzzing work dir
mkdir obj-savior && pushd obj-savior && cp ../djpeg.bc .

#generate binary to be fuzzed and target bc to be analyzed
~/work/savior/AFL/afl-clang-fast djpeg.bc -o savior-djpeg -lubsan -lm

#run svf analyzer (llvm-4.0) on the target bc
~/work/savior/svf/SVF/Release-build/bin/dma -fspta savior-djpeg.bc -savior-label-only -o djpeg.reach.bug -edge djpeg.edge

#run insertbug pass to generate bc runnable by llvm-3.6 (required by klee) with bug coverage infomation
opt -load /root/work/savior/svf/InsertBugPotential/build/insertpass/libInsertBugPass.so -InsertBug -i djpeg.reach.bug savior-djpeg.bc -o savior-djpeg.dma.bc

#NOTE: the above 2 steps can be done with dma_wrapper.py in single step
#
# python dma_wrapper.py -fspta PROG.bc -o PROG.reach -edge PROG.edge
#
# running the above command give the following output files
# -PROG.edge file records each basic block and its outgoing edge IDs
# -PROG.reach.cov file records each BB ID and how many basic blocks it can reach
# -PROG.reach.bug file records each BB ID and how many Sanitizer Lbales it can reach
# -PROG.dma.bc final BC file runnable by klee with bug coverage infomation encoded

echo "Preparation done, please edit the config file and prepare the seeding inputs for fuzzing"
cp ~/work/savior/coordinator/fuzz.cfg.template fuzz.cfg
cp -a ~/work/savior/AFL/testcases/images/jpeg/ in
echo "target direction: jpeg-9c/obj-savior"
echo "config template: jpeg-9c/obj-savior/fuzz.cfg"
