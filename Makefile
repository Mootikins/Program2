all: ping_pong game_of_life

ping_pong: src/ping_pong.c
	mpicc -g -Wall -o $@ $^ -lm

game_of_life: src/game_of_life.c
	mpicc -g -Wall -o $@ $^ -lm
