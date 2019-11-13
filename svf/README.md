# SVF

## Note
SVF requires llvm-4.0.0

## How to compile
```
cd $PATH_TO_SVF
mkdir Release-build
cd Release-build
export LLVM_DIR=$PATH_TO_LLVM
export PATH=$LLVM_DIR/bin:$PATH
cmake ../
make -j4
```

## How to use dma

### Directly from the binary
```
cd Release-build/bin

dma -o /path/to/output/file -[Point-to-scheme] -edge out.edge -o out.reach /path/to/bitcode/file 
e.g., dma -o test.test -fspta -edge minigzip.edge -o minigzip.reach.cov ~/test_bc/minigzip.bc
```

### or using the wrapper
```
dma_wrapper.py -ander -o tcpdump.reach -edge tcpdump.edge savior-tcpdump.bc
```

Point-to-scheme
```
-nander - use Standard inclusion-based pointer to analysis
-lander - use Lazy cycle detection inclusion-based pointer to analysis 
-wander - use Wave propagation inclusion-based pointer to analysis
-ander - use Diff wave propagation inclusion-based pointer to analysis
-fspta	- use Sparse flow sensitive pointer to analysis
```
