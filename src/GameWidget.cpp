#include "GameWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>
#include <QFile>
#include <QDataStream>
#include <QSettings>

static void generateBounceWavFile() {
    if (QFile::exists("bounce.wav")) return;

    QFile file("bounce.wav");
    if (file.open(QIODevice::WriteOnly)) {
        int sampleRate = 11025;
        int durationMs = 80;
        int totalSamples = (sampleRate * durationMs) / 1000;
        int dataSize = totalSamples * 2;

        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);

        out.writeRawData("RIFF", 4);
        out << quint32(36 + dataSize);
        out.writeRawData("WAVE", 4);

        out.writeRawData("fmt ", 4);
        out << quint32(16);
        out << quint16(1);
        out << quint16(1);
        out << quint32(sampleRate);
        out << quint32(sampleRate * 2);
        out << quint16(2);
        out << quint16(16);

        out.writeRawData("data", 4);
        out << quint32(dataSize);

        for (int i = 0; i < totalSamples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double envelope = 1.0 - (static_cast<double>(i) / totalSamples);
            double freq = 700.0 - (500.0 * (static_cast<double>(i) / totalSamples));
            double wave = std::sin(2.0 * 3.1415926535 * freq * t);
            
            int16_t sample = static_cast<int16_t>(32767.0 * wave * envelope * 0.5);
            out << sample;
        }
        file.close();
    }
}

GameWidget::GameWidget(QWidget *parent) : QWidget(parent), currentOrientation(Qt::Horizontal) {
    setFocusPolicy(Qt::StrongFocus);
    playArea = QRectF(80, 90, 600, 420);
    
    generateBounceWavFile();

    if (QFile::exists("bounce.wav")) {
        bounceSound.setSource(QUrl::fromLocalFile("bounce.wav"));
        bounceSound.setVolume(0.7f);
    }

    loadHighScore();

    connect(&timer, &QTimer::timeout, this, &GameWidget::updateGame);
    connect(&levelTimer, &QTimer::timeout, this, &GameWidget::updateTimerTick);

    score = 0;
    levelCompletePaused = false;
    startNewGame();
}

void GameWidget::loadHighScore() {
    QSettings settings("JezzballQt", "Jezzball");
    highScore = settings.value("highScore", 0).toInt();
}

void GameWidget::saveHighScore() {
    if (score > highScore) {
        highScore = score;
        QSettings settings("JezzballQt", "Jezzball");
        settings.setValue("highScore", highScore);
    }
}

void GameWidget::startNewGame() {
    level = 1;
    score = 0;
    levelCompletePaused = false;
    initLevel();
}

void GameWidget::initLevel() {
    clearedPercentage = 0.0;
    walls.clear();
    rooms.clear();
    rooms.append({playArea, false});
    activeWallBuilding = false;
    levelCompletePaused = false;

    int atomCount = qMin(50, level + 1);
    lives = atomCount;
    timeLeft = qMax(30, 90 - (level * 1));

    balls.clear();
    for (int i = 0; i < atomCount; ++i) {
        Ball b;
        b.radius = 9.0;
        double rx = playArea.x() + 60 + (i * 47) % (int(playArea.width()) - 120);
        double ry = playArea.y() + 60 + (i * 37) % (int(playArea.height()) - 120);
        b.pos = QPointF(rx, ry);

        double vx = (2.2 + QRandomGenerator::global()->generateDouble() * 1.2) * (QRandomGenerator::global()->bounded(2) == 0 ? 1 : -1);
        double vy = (2.2 + QRandomGenerator::global()->generateDouble() * 1.2) * (QRandomGenerator::global()->bounded(2) == 0 ? 1 : -1);
        b.velocity = QPointF(vx, vy);
        
        b.rotationAngle = QRandomGenerator::global()->generateDouble() * 6.28;
        b.rotationSpeed = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.15;
        
        balls.append(b);
    }

    timer.start(16);
    levelTimer.start(1000);
}

void GameWidget::updateTimerTick() {
    if (!levelCompletePaused && lives > 0 && clearedPercentage < 75.0) {
        timeLeft--;
        if (timeLeft <= 0) {
            lives--;
            if (lives <= 0) {
                timer.stop();
                levelTimer.stop();
                saveHighScore();
            } else {
                timeLeft = 60;
            }
        }
    }
}

void GameWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (levelCompletePaused) {
            if (level < 49) {
                level++;
            } else {
                level = 49;
            }
            initLevel();
            return;
        }

        if (activeWallBuilding || lives <= 0) return;

        QPointF clickPos = event->position();
        auto it = std::find_if(rooms.begin(), rooms.end(), [clickPos](const Room &r) {
            return !r.cleared && r.rect.contains(clickPos);
        });

        if (it != rooms.end()) {
            parentRoom = it->rect;
            activeWallBuilding = true;
            wallOrigin = clickPos;
            wallOrientation = currentOrientation;
            currentLengthNeg = 0;
            currentLengthPos = 0;
            stoppedNegByWall = false;
            stoppedPosByWall = false;
            destroyedNegByBall = false;
            destroyedPosByBall = false;
        }
    } else if (event->button() == Qt::RightButton && !levelCompletePaused) {
        currentOrientation = (currentOrientation == Qt::Horizontal) ? Qt::Vertical : Qt::Horizontal;
    }
    update();
}

void GameWidget::keyPressEvent(QKeyEvent *event) {
    if (levelCompletePaused) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space || event->key() == Qt::Key_Enter) {
            if (level < 49) level++; else level = 49;
            initLevel();
        }
        return;
    }

    if (event->key() == Qt::Key_Space) {
        currentOrientation = (currentOrientation == Qt::Horizontal) ? Qt::Vertical : Qt::Horizontal;
    }
}

