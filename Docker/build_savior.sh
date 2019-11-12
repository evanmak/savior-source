#!/bin/bash
PROJ=savior
SOFTWARE_DIR=~/softwares
WORK_DIR=~/work/

function dir_check {
    if [ -d $2 ]
    then
        echo "[DIRECTORY CHECK]" $1 " pass"
    else
        echo "[DIRECTORY CHECK] Error: missing "$1
        exit -1
    fi
}
################Some sanity check, everything we need is in the WORK_DIR/savior directory
    dir_check $PROJ       $WORK_DIR/$PROJ
    dir_check AFL         $WORK_DIR/$PROJ/AFL
    dir_check svf         $WORK_DIR/$PROJ/svf
    # dir_check coordinator $WORK_DIR/$PROJ/coordinator
    dir_check KLEE        $WORK_DIR/$PROJ/KLEE
    dir_check uclibc++    $WORK_DIR/$PROJ/klee-uclibc++
    dir_check patches     $WORK_DIR/$PROJ/patches


################Download packages, might need to manually compile and install

    rm -rf $SOFTWARE_DIR
    mkdir $SOFTWARE_DIR

    PROG=cmake
    echo "Downloading $PROG"
    cd $SOFTWARE_DIR
    wget https://cmake.org/files/v3.7/cmake-3.7.2.tar.gz

    PROG=llvm-3.6
    echo "Downloading $PROG"
    cd $SOFTWARE_DIR
    wget http://releases.llvm.org/3.6.0/clang-tools-extra-3.6.0.src.tar.xz
    wget http://releases.llvm.org/3.6.0/compiler-rt-3.6.0.src.tar.xz
    wget http://releases.llvm.org/3.6.0/llvm-3.6.0.src.tar.xz
    wget http://releases.llvm.org/3.6.0/cfe-3.6.0.src.tar.xz

    PROG=llvm-4.0
    echo "Downloading $PROG"
    cd $SOFTWARE_DIR
    wget http://releases.llvm.org/4.0.0/clang-tools-extra-4.0.0.src.tar.xz
    wget http://releases.llvm.org/4.0.0/compiler-rt-4.0.0.src.tar.xz
    wget http://releases.llvm.org/4.0.0/llvm-4.0.0.src.tar.xz
    wget http://releases.llvm.org/4.0.0/cfe-4.0.0.src.tar.xz

    PROG=stp
    echo "Downloading $PROG"
    cd $SOFTWARE_DIR
    git clone https://github.com/stp/stp.git


    PROG=minisat
    echo "Downloading $PROG"
    cd $SOFTWARE_DIR
    git clone https://github.com/stp/minisat.git || exit -1

    PROG=z3
    echo "Downloading $PROG"
    git clone https://github.com/Z3Prover/z3.git $SOFTWARE_DIR/z3

    PROG=uclibc
    echo "Downloading $PROG"
    cd $SOFTWARE_DIR
    git clone https://github.com/klee/klee-uclibc.git


    echo "Download finished"
    echo "Now go to $SOFTWARE_DIR and compile then install them"
    cd $SOFTWARE_DIR

