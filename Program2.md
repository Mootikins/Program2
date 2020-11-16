---
title: Program 2
subtitle: Parallel Programming with OpenMPI
author: Matthew Krohn
date: 2020-11-16
pagestyle: empty
papersize: letter
geometry:
   - margin=0.5in
---

\pagenumbering{gobble}

# What is in here?

In this project are two programs, both leveraging OpenMPI -- a "Ping Pong"
program that can be used to determine some network statistics, and a distributed
implementation of [Conway's Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life).

The Ping Pong program specifically has three implementations: one using blocking
send/receive calls, one using non-blocking send/receive calls, and the last
using the combined send/receive call.

# Building the Programs

A Makefile is included, as is normal for most C projects. It includes four
targets, though three of those four are solely for the Ping Pong program.

### Ping Pong

Since the Ping Pong program includes three implementations, the three targets
correspond to each of these. The make commands for each are listed below. It
should be noted that these all compile to the same binary, so A/B testing
requires one to compile the second or third version after running the first.

- Blocking

    ```sh
    % make ping_pong_blocking
    ```
- Non-Blocking

    ```sh
    % make ping_pong_nonblocking
    ```
- Combined Send/Receive

    ```sh
    % make ping_pong_combo
    ```

The user can also change the number of round trips the ball makes between each
computer to theoretically reduce variance, as networks can be a little fickle
and erratic when you first start transmitting data. For most use cases, the
default of 500 should suffice.

```sh
% make ping_pong_blocking CFLAGS=-DPING_PONG_LIMIT=1000
```

### Game of Life

The game of life, lacking any kind of build configuration, is easy to build:

```sh
% make life
```

# Running the Programs

Since each of these are built with OpenMPI, running them in a normal way (e.g.
directly via `./ping_pong` or `./life`) could cause some issues. Because of
this, each requires one to use `mpiexec` or `mpirun`.

\newpage

### Ping Pong

Ping Pong has one argument, which is the size of the message to pass in `int`s.
Since the size of an int changes based on architecture, it is a good idea to
check, though for most modern 64bit systems an integer takes up 8 bytes.

```sh
# The number of processors must be 2
% mpiexec -n 2 ./ping_pong 8
Average ping-pong time: 0.00383249
```

The output is short and sweet; just the average time in seconds for a single
ping or pong from one processor/computer to another. Since the computers used in
this example are on the same network and are in the same location, the time is
naturally tiny.

### Game of Life

Game of life has five parameters when you do not include the number of
processors and are given in the following order:

- Number of live cells to start the game with.
- Number of iterations to play out (after the initial state).
- A modulo for iterations to print. For example, passing 3 here would print
  every third iteration or every iteration where the iteration number mod 3
  would equal 0.
- The width of the game field.
- The height of the game field.

Here it is in practice:

```sh
% mpiexec -n 5 ./life 15 10 2 5 5
Game state on iteration 0:
...
```

# Program Structure

### Ping Pong

`ping_pong.c` follows this basic structure when you ignore the preprocessor
directives:

- Initialize MPI and get information like number of processors and rank
- Check command line arguments
- Allocate memory for the buffer to send data back and forth
- For each iteration (ping-pong round trip)
    - Either send and then receive or vice versa depending on if the given
      processor is "serving" or "receiving", in ping pong lingo. How this is
      done varies based on which implementation is being used.
    - Record timing data for the round trip
- Print the average one-way travel time in seconds for the messages.
- Free memory (an additional buffer may be freed if using the combination
  send/receive)

The meat and potatoes for this is the sending and receiving portion, which
changes based on each implementation.

#### MPI Calls

Depending on the implementation, one of three types of MPI calls are made:

- Blocking calls that stop the program until the message is sent/received
  (`MPI_Send` and `MPI_Recv`).
- Non-blocking calls that in most other circumstances allow for other
  calculations to be done while the message is being transferred over the
  network (`MPI_Isend` and `MPI_Irecv`). These also require the use of some
  extra variables and calls:
  
    - `MPI_Request` which gives us info about the request that is in transit
    - `MPI_Status` which we can use to get the actual status of the request
    - `MPI_Wait` which we use with the two preceding to ensure the message was
      sent and received correctly.
      
  In our use case for these, they all effectively work in the same fashion as
  the `MPI_Send` and `MPI_Recv` calls since we use `MPI_Wait` to block the
  processor immediately after the send/receive call.
- A combination call that uses two buffers, `MPI_Sendrecv`. This is blocking but
  allows us to use the `MPI_Status` from before to check the status of both the
  sent and received message.
  
  In our use here, this is almost like having both players serve a ball at the
  same time rather than just sending or returning the message.

### Game of Life

Here is the basic structure for the Game of Life:

- Initialize MPI and get processor information
- Read command line arguments
- Allocate memory for two copies of the game field (one to read as the last
  state, and one to write the new state to) as well as some information we use
  to track which processor is working on which part of the board
- Build our game board and determine how we are going to split up the work
  between all the processors, sending this information out to all workers
- For each iteration:
    - Send out the last game state and print it if needed
    - Each processor/worker generates their part of the game field and sends it
      to the master processor
    - The master processor combines all the parts received from the other
      workers

#### MPI Calls

While I could have used some more advanced MPI calls like `MPI_Bcast`,
`MPI_Scatterv`, and `MPI_Gatherv`, I had already thought about the program in
terms of just send and receives with some pointer manipulation, so I opted to
not complicated things further. If this were performance critical or we were
timing it, I would have probably opted to change them.

Of note though, I did use a few `MPI_Barrier` calls to ensure all the processors
did not move too far ahead.

# Performance analysis for Ping Pong

Since we are only truly concerned about performance for ping pong in the context
of $\lambda + \frac{n}{B}$, we have some statistics shown below:

| Send/Receive method   | Message size in bytes | One-Way Transfer Time (s) |
| :----:                | :----:                | :----:                    |
| `MPI_Send/MPI_Recv`   | 8                     | 0.00101179                |
| `MPI_Send/MPI_Recv`   | 128                   | 0.00093873                |
| `MPI_Send/MPI_Recv`   | 1024                  | 0.00061194                |
| `MPI_Send/MPI_Recv`   | 4096                  | 0.00056587                |
| `MPI_Isend/MPI_Irecv` | 8                     | 0.00082576                |
| `MPI_Isend/MPI_Irecv` | 128                   | 0.00073015                |
| `MPI_Isend/MPI_Irecv` | 1024                  | 0.00050707                |
| `MPI_Isend/MPI_Irecv` | 4096                  | 0.00074688                |
| `MPI_Sendrecv`        | 8                     | 0.00085374                |
| `MPI_Sendrecv`        | 128                   | 0.00051087                |
| `MPI_Sendrecv`        | 1024                  | 0.00068670                |
| `MPI_Sendrecv`        | 4096                  | 0.00035657                |

Performing a linear regression on each set of data gives us the following three
equations:

- `MPI_Send/MPI_Recv`: $y = \frac{-1.04}{10935856}x + 0.00091$
- `MPI_Isend/MPI_Irecv`: $y = \frac{-0.01}{10935856}x + 0.0007$
- `MPI_Sendrecv`: $y = \frac{-0.93}{10935856}x + 0.00071$

Now, while this data does give us some indication that our $\lambda$, or
latency, is around 0.7ms, our bandwidth is kind of erratic. I will attest this
to a certain user with ID 7442451 who had their `pingPong` program running
throughout my writing and testing.

Given the number of other programs that were also running under other user IDs
that also had high CPU times, I'd say that these results (at least for the
bandwidth) are almost meaningless.

# Conclusion

OpenMPI is cool, though I hope I am never put into a position where I have to
combine it with CUDA. I would not mind using it in conjunction with OpenMP, though,
as I find CUDA to definitely be the least enjoyable of the three.
