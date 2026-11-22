#pragma once
#include "Board.h"
#include <QWidget>
#include <QPointF>
#include <QVariantAnimation>
#include <vector>
#include <array>

class QPainter;

class BoardWidget final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QPointF animatedPosition READ animatedPosition WRITE setAnimatedPosition)
public:
    explicit BoardWidget(QWidget* parent = nullptr);
    void setPosition(const checkers::Board& board, checkers::Side sideToMove,
                     const std::vector<checkers::Move>& legalMoves);
    void setInteractionEnabled(bool enabled);
    void setLastMove(const checkers::Move& move);
    void setSelection(checkers::Square square);
    void animateMove(const checkers::Move& move, const checkers::Board& resultingBoard);
    void setDarkTheme(bool dark);

    QPointF animatedPosition() const { return animatedPosition_; }
    void setAnimatedPosition(const QPointF& p);

signals:
    void squareClicked(int square);
    void animationFinished();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return {640, 640}; }

private:
    QPointF squareCenter(int square) const;
    int squareAt(const QPointF& point) const;
    void drawPiece(QPainter& painter, const QPointF& center, bool white, bool king, qreal radius) const;

    checkers::Board board_;
    checkers::Side sideToMove_{checkers::Side::White};
    std::vector<checkers::Move> legalMoves_;
    std::vector<checkers::Square> targets_;
    checkers::Move lastMove_{};
    bool hasLastMove_{false};
    checkers::Square selectedSquare_{-1};
    bool interactionEnabled_{true};
    bool darkTheme_{true};
    bool animating_{false};
    checkers::Square animatedSquare_{-1};
    QPointF animatedPosition_;
    QVariantAnimation animation_;
    QVariantAnimation captureAnimation_;
    std::array<checkers::Square, checkers::MaxCaptures> fadingSquares_{};
    std::array<checkers::Piece, checkers::MaxCaptures> fadingPieces_{};
    int fadingCount_{0};
    qreal captureOpacity_{0.0};
};
