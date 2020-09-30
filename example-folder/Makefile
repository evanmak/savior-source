## Variable storing paths
SAVIOR_PATH=${HOME}/work/savior

all: seed_folder

example : example.c
	LLVM_COMPILER=clang wllvm -fsanitize=integer,bounds,shift -g -o example example.c

example.bc : example
	extract-bc example

# and labelmap.csv  locmap.csv  paired_edges.csv
savior-example :  example.bc
	${SAVIOR_PATH}/AFL/afl-clang-fast example.bc -o savior-example -lubsan -lm

# and savior-example.dma example.reach.bug example.edge
savior-example.bc : savior-example
	${SAVIOR_PATH}/svf/SVF/Release-build/bin/dma -fspta savior-example.bc -savior-label-only -o example.reach.bug -edge example.edge

savior-example.dma.bc : savior-example.bc
	opt -load ${SAVIOR_PATH}/svf/InsertBugPotential/build/insertpass/libInsertBugPass.so -InsertBug -i example.reach.bug savior-example.bc -o savior-example.dma.bc

example.conf : savior-example.dma.bc
	./set-configuration.sh

seed_folder :  example.conf
	mkdir -p "seed_folder" && echo "a" > seed_folder/seed1.txt

# and run: python ../savior/coordinator/moriarty.py -t $(pwd) -c $(pwd)/example.conf

clean:
	rm example *.o *.o.bc *.bc savior-example savior-example.dma example.conf example.edge example.reach.bug labelmap.csv locmap.csv paired_edges.csv .savior_sanitizer_combination .afl_coverage_combination
