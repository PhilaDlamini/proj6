#include "glrenderer.h"

#include <QCoreApplication>
#include "src/shaderloader.h"

#include <QMouseEvent>
#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"


GLRenderer::GLRenderer(QWidget *parent)
    : QOpenGLWidget(parent),
    m_ka(0.1f),
    m_kd(0.8f),
    m_ks(1.0f),
    m_shininess(16.0f),
    m_angleX(6),
    m_angleY(0),
    m_zoom(2.0f),
    m_hdrFBO(0),
    m_hdrColorTex(0),
    m_hdrDepthRBO(0),
    m_quadVAO(0),
    m_quadVBO(0),
    m_phong_shader(0),
    m_tonemap_shader(0)
{
    // initialize matrices
    rebuildMatrices();
}

GLRenderer::~GLRenderer()
{
    makeCurrent();
    finish();
    doneCurrent();
}

void GLRenderer::finish()
{
    // Delete GL programs
    if (m_phong_shader) glDeleteProgram(m_phong_shader);
    if (m_tonemap_shader) glDeleteProgram(m_tonemap_shader);

    // Delete fullscreen quad
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);

    // Delete HDR framebuffer resources
    if (m_hdrColorTex) glDeleteTextures(1, &m_hdrColorTex);
    if (m_hdrDepthRBO) glDeleteRenderbuffers(1, &m_hdrDepthRBO);
    if (m_hdrFBO) glDeleteFramebuffers(1, &m_hdrFBO);
}

void GLRenderer::initializeGL()
{
    m_devicePixelRatio = this->devicePixelRatio();
    m_defaultFBO = 2;

    // initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK)
        fprintf(stderr, "Error while initializing GLEW: %s\n", glewGetErrorString(err));
    fprintf(stdout, "Successfully initialized GLEW %s\n", glewGetString(GLEW_VERSION));

    // GL default state
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST); // raymarch doesn't need standard depth test
    glEnable(GL_CULL_FACE);

    // Compile shaders
    m_phong_shader   = ShaderLoader::createShaderProgram(":/resources/shaders/phong.vert",   ":/resources/shaders/phong.frag");
    m_tonemap_shader = ShaderLoader::createShaderProgram(":/resources/shaders/tonemap.vert",   ":/resources/shaders/tonemap.frag");

    // Fullscreen quad: position XY, uv
    float quadVertices[] = {
        // pos      // uv
        -1.0f, -1.0f,  0.0f, 0.0f,
        1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
        1.0f, -1.0f,  1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // pos attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // uv attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Setup sizes and HDR FBO
    m_screen_width  = width()  * m_devicePixelRatio;
    m_screen_height = height() * m_devicePixelRatio;
    createHDRFramebuffer(m_screen_width, m_screen_height);

    // Set tonemap shader sampler once
    glUseProgram(m_tonemap_shader);
    GLint loc = glGetUniformLocation(m_tonemap_shader, "uHDRTexture");
    if (loc >= 0) glUniform1i(loc, 0);
    glUseProgram(0);
}

