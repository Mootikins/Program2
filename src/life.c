/* File:    life.c
 *
 * Compile: mpicc -g -Wall -o life life.c -lm
 * Run:     ./life i j k m n
 * Input:   i is the number of live cells
 *          j is the number of iterations of the game of life
 *          k is how often to print the game state (eg every kth iteration)
 *          m is the game width
 *          n is the game height
 * Output:  Each game state at iterations that are a multiple of k
 */

#include <limits.h>
#include <math.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum MessageTag
{
	UPDATE_WORLD,
	PROC_DATA,
	BUILT_GAME_STATE,
};

typedef struct ProcInfo
{
	int offset;
	int num_cells;
} ProcInfo;

void apply_rules( ProcInfo* proc_data, bool* last_game_state,
                  bool* new_game_state, int world_rank, int* adjacency_offsets,
                  int width, int height );
void read_args( int argc, char* argv[], int* live_cells, int* iterations,
                int* print_modulo, int* width, int* height, int proc_id );
void fill_game_field( bool* field, int width, int height, int live_cells );
void print_game( bool* game_field, int width, int height );

int main( int argc, char* argv[] )
{
	// Fixes our args so that they match if we were running a "normal" program
	MPI_Init( &argc, &argv );

	int world_rank, world_size;
	MPI_Comm_rank( MPI_COMM_WORLD, &world_rank );
	MPI_Comm_size( MPI_COMM_WORLD, &world_size );

	// Read in our args -- easiest to just populate them in the function call.
	// The world_rank is used to only print the usage statement once
	int live_cells, iterations, print_modulo, width, height;
	read_args( argc, argv, &live_cells, &iterations, &print_modulo, &width,
	           &height, world_rank );

	// Game and related information allocation
	bool* last_game_state = calloc( width * height, sizeof( bool ) );
	bool* new_game_state = calloc( width * height, sizeof( bool ) );
	ProcInfo* proc_data = calloc( world_size, sizeof( ProcInfo ) );

	// Adjacency offsets makes it easier to count cells nearby; we don't include
	// the offset of zero since the current cell's status is only used to
	// determine what rule to use
	const int adjacency_offsets[] = {
	  -( width + 1 ), -width, -( width - 1 ), -1, 1,
	  width - 1,      width,  width + 1 };

	// Processor with id 0 will be our controller -- the remaining processors
	// will just be used to do work. Once they all finish their jobs for an
	// iteration, we only need to collect them in the controller and apply the
	// data to the new game state, which will then be distributed again.
	if ( world_rank == 0 )
	{
		// Fill up our game field if we are on the "master" processor
		fill_game_field( last_game_state, width, height, live_cells );

		// Calculate each processor's offset and number of cells
		int normal_num_cells = floor( (double)width * height / world_size );
		int overflow_cells = width * height % world_size;

		// Setup our offset/cell data for each processor
		for ( int i = 0; i < world_size - 1; ++i )
		{
			proc_data[i].offset = i * normal_num_cells;
			proc_data[i].num_cells = normal_num_cells;
		}

		// To make things as simple as possible, the last processor gets the
		// remainder since we can't give processors parts of cells.
		proc_data[world_size - 1].offset = normal_num_cells * ( world_size - 1 );
		proc_data[world_size - 1].num_cells = normal_num_cells + overflow_cells;

		for ( int proc = 1; proc < world_size; proc++ )
		{
			// While we could go through the headache of making a custom type for
			// MPI, we know that our ProcInfo struct is stored in memory as just
			// two ints; this means that we effectively are just sending an array
			// of 2 * world_size ints. Once it gets received, the compiler knows
			// that the data we got is of the same struct type (because that is
			// the type of the variable it is stored in). This means that we can
			// avoid this extra step.
			//
			// If we were using two variables of different types/sizes, we would
			// need to go through the process of defining a separate type for MPI
			MPI_Send( proc_data, 2 * world_size, MPI_INT, proc, PROC_DATA,
			          MPI_COMM_WORLD );
		}
	}
	else
	{
		MPI_Recv( proc_data, 2 * world_size, MPI_INT, 0, PROC_DATA, MPI_COMM_WORLD,
		          MPI_STATUS_IGNORE );
	}

	MPI_Barrier( MPI_COMM_WORLD );

	// Now for the actual loop
	// We use iterations + 1 since we are not including the base state
	for ( int iteration = 0; iteration < iterations + 1; ++iteration )
	{
		if ( world_rank == 0 )
		{
			// Only print on every nth iteration, which mod makes easy
			if ( iteration % print_modulo == 0 )
			{
				printf( "Game state on iteration %d:\n", iteration );
				print_game( last_game_state, width, height );
			}

			// Send the last iteration's game state to all of the other processors
			for ( int proc = 1; proc < world_size; proc++ )
			{
				MPI_Send( last_game_state, width * height, MPI_C_BOOL, proc,
				          UPDATE_WORLD, MPI_COMM_WORLD );
			}

			apply_rules( proc_data, last_game_state, new_game_state, world_rank,
			             adjacency_offsets, width, height );

			for ( int proc = 1; proc < world_size; ++proc )
			{
				MPI_Recv( new_game_state + proc_data[proc].offset,
				          proc_data[proc].num_cells, MPI_C_BOOL, proc, BUILT_GAME_STATE,
				          MPI_COMM_WORLD, MPI_STATUS_IGNORE );
			}

			// Easiest to just swap the pointers on every iteration since we have to
			// give each processor the newly updated game state anyways
			bool* temp_game_state = last_game_state;
			last_game_state = new_game_state;
			new_game_state = temp_game_state;
		}
		else
		{
			// Receive our game state
			MPI_Recv( last_game_state, width * height, MPI_C_BOOL, 0, UPDATE_WORLD,
			          MPI_COMM_WORLD, MPI_STATUS_IGNORE );

			apply_rules( proc_data, last_game_state, new_game_state, world_rank,
			             adjacency_offsets, width, height );

			MPI_Send( new_game_state + proc_data[world_rank].offset,
			          proc_data[world_rank].num_cells, MPI_C_BOOL, 0,
			          BUILT_GAME_STATE, MPI_COMM_WORLD );
		}
		MPI_Barrier( MPI_COMM_WORLD );
	}

	// Don't forget to clean up
	free( last_game_state );
	free( new_game_state );
	free( proc_data );

	MPI_Finalize();
}

