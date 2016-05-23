#include "MainWindow.hpp"
#include "DirectXWidget.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setCentralWidget(new DirectXWidget(this));
}

MainWindow::~MainWindow()
{

}
