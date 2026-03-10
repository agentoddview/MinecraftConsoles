#pragma once

#include <emscripten/html5.h>

#include <string>

namespace web {

struct RenderState {
    double timeSeconds = 0.0;
    double playerX = 0.0;
    double playerY = 1.7;
    double playerZ = 6.0;
    float yawRadians = 3.1415927f;
    float pitchRadians = -0.18f;
    float dimStrength = 0.0f;
};

class WebRenderer {
public:
    bool Initialize();
    void Resize(int renderScalePercent);
    void Render(const RenderState& state);

    const std::string& LastError() const;

private:
    bool EnsureProgram();
    bool BuildShaderProgram();
    unsigned int CompileShader(unsigned int shaderType, const char* source);
    void SetError(std::string message);

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context_ = 0;
    unsigned int program_ = 0;
    unsigned int vao_ = 0;

    int resolutionUniform_ = -1;
    int timeUniform_ = -1;
    int cameraPosUniform_ = -1;
    int cameraAnglesUniform_ = -1;
    int dimUniform_ = -1;

    int backbufferWidth_ = 0;
    int backbufferHeight_ = 0;
    int renderScalePercent_ = 100;
    std::string lastError_;
};

} // namespace web