################Installation script

    PROG=cmake-3.7.2
    echo "Installing $PROG"
    cd $SOFTWARE_DIR
    tar zxvf cmake-3.7.2.tar.gz
    cd $SOFTWARE_DIR/$PROG
    ./configure
    make -j8
    make install

    apt-get install -y zlib1g-dev libncurses5-dev
    apt-get install -y build-essential libssl-dev libcurl4-gnutls-dev libexpat1-dev gettext unzip vim

    PROG=llvm-3.6
    cd $SOFTWARE_DIR
    echo "Installing $PROG"
    tar xvf cfe-3.6.0.src.tar.xz
    tar xvf llvm-3.6.0.src.tar.xz
    tar xvf compiler-rt-3.6.0.src.tar.xz
    tar xvf clang-tools-extra-3.6.0.src.tar.xz

    mv llvm-3.6.0.src llvm-3.6
    mv cfe-3.6.0.src llvm-3.6/tools/clang
    mv clang-tools-extra-3.6.0.src llvm-3.6/tools/clang/extra
    mv compiler-rt-3.6.0.src llvm-3.6/projects/compiler-rt

    #patch clang for sanitizer metadata
    patch -p0 < $WORK_DIR/savior/patches/clang-3.6.patch

    cd $SOFTWARE_DIR/llvm-3.6
    mkdir build
    cd $SOFTWARE_DIR/llvm-3.6/build
    cmake -DLLVM_ENABLE_RTTI:BOOL=ON ..
    make install -j64

    PROG=KLEE
    apt-get install -y build-essential curl libcap-dev libncurses5-dev python-minimal unzip
    apt-get install -y bison flex libboost-all-dev python perl zlib1g-dev

    #installing minisat
    cd $SOFTWARE_DIR/minisat
    mkdir build
    cd build
    cmake -DSTATIC_BINARIES=ON -DCMAKE_INSTALL_PREFIX=/usr/local/ ../
    make install

    #installing stp solver
    cd ../..
    cd stp
    # git checkout tags/2.1.2
    mkdir build
    cd build
    cmake -DBUILD_SHARED_LIBS:BOOL=OFF -DENABLE_PYTHON_INTERFACE:BOOL=OFF ..
    make -j16
    make install

    cd ..
    ulimit -s unlimited
    sed -e "\$a$(whoami)\t\thard\tstack\tunlimited" /etc/security/limits.conf > /tmp/.limits.conf
    mv /tmp/.limits.conf /etc/security/limits.conf

    #installing uclibc
    cd $SOFTWARE_DIR/klee-uclibc
    ./configure --make-llvm-lib
    make -j16
    cd ..

    #installing uclibc++
    cd $WORK_DIR/savior/klee-uclibc++
    ./configure --with-llvm-build-dir=/usr/local/ --with-klee-src-dir=$WORK_DIR/savior/KLEE
    make -j16
    cd ..

    #installing tcmalloc
    apt-get install -y libtcmalloc-minimal4 libgoogle-perftools-dev

    #installing z3 solver
    cd $SOFTWARE_DIR/z3
    python scripts/mk_make.py
    cd build
    make -j$(nproc)
    make install

    #TODO: open source KLEE concolic executor separately
    #installing klee-3.6
    rm -rf /root/savior/KLEE/klee-build
    cd ~/work/savior/KLEE
    mkdir klee-build
    cd klee-build
    echo "NOTE: you might need to rebuild libboost for C++ ABI compatibility on Ubuntu 16.04"
    cmake -DLLVM_CONFIG_BINARY=/usr/local/bin/llvm-config -DENABLE_SOLVER_STP=ON -DENABLE_SOLVER_Z3=ON -DENABLE_POSIX_RUNTIME=ON -DENABLE_KLEE_UCLIBCXX=ON -DKLEE_UCLIBCXX_PATH=$WORK_DIR/savior/klee-uclibc++ -DENABLE_KLEE_UCLIBC=ON -DKLEE_UCLIBC_PATH=$SOFTWARE_DIR/klee-uclibc  -DENABLE_UNIT_TESTS=OFF -DENABLE_SYSTEM_TESTS=OFF -DENABLE_TCMALLOC=ON ..
    make -j$(nproc)
    make install

    apt-get install -y zlib1g-dev libncurses5-dev libdbus-1-3
    apt-get install -y build-essential libssl-dev libcurl4-gnutls-dev libexpat1-dev gettext unzip

    PROG=afl
    echo "Installing $PROG"
    cd $WORK_DIR/$PROJ/AFL
    make
    cd $WORK_DIR/$PROJ/AFL/llvm_mode
    make

    #install llvm-4.0
    PROG=llvm-4.0
    cd $SOFTWARE_DIR
    tar xvf cfe-4.0.0.src.tar.xz
    tar xvf llvm-4.0.0.src.tar.xz
    tar xvf compiler-rt-4.0.0.src.tar.xz
    tar xvf clang-tools-extra-4.0.0.src.tar.xz

    mv llvm-4.0.0.src llvm-4.0
    mv cfe-4.0.0.src llvm-4.0/tools/clang
    mv clang-tools-extra-4.0.0.src llvm-4.0/tools/clang/extra
    mv compiler-rt-4.0.0.src llvm-4.0/projects/compiler-rt

    cd $SOFTWARE_DIR/llvm-4.0
    mkdir build
    mkdir install
    cd $SOFTWARE_DIR/llvm-4.0/build
    cmake -DLLVM_ENABLE_RTTI:BOOL=ON -DCMAKE_INSTALL_PREFIX=$SOFTWARE_DIR/llvm-4.0/install ..
    make install -j64

    #install svf
    PROG=svf
    
    #build insertbug pass with llvm-3.6 first
    cd $WORK_DIR/$PROJ/svf/InsertBugPotential
    mkdir build && cd build && cmake .. && make -j$(nproc)
    ln -s $(pwd)/insertpass/libInsertBugPass.so $(pwd)/../../SVF/libInsertBugPass.so

    cd $WORK_DIR/$PROJ/svf/SVF
    export LLVM_DIR=$SOFTWARE_DIR/llvm-4.0/install
    export PATH=$LLVM_DIR/bin:$PATH
    mkdir Release-build
    cd Release-build
    cmake ..
    make -j$(nproc)
    cd .. && ln -sf $(pwd)/Release-build/bin/dma_wrapper.py dma_wrapper.py

