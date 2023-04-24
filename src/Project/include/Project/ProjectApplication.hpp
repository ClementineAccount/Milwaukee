#pragma once

#include <Milwaukee/Application.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <string_view>
#include <vector>
#include <memory>
#include <queue>
#include <functional>

class ProjectApplication final : public Application
{
protected:
    void AfterCreatedUiContext() override;
    void BeforeDestroyUiContext() override;

    bool Load() override;
    void RenderScene(double dt) override;
    void RenderUI(double dt) override;
    void Update(double dt) override;

private:

    void CreateBuffers();
    void ClearFBO(uint32_t fbo, glm::vec4 color);

    void DrawPixelsToScreen();

    //Render commands

    void DrawPixel(int32_t x, int32_t y, glm::vec4 color, int32_t x_offset = 0, int32_t y_offset = 0);
    void DrawPixelCentreOrigin(int32_t x, int32_t y, glm::vec4 color);


    void DrawPixelLineNaive(int32_t x_start, int32_t y_start, int32_t x_end, int32_t y_end, glm::vec4 color);
    void DrawLineBresenhamNaive(glm::i32vec2 start_pos, glm::i32vec2 end_pos, glm::vec4 color);
    
    void DrawLineBresenham(glm::i32vec2 start_pos, glm::i32vec2 end_pos, glm::vec4 color);


    //Some 'scenes' which are just collection of function calls
    void BuildSceneOneCommands();
    void RenderSceneOne();
    void RenderSceneTwo();


    bool MakeShader(std::string_view vertexShaderFilePath, std::string_view fragmentShaderFilePath);

private:
    uint32_t _shaderProgram;

    std::queue<std::function<void()>> renderCommandQueue;

    glm::vec4 clear_screen_color{0.0f, 1.0f, 0.0f, 1.0f};
    glm::vec4 pixel_clear_screen_color{0.2f, 0.2f, 0.2f, 1.0f};


    //Ideally should be a PBO that I can directly blit to default framebuffer later
    uint32_t  pixel_draw_fbo;

    //Destination for drawing/reading pixels
    uint32_t  pixel_texture;

    uint32_t currently_binded_fbo;

    //default framebuffer
    uint32_t screen_draw_fbo = 0;

    bool is_screen_dirty = true;

};