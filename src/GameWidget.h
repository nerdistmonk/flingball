#pragma once
#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QPointF>
#include <QRectF>
#include <QSoundEffect>
#include <QUrl>

struct Ball {
    QPointF pos;
    QPointF velocity;
    double radius;
    double rotationAngle;
    double rotationSpeed;
};

struct Wall {
    QRectF rect;
    Qt::Orientation orientation;
};

struct Room {
    QRectF rect;
    bool cleared;
};

class GameWidget : public QWidget {
    Q_OBJECT
public:
    GameWidget(QWidget *parent = nullptr);

public slots:
    void startNewGame();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateGame();
    void updateTimerTick();

private:
    QTimer timer;
    QTimer levelTimer;
    Qt::Orientation currentOrientation;
    
    QRectF playArea;
    QVector<Ball> balls;
    QVector<Wall> walls;
    QVector<Room> rooms;

    int level;
    int lives;
    int score;
    int highScore; // Persistent high score
    int timeLeft;
    double clearedPercentage;
    bool activeWallBuilding;
    bool levelCompletePaused;

    int lastBaseScore;
    int lastTimeBonus;
    int lastLifeBonus;

    QPointF wallOrigin;
    Qt::Orientation wallOrientation;
    QRectF parentRoom;
    double currentLengthNeg;
    double currentLengthPos;
    bool stoppedNegByWall;
    bool stoppedPosByWall;
    bool destroyedNegByBall;
    bool destroyedPosByBall;

    QSoundEffect bounceSound;

    void initLevel();
    void calculateClearedPercentage();
    void completeLevel();
    bool roomContainsBalls(const QRectF &roomRect);
    void drawGridPattern(QPainter &painter, const QRectF &rect, bool cleared);
    void loadHighScore();
    void saveHighScore();
};