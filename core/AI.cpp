#include "AI.h"
#include <algorithm>
#include <array>
#include <limits>
#include <cstdlib>

namespace checkers {
namespace {
constexpr int Infinity = 32000;
constexpr int MateScore = 30000;
constexpr int MaxPly = 64;

std::uint64_t splitmix64(std::uint64_t& x) noexcept {
    std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

const std::array<std::array<std::uint64_t, 5>, PlayableSquares>& zobrist() {
    static const auto table = [] {
        std::array<std::array<std::uint64_t, 5>, PlayableSquares> t{};
        std::uint64_t seed = 0xC0FFEE123456789ULL;
        for (auto& row : t) for (auto& value : row) value = splitmix64(seed);
        return t;
    }();
    return table;
}

std::uint64_t sideKey() noexcept { return 0xA0761D6478BD642FULL; }


}

AI::AI(std::size_t transpositionEntries)
    : tt_(std::max<std::size_t>(1, transpositionEntries)) {
    for (auto& buffer : moveBuffers_) buffer.reserve(256);
}

void AI::clearTranspositionTable() {
    std::fill(tt_.begin(), tt_.end(), TTEntry{});
}

int AI::pieceValue(Piece p) noexcept {
    switch (p) {
        case Piece::WhiteMan: case Piece::BlackMan: return 100;
        case Piece::WhiteKing: case Piece::BlackKing: return 320;
        default: return 0;
    }
}

bool AI::sameMove(const Move& a, const Move& b) noexcept {
    if (!a.valid() || !b.valid() || a.from != b.from || a.length != b.length || a.isCapture != b.isCapture) return false;
    for (std::uint8_t i = 0; i < a.length; ++i)
        if (a.landings[i] != b.landings[i] || (a.isCapture && a.captured[i] != b.captured[i])) return false;
    return true;
}

int AI::scoreToTable(int score, int ply) noexcept {
    if (score > MateScore - MaxPly) return score + ply;
    if (score < -MateScore + MaxPly) return score - ply;
    return score;
}
int AI::scoreFromTable(int score, int ply) noexcept {
    if (score > MateScore - MaxPly) return score - ply;
    if (score < -MateScore + MaxPly) return score + ply;
    return score;
}

std::uint64_t AI::hash(const Board& board, Side side) const noexcept {
    std::uint64_t key = side == Side::Black ? sideKey() : 0;
    const auto& z = zobrist();
    for (int i = 0; i < PlayableSquares; ++i) {
        const auto p = static_cast<std::uint8_t>(board.cells()[static_cast<std::size_t>(i)]);
        if (p != 0) key ^= z[static_cast<std::size_t>(i)][p];
    }
    return key;
}

bool AI::timeExpired() noexcept {
    if (!useDeadline_ || stopped_) return stopped_;
    if (((nodes_ + qnodes_) & 1023ULL) == 0 && std::chrono::steady_clock::now() >= deadline_) stopped_ = true;
    return stopped_;
}

void AI::orderMoves(const Board& board, std::vector<Move>& moves, const Move* ttMove, Side side) {
    // Capture moves are generated first by the board. This score promotes TT and
    // high-value captures while retaining a cheap history heuristic for quiet moves.
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        auto score = [&](const Move& m) {
            if (ttMove && sameMove(m, *ttMove)) return 10000000;
            if (m.isCapture) {
                int tactical = static_cast<int>(m.length) * 1000;
                for (std::uint8_t i = 0; i < m.length; ++i)
                    tactical += pieceValue(board.at(m.captured[i]));
                return 100000 + tactical;
            }
            const int from = static_cast<unsigned char>(m.from);
            const int to = static_cast<unsigned char>(m.to());
            return static_cast<int>(history_[static_cast<std::size_t>(side)][static_cast<std::size_t>(to)]) + (to / 4 - from / 4) * (side == Side::White ? -2 : 2);
        };
        return score(a) > score(b);
    });
}

int AI::evaluate(const Board& board, Side perspective) const noexcept {
    int score = 0;
    for (int i = 0; i < PlayableSquares; ++i) {
        const Piece p = board.cells()[static_cast<std::size_t>(i)];
        if (p == Piece::Empty) continue;
        int row, col; Board::toCoordinates(static_cast<Square>(i), row, col);
        const bool white = isWhite(p);
        int value = pieceValue(p);
        if (isKing(p)) {
            const int center = 3 - std::min(3, std::abs(3 - col)) + 3 - std::min(3, std::abs(3 - row));
            value += center * 3;
        } else {
            const int advance = white ? 7 - row : row;
            value += advance * 4;
            if (col >= 2 && col <= 5) value += 3;
        }
        score += white ? value : -value;
    }
    return perspective == Side::White ? score : -score;
}

