#include "WebRenderer.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <string>
#include <utility>

namespace {
constexpr const char* kVertexShaderSource = R"(#version 300 es
precision highp float;
out vec2 v_uv;

void main() {
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 position = positions[gl_VertexID];
    v_uv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

constexpr const char* kFragmentShaderSource = R"(#version 300 es
precision highp float;
in vec2 v_uv;

uniform vec2 u_resolution;
uniform float u_time;
uniform vec3 u_camera_pos;
uniform vec2 u_camera_angles;
uniform float u_dim;

out vec4 fragColor;

mat3 rotateY(float radians) {
    float s = sin(radians);
    float c = cos(radians);
    return mat3(
        c, 0.0, -s,
        0.0, 1.0, 0.0,
        s, 0.0, c
    );
}

mat3 rotateX(float radians) {
    float s = sin(radians);
    float c = cos(radians);
    return mat3(
        1.0, 0.0, 0.0,
        0.0, c, s,
        0.0, -s, c
    );
}

vec3 sampleSky(vec3 rayDir) {
    float horizon = clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 low = vec3(0.95, 0.79, 0.55);
    vec3 high = vec3(0.23, 0.54, 0.86);
    vec3 color = mix(low, high, pow(horizon, 0.72));

    vec3 sunDirection = normalize(vec3(0.35, 0.68, -0.42));
    float sun = pow(max(dot(rayDir, sunDirection), 0.0), 96.0);
    color += sun * vec3(1.0, 0.78, 0.46);
    return color;
}

void main() {
    vec2 ndc = v_uv * 2.0 - 1.0;
    ndc.x *= u_resolution.x / max(u_resolution.y, 1.0);

    vec3 rayDir = normalize(vec3(ndc.x, ndc.y, -1.7));
    rayDir = rotateY(u_camera_angles.x) * rotateX(u_camera_angles.y) * rayDir;

    vec3 rayOrigin = u_camera_pos;
    vec3 color = sampleSky(rayDir);

    if (rayDir.y < -0.01) {
        float hitDistance = -rayOrigin.y / rayDir.y;
        if (hitDistance > 0.0) {
            vec3 hit = rayOrigin + rayDir * hitDistance;
            vec2 cell = floor(hit.xz);
            float checker = mod(cell.x + cell.y, 2.0);
            vec3 groundA = vec3(0.19, 0.28, 0.16);
            vec3 groundB = vec3(0.25, 0.37, 0.21);
            color = mix(groundA, groundB, checker);

            float grid = min(abs(fract(hit.x) - 0.5), abs(fract(hit.z) - 0.5));
            float gridLine = smoothstep(0.03, 0.0, grid);
            color = mix(color, vec3(0.66, 0.77, 0.62), gridLine * 0.55);

            float towerPattern = step(0.82, fract((floor(hit.x / 6.0) + floor(hit.z / 6.0)) * 0.35 + 0.15));
            color = mix(color, vec3(0.40, 0.42, 0.46), towerPattern * 0.18);

            float fog = clamp(exp(-0.022 * hitDistance), 0.0, 1.0);
            color = mix(sampleSky(rayDir), color, fog);
        }
    }

    color = mix(color, color * 0.35, clamp(u_dim, 0.0, 1.0));
    fragColor = vec4(pow(color, vec3(0.96)), 1.0);
}
)";
} // namespace