void GameWidget::updateGame() {
    if (levelCompletePaused) return;

    if (activeWallBuilding) {
        double growthSpeed = 7.0;
        if (!stoppedNegByWall && !destroyedNegByBall) currentLengthNeg += growthSpeed;
        if (!stoppedPosByWall && !destroyedPosByBall) currentLengthPos += growthSpeed;

        QRectF negRect, posRect;
        if (wallOrientation == Qt::Horizontal) {
            negRect = QRectF(wallOrigin.x() - currentLengthNeg, wallOrigin.y() - 3, currentLengthNeg, 6);
            posRect = QRectF(wallOrigin.x(), wallOrigin.y() - 3, currentLengthPos, 6);
        } else {
            negRect = QRectF(wallOrigin.x() - 3, wallOrigin.y() - currentLengthNeg, 6, currentLengthNeg);
            posRect = QRectF(wallOrigin.x() - 3, wallOrigin.y(), 6, currentLengthPos);
        }

        if (!stoppedNegByWall && !destroyedNegByBall) {
            if (wallOrientation == Qt::Horizontal) {
                if (negRect.left() <= parentRoom.left()) stoppedNegByWall = true;
            } else {
                if (negRect.top() <= parentRoom.top()) stoppedNegByWall = true;
            }
        }

        if (!stoppedPosByWall && !destroyedPosByBall) {
            if (wallOrientation == Qt::Horizontal) {
                if (posRect.right() >= parentRoom.right()) stoppedPosByWall = true;
            } else {
                if (posRect.bottom() >= parentRoom.bottom()) stoppedPosByWall = true;
            }
        }

        for (const auto &ball : balls) {
            QRectF ballRect(ball.pos.x() - ball.radius, ball.pos.y() - ball.radius, ball.radius * 2, ball.radius * 2);
            
            if (!destroyedNegByBall && !stoppedNegByWall && negRect.intersects(ballRect)) {
                destroyedNegByBall = true;
                lives--;
                if (lives <= 0) {
                    timer.stop();
                    levelTimer.stop();
                    saveHighScore();
                }
            }
            if (!destroyedPosByBall && !stoppedPosByWall && posRect.intersects(ballRect)) {
                destroyedPosByBall = true;
                lives--;
                if (lives <= 0) {
                    timer.stop();
                    levelTimer.stop();
                    saveHighScore();
                }
            }
        }

        bool negDone = stoppedNegByWall || destroyedNegByBall;
        bool posDone = stoppedPosByWall || destroyedPosByBall;

        if (negDone && posDone && activeWallBuilding) {
            activeWallBuilding = false;

            if (stoppedNegByWall || stoppedPosByWall) {
                if (wallOrientation == Qt::Horizontal) {
                    double finalLeft = stoppedNegByWall ? parentRoom.left() : wallOrigin.x();
                    double finalRight = stoppedPosByWall ? parentRoom.right() : wallOrigin.x();
                    if (destroyedNegByBall) finalLeft = wallOrigin.x();
                    if (destroyedPosByBall) finalRight = wallOrigin.x();

                    if (finalLeft < finalRight) {
                        QRectF finalWallRect(finalLeft, wallOrigin.y() - 3, finalRight - finalLeft, 6);
                        walls.append({finalWallRect, wallOrientation});

                        bool isFullWall = stoppedNegByWall && stoppedPosByWall;

                        if (isFullWall) {
                            QRectF roomA(parentRoom.left(), parentRoom.top(), parentRoom.width(), finalWallRect.top() - parentRoom.top());
                            QRectF roomB(parentRoom.left(), finalWallRect.bottom(), parentRoom.width(), parentRoom.bottom() - finalWallRect.bottom());

                            rooms.erase(std::remove_if(rooms.begin(), rooms.end(), [this](const Room &r) {
                                return r.rect == parentRoom;
                            }), rooms.end());

                            rooms.append({roomA, !roomContainsBalls(roomA)});
                            rooms.append({roomB, !roomContainsBalls(roomB)});
                            calculateClearedPercentage();
                        }
                    }
                } else {
                    double finalTop = stoppedNegByWall ? parentRoom.top() : wallOrigin.y();
                    double finalBot = stoppedPosByWall ? parentRoom.bottom() : wallOrigin.y();
                    if (destroyedNegByBall) finalTop = wallOrigin.y();
                    if (destroyedPosByBall) finalBot = wallOrigin.y();

                    if (finalTop < finalBot) {
                        QRectF finalWallRect(wallOrigin.x() - 3, finalTop, 6, finalBot - finalTop);
                        walls.append({finalWallRect, wallOrientation});

                        bool isFullWall = stoppedNegByWall && stoppedPosByWall;

                        if (isFullWall) {
                            QRectF roomA(parentRoom.left(), parentRoom.top(), finalWallRect.left() - parentRoom.left(), parentRoom.height());
                            QRectF roomB(finalWallRect.right(), parentRoom.top(), parentRoom.right() - finalWallRect.right(), parentRoom.height());

                            rooms.erase(std::remove_if(rooms.begin(), rooms.end(), [this](const Room &r) {
                                return r.rect == parentRoom;
                            }), rooms.end());

                            rooms.append({roomA, !roomContainsBalls(roomA)});
                            rooms.append({roomB, !roomContainsBalls(roomB)});
                            calculateClearedPercentage();
                        }
                    }
                }
            }
        }
    }

    for (auto &ball : balls) {
        ball.pos += ball.velocity;
        ball.rotationAngle += ball.rotationSpeed;
        bool bounced = false;

        if (ball.pos.x() - ball.radius < playArea.left()) { ball.pos.setX(playArea.left() + ball.radius); ball.velocity.rx() *= -1; bounced = true; }
        if (ball.pos.x() + ball.radius > playArea.right()) { ball.pos.setX(playArea.right() - ball.radius); ball.velocity.rx() *= -1; bounced = true; }
        if (ball.pos.y() - ball.radius < playArea.top()) { ball.pos.setY(playArea.top() + ball.radius); ball.velocity.ry() *= -1; bounced = true; }
        if (ball.pos.y() + ball.radius > playArea.bottom()) { ball.pos.setY(playArea.bottom() - ball.radius); ball.velocity.ry() *= -1; bounced = true; }

        for (const auto &wall : walls) {
            QRectF ballRect(ball.pos.x() - ball.radius, ball.pos.y() - ball.radius, ball.radius * 2, ball.radius * 2);
            if (wall.rect.intersects(ballRect)) {
                double overlapLeft = ballRect.right() - wall.rect.left();
                double overlapRight = wall.rect.right() - ballRect.left();
                double overlapTop = ballRect.bottom() - wall.rect.top();
                double overlapBottom = wall.rect.bottom() - ballRect.top();

                double minX = qMin(overlapLeft, overlapRight);
                double minY = qMin(overlapTop, overlapBottom);

                if (minX < minY) {
                    if (overlapLeft < overlapRight) ball.pos.setX(wall.rect.left() - ball.radius);
                    else ball.pos.setX(wall.rect.right() + ball.radius);
                    ball.velocity.rx() *= -1;
                } else {
                    if (overlapTop < overlapBottom) ball.pos.setY(wall.rect.top() - ball.radius);
                    else ball.pos.setY(wall.rect.bottom() + ball.radius);
                    ball.velocity.ry() *= -1;
                }
                bounced = true;
            }
        }

        if (bounced && bounceSound.isLoaded()) {
            bounceSound.play();
        }
    }

    update();
}

