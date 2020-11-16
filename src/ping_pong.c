/* File:    ping_pong.c
 *
 * Compile: mpicc -g -Wall -o ping_pong ping_pong.c -lm -DBLOCKING
 *          mpicc -g -Wall -o ping_pong ping_pong.c -lm -DNONBLOCKING
 *          mpicc -g -Wall -o ping_pong ping_pong.c -lm -DCOMBINATION
 *          Each one of the above will compile the program to the same binary,
 *          but with different MPI calls.
 * Run:     mpiexec -n 2 ./ping_pong m
 * Input:   m is the size of the message to send
 * Output:  The average of PING_PONG_LIMIT round trips of the "ball"
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define PING_TAG 0
#define PONG_TAG 1
#define PING_PONG_LIMIT 500

int main( int argc, char* argv[] )
{
	MPI_Init( &argc, &argv );

	int world_rank, world_size;
	MPI_Comm_rank( MPI_COMM_WORLD, &world_rank );
	MPI_Comm_size( MPI_COMM_WORLD, &world_size );
	int partner_rank = ( world_rank + 1 ) % 2;
	double elapsed_time, total_time = 0;
	int message_size;

	if ( world_size != 2 )
	{
		fprintf( stderr, "World size must be two for %s\n", argv[0] );
		MPI_Abort( MPI_COMM_WORLD, 1 );
	}

	if ( argc == 2 )
	{
		message_size = strtol( argv[1], NULL, 10 );
	}
	else
	{
		message_size = 1;
	}

	if ( message_size < 0 )
	{
		fprintf( stderr, "Message size must be greater than 0" );
		MPI_Abort( MPI_COMM_WORLD, 1 );
	}

	int* buffer = (int*)malloc( sizeof( int ) * message_size );
	for ( int i = 0; i < message_size; ++i )
	{
		buffer[i] = 0;
	}

#ifndef BLOCKING
	MPI_Request request;
	MPI_Status status;
#endif
#ifdef COMBINATION
	int* combo_buffer = (int*)malloc( sizeof( int ) * message_size );
	for ( int i = 0; i < message_size; ++i )
	{
		combo_buffer[i] = 0;
	}
#endif

	for ( int i = 0; i < PING_PONG_LIMIT; i++ )
	{
		if ( world_rank == 0 )
		{
			elapsed_time = -MPI_Wtime();
#ifdef BLOCKING
			MPI_Send( buffer, message_size, MPI_INT, partner_rank, PING_TAG,
			          MPI_COMM_WORLD );
			MPI_Recv( buffer, message_size, MPI_INT, partner_rank, PONG_TAG,
			          MPI_COMM_WORLD, MPI_STATUS_IGNORE );
#elif NONBLOCKING
			MPI_Isend( buffer, message_size, MPI_INT, partner_rank, PING_TAG,
			           MPI_COMM_WORLD, &request );
			MPI_Wait( &request, &status );
			MPI_Irecv( buffer, message_size, MPI_INT, partner_rank, PONG_TAG,
			           MPI_COMM_WORLD, &request );
			MPI_Wait( &request, &status );
#elif COMBINATION
			MPI_Sendrecv( buffer, message_size, MPI_INT, partner_rank, PING_TAG,
			              combo_buffer, message_size, MPI_INT, partner_rank, PONG_TAG,
			              MPI_COMM_WORLD, &status );
#endif
			elapsed_time += MPI_Wtime();
			total_time += elapsed_time;
		}
		else
		{
#ifdef BLOCKING
			MPI_Recv( buffer, message_size, MPI_INT, partner_rank, PING_TAG,
			          MPI_COMM_WORLD, MPI_STATUS_IGNORE );
			MPI_Send( buffer, message_size, MPI_INT, partner_rank, PONG_TAG,
			          MPI_COMM_WORLD );
#elif NONBLOCKING
			MPI_Irecv( buffer, message_size, MPI_INT, partner_rank, PING_TAG,
			           MPI_COMM_WORLD, &request );
			MPI_Wait( &request, &status );
			MPI_Isend( buffer, message_size, MPI_INT, partner_rank, PONG_TAG,
			           MPI_COMM_WORLD, &request );
			MPI_Wait( &request, &status );
#elif COMBINATION
			MPI_Sendrecv( combo_buffer, message_size, MPI_INT, partner_rank, PONG_TAG,
			              buffer, message_size, MPI_INT, partner_rank, PING_TAG,
			              MPI_COMM_WORLD, &status );
#endif
		}
	}

	if ( world_rank == 0 )
	{
		printf( "Average ping-pong time: %lf\n",
		        total_time / ( 2 * PING_PONG_LIMIT ) );
	}

	free( buffer );
#ifdef COMBINATION
	free( combo_buffer );
#endif

	MPI_Finalize();
}