// HDR Framebuffer Creation
void GLRenderer::createHDRFramebuffer(int w, int h)
{
    // delete old resources if they exist
    if (m_hdrColorTex) { glDeleteTextures(1, &m_hdrColorTex); m_hdrColorTex = 0; }
    if (m_hdrDepthRBO) { glDeleteRenderbuffers(1, &m_hdrDepthRBO); m_hdrDepthRBO = 0; }
    if (m_hdrFBO)      { glDeleteFramebuffers(1, &m_hdrFBO); m_hdrFBO = 0; }

    m_screen_width  = w;
    m_screen_height = h;

    // Create float color texture (HDR)
    glGenTextures(1, &m_hdrColorTex);
    glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_screen_width, m_screen_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Clamp to edge to avoid wrap seams
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create depth renderbuffer (optional - but good practice)
    glGenRenderbuffers(1, &m_hdrDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_hdrDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_screen_width, m_screen_height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create framebuffer and attach
    glGenFramebuffers(1, &m_hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrColorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_hdrDepthRBO);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "ERROR: HDR Framebuffer not complete (status 0x%x)\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLRenderer::paintGL()
{
    // 1) Render scene into the HDR framebuffer using phong shader
    renderSceneHDR();

    // 2) Tone-map the HDR texture to the default framebuffer
    toneMapToScreen();
}

void GLRenderer::renderSceneHDR()
{
    // Bind HDR FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_screen_width, m_screen_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use phong shader
    glUseProgram(m_phong_shader);

    //Upload shape information
    int shape = 0;
    float shapeZoomMultiplier = 1.0f;
    glUniform1i(glGetUniformLocation(m_phong_shader, "uShape"), shape);

    // Adjust camera distance per-shape
    switch(shape) {
    case 1:
        shapeZoomMultiplier = 1.4f;
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
    GLuint locView = glGetUniformLocation(m_phong_shader, "uView");
    glUniformMatrix4fv(locView, 1, GL_FALSE, &m_view[0][0]);

    GLuint locProj = glGetUniformLocation(m_phong_shader, "uProj");
    glUniformMatrix4fv(locProj, 1, GL_FALSE, &m_proj[0][0]);

    // Upload camera pos for shaders
    glm::vec3 camPos = glm::vec3(glm::inverse(m_view) * glm::vec4(0,0,0,1));
    camPos *= shapeZoomMultiplier;
    GLuint locCam = glGetUniformLocation(m_phong_shader, "uCameraPos");
    glUniform3fv(locCam, 1, &camPos[0]);

    //Upload lights
    glUniform1i(glGetUniformLocation(m_phong_shader, "numLights"), lights.size());

    for(int i=0; i<lights.size(); i++) {
        std::string posName = "lights[" + std::to_string(i) + "].position";
        std::string colName = "lights[" + std::to_string(i) + "].color";
        glUniform3fv(glGetUniformLocation(m_phong_shader, posName.c_str()), 1, &lights[i].position[0]);
        glUniform3fv(glGetUniformLocation(m_phong_shader, colName.c_str()), 1, &lights[i].color[0]);
    }

    // with material coefficients
    glUniform1f(glGetUniformLocation(m_phong_shader,"ka"), m_ka);
    glUniform1f(glGetUniformLocation(m_phong_shader,"kd"), m_kd);
    glUniform1f(glGetUniformLocation(m_phong_shader,"ks"), m_ks);
    glUniform1f(glGetUniformLocation(m_phong_shader,"shininess"), m_shininess);


    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);


}

void GLRenderer::toneMapToScreen()
{
    // Bind default framebuffer (screen)
    glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFBO);
    glViewport(0, 0, width() * m_devicePixelRatio, height() * m_devicePixelRatio);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_tonemap_shader);

    // Bind HDR texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);


    // draw fullscreen quad
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void GLRenderer::resizeGL(int w, int h)
{
    // update device pixel scaled size
    m_devicePixelRatio = this->devicePixelRatio();
    m_screen_width  = w * m_devicePixelRatio;
    m_screen_height = h * m_devicePixelRatio;

    // Recreate HDR FBO at new size
    createHDRFramebuffer(m_screen_width, m_screen_height);

    // Update projection
    m_proj = glm::perspective(glm::radians(45.0f), 1.0f * w / h, 0.01f, 100.0f);
}


void GLRenderer::mousePressEvent(QMouseEvent *event)
{
    m_prevMousePos = event->pos();
}

void GLRenderer::mouseMoveEvent(QMouseEvent *event)
{
    m_angleX += 10 * (event->position().x() - m_prevMousePos.x()) / (float) width();
    m_angleY += 10 * (event->position().y() - m_prevMousePos.y()) / (float) height();
    m_prevMousePos = event->pos();
    rebuildMatrices();
}

void GLRenderer::wheelEvent(QWheelEvent *event)
{
    m_zoom -= event->angleDelta().y() / 100.f;
    rebuildMatrices();
}

void GLRenderer::rebuildMatrices()
{
    // Update view matrix by rotating eye vector based on x and y angles
    m_view = glm::mat4(1.0f);
    glm::mat4 rot = glm::rotate(glm::radians(-10.0f * (float)m_angleX), glm::vec3(0,0,1));
    glm::vec3 eye = glm::vec3(2.0f,0,0);
    eye = glm::vec3(rot * glm::vec4(eye,1.0f));

    rot = glm::rotate(glm::radians(-10.0f * (float)m_angleY), glm::cross(glm::vec3(0,0,1), eye));
    eye = glm::vec3(rot * glm::vec4(eye,1.0f));

    eye = eye * m_zoom;
    m_view = glm::lookAt(eye, glm::vec3(0,0,0), glm::vec3(0,0,1));
    update();
}