namespace web {

bool WebRenderer::Initialize()
{
    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.alpha = EM_FALSE;
    attributes.depth = EM_FALSE;
    attributes.stencil = EM_FALSE;
    attributes.antialias = EM_TRUE;
    attributes.premultipliedAlpha = EM_FALSE;
    attributes.majorVersion = 2;
    attributes.minorVersion = 0;

    context_ = emscripten_webgl_create_context("#game-canvas", &attributes);
    if (context_ <= 0) {
        SetError("Failed to create a WebGL 2 context for #game-canvas.");
        return false;
    }

    if (emscripten_webgl_make_context_current(context_) != EMSCRIPTEN_RESULT_SUCCESS) {
        SetError("Failed to make the WebGL 2 context current.");
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    if (!BuildShaderProgram()) {
        return false;
    }

    Resize(renderScalePercent_);
    return true;
}

void WebRenderer::Resize(int renderScalePercent)
{
    renderScalePercent_ = std::clamp(renderScalePercent, 50, 150);

    double cssWidth = 1280.0;
    double cssHeight = 720.0;
    emscripten_get_element_css_size("#game-canvas", &cssWidth, &cssHeight);

    const int width = std::max(1, static_cast<int>(cssWidth * renderScalePercent_ / 100.0));
    const int height = std::max(1, static_cast<int>(cssHeight * renderScalePercent_ / 100.0));

    if (width == backbufferWidth_ && height == backbufferHeight_) {
        return;
    }

    backbufferWidth_ = width;
    backbufferHeight_ = height;
    emscripten_set_canvas_element_size("#game-canvas", backbufferWidth_, backbufferHeight_);
    glViewport(0, 0, backbufferWidth_, backbufferHeight_);
}

void WebRenderer::Render(const RenderState& state)
{
    if (!EnsureProgram()) {
        return;
    }

    glUseProgram(program_);
    glBindVertexArray(vao_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, backbufferWidth_, backbufferHeight_);
    glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUniform2f(resolutionUniform_, static_cast<float>(backbufferWidth_), static_cast<float>(backbufferHeight_));
    glUniform1f(timeUniform_, static_cast<float>(state.timeSeconds));
    glUniform3f(cameraPosUniform_,
        static_cast<float>(state.playerX),
        static_cast<float>(state.playerY),
        static_cast<float>(state.playerZ));
    glUniform2f(cameraAnglesUniform_, state.yawRadians, state.pitchRadians);
    glUniform1f(dimUniform_, state.dimStrength);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

const std::string& WebRenderer::LastError() const
{
    return lastError_;
}

bool WebRenderer::EnsureProgram()
{
    if (program_ != 0) {
        return true;
    }

    return BuildShaderProgram();
}

bool WebRenderer::BuildShaderProgram()
{
    const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShaderSource);
    if (vertexShader == 0) {
        return false;
    }

    const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader);
    glAttachShader(program_, fragmentShader);
    glLinkProgram(program_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);

        std::string infoLog(static_cast<std::size_t>(length), '\0');
        if (length > 0) {
            GLsizei written = 0;
            glGetProgramInfoLog(program_, length, &written, infoLog.data());
            infoLog.resize(static_cast<std::size_t>(std::max<GLsizei>(written, 0)));
        }

        glDeleteProgram(program_);
        program_ = 0;
        SetError("Failed to link the WebGL shader program: " + infoLog);
        return false;
    }

    resolutionUniform_ = glGetUniformLocation(program_, "u_resolution");
    timeUniform_ = glGetUniformLocation(program_, "u_time");
    cameraPosUniform_ = glGetUniformLocation(program_, "u_camera_pos");
    cameraAnglesUniform_ = glGetUniformLocation(program_, "u_camera_angles");
    dimUniform_ = glGetUniformLocation(program_, "u_dim");
    return true;
}

unsigned int WebRenderer::CompileShader(unsigned int shaderType, const char* source)
{
    const GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

        std::string infoLog(static_cast<std::size_t>(length), '\0');
        if (length > 0) {
            GLsizei written = 0;
            glGetShaderInfoLog(shader, length, &written, infoLog.data());
            infoLog.resize(static_cast<std::size_t>(std::max<GLsizei>(written, 0)));
        }

        glDeleteShader(shader);
        SetError("Failed to compile a WebGL shader: " + infoLog);
        return 0;
    }

    return shader;
}

void WebRenderer::SetError(std::string message)
{
    lastError_ = std::move(message);
}

} // namespace web
