#include "MainWindow.h"
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Flingball");
    
    gameWidget = new GameWidget(this);
    setCentralWidget(gameWidget);

    QMenu *gameMenu = menuBar()->addMenu(tr("&Game"));
    
    // Using modern Qt6 signature: addAction(text, shortcut, object, slot)
    QAction *newGameAction = gameMenu->addAction(tr("&New Game"), QKeySequence(Qt::Key_F2), gameWidget, &GameWidget::startNewGame);
    QAction *quitAction = gameMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);
}