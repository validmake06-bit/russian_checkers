#include "Board.h"
#include <sstream>
#include <algorithm>
#include <cassert>
#include <cstdint>

namespace checkers {
Board::Board() { reset(); }
Board::Board(const std::array<Piece, PlayableSquares>& cells) : cells_(cells) {}

void Board::reset() noexcept {
    cells_.fill(Piece::Empty);
    for (int i = 0; i < PlayableSquares; ++i) {
        if (i < 12) cells_[i] = Piece::BlackMan;
        else if (i >= 20) cells_[i] = Piece::WhiteMan;
    }
}

Piece Board::at(Square sq) const noexcept {
    return sq >= 0 && sq < PlayableSquares ? cells_[static_cast<std::size_t>(sq)] : Piece::Empty;
}

Square Board::toSquare(int row, int col) noexcept {
    if (row < 0 || row >= 8 || col < 0 || col >= 8 || ((row + col) & 1) == 0) return -1;
    return static_cast<Square>(row * 4 + col / 2);
}

void Board::toCoordinates(Square sq, int& row, int& col) noexcept {
    if (sq < 0 || sq >= PlayableSquares) { row = col = -1; return; }
    row = sq / 4;
    col = 2 * (sq % 4) + ((row + 1) & 1);
}

bool Board::inside(int row, int col) const noexcept {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

std::vector<Move> Board::generateCaptures(Side side) const {
    std::vector<Move> captures;
    captures.reserve(64);
    generateCaptures(side, captures);
    return captures;
}

void Board::generateCaptures(Side side, std::vector<Move>& out) const {
    out.clear();
    for (int i = 0; i < PlayableSquares; ++i) {
        const Piece p = cells_[i];
        if (p != Piece::Empty && belongsTo(p, side))
            generateCapturesFrom(static_cast<Square>(i), side, out);
    }
}

std::vector<Move> Board::generateMoves(Side side) const {
    std::vector<Move> moves;
    moves.reserve(64);
    generateMoves(side, moves);
    return moves;
}

void Board::generateMoves(Side side, std::vector<Move>& out) const {
    generateCaptures(side, out);
    if (!out.empty()) return;

    out.clear();
    for (int i = 0; i < PlayableSquares; ++i) {
        const Square from = static_cast<Square>(i);
        const Piece p = cells_[i];
        if (p == Piece::Empty || !belongsTo(p, side)) continue;
        int r, c; toCoordinates(from, r, c);
        if (isKing(p)) {
            for (int d = 0; d < 4; ++d) {
                int nr = r + Dr[d], nc = c + Dc[d];
                while (inside(nr, nc)) {
                    const Square to = toSquare(nr, nc);
                    if (cells_[to] != Piece::Empty) break;
                    Move m; m.from = from; m.length = 1; m.landings[0] = to;
                    out.push_back(m);
                    nr += Dr[d]; nc += Dc[d];
                }
            }
        } else {
            const int forward = side == Side::White ? -1 : 1;
            for (int dc : {-1, 1}) {
                const int nr = r + forward, nc = c + dc;
                if (!inside(nr, nc)) continue;
                const Square to = toSquare(nr, nc);
                if (to >= 0 && cells_[to] == Piece::Empty) {
                    Move m; m.from = from; m.length = 1; m.landings[0] = to;
                    out.push_back(m);
                }
            }
        }
    }
}

void Board::generateCapturesFrom(Square from, Side side, std::vector<Move>& out) const {
    Move path; path.from = from; path.isCapture = true;
    captureDfs(from, from, cells_[from], side, 0u, path, out);
}

bool Board::occupiedForCapture(Square sq, Square origin, Square current) const noexcept {
    if (sq == current) return true;
    if (sq == origin) return false; // the moving piece has left its starting square
    return cells_[sq] != Piece::Empty; // captured pieces deliberately remain blockers until move ends
}

void Board::captureDfs(Square origin, Square current, Piece movingPiece, Side side,
                       std::uint32_t capturedMask, Move& path, std::vector<Move>& out) const {
    int r, c; toCoordinates(current, r, c);
    bool extended = false;

    for (int d = 0; d < 4; ++d) {
        if (!isKing(movingPiece)) {
            const int mr = r + Dr[d], mc = c + Dc[d];
            const int lr = r + 2 * Dr[d], lc = c + 2 * Dc[d];
            if (!inside(mr, mc) || !inside(lr, lc)) continue;
            const Square victim = toSquare(mr, mc), landing = toSquare(lr, lc);
            if (victim < 0 || landing < 0 || (capturedMask & (1u << victim))) continue;
            const Piece target = cells_[victim];
            if (target == Piece::Empty || belongsTo(target, side) || occupiedForCapture(landing, origin, current)) continue;
            if (path.length >= MaxCaptures) continue;

            extended = true;
            Piece nextPiece = movingPiece;
            const int promotionRow = side == Side::White ? 0 : 7;
            int landingRow, landingCol; toCoordinates(landing, landingRow, landingCol);
            if (!isKing(nextPiece) && landingRow == promotionRow) nextPiece = kingOf(side);
            const auto oldLen = path.length;
            path.landings[path.length] = landing;
            path.captured[path.length] = victim;
            ++path.length;
            captureDfs(origin, landing, nextPiece, side, capturedMask | (1u << victim), path, out);
            path.length = oldLen;
        } else {
            int nr = r + Dr[d], nc = c + Dc[d];
            Square victim = -1;
            while (inside(nr, nc)) {
                const Square sq = toSquare(nr, nc);
                if (sq < 0) break;
                if (occupiedForCapture(sq, origin, current)) {
                    const Piece target = cells_[sq];
                    if (target == Piece::Empty || belongsTo(target, side) || (capturedMask & (1u << sq))) break;
                    victim = sq;
                    break;
                }
                nr += Dr[d]; nc += Dc[d];
            }
            if (victim < 0) continue;
            nr += Dr[d]; nc += Dc[d];
            while (inside(nr, nc)) {
                const Square landing = toSquare(nr, nc);
                if (landing < 0 || occupiedForCapture(landing, origin, current)) break;
                if (path.length < MaxCaptures) {
                    extended = true;
                    const auto oldLen = path.length;
                    path.landings[path.length] = landing;
                    path.captured[path.length] = victim;
                    ++path.length;
                    captureDfs(origin, landing, movingPiece, side, capturedMask | (1u << victim), path, out);
                    path.length = oldLen;
                }
                nr += Dr[d]; nc += Dc[d];
            }
        }
    }

    if (!extended && path.length > 0) out.push_back(path);
}

bool Board::makeMove(const Move& move, Undo& undo) noexcept {
    if (!move.valid() || move.length > MaxCaptures || move.from < 0 || move.from >= PlayableSquares) return false;
    const Square destination = move.to();
    if (destination < 0 || destination >= PlayableSquares) return false;
    const Piece moving = cells_[move.from];
    if (moving == Piece::Empty || (destination != move.from && cells_[destination] != Piece::Empty)) return false;
    if (!move.isCapture && move.length != 1) return false;

    // Validate the complete move before mutating the board. This also protects
    // make/unmake from malformed externally supplied Move objects.
    for (std::uint8_t i = 0; i < move.length; ++i) {
        const Square landing = move.landings[i];
        if (landing < 0 || landing >= PlayableSquares) return false;
        if (landing != move.from && cells_[landing] != Piece::Empty) return false;
    }
    if (move.isCapture) {
        std::uint32_t seenCaptured = 0;
        const Side movingSide = isWhite(moving) ? Side::White : Side::Black;
        for (std::uint8_t i = 0; i < move.length; ++i) {
            const Square sq = move.captured[i];
            if (sq < 0 || sq >= PlayableSquares || sq == move.from || sq == destination) return false;
            const std::uint32_t bit = (1u << static_cast<unsigned>(sq));
            if ((seenCaptured & bit) != 0 || cells_[sq] == Piece::Empty || belongsTo(cells_[sq], movingSide)) return false;
            seenCaptured |= bit;
        }
    }

    undo = Undo{};
    undo.from = move.from; undo.to = destination; undo.moved = moving;
    if (move.isCapture) {
        for (std::uint8_t i = 0; i < move.length; ++i) {
            const Square sq = move.captured[i];
            undo.capturedSquares[i] = sq;
            undo.capturedPieces[i] = cells_[sq];
            ++undo.capturedCount;
        }
    }
    cells_[move.from] = Piece::Empty;
    for (std::uint8_t i = 0; i < undo.capturedCount; ++i) cells_[undo.capturedSquares[i]] = Piece::Empty;
    Piece placed = moving;
    int row, col; toCoordinates(move.to(), row, col);
    if (!isKing(placed) && ((isWhite(placed) && row == 0) || (isBlack(placed) && row == 7)))
        placed = kingOf(isWhite(placed) ? Side::White : Side::Black);
    cells_[move.to()] = placed;
    return true;
}

void Board::unmakeMove(const Undo& undo) noexcept {
    if (undo.from < 0 || undo.to < 0) return;
    cells_[undo.to] = Piece::Empty;
    cells_[undo.from] = undo.moved;
    for (std::uint8_t i = 0; i < undo.capturedCount; ++i)
        cells_[undo.capturedSquares[i]] = undo.capturedPieces[i];
}

std::uint64_t Board::positionKey(Side sideToMove) const noexcept {
    // FNV-1a over all 32 squares plus side-to-move. This is used only for
    // repetition adjudication, not as a cryptographic identifier.
    std::uint64_t key = 14695981039346656037ULL;
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        key ^= static_cast<std::uint8_t>(cells_[i]);
        key *= 1099511628211ULL;
        key ^= static_cast<std::uint8_t>(i);
        key *= 1099511628211ULL;
    }
    key ^= static_cast<std::uint8_t>(sideToMove);
    key *= 1099511628211ULL;
    return key;
}

std::string Board::toString() const {
    std::ostringstream out;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            const Square sq = toSquare(r, c);
            if (sq < 0) { out << "   "; continue; }
            char ch = '.';
            switch (cells_[sq]) {
                case Piece::WhiteMan: ch = 'w'; break;
                case Piece::WhiteKing: ch = 'W'; break;
                case Piece::BlackMan: ch = 'b'; break;
                case Piece::BlackKing: ch = 'B'; break;
                default: break;
            }
            out << ' ' << ch << ' ';
        }
        out << '\n';
    }
    return out.str();
}
}
