#pragma once

#include <Milwaukee/Application.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <string_view>
#include <vector>
#include <memory>

class ProjectApplication final : public Application
{
protected:
    void AfterCreatedUiContext() override;
    void BeforeDestroyUiContext() override;
    bool Load() override;
    void RenderScene() override;
    void RenderUI() override;
    void Update() override;

private:
    uint32_t _shaderProgram;

    bool MakeShader(std::string_view vertexShaderFilePath, std::string_view fragmentShaderFilePath);

    glm::vec4 clear_screen_color{0.0f, 1.0f, 0.0f, 1.0f};
    void ClearScreen();
};