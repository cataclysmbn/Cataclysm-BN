#include "cellular_automata.h"

int CellularAutomata::neighbor_count(
    const std::vector<std::vector<int>>& cells, point size, point p) {
    int neighbors = 0;
    for (int ni = -1; ni <= 1; ni++) {
        for (int nj = -1; nj <= 1; nj++) {
            const point n(p + point(ni, nj));

            // These neighbors are outside the bounds, so they can't contribute.
            if (n.x < 0 || n.x >= size.x || n.y < 0 || n.y >= size.y) { continue; }

            neighbors += cells[n.x][n.y];
        }
    }
    // Because we included ourself in the loop above, subtract ourselves back out.
    neighbors -= cells[p.x][p.y];

    return neighbors;
}
std::vector<std::vector<int>> CellularAutomata::generate_cellular_automaton(
    point size, const int alive, const int iterations, const int birth_limit,
    const int stasis_limit) {
    std::vector<std::vector<int>> current(size.x, std::vector<int>(size.y, 0));
    std::vector<std::vector<int>> next(size.x, std::vector<int>(size.y, 0));

    // Initialize our initial set of cells.
    for (int i = 0; i < size.x; i++) {
        for (int j = 0; j < size.y; j++) { current[i][j] = x_in_y(alive, 100); }
    }

    for (int iteration = 0; iteration < iterations; iteration++) {
        for (int i = 0; i < size.x; i++) {
            for (int j = 0; j < size.y; j++) {
                // Skip the edges--no need to complicate this with more complex neighbor
                // calculations, just keep them constant.
                if (i == 0 || i == size.x - 1 || j == 0 || j == size.y - 1) {
                    next[i][j] = 0;
                    continue;
                }

                // Count our neighors.
                const int neighbors = neighbor_count(current, size, point(i, j));

                // Dead and > birth_limit neighbors, so become alive.
                if ((current[i][j] == 0) && (neighbors > birth_limit)) {
                    next[i][j] = 1;
                }
                // Alive and > statis_limit neighbors, so stay alive.
                else if ((current[i][j] == 1) && (neighbors > stasis_limit)) {
                    next[i][j] = 1;
                }
                // Else, die.
                else {
                    next[i][j] = 0;
                }
            }
        }

        // Swap our current and next vectors and repeat.
        std::swap(current, next);
    }

    return current;
}
