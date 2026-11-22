#pragma once
#include "Board.h"
#include <chrono>
#include <cstdint>
#include <vector>

namespace checkers {
struct SearchLimits {
    int maxDepth{6};
    int timeLimitMs{1500}; // 0 = no time limit
};

struct SearchResult {
    Move bestMove{};
    bool hasMove{false};
    int score{0}; // centipawn-like units, from the root side's perspective
    int completedDepth{0};
    std::uint64_t nodes{0};
    std::uint64_t qnodes{0};
    std::uint64_t elapsedMs{0};
    std::uint64_t nodesPerSecond{0};
};

class AI {
public:
    explicit AI(std::size_t transpositionEntries = 32768);
    SearchResult findBestMove(const Board& board, Side side, const SearchLimits& limits);
    void clearTranspositionTable();

private:
    enum class Bound : std::uint8_t { Exact, Lower, Upper };
    struct TTEntry {
        std::uint64_t key{0};
        Move best{};
        std::int16_t score{0};
        std::int8_t depth{-1};
        Bound bound{Bound::Exact};
        bool occupied{false};
    };

    std::vector<TTEntry> tt_;
    std::array<std::vector<Move>, 96> moveBuffers_{};
    std::uint64_t nodes_{0};
    std::uint64_t qnodes_{0};
    bool stopped_{false};
    std::chrono::steady_clock::time_point deadline_{};
    bool useDeadline_{false};
    std::array<std::array<std::int16_t, PlayableSquares>, 2> history_{};

    int alphaBeta(Board& board, Side side, int depth, int alpha, int beta, int ply);
    int quiescence(Board& board, Side side, int alpha, int beta, int ply, int qdepth = 0);
    int evaluate(const Board& board, Side perspective) const noexcept;
    std::uint64_t hash(const Board& board, Side side) const noexcept;
    bool timeExpired() noexcept;
    void orderMoves(const Board& board, std::vector<Move>& moves, const Move* ttMove, Side side);
    static bool sameMove(const Move& a, const Move& b) noexcept;
    static int pieceValue(Piece p) noexcept;
    static int scoreToTable(int score, int ply) noexcept;
    static int scoreFromTable(int score, int ply) noexcept;
};
}
