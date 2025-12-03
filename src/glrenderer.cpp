#include "glrenderer.h"

#include <QCoreApplication>
#include "src/shaderloader.h"

#include <QMouseEvent>
#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"

GLRenderer::GLRenderer(QWidget *parent)
    : QOpenGLWidget(parent),
      m_ka(0.1),
      m_kd(0.8),
      m_ks(1),
      m_shininess(15.f),
      m_angleX(6),
      m_angleY(0),
      m_zoom(2)
{
    rebuildMatrices();
}

GLRenderer::~GLRenderer()
{
    makeCurrent();
    doneCurrent();
}

void GLRenderer::initializeGL()
{
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) fprintf(stderr, "Error while initializing GLEW: %s\n", glewGetErrorString(err));
    fprintf(stdout, "Successfully initialized GLEW %s\n", glewGetString(GLEW_VERSION));

    glClearColor(0,0,0,1);
    glDisable(GL_DEPTH_TEST);
    m_shader = ShaderLoader::createShaderProgram(":/resources/shaders/default.vert",":/resources/shaders/default.frag");

    //Fullscreen Quad
    float quadVertices[] = {
        // pos      // uv
        -1, -1,     0, 0,
        1, -1,     1, 0,
        -1,  1,     0, 1,

        -1,  1,     0, 1,
        1, -1,     1, 0,
        1,  1,     1, 1
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);

    // uv
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

    glBindVertexArray(0);
}

void GLRenderer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_shader);

    //Upload shape information
    int shape = 4;
    float shapeZoomMultiplier = 1.0f;
    glUniform1i(glGetUniformLocation(m_shader, "uShape"), shape);

    // Adjust camera distance per-shape
    switch(shape) {
        case 1:
            shapeZoomMultiplier = 1.4f; // Menger Sponge
            break;

        case 3:
            m_view = glm::lookAt(glm::vec3(0.0f, 5.0f, -10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            shapeZoomMultiplier = 3.f;
            break;

        case 4:
            shapeZoomMultiplier = 2.5f;
            break;
    }

    // Upload camera uniforms
    GLuint locView = glGetUniformLocation(m_shader, "uView");
    glUniformMatrix4fv(locView, 1, GL_FALSE, &m_view[0][0]);

    GLuint locProj = glGetUniformLocation(m_shader, "uProj");
    glUniformMatrix4fv(locProj, 1, GL_FALSE, &m_proj[0][0]);

    // Upload camera pos for shaders
    glm::vec3 camPos = glm::vec3(glm::inverse(m_view) * glm::vec4(0,0,0,1));
    camPos *= shapeZoomMultiplier;
    GLuint locCam = glGetUniformLocation(m_shader, "uCameraPos");
    glUniform3fv(locCam, 1, &camPos[0]);

    //Upload lights
    glUniform1i(glGetUniformLocation(m_shader, "numLights"), lights.size());

    for(int i=0; i<lights.size(); i++) {
        std::string posName = "lights[" + std::to_string(i) + "].position";
        std::string colName = "lights[" + std::to_string(i) + "].color";
        glUniform3fv(glGetUniformLocation(m_shader, posName.c_str()), 1, &lights[i].position[0]);
        glUniform3fv(glGetUniformLocation(m_shader, colName.c_str()), 1, &lights[i].color[0]);
    }

    // with material coefficients
    glUniform1f(glGetUniformLocation(m_shader,"ka"), m_ka);
    glUniform1f(glGetUniformLocation(m_shader,"kd"), m_kd);
    glUniform1f(glGetUniformLocation(m_shader,"ks"), m_ks);
    glUniform1f(glGetUniformLocation(m_shader,"shininess"), m_shininess);

    // and HDR toggle information
    glUniform1i(glGetUniformLocation(m_shader,"useHDR"), false);


    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
}


void GLRenderer::resizeGL(int w, int h)
{
    m_proj = glm::perspective(glm::radians(45.0),1.0 * w / h,0.01,100.0);
}

void GLRenderer::mousePressEvent(QMouseEvent *event) {
    // Set initial mouse position
    m_prevMousePos = event->pos();
}

void GLRenderer::mouseMoveEvent(QMouseEvent *event) {
    // Update angle member variables based on event parameters
    m_angleX += 10 * (event->position().x() - m_prevMousePos.x()) / (float) width();
    m_angleY += 10 * (event->position().y() - m_prevMousePos.y()) / (float) height();
    m_prevMousePos = event->pos();
    rebuildMatrices();
}

void GLRenderer::wheelEvent(QWheelEvent *event) {
    // Update zoom based on event parameter
    m_zoom -= event->angleDelta().y() / 100.f;
    rebuildMatrices();
}

void GLRenderer::rebuildMatrices() {
    // Update view matrix by rotating eye vector based on x and y angles
    m_view = glm::mat4(1);
    glm::mat4 rot = glm::rotate(glm::radians(-10 * m_angleX),glm::vec3(0,0,1));
    glm::vec3 eye = glm::vec3(2,0,0);
    eye = glm::vec3(rot * glm::vec4(eye,1));

    rot = glm::rotate(glm::radians(-10 * m_angleY),glm::cross(glm::vec3(0,0,1),eye));
    eye = glm::vec3(rot * glm::vec4(eye,1));

    eye = eye * m_zoom;

    m_view = glm::lookAt(eye,glm::vec3(0,0,0),glm::vec3(0,0,1));

    m_proj = glm::perspective(glm::radians(45.0),1.0 * width() / height(),0.01,100.0);

    update();
}
