#pragma once

// Defined before including GLEW to suppress deprecation messages on macOS
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include "light.h"
#include "GL/glew.h" // Must always be first include
#include <QOpenGLWidget>
#include "glm/glm.hpp"

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)

class GLRenderer : public QOpenGLWidget
{
public:
    GLRenderer(QWidget *parent = nullptr);
    ~GLRenderer();

protected:
    void initializeGL()                  override; // Called once at the start of the program
    void paintGL()                       override; // Called every frame in a loop
    void resizeGL(int width, int height) override; // Called when window size changes
    void createHDRFBO(int w, int h);

    void mousePressEvent(QMouseEvent *e) override; // Used for camera movement
    void mouseMoveEvent(QMouseEvent *e)  override; // Used for camera movement
    void wheelEvent(QWheelEvent *e)      override; // Used for camera movement
    void rebuildMatrices();                        // Used for camera movement
    void finish();
    void createHDRFramebuffer(int w, int h);
    void renderSceneHDR();
    void toneMapToScreen();


private:
    GLuint m_phong_shader, m_tonemap_shader;
    GLuint m_quadVAO, m_quadVBO;
    GLuint m_hdrFBO, m_hdrColorTex, m_hdrDepthRBO;
    GLuint m_defaultFBO;
    glm::mat4 m_model = glm::mat4(1);
    glm::mat4 m_view  = glm::mat4(1);
    glm::mat4 m_proj  = glm::mat4(1);

    std::vector<Light> lights= {
        { {-5,  5,  5}, {0.9, 0.5, 0.2}},   // warm orange key
        { { 5,  4, -3}, {0.2, 0.4, 1.3}},   // cool blue fill
        { { 0, -3,  6}, {1.6, 1.6, 1.6}},   // soft white bounce
        { { 0,  6,  0}, {0.3, 1.5, 0.3}}    // subtle green rim
    };


    float m_ka;
    float m_kd;
    float m_ks;
    float m_shininess;

    QPoint m_prevMousePos;
    float  m_angleX;
    float  m_angleY;
    float  m_zoom;

    // GLuint m_phong_shader, m_tonemap_shader;
    int m_screen_width, m_screen_height;
    float m_devicePixelRatio;
//    glm::vec3 m_cameraPos (or compute from inverse view);
};