void apply_rules( ProcInfo* proc_data, bool* last_game_state,
                  bool* new_game_state, int world_rank, int* adjacency_offsets,
                  int width, int height )
{
	for ( int idx = 0; idx < proc_data[world_rank].num_cells; ++idx )
	{
		int curr_cell_index = proc_data[world_rank].offset + idx;
		int alive_adj_cells = 0;
		for ( int adj_ind = 0; adj_ind < 8; ++adj_ind )
		{
			int offset = adjacency_offsets[adj_ind];
			// Check the following:
			//     Current adjacent cell is not off the top or bottom edges
			//     Current adjacent cell is not off the left or right edges
			//     Current adjacent cell is alive
			//
			//      Top Edge
			if ( curr_cell_index + offset >= 0 &&
			     // Bottom edge
			     curr_cell_index + offset < width * height &&
			     // Left and Right edges
			     // If the left/right adjacent cells overflow to another
			     // row, then the differnce between their column numbers
			     // is greater than 1 -- otherwise it is 1 or 0
			     abs( ( curr_cell_index % width ) -
			          ( ( curr_cell_index + offset ) % width ) ) <= 1 &&
			     // Currently checked adjacent cell is alive
			     last_game_state[curr_cell_index + offset] )
			{
				alive_adj_cells++;
			}
		}

		if ( last_game_state[curr_cell_index] )
		{
			if ( alive_adj_cells == 2 || alive_adj_cells == 3 )
				new_game_state[curr_cell_index] = true;
			else
				new_game_state[curr_cell_index] = false;
		}
		else
		{
			if ( alive_adj_cells == 3 )
				new_game_state[curr_cell_index] = true;
			else
				new_game_state[curr_cell_index] = false;
		}
	}
}

void print_game( bool* game_field, int width, int height )
{
	for ( int i = 0; i < height; ++i )
	{
		for ( int j = 0; j < width; ++j )
		{
			if ( game_field[i * width + j] )
			{
				printf( "█" );
			}
			else
			{
				printf( " " );
			}
		}
		printf( "\n" );
	}
}

void fill_game_field( bool* field, int width, int height, int live_cells )
{
	int cells_to_generate = live_cells;
	srand( time( NULL ) );
	for ( int i = 0; i < width * height; ++i )
	{
		// Fill the cell if we have to fill the rest of the cells
		// or we can generate it randomly
		if ( cells_to_generate > 0 &&
		     ( width * height - ( i + 1 ) == cells_to_generate ||
		       rand() % ( width * height ) < live_cells ) )
		{
			field[i] = true;
			cells_to_generate--;
		}
	}
}

void read_args( int argc, char* argv[], int* live_cells, int* iterations,
                int* print_modulo, int* width, int* height, int proc_id )
{
	if ( argc != 6 && proc_id == 0 )
	{
		fprintf(
		  stderr,
		  "USAGE: ./%s i j k m n\n"
		  "\ti is the number of live cells\n"
		  "\tj is the number of iterations of the game of life\n"
		  "\tk is how often to print the game state (eg every kth iteration)\n"
		  "\tm is the game width\n"
		  "\tn is the game height\n",
		  argv[0] );
		MPI_Abort( MPI_COMM_WORLD, 1 );
	}

	*live_cells = strtol( argv[1], NULL, 10 );
	*iterations = strtol( argv[2], NULL, 10 );
	*print_modulo = strtol( argv[3], NULL, 10 );
	*width = strtol( argv[4], NULL, 10 );
	*height = strtol( argv[5], NULL, 10 );
}
