#pragma once
#include <cstdint>
#include <array>

namespace checkers {
constexpr int BoardSize = 8;
constexpr int PlayableSquares = 32;
constexpr int MaxCaptures = 12;
using Square = std::int8_t; // 0..31; -1 means invalid

enum class Side : std::uint8_t { White = 0, Black = 1 };
enum class Piece : std::uint8_t { Empty = 0, WhiteMan, WhiteKing, BlackMan, BlackKing };

constexpr Side opposite(Side s) noexcept { return s == Side::White ? Side::Black : Side::White; }
constexpr bool isWhite(Piece p) noexcept { return p == Piece::WhiteMan || p == Piece::WhiteKing; }
constexpr bool isBlack(Piece p) noexcept { return p == Piece::BlackMan || p == Piece::BlackKing; }
constexpr bool isKing(Piece p) noexcept { return p == Piece::WhiteKing || p == Piece::BlackKing; }
constexpr bool belongsTo(Piece p, Side s) noexcept {
    return s == Side::White ? isWhite(p) : isBlack(p);
}
constexpr Piece kingOf(Side s) noexcept { return s == Side::White ? Piece::WhiteKing : Piece::BlackKing; }
constexpr Piece manOf(Side s) noexcept { return s == Side::White ? Piece::WhiteMan : Piece::BlackMan; }

// Directions: NW, NE, SW, SE in board coordinates (row grows downward).
constexpr std::array<int, 4> Dr{{-1, -1, 1, 1}};
constexpr std::array<int, 4> Dc{{-1, 1, -1, 1}};
}