bool GameWidget::roomContainsBalls(const QRectF &roomRect) {
    for (const auto &ball : balls) {
        if (roomRect.adjusted(2, 2, -2, -2).contains(ball.pos)) {
            return true;
        }
    }
    return false;
}

void GameWidget::calculateClearedPercentage() {
    double totalArea = playArea.width() * playArea.height();
    double clearedArea = 0;
    for (const auto &r : rooms) {
        if (r.cleared) {
            clearedArea += r.rect.width() * r.rect.height();
        }
    }
    clearedPercentage = (clearedArea / totalArea) * 100.0;

    if (clearedPercentage >= 75.0) {
        completeLevel();
    }
}

void GameWidget::completeLevel() {
    levelCompletePaused = true;
    timer.stop();
    levelTimer.stop();

    lastBaseScore = static_cast<int>(clearedPercentage * 100);
    lastTimeBonus = timeLeft * 40;
    lastLifeBonus = lives * 2500;

    score += lastBaseScore + lastTimeBonus + lastLifeBonus;
    saveHighScore();
}

void GameWidget::drawGridPattern(QPainter &painter, const QRectF &rect, bool cleared) {
    int tileSize = 20;
    QColor tileBase = cleared ? QColor(45, 52, 64) : QColor(32, 38, 48);
    QColor gridLine = cleared ? QColor(60, 70, 85) : QColor(45, 55, 70);

    painter.fillRect(rect, tileBase);
    painter.setPen(QPen(gridLine, 1));

    painter.save();
    painter.setClipRect(rect);

    for (double x = rect.left(); x <= rect.right(); x += tileSize) {
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    for (double y = rect.top(); y <= rect.bottom(); y += tileSize) {
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }

    painter.restore();
}

void GameWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), QColor(15, 18, 25));

    for (const auto &r : rooms) {
        drawGridPattern(painter, r.rect, r.cleared);
    }

    painter.setPen(QPen(QColor(100, 115, 140), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(playArea);

    for (const auto &wall : walls) {
        painter.fillRect(wall.rect, QColor(180, 190, 205));
        
        painter.setPen(QColor(230, 240, 250));
        painter.drawLine(wall.rect.topLeft(), wall.rect.topRight());
        painter.drawLine(wall.rect.topLeft(), wall.rect.bottomLeft());

        painter.setPen(QColor(100, 110, 125));
        painter.drawLine(wall.rect.bottomLeft(), wall.rect.bottomRight());
        painter.drawLine(wall.rect.topRight(), wall.rect.bottomRight());
    }

    if (activeWallBuilding) {
        double leftX = qMax(parentRoom.left(), wallOrigin.x() - (stoppedNegByWall ? parentRoom.width() : currentLengthNeg));
        double rightX = qMin(parentRoom.right(), wallOrigin.x() + (stoppedPosByWall ? parentRoom.width() : currentLengthPos));
        double topY = qMax(parentRoom.top(), wallOrigin.y() - (stoppedNegByWall ? parentRoom.height() : currentLengthNeg));
        double botY = qMin(parentRoom.bottom(), wallOrigin.y() + (stoppedPosByWall ? parentRoom.height() : currentLengthPos));

        QRectF activeRect;
        if (wallOrientation == Qt::Horizontal) {
            double drawLeft = destroyedNegByBall ? wallOrigin.x() : leftX;
            double drawRight = destroyedPosByBall ? wallOrigin.x() : rightX;
            if (drawLeft < drawRight) {
                activeRect = QRectF(drawLeft, wallOrigin.y() - 3, drawRight - drawLeft, 6);
                painter.fillRect(activeRect, QColor(235, 50, 50));
                painter.setPen(QColor(255, 255, 255));
                painter.drawRect(activeRect);
            }
        } else {
            double drawTop = destroyedNegByBall ? wallOrigin.y() : topY;
            double drawBot = destroyedPosByBall ? wallOrigin.y() : botY;
            if (drawTop < drawBot) {
                activeRect = QRectF(wallOrigin.x() - 3, drawTop, 6, drawBot - drawTop);
                painter.fillRect(activeRect, QColor(235, 50, 50));
                painter.setPen(QColor(255, 255, 255));
                painter.drawRect(activeRect);
            }
        }
    }

    for (const auto &ball : balls) {
        painter.setBrush(QColor(0, 0, 0, 80));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(ball.pos.x() + 2, ball.pos.y() + 2), ball.radius, ball.radius);

        painter.save();
        painter.translate(ball.pos);
        painter.rotate(ball.rotationAngle * (180.0 / 3.14159));

        painter.setBrush(QColor(40, 200, 80));
        painter.setPen(QPen(QColor(20, 100, 40), 1));
        painter.drawChord(QRectF(-ball.radius, -ball.radius, ball.radius * 2, ball.radius * 2), 90 * 16, 180 * 16);

        painter.setBrush(QColor(240, 240, 240));
        painter.setPen(QPen(QColor(160, 160, 160), 1));
        painter.drawChord(QRectF(-ball.radius, -ball.radius, ball.radius * 2, ball.radius * 2), 270 * 16, 180 * 16);

        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(-3, -3), 2.0, 2.0);

        painter.restore();
    }

    // Countdown Time Bar display properly scaled & shrinking correctly
    double maxTime = qMax(30.0, 90.0 - (level * 1.0));
    double timeRatio = qMax(0.0, qMin(1.0, static_cast<double>(timeLeft) / maxTime));
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(220, 160, 40));
    painter.drawRect(QRectF(playArea.left(), playArea.top() - 14, playArea.width() * timeRatio, 6));
    
    painter.setPen(QColor(100, 115, 140));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(playArea.left(), playArea.top() - 14, playArea.width(), 6));

    if (levelCompletePaused) {
        painter.setBrush(QColor(10, 15, 25, 210));
        painter.drawRect(playArea);

        painter.setPen(Qt::white);
        painter.setFont(QFont("Sans Serif", 20, QFont::Bold));
        painter.drawText(playArea.center().x() - 120, playArea.top() + 70, "LEVEL COMPLETE!");

        painter.setFont(QFont("Sans Serif", 12, QFont::Normal));
        int startY = playArea.top() + 130;
        int spacing = 30;

        painter.drawText(playArea.center().x() - 150, startY, QString("Base Clear Score: +%1").arg(lastBaseScore));
        painter.drawText(playArea.center().x() - 150, startY + spacing, QString("Time Bonus: +%1").arg(lastTimeBonus));
        painter.drawText(playArea.center().x() - 150, startY + (spacing * 2), QString("Life Bonus: +%1").arg(lastLifeBonus));

        painter.setPen(QColor(255, 200, 0));
        painter.setFont(QFont("Sans Serif", 13, QFont::Bold));
        painter.drawText(playArea.center().x() - 150, startY + (spacing * 3.8), QString("Total Score: %1").arg(score));
        painter.drawText(playArea.center().x() - 150, startY + (spacing * 4.8), QString("High Score: %1").arg(highScore));

        painter.setPen(Qt::cyan);
        painter.setFont(QFont("Sans Serif", 11, QFont::Bold));
        painter.drawText(playArea.center().x() - 130, playArea.bottom() - 40, "Click anywhere to continue");
    }

    if (lives <= 0) {
        painter.setPen(Qt::red);
        painter.setFont(QFont("Sans Serif", 28, QFont::Bold));
        painter.drawText(playArea.center().x() - 130, playArea.center().y(), "GAME OVER");
    }

    painter.setPen(QColor(220, 230, 240));
    painter.setFont(QFont("Sans Serif", 10, QFont::Bold));
    painter.drawText(80, 40, QString("Score: %1   High: %2   Level: %3   Lives: %4   Time: %5s   Cleared: %6%")
                     .arg(score)
                     .arg(highScore)
                     .arg(level)
                     .arg(lives)
                     .arg(timeLeft)
                     .arg(clearedPercentage, 0, 'f', 1));
}