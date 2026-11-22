#pragma once
#include "Move.h"
#include <array>
#include <vector>
#include <string>

namespace checkers {
class Board {
public:
    Board();
    explicit Board(const std::array<Piece, PlayableSquares>& cells);

    void reset() noexcept;
    Piece at(Square sq) const noexcept;
    const std::array<Piece, PlayableSquares>& cells() const noexcept { return cells_; }

    // Legal moves. Captures are mandatory.
    std::vector<Move> generateMoves(Side side) const;
    void generateMoves(Side side, std::vector<Move>& out) const;
    // Captures only; useful for quiescence search.
    std::vector<Move> generateCaptures(Side side) const;
    void generateCaptures(Side side, std::vector<Move>& out) const;
    bool makeMove(const Move& move, Undo& undo) noexcept;
    void unmakeMove(const Undo& undo) noexcept;

    static Square toSquare(int row, int col) noexcept;
    static void toCoordinates(Square sq, int& row, int& col) noexcept;
    std::string toString() const;
    // Stable position identity for repetition detection; includes the side to move.
    std::uint64_t positionKey(Side sideToMove) const noexcept;

private:
    std::array<Piece, PlayableSquares> cells_{};

    bool inside(int row, int col) const noexcept;
    void generateCapturesFrom(Square from, Side side, std::vector<Move>& out) const;
    void captureDfs(Square origin, Square current, Piece movingPiece, Side side,
                    std::uint32_t capturedMask, Move& path, std::vector<Move>& out) const;
    bool occupiedForCapture(Square sq, Square origin, Square current) const noexcept;
};
}
