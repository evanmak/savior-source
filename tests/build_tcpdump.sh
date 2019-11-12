#! /bin/bash

# get source code
git clone https://github.com/the-tcpdump-group/tcpdump.git
git clone https://github.com/the-tcpdump-group/libpcap.git

pushd libpcap
# generate whole program bc
CC=wllvm LLVM_COMPILER=clang CFLAGS="-fsanitize=integer,bounds,shift -g" ./configure  --enable-shared=no
sed -i -e '/-fpic/ s/pic/PIC/' Makefile
LLVM_COMPILER=clang make -j$(nproc)

popd
pushd tcpdump
CC=wllvm LLVM_COMPILER=clang CFLAGS="-fsanitize=integer,shift,bounds  -O3 -g" LDFLAGS="-lubsan" ./configure
LLVM_COMPILER=clang make -j$(nproc)

# extract bc
extract-bc tcpdump

#set up fuzzing work dir
mkdir obj-savior && pushd obj-savior && cp ../tcpdump.bc .

#generate binary to be fuzzed and target bc to be analyzed
apt-get install -y libdbus-1-dev
~/work/savior/AFL/afl-clang-fast tcpdump.bc -o savior-tcpdump -lcrypto -libverbs  -ldbus-1 -lubsan

#run svf analyzer (llvm-4.0) on the target bc
~/work/savior/svf/SVF/Release-build/bin/dma -fspta savior-tcpdump.bc -savior-label-only -o tcpdump.reach.bug -edge tcpdump.edge

#run insertbug pass to generate bc runnable by llvm-3.6 (required by klee) with bug coverage infomation
opt -load /root/work/savior/svf/InsertBugPotential/build/insertpass/libInsertBugPass.so -InsertBug -i tcpdump.reach.bug savior-tcpdump.bc -o savior-tcpdump.dma.bc

#NOTE: the above 2 steps can be done with dma_wrapper.py in single step
#
# python dma_wrapper.py -fspta PROG.bc -o PROG.reach -edge PROG.edge
#
# running the above command give the following output files
# -PROG.edge file records each basic block and its outgoing edge IDs
# -PROG.reach.cov file records each BB ID and how many basic blocks it can reach
# -PROG.reach.bug file records each BB ID and how many Sanitizer Lbales it can reach
# -PROG.dma.bc final BC file runnable by klee with bug coverage infomation encoded

popd
echo "Preparation done, please edit the config file and prepare the seeding inputs for fuzzing"
cp ~/work/savior/coordinator/fuzz.cfg.template fuzz.cfg
mkdir -p tcpdump/obj-savior/in &&  cp tcpdump/tests/02-sunrise-sunset-esp.pcap tcpdump/obj-savior/in
echo "target direction: tcpdump/obj-savior"
echo "config template: tcpdump/obj-savior/fuzz.cfg"
