#pragma once
#include "Types.h"
#include <array>
#include <cstdint>

namespace checkers {
struct Move {
    Square from{-1};
    std::array<Square, MaxCaptures> landings{};
    std::array<Square, MaxCaptures> captured{};
    std::uint8_t length{0}; // number of landing squares; 1 for a quiet move
    bool isCapture{false};

    Square to() const noexcept { return length ? landings[length - 1] : Square{-1}; }
    bool valid() const noexcept { return from >= 0 && length > 0; }
};

struct Undo {
    Square from{-1};
    Square to{-1};
    Piece moved{Piece::Empty};
    std::array<Square, MaxCaptures> capturedSquares{};
    std::array<Piece, MaxCaptures> capturedPieces{};
    std::uint8_t capturedCount{0};
};
}
