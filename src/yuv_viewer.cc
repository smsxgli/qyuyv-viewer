#include "yuv_viewer.hh"
#include "yuv_worker.hh"
#include <QCloseEvent>
#include <QDebug>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QThread>
#include <cstdlib>

yuvViewer::yuvViewer(QWindow *parent)
    : QOpenGLWindow(NoPartialUpdate, parent), m_thread(new QThread(this)),
      m_worker(new yuvWorker(this, nullptr)) {
  m_key.store(true, std::memory_order_relaxed);
  connect(m_worker, &yuvWorker::yuv444Ready, this, &yuvViewer::onYuv444Ready);

  m_worker->moveToThread(m_thread);
  connect(m_thread, &QThread::started, m_worker, &yuvWorker::start);
  // connect(m_worker, &yuvWorker::finished, m_thread, &QThread::quit);
  connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);

  m_thread->start();
}

yuvViewer::~yuvViewer() {
  m_key.store(false, std::memory_order_release);
  m_thread->quit();
  m_thread->wait();
}

bool yuvViewer::getKey() { return m_key.load(std::memory_order_acquire); }

void yuvViewer::onYuv444Ready(void *data, unsigned long length, quint32 w,
                              quint32 h) {
  m_data = data;
  m_width = w;
  m_height = h;
  update();
}

void yuvViewer::initializeGL() {
  initializeOpenGLFunctions();
  GLfloat vertices[] = {
      -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, //
      -1.0f, 1.0f,  0.0f, 0.0f, 0.0f, //
      1.0f,  1.0f,  0.0f, 1.0f, 0.0f, //
      1.0f,  -1.0f, 0.0f, 1.0f, 1.0f, //
  };

  m_buffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
  m_buffer->create();
  m_buffer->bind();
  m_buffer->allocate(vertices, sizeof(vertices));

  const char *vsrc = "attribute highp vec3 vertex;\n"
                     "attribute highp vec2 texIn;\n"
                     "varying highp vec2 texOut;\n"
                     "void main(void)\n"
                     "{\n"
                     "    gl_Position = vec4(vertex, 1.0);\n"
                     "    texOut = texIn;\n"
                     "}";
  // 部分色域 BT.601 YUV转 RGB
  // https://zhuanlan.zhihu.com/p/436186749
  const char *fsrc =
      "varying highp vec2 texOut;\n"
      "uniform sampler2D texture;\n"
      "void main(void)\n"
      "{\n"
      "    highp vec3 yuv;\n"
      "    highp vec3 rgb;\n"
      "    yuv = texture2D(texture, texOut).rgb - vec3(0.0625, 0.5, 0.5);\n"
      "    rgb = mat3(1.164, 1.164, 1.164, 0, -0.391, 2.018, 1.596, -0.813, 0) "
      "* yuv;\n"
      "    gl_FragColor = vec4(rgb, 1.0);\n"
      "}";
  // 全色域 BT.601 YUV转 RGB
  // const char *fsrc =
  //     "varying highp vec2 texOut;\n"
  //     "uniform sampler2D texture;\n"
  //     "void main(void)\n"
  //     "{\n"
  //     "    highp vec3 yuv;\n"
  //     "    highp vec3 rgb;\n"
  //     "    yuv = texture2D(texture, texOut).rgb - vec3(0, 0.5, 0.5);\n"
  //     "    rgb = mat3(1, 1, 1, 0, -0.344, 1.772, 1.402, -0.714, 0) "
  //     "* yuv;\n"
  //     "    gl_FragColor = vec4(rgb, 1.0);\n"
  //     "}";

  m_shaderProgram = new QOpenGLShaderProgram(this);
  m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
  m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
  m_shaderProgram->link();
  m_shaderProgram->bind();
  int vertexLocation = m_shaderProgram->attributeLocation("vertex");
  int texInLocation = m_shaderProgram->attributeLocation("texIn");
  m_shaderProgram->enableAttributeArray(vertexLocation);
  m_shaderProgram->enableAttributeArray(texInLocation);
  m_shaderProgram->setAttributeBuffer(vertexLocation, GL_FLOAT, 0, 3,
                                      5 * sizeof(GLfloat));
  m_shaderProgram->setAttributeBuffer(
      texInLocation, GL_FLOAT, 3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));

  m_uniformLoc = m_shaderProgram->uniformLocation("texture");

  m_yuvTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
  m_yuvTexture->create();
}

void yuvViewer::paintGL() {
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  if (m_data) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_yuvTexture->textureId());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)m_width, (GLsizei)m_height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, m_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glUniform1i(m_uniformLoc, 0);
    std::free(m_data);
    m_data = nullptr;
  }

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// void yuvViewer::closeEvent(QCloseEvent *event) {
//   m_key.store(false, std::memory_order_release);
//   event->accept();
// }
