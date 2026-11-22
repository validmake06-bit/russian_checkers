#include "Board.h"
#include "AI.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>

int main(int argc, char** argv) {
    checkers::Board board;
    std::cout << "Russian draughts - stage 2 engine\n" << board.toString();

    int depth = 4;
    int timeMs = 1500;
    if (argc > 1) depth = std::max(1, std::atoi(argv[1]));
    if (argc > 2) timeMs = std::max(0, std::atoi(argv[2]));

    checkers::AI ai;
    checkers::SearchLimits limits;
    limits.maxDepth = depth;
    limits.timeLimitMs = timeMs;
    const auto result = ai.findBestMove(board, checkers::Side::White, limits);

    if (!result.hasMove) {
        std::cout << "White has no legal moves.\n";
        return 0;
    }
    std::cout << "Best move: " << static_cast<int>(result.bestMove.from)
              << " -> " << static_cast<int>(result.bestMove.to())
              << (result.bestMove.isCapture ? " (capture)" : "") << '\n'
              << "Depth: " << result.completedDepth
              << ", score: " << result.score
              << ", nodes: " << result.nodes
              << ", qnodes: " << result.qnodes
              << ", time: " << result.elapsedMs << " ms"
              << ", NPS: " << result.nodesPerSecond << '\n';
    return 0;
}
