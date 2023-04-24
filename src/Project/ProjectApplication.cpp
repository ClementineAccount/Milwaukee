#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <Project/ProjectApplication.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <spdlog/spdlog.h>

#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <vector>
#include <queue>
#include <set>


void ProjectApplication::AfterCreatedUiContext()
{
}

void ProjectApplication::BeforeDestroyUiContext()
{

}

void ProjectApplication::CreateBuffers()
{
    glCreateTextures(GL_TEXTURE_2D, 1, &pixel_texture);

    glTextureParameteri(pixel_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTextureParameteri(pixel_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTextureParameteri(pixel_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(pixel_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTextureStorage2D(pixel_texture, 1, GL_RGBA8, windowWidth, windowHeight);

    glCreateFramebuffers(1, &pixel_draw_fbo);
    glNamedFramebufferTexture(pixel_draw_fbo, GL_COLOR_ATTACHMENT0, pixel_texture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, pixel_draw_fbo);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    currently_binded_fbo = 0;
}

bool ProjectApplication::Load()
{
    if (!Application::Load())
    {
        spdlog::error("App: Unable to load");
        return false;
    }

    if (!MakeShader("./data/shaders/main.vs.glsl", "./data/shaders/main.fs.glsl"))
    {
        return false;
    }

    CreateBuffers();

    return true;
}

void ProjectApplication::Update()
{
    if (IsKeyPressed(GLFW_KEY_ESCAPE))
    {
        Close();
    }
}

void ProjectApplication::ClearFBO(uint32_t fbo, glm::vec4 color)
{
    if (fbo != currently_binded_fbo)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, currently_binded_fbo);
    }
    else
    {
        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void ProjectApplication::DrawPixelsToScreen()
{
    glBlitNamedFramebuffer(pixel_draw_fbo, screen_draw_fbo, 0, 0, windowWidth, windowHeight, 0, 0, windowWidth, windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void ProjectApplication::DrawPixel(int32_t x, int32_t y, glm::vec4 color)
{
    glTextureSubImage2D(pixel_texture, 0, x, y, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(color));
}



void ProjectApplication::DrawPixelLineNaive(int32_t x_start, int32_t y_start, int32_t x_end, int32_t y_end, glm::vec4 color)
{
    //Must be left to right under the naive interperaiton
    assert(x_start <= x_end);

    int32_t y_curr = y_start;
    for (int32_t x_curr = x_start; x_curr < x_end; x_curr += 1)
    {
        DrawPixel(x_curr, y_curr, color);
        y_curr = y_curr < y_end ? y_curr + 1 : y_curr;
    }
}

void ProjectApplication::DrawLineBresenham(glm::i32vec2 start_pos, glm::i32vec2 end_pos, glm::vec4 color)
{
    int32_t dx = end_pos.x - start_pos.x;
    int32_t dy = end_pos.y - start_pos.y;
    int32_t D = 2 * dy -  dx;

    int32_t y = start_pos.y;
    for (int32_t x = start_pos.x; x < end_pos.x; x += 1)
    {
        DrawPixel(x, y, color);
        if (D > 0)
        {
            y = y + 1;
            D = D - 2 * dx;
        }
        D = D + 2 * dy;
    }
}

void ProjectApplication::RenderScene()
{
    RenderSceneOne();
}

void ProjectApplication::RenderSceneOne()
{
    ClearFBO(pixel_draw_fbo, pixel_clear_screen_color);
    ClearFBO(screen_draw_fbo, clear_screen_color);

    DrawPixel(10, 10, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    DrawPixel(11, 11, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    DrawPixel(12, 12, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    DrawPixelLineNaive(0, 40, 800, 700, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    DrawLineBresenham(glm::i32vec2(0, 40), glm::i32vec2(800, 700), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    DrawPixelsToScreen();
}

void ProjectApplication::RenderUI()
{
    ImGui::Begin("Window");
    {
        ImGui::TextUnformatted("Hello World!");
        ImGui::End();
    }
}


bool ProjectApplication::MakeShader(std::string_view vertexShaderFilePath, std::string_view fragmentShaderFilePath)
{
    namespace fs = std::filesystem;

    auto Slurp = [&](std::string_view path)
    {
        std::ifstream file(path.data(), std::ios::ate);
        std::string result(file.tellg(), '\0');
        file.seekg(0);
        file.read((char*)result.data(), result.size());
        return result;
    };


    int success = false;
    char log[1024] = {};
    const auto vertexShaderSource = Slurp(vertexShaderFilePath);
    const char* vertexShaderSourcePtr = vertexShaderSource.c_str();
    const auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSourcePtr, nullptr);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 1024, nullptr, log);
        spdlog::error(log);
        return false;
    }

    const auto fragmentShaderSource = Slurp(fragmentShaderFilePath);
    const char* fragmentShaderSourcePtr = fragmentShaderSource.c_str();
    const auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSourcePtr, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 1024, nullptr, log);
        spdlog::error(log);
        return false;
    }

    _shaderProgram = glCreateProgram();
    glAttachShader(_shaderProgram, vertexShader);
    glAttachShader(_shaderProgram, fragmentShader);
    glLinkProgram(_shaderProgram);
    glGetProgramiv(_shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(_shaderProgram, 1024, nullptr, log);
        spdlog::error(log);

        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}