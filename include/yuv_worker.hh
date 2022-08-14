#pragma once

#include <QObject>

class yuvViewer;

class yuvWorker : public QObject {
  Q_OBJECT

public:
  explicit yuvWorker(class yuvViewer *viewer, QObject *parent = nullptr);
  ~yuvWorker() Q_DECL_OVERRIDE;
  Q_DISABLE_COPY_MOVE(yuvWorker);

  void yuv444ReadyHelper(void *addr, size_t length, quint32 w, quint32 h);

signals:
  void yuv444Ready(void *, unsigned long, quint32, quint32);
  void finished();

public slots:
  void start();

private:
  void *m_yuvHandle;
  class yuvViewer *m_viewer;
};
