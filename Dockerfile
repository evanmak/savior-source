From ubuntu:16.04
LABEL maintainers="Yaohui Chen"
LABEL version="0.1" 


RUN           apt-get update && apt-get install -y \
              wget            \
              git             \
              python          \  
              build-essential \
              autoconf        \
              python-pip      \
              python-dev      



RUN           pip install -U pip
RUN           pip install psutil
RUN           pip install wllvm
RUN           mkdir  -p      /root/work/savior
COPY          Docker/build_savior.sh /root/work/savior/
COPY          AFL            /root/work/savior/AFL
COPY          KLEE           /root/work/savior/KLEE         
COPY          klee-uclibc++  /root/work/savior/klee-uclibc++ 
COPY          patches        /root/work/savior/patches
COPY          svf            /root/work/savior/svf        
COPY          tests          /root/work/savior/tests

WORKDIR       /root/work/savior
RUN           cd /root/work/savior && ./build_savior.sh
COPY          coordinator    /root/work/savior/coordinator        
  
