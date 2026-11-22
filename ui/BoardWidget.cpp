#include "BoardWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>
#include <algorithm>

using namespace checkers;

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(420, 420);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    animation_.setDuration(240);
    animation_.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        setAnimatedPosition(v.toPointF());
    });
    connect(&animation_, &QVariantAnimation::finished, this, [this] {
        if (animatedMove_.isCapture && animationSegment_ < animatedMove_.length) {
            fadingCount_ = 1;
            fadingSquares_[0] = animatedMove_.captured[static_cast<std::size_t>(animationSegment_)];
            fadingPieces_[0] = capturedPieces_[static_cast<std::size_t>(animationSegment_)];
            captureOpacity_ = 1.0;
            captureAnimation_.stop();
            captureAnimation_.setDuration(170);
            captureAnimation_.start();
        } else {
            advanceAnimationSequence();
        }
    });
    captureAnimation_.setDuration(170);
    captureAnimation_.setStartValue(1.0);
    captureAnimation_.setEndValue(0.0);
    captureAnimation_.setEasingCurve(QEasingCurve::OutCubic);
    connect(&captureAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        captureOpacity_ = value.toReal();
        update();
    });
    connect(&captureAnimation_, &QVariantAnimation::finished, this, [this] {
        fadingCount_ = 0;
        advanceAnimationSequence();
    });
}

void BoardWidget::setPosition(const Board& board, Side side, const std::vector<Move>& moves) {
    board_ = board;
    sideToMove_ = side;
    legalMoves_ = moves;
    selectedSquare_ = -1;
    targets_.clear();
    update();
}
void BoardWidget::setInteractionEnabled(bool enabled) { interactionEnabled_ = enabled; update(); }
void BoardWidget::setLastMove(const Move& move) { lastMove_ = move; hasLastMove_ = move.valid(); update(); }
void BoardWidget::setSelection(Square square) { selectedSquare_ = square; targets_.clear(); if (square >= 0) for (const auto& move : legalMoves_) if (move.from == square) targets_.push_back(move.to()); update(); }
void BoardWidget::setDarkTheme(bool dark) { darkTheme_ = dark; update(); }
void BoardWidget::setAnimatedPosition(const QPointF& p) { animatedPosition_ = p; update(); }

QPointF BoardWidget::squareCenter(int square) const {
    int row, col; Board::toCoordinates(static_cast<Square>(square), row, col);
    const qreal side = qMin(width(), height()) - 42.0;
    const qreal cell = side / 8.0;
    const qreal left = (width() - side) / 2.0;
    const qreal top = (height() - side) / 2.0;
    return {left + (col + 0.5) * cell, top + (row + 0.5) * cell};
}

int BoardWidget::squareAt(const QPointF& p) const {
    const qreal side = qMin(width(), height()) - 42.0;
    const qreal cell = side / 8.0;
    const qreal left = (width() - side) / 2.0;
    const qreal top = (height() - side) / 2.0;
    const int col = int((p.x() - left) / cell), row = int((p.y() - top) / cell);
    if (row < 0 || row >= 8 || col < 0 || col >= 8) return -1;
    return Board::toSquare(row, col);
}

