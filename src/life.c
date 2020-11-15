#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool read_args( int argc, char *argv[], int *live_cells, int *iterations,
                int *print_modulo, int *width, int *height );

int main( int argc, char *argv[] )
{
	MPI_Init( &argc, &argv );

	int world_rank, world_size;
	MPI_Comm_rank( MPI_COMM_WORLD, &world_rank );
	MPI_Comm_size( MPI_COMM_WORLD, &world_size );

	int live_cells, iterations, print_modulo, width, height;
	if ( world_rank == 0 )
	{
		read_args( argc, argv, &live_cells, &iterations, &print_modulo, &width,
		           &height );
	}

	MPI_Finalize();
}

bool read_args( int argc, char *argv[], int *live_cells, int *iterations,
                int *print_modulo, int *width, int *height )
{
	if ( argc != 6 )
	{
		fprintf( stderr,
		         "USAGE: %s i j k m n\n\
	i is the number of live cells\n\
	j is the number of iterations of the game of life\n\
	k is how often to print the game state (eg every kth iteration)\n\
	m is the game width\n\
	n is the game height\n",
		         argv[0] );
		return false;
	}

	*live_cells = strtol( argv[1], NULL, 10 );
	*iterations = strtol( argv[2], NULL, 10 );
	*print_modulo = strtol( argv[3], NULL, 10 );
	*width = strtol( argv[4], NULL, 10 );
	*height = strtol( argv[5], NULL, 10 );

	return true;
}
