#include "MainWindow.hpp"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  qDebug() << argv[0] << argv[1];

  MainWindow mainWindow(argv[1]);
  mainWindow.setWindowTitle("Resizable Collapsible Sidebar");

  app.installEventFilter(&mainWindow);

  mainWindow.showFullScreen();

  return app.exec();
}

#include "main.moc"