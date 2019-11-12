all:
	clang -flto -Wall -g -c main.c ftp.c tty.c listparse.c cmdloop.c misc.c -lncurses
	llvm-link main.o ftp.o tty.o listparse.o cmdloop.o misc.o -o stftp.o
