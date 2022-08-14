#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLWindow>
#include <atomic>
#include <qglobal.h>

QT_BEGIN_NAMESPACE
class QCloseEvent;
class QOpenGLBuffer;
class QOpenGLShaderProgram;
class QOpenGLTexture;
class QThread;
QT_END_NAMESPACE
class yuvWorker;

class yuvViewer : public QOpenGLWindow, protected QOpenGLFunctions {
  Q_OBJECT

public:
  explicit yuvViewer(QWindow *parent = nullptr);
  ~yuvViewer() Q_DECL_OVERRIDE;
  Q_DISABLE_COPY_MOVE(yuvViewer);

  bool getKey();
public slots:
  void onYuv444Ready(void *data, unsigned long length, quint32 w, quint32 h);

protected:
  void initializeGL() Q_DECL_OVERRIDE;
  void paintGL() Q_DECL_OVERRIDE;
  // void closeEvent(QCloseEvent *event);

private:
  QOpenGLBuffer *m_buffer{};
  QOpenGLShaderProgram *m_shaderProgram{};
  QOpenGLTexture *m_yuvTexture{};
  QThread *m_thread;
  yuvWorker *m_worker;
  GLint m_uniformLoc{};
  std::atomic_bool m_key;
  void* m_data{};
  quint32 m_height{}, m_width{};
};