int AI::quiescence(Board& board, Side side, int alpha, int beta, int ply, int qdepth) {
    ++qnodes_;
    if (timeExpired()) return 0;
    if (ply >= static_cast<int>(moveBuffers_.size()) - 1) return evaluate(board, side);
    auto& captures = moveBuffers_[static_cast<std::size_t>(ply)];
    board.generateCaptures(side, captures);
    if (captures.empty()) {
        // If the side has no legal moves, it has lost; otherwise stand pat.
        board.generateMoves(side, captures);
        if (captures.empty()) return -MateScore + ply;
        const int standPat = evaluate(board, side);
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
        return alpha;
    }
    orderMoves(board, captures, nullptr, side);
    for (const Move& move : captures) {
        Undo undo;
        if (!board.makeMove(move, undo)) continue;
        const int score = -quiescence(board, opposite(side), -beta, -alpha, ply + 1, qdepth + 1);
        board.unmakeMove(undo);
        if (stopped_) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

int AI::alphaBeta(Board& board, Side side, int depth, int alpha, int beta, int ply) {
    ++nodes_;
    if (timeExpired()) return 0;
    if (depth <= 0) return quiescence(board, side, alpha, beta, ply);

    const int originalAlpha = alpha;
    const std::uint64_t key = hash(board, side);
    TTEntry& entry = tt_[static_cast<std::size_t>(key % tt_.size())];
    Move ttMove;
    if (entry.occupied && entry.key == key) {
        ttMove = entry.best;
        if (entry.depth >= depth) {
            const int ttScore = scoreFromTable(entry.score, ply);
            if (entry.bound == Bound::Exact) return ttScore;
            if (entry.bound == Bound::Lower && ttScore >= beta) return ttScore;
            if (entry.bound == Bound::Upper && ttScore <= alpha) return ttScore;
        }
    }

    if (ply >= static_cast<int>(moveBuffers_.size()) - 1) return evaluate(board, side);
    auto& moves = moveBuffers_[static_cast<std::size_t>(ply)];
    board.generateMoves(side, moves);
    if (moves.empty()) return -MateScore + ply;
    orderMoves(board, moves, ttMove.valid() ? &ttMove : nullptr, side);

    int bestScore = -Infinity;
    Move bestMove;
    for (const Move& move : moves) {
        Undo undo;
        if (!board.makeMove(move, undo)) continue;
        const int score = -alphaBeta(board, opposite(side), depth - 1, -beta, -alpha, ply + 1);
        board.unmakeMove(undo);
        if (stopped_) return 0;
        if (score > bestScore) { bestScore = score; bestMove = move; }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            if (!move.isCapture) {
                auto& h = history_[static_cast<std::size_t>(side)][static_cast<std::size_t>(move.to())];
                const int bonus = depth * depth;
                h = static_cast<std::int16_t>(std::min(30000, static_cast<int>(h) + bonus));
            }
            break;
        }
    }

    if (!stopped_ && bestMove.valid()) {
        entry.key = key;
        entry.best = bestMove;
        entry.score = static_cast<std::int16_t>(scoreToTable(bestScore, ply));
        entry.depth = static_cast<std::int8_t>(std::min(depth, 127));
        entry.bound = bestScore <= originalAlpha ? Bound::Upper : (bestScore >= beta ? Bound::Lower : Bound::Exact);
        entry.occupied = true;
    }
    return bestScore;
}

SearchResult AI::findBestMove(const Board& source, Side side, const SearchLimits& limits) {
    SearchResult result;
    nodes_ = qnodes_ = 0;
    stopped_ = false;
    history_ = {};
    useDeadline_ = limits.timeLimitMs > 0;
    const auto start = std::chrono::steady_clock::now();
    deadline_ = start + std::chrono::milliseconds(std::max(0, limits.timeLimitMs));

    Board board = source;
    auto& rootMoves = moveBuffers_[0];
    board.generateMoves(side, rootMoves);
    if (rootMoves.empty()) return result;
    result.hasMove = true;
    result.bestMove = rootMoves.front();
    const int maxDepth = std::max(1, std::min(limits.maxDepth, 64));

    Move previousIterationBest;
    for (int depth = 1; depth <= maxDepth; ++depth) {
        if (timeExpired()) break;
        auto& moves = rootMoves;
        board.generateMoves(side, moves);
        // The best move from the previous completed iteration is the principal
        // variation candidate; search it first to improve alpha-beta cutoffs.
        orderMoves(board, moves, previousIterationBest.valid() ? &previousIterationBest : nullptr, side);
        int alpha = -Infinity;
        int beta = Infinity;
        int bestScore = -Infinity;
        Move iterationBest = moves.front();
        for (const Move& move : moves) {
            Undo undo;
            if (!board.makeMove(move, undo)) continue;
            const int score = -alphaBeta(board, opposite(side), depth - 1, -beta, -alpha, 1);
            board.unmakeMove(undo);
            if (stopped_) break;
            if (score > bestScore) { bestScore = score; iterationBest = move; }
            if (score > alpha) alpha = score;
        }
        if (stopped_) break;
        result.bestMove = iterationBest;
        previousIterationBest = iterationBest;
        result.score = bestScore;
        result.completedDepth = depth;
    }

    const auto end = std::chrono::steady_clock::now();
    result.nodes = nodes_;
    result.qnodes = qnodes_;
    result.elapsedMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    result.nodesPerSecond = result.elapsedMs ? (nodes_ + qnodes_) * 1000ULL / result.elapsedMs : (nodes_ + qnodes_) * 1000ULL;
    return result;
}
}
