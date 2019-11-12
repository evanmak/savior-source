.PHONY : all clean

objs=$(patsubst %.c, %.o, $(wildcard *.c))
lls =$(patsubst %.c, %.ll, $(wildcard *.c))


all:$(objs) $(lls)

%.o:%.c
	clang $^ -fsanitize=undefined -o $@

%.ll:%.c
	clang $^ -fsanitize=undefined -emit-llvm -S

clean:
	rm *.ll *.o