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

private:
    GLuint m_shader;     // Stores id of shader program
    GLuint m_quadVAO;
    GLuint m_quadVBO;

    glm::mat4 m_model = glm::mat4(1);
    glm::mat4 m_view  = glm::mat4(1);
    glm::mat4 m_proj  = glm::mat4(1);

    //Our lights
    std::vector<Light> lights = {
        // {{ 5, 5, 5 }, { 1.0f, 0.5f, 0.9f }}

        {{ 5, 5, 5 }, { 8.0f, 4.0f, 6.0f }}, //Use to highligh HDR rendering
        {{-5, 5, 5 }, { 3.0f, 8.0f, 2.0f }},
        {{ 0,-5, 5 }, { 6.0f, 2.0f, 1.0f }},
        {{ 0, 5,-5 }, { 2.0f, 3.0f,10.0f }}
    };
    GLuint m_defaultFBO = 2;

    GLuint hdrFBO = 0;
    GLuint hdrColorBuffer = 0;
    GLuint rboDepth = 0;
    bool m_useHDR = false;

    float m_ka;
    float m_kd;
    float m_ks;
    float m_shininess;

    QPoint m_prevMousePos;
    float  m_angleX;
    float  m_angleY;
    float  m_zoom;
};