void BoardWidget::drawPiece(QPainter& painter, const QPointF& center, bool white, bool king, qreal radius) const {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(18, 24, 20, darkTheme_ ? 95 : 42));
    painter.drawEllipse(center + QPointF(1.5, 3.0), radius, radius);

    QRadialGradient gradient(center + QPointF(-radius * .28, -radius * .34), radius * 1.55);
    if (white) {
        gradient.setColorAt(0.0, QColor("#ffffff"));
        gradient.setColorAt(0.72, QColor(darkTheme_ ? "#e7e9e5" : "#fdfefd"));
        gradient.setColorAt(1.0, QColor(darkTheme_ ? "#b8c0b9" : "#d9dfd9"));
    } else {
        gradient.setColorAt(0.0, QColor(darkTheme_ ? "#65716b" : "#59645d"));
        gradient.setColorAt(0.68, QColor("#303a34"));
        gradient.setColorAt(1.0, QColor("#202923"));
    }
    painter.setBrush(gradient);
    painter.setPen(QPen(white ? QColor("#cbd2cb") : QColor("#1b241e"), 1.0));
    painter.drawEllipse(center, radius, radius);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(white ? QColor(255,255,255,190) : QColor(186,202,188,105), 1.4));
    painter.drawEllipse(center, radius * .77, radius * .77);
    if (king) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(darkTheme_ ? QColor("#c4d5c8") : QColor("#718a76"));
        QPolygonF crown;
        crown << center + QPointF(-radius*.42, radius*.15)
              << center + QPointF(-radius*.34, -radius*.24)
              << center + QPointF(-radius*.10, -radius*.02)
              << center + QPointF(0, -radius*.38)
              << center + QPointF(radius*.12, -radius*.02)
              << center + QPointF(radius*.35, -radius*.24)
              << center + QPointF(radius*.43, radius*.15);
        painter.drawPolygon(crown);
        painter.setBrush(white ? QColor("#657d6b") : QColor("#dce7dd"));
        painter.drawEllipse(center + QPointF(0, -radius*.13), radius*.045, radius*.045);
    }
    painter.restore();
}
void BoardWidget::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), darkTheme_ ? QColor("#171b22") : QColor("#f3f4f1"));
    const qreal side = qMin(width(), height()) - 42.0, cell = side / 8.0;
    const qreal left = (width() - side) / 2.0, top = (height() - side) / 2.0;
    p.save();
    p.setPen(QPen(darkTheme_ ? QColor("#39443e") : QColor("#dfe5de"), 1.0));
    p.setBrush(darkTheme_ ? QColor("#202630") : QColor("#ffffff"));
    p.drawRoundedRect(QRectF(left - 8, top - 8, side + 16, side + 16), 9, 9);
    p.restore();
    p.setPen(Qt::NoPen);
    for (int r=0;r<8;++r) for (int c=0;c<8;++c) {
        const QRectF sq(left+c*cell, top+r*cell, cell, cell);
        if (((r+c)&1) != 0) {
            p.setBrush(darkTheme_ ? QColor("#414d46") : QColor("#aab9ac"));
        } else {
            p.setBrush(darkTheme_ ? QColor("#252d35") : QColor("#e9eee8"));
        }
        p.drawRect(sq);
    }
    if (selectedSquare_ >= 0) {
        const QPointF center = squareCenter(selectedSquare_);
        p.setPen(QPen(darkTheme_ ? QColor("#b7cdbb") : QColor("#718a76"), 3)); p.setBrush(darkTheme_ ? QColor(183,205,187,42) : QColor(113,138,118,38)); p.drawEllipse(center, cell*.43, cell*.43);
    }
    if (hasLastMove_) {
        p.setBrush(darkTheme_ ? QColor(183,205,187,65) : QColor(113,138,118,65));
        int r,c; Board::toCoordinates(lastMove_.from,r,c); p.drawRect(QRectF(left+c*cell,top+r*cell,cell,cell));
        Board::toCoordinates(lastMove_.to(),r,c); p.drawRect(QRectF(left+c*cell,top+r*cell,cell,cell));
    }
    if (!targets_.empty()) {
        for (Square s : targets_) { const QPointF center=squareCenter(s); p.setPen(QPen(darkTheme_ ? QColor("#b7cdbb") : QColor("#718a76"),3)); p.setBrush(darkTheme_ ? QColor(183,205,187,55) : QColor(113,138,118,55)); p.drawEllipse(center,cell*.17,cell*.17); }
    }
    for (int i=0;i<PlayableSquares;++i) {
        const Piece piece=board_.at(static_cast<Square>(i)); if(piece==Piece::Empty) continue;
        if (animating_ && i==animatedSquare_) continue;
        drawPiece(p,squareCenter(i),isWhite(piece),isKing(piece),cell*.37);
    }
    if (animating_ && fadingCount_ > 0 && captureOpacity_ > 0.0) {
        p.save();
        p.setOpacity(captureOpacity_);
        for (int i = 0; i < fadingCount_; ++i) {
            const Piece captured = fadingPieces_[static_cast<std::size_t>(i)];
            if (captured != Piece::Empty)
                drawPiece(p, squareCenter(fadingSquares_[static_cast<std::size_t>(i)]), isWhite(captured), isKing(captured), cell*.37);
        }
        p.restore();
    }
    if (animating_ && animatedSquare_>=0) {
        const Piece piece=board_.at(animatedSquare_);
        // The piece is already on its destination in board_; render it at the animated position instead.
        drawPiece(p,animatedPosition_,isWhite(piece),isKing(piece),cell*.37);
    }
}

void BoardWidget::mousePressEvent(QMouseEvent* event) {
    if (!interactionEnabled_ || animating_ || event->button()!=Qt::LeftButton) return;
    const int square=squareAt(event->position());
    if(square>=0) emit squareClicked(square);
}

void BoardWidget::startAnimationSegment() {
    if (animationSegment_ >= animatedMove_.length) {
        finishAnimationSequence();
        return;
    }
    const Square fromSquare = animationSegment_ == 0
        ? animatedMove_.from
        : animatedMove_.landings[static_cast<std::size_t>(animationSegment_ - 1)];
    const Square toSquare = animatedMove_.landings[static_cast<std::size_t>(animationSegment_)];
    const QPointF from = squareCenter(fromSquare);
    const QPointF to = squareCenter(toSquare);
    animatedPosition_ = from;
    animatedSquare_ = animatedMove_.to();
    animation_.stop();
    animation_.setDuration(animatedMove_.isCapture ? 235 : 210);
    animation_.setStartValue(from);
    animation_.setEndValue(to);
    update();
    animation_.start();
}

void BoardWidget::advanceAnimationSequence() {
    ++animationSegment_;
    if (animationSegment_ < animatedMove_.length) {
        startAnimationSegment();
        return;
    }
    finishAnimationSequence();
}

void BoardWidget::finishAnimationSequence() {
    animating_ = false;
    animatedSquare_ = -1;
    fadingCount_ = 0;
    movingPiece_ = Piece::Empty;
    captureOpacity_ = 0.0;
    update();
    emit animationFinished();
}

void BoardWidget::animateMove(const Move& move, const Board& resultingBoard) {
    const Board before = board_;
    movingPiece_ = before.at(move.from);
    if (movingPiece_ == Piece::Empty || !move.valid()) {
        board_ = resultingBoard;
        update();
        emit animationFinished();
        return;
    }

    animatedMove_ = move;
    animationSegment_ = 0;
    fadingCount_ = 0;
    capturedPieces_.fill(Piece::Empty);
    if (move.isCapture) {
        for (int i = 0; i < move.length && i < checkers::MaxCaptures; ++i) {
            const Square capturedSquare = move.captured[static_cast<std::size_t>(i)];
            if (capturedSquare >= 0)
                capturedPieces_[static_cast<std::size_t>(i)] = before.at(capturedSquare);
        }
    }

    board_ = resultingBoard;
    animating_ = true;
    captureOpacity_ = 0.0;
    captureAnimation_.stop();
    startAnimationSegment();
}
