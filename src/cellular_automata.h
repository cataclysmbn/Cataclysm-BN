#pragma once

#include <vector>

#include "point.h"
#include "rng.h"

namespace CellularAutomata
{
/**
* Calculates the number of alive neighbors by looking at the Moore neighborhood (3x3 grid of cells).
* @param cells The cells to look at. Assumed to be of a consistent width/height equal to the specified
* values.
* @param size The width+height of the cells. Specified up front to avoid checked it each time.
* @param p
* @returns The number of neighbors that are alive, a value between 0 and 8.
*/
int neighbor_count( const std::vector<std::vector<int>> &cells,
                    point size,
                    point p );

/**
* Generate a cellular automaton using the provided parameters.
* Basic rules are as follows:
* - alive% of cells start alive
* - Run for specified number of iterations
* - Dead cells with > birth_limit neighbors become alive
* - Alive cells with > statis_limit neighbors stay alive
* - The rest die
* @param size Dimensions of the grid.
* @param alive Number between 0 and 100, used to roll an x_in_y check for the cell to start alive.
* @param iterations Number of iterations to run the cellular automaton.
* @param birth_limit Dead cells with > birth_limit neighbors become alive.
* @param stasis_limit Alive cells with > statis_limit neighbors stay alive
* @returns The width x height grid of cells. Each cell is a 0 if dead or a 1 if alive.
*/
std::vector<std::vector<int>> generate_cellular_automaton(
                               point size,
                               const int alive,
                               const int iterations,
                               const int birth_limit,
                               const int stasis_limit );
} // namespace CellularAutomata


