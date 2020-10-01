

## Variable storing paths
SAVIOR_PATH=$HOME/work/savior
SYNC_FOLDER=@target/output_folder
SEED_FOLDER=@target/seed_folder
# target directory: $(pwd)

sed s/PROG/example/g $SAVIOR_PATH/coordinator/fuzz.cfg.template | \
    #sed s/inputtype=stdin/inputtype=symfile/g | \
    #sed "s|target_bin=@target/savior-fuzzer|target_bin=$AFL_CMD|g" | \
    #sed "s|target_bc=@target/savior-fuzzer.dma.bc|target_bc=$KLEE_CMD|g" | \
    sed "s|sync_dir=@target/out|sync_dir=$SYNC_FOLDER|g" | \
    sed "s,in_dir=@target/in,in_dir=$SEED_FOLDER,g" | \
    sed "s|heuristics=san-guided|heuristics=san-guided:sequential:bug-potential:avg-bug-potential|g" | \
    sed "s|SAVIOR|$SAVIOR_PATH|g" > example.conf

## Note: to use uncov-edge-bug generates code_reach_map by running the dma pass without -savior-label-only option
## ref: https://github.com/evanmak/savior-source/blob/b419c868e3dd966e77bb2707a7901e6f1872fb92/coordinator/README.md
