all: mpmc_test spsc_test

mpmc_test: mpmc_test.c ../mpmc.c ../mqueue.c
	gcc -g -o $@ -I../ $^ -lpthread

spsc_test: spsc_test.c ../spsc.c ../mqueue.c
	gcc -g -o $@ -I../ $^ -lpthread
