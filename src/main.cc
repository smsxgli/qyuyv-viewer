#include "yuv_viewer.hh"
#include <QGuiApplication>

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);

  yuvViewer viewer;

  viewer.show();

  return app.exec();
}
