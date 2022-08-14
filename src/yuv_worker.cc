#include "yuv_worker.hh"
#include "yuv_viewer.hh"
#include <QDebug>
#include <exception>

extern "C" {
extern int yuv_dev_create(void **);
extern void yuv_dev_destroy(void *);
extern int yuv_start_capture(void *);
extern int yuv_stop_capture(void *);
extern int yuv_loop(void *, int (*)(void *),
                    int (*)(void *, size_t, unsigned int, unsigned int, void *),
                    void *);
}

yuvWorker::yuvWorker(class yuvViewer *viewer, QObject *parent)
    : QObject(parent), m_viewer(viewer) {
  if (yuv_dev_create(&m_yuvHandle)) {
    throw std::runtime_error("Failed at yuv_dev_create!");
  }
}

yuvWorker::~yuvWorker() { yuv_dev_destroy(m_yuvHandle); }

void yuvWorker::yuv444ReadyHelper(void *addr, unsigned long length, quint32 w,
                                  quint32 h) {
  emit yuv444Ready(addr, length, w, h);
}

void yuvWorker::start() {
  if (yuv_start_capture(m_yuvHandle)) {
    qCritical() << "Failed at yuv_start_capture!";
    emit finished();
    return;
  }

  auto k = [](void *ptr) -> int {
    yuvWorker *thiz = static_cast<yuvWorker *>(ptr);
    return thiz->m_viewer->getKey() == true;
  };

  auto cb = [](void *addr, size_t length, unsigned int width,
               unsigned int height, void *ptr) -> int {
    yuvWorker *thiz = static_cast<yuvWorker *>(ptr);
    thiz->yuv444ReadyHelper(addr, length, width, height);

    return 0;
  };

  if (yuv_loop(m_yuvHandle, k, cb, this)) {
    qCritical() << "Failed at yuv_loop!";
  }

  if (yuv_stop_capture(m_yuvHandle)) {
    qCritical() << "Failed at yuv_stop_capture!";
    emit finished();
    return;
  }

  emit finished();
}
