#include "Board.h"
#include "AI.h"
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <utility>
#define CHECK(expr) do { if (!(expr)) { std::cerr << "CHECK failed: " #expr << " at " << __FILE__ << ":" << __LINE__ << '\n'; std::abort(); } } while (false)
using namespace checkers;

static std::array<Piece, PlayableSquares> emptyBoard() {
    std::array<Piece, PlayableSquares> a{};
    a.fill(Piece::Empty);
    return a;
}

int main() {
    // Mapping is bijective over all 32 playable squares.
    bool seen[32]{};
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
        Square s = Board::toSquare(r, c);
        if (s >= 0) {
            CHECK(!seen[static_cast<int>(s)]);
            seen[static_cast<int>(s)] = true;
            int rr, cc; Board::toCoordinates(s, rr, cc);
            CHECK(rr == r && cc == c);
        }
    }
    for (bool x : seen) CHECK(x);

    Board initial;
    CHECK(initial.generateMoves(Side::White).size() == 7);
    CHECK(initial.generateMoves(Side::Black).size() == 7);
    CHECK(initial.positionKey(Side::White) != initial.positionKey(Side::Black));
    Board sameInitial;
    CHECK(initial.positionKey(Side::White) == sameInitial.positionKey(Side::White));

    // A reversible two-king cycle returns to the same key only when the same
    // side is to move, which is the identity used by the threefold rule.
    auto repetitionCells = emptyBoard();
    repetitionCells[Board::toSquare(5, 0)] = Piece::WhiteKing;
    repetitionCells[Board::toSquare(2, 7)] = Piece::BlackKing;
    Board repetitionBoard(repetitionCells);
    const auto initialRepetitionKey = repetitionBoard.positionKey(Side::White);
    Side repetitionTurn = Side::White;
    const std::array<std::array<int, 4>, 4> cycle{{
        {{5, 0, 4, 1}}, {{2, 7, 3, 6}},
        {{4, 1, 5, 0}}, {{3, 6, 2, 7}}
    }};
    int occurrences = 1;
    for (int repetition = 0; repetition < 2; ++repetition) {
        for (const auto& step : cycle) {
            const Square from = Board::toSquare(step[0], step[1]);
            const Square to = Board::toSquare(step[2], step[3]);
            const auto legal = repetitionBoard.generateMoves(repetitionTurn);
            auto it = std::find_if(legal.begin(), legal.end(), [&](const Move& m) { return m.from == from && m.to() == to; });
            CHECK(it != legal.end());
            Undo cycleUndo;
            CHECK(repetitionBoard.makeMove(*it, cycleUndo));
            repetitionTurn = opposite(repetitionTurn);
        }
        if (repetitionBoard.positionKey(repetitionTurn) == initialRepetitionKey) ++occurrences;
    }
    CHECK(occurrences == 3);

    // Mandatory capture suppresses quiet moves; a man captures backward too.
    auto a = emptyBoard();
    a[Board::toSquare(4, 3)] = Piece::WhiteMan;
    a[Board::toSquare(5, 2)] = Piece::BlackMan;
    Board captureBoard(a);
    auto ms = captureBoard.generateMoves(Side::White);
    CHECK(ms.size() == 1 && ms[0].isCapture && ms[0].to() == Board::toSquare(6, 1));
    CHECK(captureBoard.generateCaptures(Side::White).size() == 1);

    // A man crowns during a capture and continues as a flying king.
    a = emptyBoard();
    a[Board::toSquare(2, 1)] = Piece::WhiteMan;
    a[Board::toSquare(1, 2)] = Piece::BlackMan;
    a[Board::toSquare(2, 5)] = Piece::BlackMan;
    Board crownBoard(a);
    ms = crownBoard.generateMoves(Side::White);
    bool found = false;
    for (const auto& m : ms)
        if (m.length == 2 && m.landings[0] == Board::toSquare(0, 3)) found = true;
    CHECK(found);

    // Capture sequence continues to the end; the man promotes then captures as a king.
    a = emptyBoard();
    a[Board::toSquare(4, 1)] = Piece::WhiteMan;
    a[Board::toSquare(3, 2)] = Piece::BlackMan;
    a[Board::toSquare(1, 4)] = Piece::BlackMan;
    a[Board::toSquare(1, 6)] = Piece::BlackMan;
    Board chainBoard(a);
    ms = chainBoard.generateMoves(Side::White);
    CHECK(ms.size() == 1);
    CHECK(ms[0].isCapture && ms[0].length == 3);
    CHECK(ms[0].landings[1] == Board::toSquare(0, 5));
    CHECK(ms[0].to() == Board::toSquare(2, 7));

    // A flying king can choose any free landing square beyond the victim.
    a = emptyBoard();
    a[Board::toSquare(5, 0)] = Piece::WhiteKing;
    a[Board::toSquare(3, 2)] = Piece::BlackMan;
    Board flyingKingBoard(a);
    ms = flyingKingBoard.generateMoves(Side::White);
    CHECK(ms.size() >= 2);
    for (const auto& m : ms) CHECK(m.isCapture && m.length == 1);

    // Captured pieces remain blockers during a combination: no victim can be crossed again.
    for (const auto& m : ms) {
        for (std::uint8_t i = 0; i < m.length; ++i)
            for (std::uint8_t j = i + 1; j < m.length; ++j)
                CHECK(m.captured[i] != m.captured[j]);
    }

    // Apply / undo restores the exact position, including all captured pieces.
    auto captureMoves = captureBoard.generateMoves(Side::White);
    Undo u; const auto before = captureBoard.cells();
    CHECK(captureBoard.makeMove(captureMoves[0], u));
    captureBoard.unmakeMove(u);
    CHECK(captureBoard.cells() == before);

    // Malformed moves must be rejected without changing the board.
    Move malformed;
    malformed.from = Board::toSquare(4, 3);
    malformed.length = 1;
    malformed.isCapture = true;
    malformed.landings[0] = Board::toSquare(6, 1);
    malformed.captured[0] = malformed.from; // cannot capture the moving piece
    const auto malformedSnapshot = captureBoard.cells();
    CHECK(!captureBoard.makeMove(malformed, u));
    CHECK(captureBoard.cells() == malformedSnapshot);

    // AI returns a legal move and does not mutate the source board.
    AI ai(4096);
    SearchLimits limits; limits.maxDepth = 3; limits.timeLimitMs = 0;
    const auto initialCells = initial.cells();
    const auto search = ai.findBestMove(initial, Side::White, limits);
    CHECK(search.hasMove && search.bestMove.valid());
    CHECK(search.completedDepth == 3);
    CHECK(initial.cells() == initialCells);
    bool legal = false;
    for (const auto& m : initial.generateMoves(Side::White))
        if (m.from == search.bestMove.from && m.to() == search.bestMove.to()) legal = true;
    CHECK(legal);

    // Deterministic legal playout: every generated move applies and undo is exact.
    Board randomBoard;
    Side turn = Side::White;
    std::uint32_t rng = 0x12345678u;
    for (int ply = 0; ply < 120; ++ply) {
        auto legalMoves = randomBoard.generateMoves(turn);
        if (legalMoves.empty()) break;
        rng = rng * 1664525u + 1013904223u;
        const Move chosen = legalMoves[rng % legalMoves.size()];
        const auto snapshot = randomBoard.cells();
        Undo randomUndo;
        CHECK(randomBoard.makeMove(chosen, randomUndo));
        randomBoard.unmakeMove(randomUndo);
        CHECK(randomBoard.cells() == snapshot);
        CHECK(randomBoard.makeMove(chosen, randomUndo));
        turn = opposite(turn);
    }

    // A side with no pieces has no legal moves.
    a = emptyBoard();
    a[Board::toSquare(0, 1)] = Piece::WhiteMan;
    Board terminalBoard(a);
    CHECK(terminalBoard.generateMoves(Side::Black).empty());
    const auto terminalSearch = ai.findBestMove(terminalBoard, Side::Black, limits);
    CHECK(!terminalSearch.hasMove);

    std::cout << "All checkers tests passed.\n";
    return 0;
}
