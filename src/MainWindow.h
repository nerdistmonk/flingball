#pragma once
#include <QMainWindow>
#include "GameWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
private:
    GameWidget *gameWidget;
};