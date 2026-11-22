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
            captureAnimation_.setDuration(qMax(1, 170 * 100 / animationSpeedPercent_));
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
    landingAnimation_.setDuration(190);
    landingAnimation_.setEasingCurve(QEasingCurve::OutCubic);
    landingAnimation_.setKeyValueAt(0.0, 0.94);
    landingAnimation_.setKeyValueAt(0.58, 1.07);
    landingAnimation_.setEndValue(1.0);
    connect(&landingAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        landingScale_ = value.toReal();
        update();
    });
    connect(&landingAnimation_, &QVariantAnimation::finished, this, [this] {
        landingPulseRunning_ = false;
        animating_ = false;
        animatedSquare_ = -1;
        fadingCount_ = 0;
        movingPiece_ = Piece::Empty;
        captureOpacity_ = 0.0;
        landingScale_ = 1.0;
        update();
        emit animationFinished();
    });
}

void BoardWidget::setPosition(const Board& board, Side side, const std::vector<Move>& moves) {
    // A new authoritative position (for example, starting another game) cancels
    // any in-flight visual sequence so stale animation frames cannot leak forward.
    if (animating_) {
        animation_.stop();
        captureAnimation_.stop();
        landingAnimation_.stop();
        landingPulseRunning_ = false;
        landingScale_ = 1.0;
        animating_ = false;
        animatedSquare_ = -1;
        fadingCount_ = 0;
        movingPiece_ = Piece::Empty;
        captureOpacity_ = 0.0;
    }
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
void BoardWidget::setAnimationSettings(bool enabled, int speedPercent) {
    animationsEnabled_ = enabled;
    animationSpeedPercent_ = std::clamp(speedPercent, 50, 150);
}
void BoardWidget::setAnimatedPosition(const QPointF& p) { animatedPosition_ = p; update(); }

QPointF BoardWidget::squareCenter(int square) const {
    int row, col; Board::toCoordinates(static_cast<Square>(square), row, col);
    const qreal side = qMin(width(), height()) - 68.0;
    const qreal cell = side / 8.0;
    const qreal left = (width() - side) / 2.0;
    const qreal top = (height() - side) / 2.0;
    return {left + (col + 0.5) * cell, top + (row + 0.5) * cell};
}

int BoardWidget::squareAt(const QPointF& p) const {
    const qreal side = qMin(width(), height()) - 68.0;
    const qreal cell = side / 8.0;
    const qreal left = (width() - side) / 2.0;
    const qreal top = (height() - side) / 2.0;
    if (p.x() < left || p.y() < top || p.x() >= left + side || p.y() >= top + side)
        return -1;
    const int col = int((p.x() - left) / cell), row = int((p.y() - top) / cell);
    if (row < 0 || row >= 8 || col < 0 || col >= 8) return -1;
    return Board::toSquare(row, col);
}

void BoardWidget::drawPiece(QPainter& painter, const QPointF& center, bool white, bool king, qreal radius) const {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(35, 22, 14, darkTheme_ ? 110 : 48));
    painter.drawEllipse(center + QPointF(1.5, 3.0), radius, radius);

    QRadialGradient gradient(center + QPointF(-radius * .28, -radius * .34), radius * 1.55);
    if (white) {
        gradient.setColorAt(0.0, QColor("#fff7e9"));
        gradient.setColorAt(0.72, QColor(darkTheme_ ? "#ead9c3" : "#fffaf1"));
        gradient.setColorAt(1.0, QColor(darkTheme_ ? "#c9ad8d" : "#dfc8ab"));
    } else {
        gradient.setColorAt(0.0, QColor(darkTheme_ ? "#9a7657" : "#87664b"));
        gradient.setColorAt(0.68, QColor("#4a3022"));
        gradient.setColorAt(1.0, QColor("#2b1b13"));
    }
    painter.setBrush(gradient);
    painter.setPen(QPen(white ? QColor("#e4cfb4") : QColor("#2b1b13"), 1.0));
    painter.drawEllipse(center, radius, radius);

    // Fine bevels and a soft top reflection give each checker a tactile,
    // polished finish without relying on platform-specific graphics effects.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(white ? QColor(255,255,255,205) : QColor(208,177,143,105), 1.35));
    painter.drawEllipse(center + QPointF(0, -radius * .025), radius * .79, radius * .79);
    painter.setPen(QPen(white ? QColor(157,126,96,105) : QColor(15,10,7,150), 1.0));
    painter.drawArc(QRectF(center.x() - radius * .68, center.y() - radius * .68,
                           radius * 1.36, radius * 1.36), 205 * 16, 145 * 16);
    QRadialGradient glint(center + QPointF(-radius * .38, -radius * .55), radius * .72);
    glint.setColorAt(0.0, QColor(255,255,255, white ? 105 : 42));
    glint.setColorAt(1.0, QColor(255,255,255,0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(glint);
    painter.drawEllipse(center + QPointF(-radius * .18, -radius * .34),
                        radius * .49, radius * .22);
    if (king) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(darkTheme_ ? QColor("#c4d5c8") : QColor("#9b6b47"));
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
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal side = qMin(width(), height()) - 68.0, cell = side / 8.0;
    const qreal left = (width() - side) / 2.0, top = (height() - side) / 2.0;
    const qreal dpr = devicePixelRatioF();
    if (cachedSurfaceSize_ != size() || qAbs(cachedSurfaceDpr_ - dpr) > 0.01
        || cachedDarkTheme_ != darkTheme_ || backgroundCache_.isNull()) {
        backgroundCache_ = QPixmap(QSize(qMax(1, qRound(width() * dpr)),
                                         qMax(1, qRound(height() * dpr))));
        backgroundCache_.setDevicePixelRatio(dpr);
        backgroundCache_.fill(darkTheme_ ? QColor("#241812") : QColor("#f4eadc"));
        QPainter surface(&backgroundCache_);
        surface.setRenderHint(QPainter::Antialiasing);

    // Layered walnut frame with a restrained inset highlight.
    surface.save();
    surface.setPen(Qt::NoPen);
    surface.setBrush(QColor(0, 0, 0, darkTheme_ ? 95 : 35));
    surface.drawRoundedRect(QRectF(left - 11, top - 8, side + 22, side + 22), 16, 16);
    QLinearGradient frame(left, top - 10, left + side, top + side + 10);
    if (darkTheme_) {
        frame.setColorAt(0.0, QColor("#9a704b"));
        frame.setColorAt(0.45, QColor("#4c3021"));
        frame.setColorAt(1.0, QColor("#2a1b14"));
    } else {
        frame.setColorAt(0.0, QColor("#c5a17d"));
        frame.setColorAt(0.5, QColor("#8b6243"));
        frame.setColorAt(1.0, QColor("#60432f"));
    }
    surface.setBrush(frame);
    surface.setPen(QPen(darkTheme_ ? QColor("#b88b61") : QColor("#d4b28d"), 1.0));
    surface.drawRoundedRect(QRectF(left - 8, top - 8, side + 16, side + 16), 12, 12);
    surface.setPen(QPen(QColor(255, 245, 225, darkTheme_ ? 42 : 90), 1.0));
    surface.setBrush(Qt::NoBrush);
    surface.drawRoundedRect(QRectF(left - 4, top - 4, side + 8, side + 8), 8, 8);
    surface.restore();

    // Individually shaded squares create a subtle, tactile board surface.
    for (int r=0;r<8;++r) for (int c=0;c<8;++c) {
        const QRectF sq(left+c*cell, top+r*cell, cell, cell);
        QLinearGradient squareGradient(sq.topLeft(), sq.bottomRight());
        if (((r+c)&1) != 0) {
            if (darkTheme_) {
                squareGradient.setColorAt(0.0, QColor("#8a6242"));
                squareGradient.setColorAt(1.0, QColor("#65442f"));
            } else {
                squareGradient.setColorAt(0.0, QColor("#b7c4b5"));
                squareGradient.setColorAt(1.0, QColor("#91a18f"));
            }
        } else {
            if (darkTheme_) {
                squareGradient.setColorAt(0.0, QColor("#f1e2ce"));
                squareGradient.setColorAt(1.0, QColor("#d7c0a3"));
            } else {
                squareGradient.setColorAt(0.0, QColor("#fffaf0"));
                squareGradient.setColorAt(1.0, QColor("#e8e9df"));
            }
        }
        surface.fillRect(sq, squareGradient);
        surface.setPen(QPen(QColor(60, 42, 28, darkTheme_ ? 24 : 18), 0.6));
        surface.drawRect(sq);
    }

    // Small coordinate marks sit in the board's quiet outer margin. They make
    // analysis and move discussion easier while keeping the playing surface clean.
    QFont coordinateFont = surface.font();
    coordinateFont.setPointSize(8);
    coordinateFont.setWeight(QFont::DemiBold);
    coordinateFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
    surface.setFont(coordinateFont);
    surface.setPen(darkTheme_ ? QColor("#d9c1a4") : QColor("#80664d"));
    for (int index = 0; index < 8; ++index) {
        const QString rank = QString::number(8 - index);
        const QString file = QStringLiteral("abcdefgh").mid(index, 1);
        surface.drawText(QRectF(left - 28, top + index * cell, 18, cell),
                   Qt::AlignCenter, rank);
        surface.drawText(QRectF(left + index * cell, top + side + 10, cell, 18),
                   Qt::AlignCenter, file);
    }

        surface.end();
        cachedSurfaceSize_ = size();
        cachedSurfaceDpr_ = dpr;
        cachedDarkTheme_ = darkTheme_;
    }
    p.drawPixmap(0, 0, backgroundCache_);

    if (selectedSquare_ >= 0) {
        const QPointF center = squareCenter(selectedSquare_);
        p.setPen(QPen(darkTheme_ ? QColor("#e1c29d") : QColor("#9b6b47"), 3)); p.setBrush(darkTheme_ ? QColor(196,145,98,55) : QColor(155,107,71,45)); p.drawEllipse(center, cell*.43, cell*.43);
    }
    if (hasLastMove_) {
        p.setBrush(darkTheme_ ? QColor(196,145,98,75) : QColor(155,107,71,75));
        int r,c; Board::toCoordinates(lastMove_.from,r,c); p.drawRect(QRectF(left+c*cell,top+r*cell,cell,cell));
        Board::toCoordinates(lastMove_.to(),r,c); p.drawRect(QRectF(left+c*cell,top+r*cell,cell,cell));
    }
    if (!targets_.empty()) {
        for (Square s : targets_) { const QPointF center=squareCenter(s); p.setPen(QPen(darkTheme_ ? QColor("#e1c29d") : QColor("#9b6b47"),3)); p.setBrush(darkTheme_ ? QColor(196,145,98,65) : QColor(155,107,71,65)); p.drawEllipse(center,cell*.17,cell*.17); }
    }
    for (int i=0;i<PlayableSquares;++i) {
        const Piece piece=board_.at(static_cast<Square>(i)); if(piece==Piece::Empty) continue;
        if (animating_ && i==animatedSquare_) continue;
        drawPiece(p,squareCenter(i),isWhite(piece),isKing(piece),cell*.37);
    }
    if (animating_ && animatedMove_.isCapture) {
        // Captured pieces stay on the board until the moving checker reaches them.
        // Each one then fades independently before the next jump begins.
        for (int i = animationSegment_; i < animatedMove_.length; ++i) {
            if (i == animationSegment_ && fadingCount_ > 0) continue;
            const Piece captured = capturedPieces_[static_cast<std::size_t>(i)];
            if (captured != Piece::Empty)
                drawPiece(p, squareCenter(animatedMove_.captured[static_cast<std::size_t>(i)]),
                          isWhite(captured), isKing(captured), cell*.37);
        }
    }
    if (animating_ && fadingCount_ > 0 && captureOpacity_ > 0.0) {
        p.save();
        p.setOpacity(captureOpacity_);
        const Piece captured = fadingPieces_[0];
        if (captured != Piece::Empty)
            drawPiece(p, squareCenter(fadingSquares_[0]), isWhite(captured), isKing(captured), cell*.37);
        p.restore();
    }
    if (animating_ && animatedSquare_>=0) {
        // Render the moving piece at the interpolated position, not at its final square.
        drawPiece(p,animatedPosition_,isWhite(movingPiece_),isKing(movingPiece_),cell*.37*landingScale_);
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
    const int baseDuration = animatedMove_.isCapture ? 285 : 255;
    animation_.setDuration(qMax(1, baseDuration * 100 / animationSpeedPercent_));
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
    if (!landingPulseRunning_) {
        // A short, restrained rebound gives the completed move a tactile landing.
        // Promotion is revealed here, when the checker actually reaches its crown row.
        landingPulseRunning_ = true;
        movingPiece_ = finalPiece_;
        animatedSquare_ = animatedMove_.to();
        animatedPosition_ = squareCenter(animatedMove_.to());
        fadingCount_ = 0;
        captureOpacity_ = 0.0;
        landingAnimation_.stop();
        landingAnimation_.setDuration(qMax(1, 190 * 100 / animationSpeedPercent_));
        landingAnimation_.start();
        update();
        return;
    }
}

void BoardWidget::animateMove(const Move& move, const Board& resultingBoard) {
    const Board before = board_;
    if (!animationsEnabled_) {
        board_ = resultingBoard;
        animating_ = false;
        animatedSquare_ = -1;
        fadingCount_ = 0;
        movingPiece_ = Piece::Empty;
        finalPiece_ = Piece::Empty;
        landingScale_ = 1.0;
        update();
        emit animationFinished();
        return;
    }
    movingPiece_ = before.at(move.from);
    if (movingPiece_ == Piece::Empty || !move.valid()) {
        board_ = resultingBoard;
        update();
        emit animationFinished();
        return;
    }

    animatedMove_ = move;
    finalPiece_ = resultingBoard.at(move.to());
    landingScale_ = 1.0;
    landingPulseRunning_ = false;
    landingAnimation_.stop();
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
