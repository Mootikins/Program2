CFLAGS=-g -Wall

ping_pong_blocking: src/ping_pong.c
	mpicc $(CFLAGS) -o ping_pong $^ -lm -DBLOCKING

ping_pong_nonblocking: src/ping_pong.c
	mpicc $(CFLAGS) -o ping_pong $^ -lm -DNONBLOCKING

ping_pong_combo: src/ping_pong.c
	mpicc $(CFLAGS) -o ping_pong $^ -lm -DCOMBINATION

life: src/life.c
	mpicc $(CFLAGS) -o $@ $^ -lm

clean:
	rm ping_pong* life
