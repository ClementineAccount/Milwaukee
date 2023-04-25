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
#include <iostream>


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



    current_brush_length = starting_brush_length;

    CreateBuffers();
    BuildSceneOneCommands();
    return true;
}

void ProjectApplication::Update( double dt)
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

void ProjectApplication::DrawPixel(int32_t x, int32_t y, glm::vec4 color, int32_t x_offset, int32_t y_offset)
{
    //Bounds check. Simply don't draw if out of bounds
    if (x + x_offset < 0 || y + y_offset < 0)
        return;

    if (x + x_offset > windowWidth || y + y_offset > windowHeight)
        return;

    glTextureSubImage2D(pixel_texture, 0, x + x_offset, y + y_offset, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(color));
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

void ProjectApplication::DrawLineBresenhamNaive(glm::i32vec2 start_pos, glm::i32vec2 end_pos, glm::vec4 color)
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

void ProjectApplication::DrawLineBresenham(glm::i32vec2 start_pos, glm::i32vec2 end_pos, glm::vec4 color, bool centerOrigin)
{

    int32_t dx = end_pos.x - start_pos.x;
    dx = dx > 0 ? dx : -dx;

    int32_t sx = start_pos.x < end_pos.x ? 1 : -1;

    int32_t dy = end_pos.y - start_pos.y;
    dy = dy > 0 ? -dy : dy;

    int32_t sy = start_pos.y < end_pos.y ? 1 : -1;
    int32_t error = dx + dy;

    int32_t x = start_pos.x;
    int32_t y = start_pos.y;

    int32_t error_doubled = error * 2;

    while (x != end_pos.x || y != end_pos.y)
    {
        if (centerOrigin)
            DrawPixelCentreOrigin(x, y, color);
        else
            DrawPixel(x, y, color);
        
        int32_t error_doubled = error * 2;
        if (error_doubled >= dy)
        {
            if (x == end_pos.x)
                return;
            error += dy;
            x += sx;
        }
        if (error_doubled <= dx)
        {
            if (y == end_pos.y)
                return;
            error += dx;
            y += sy;
        }
    }
}

void ProjectApplication::DrawPixelCentreOrigin(int32_t x, int32_t y, glm::vec4 color)
{
    DrawPixel(x, y, color, windowWidth_half, windowHeight_half);
}

void ProjectApplication::DrawFilledSquare(glm::i32vec2 center, glm::vec4 color, int32_t length, bool centerOrigin)
{
    //Start from the top right possible
    glm::i32vec2 bottom_left = center - glm::i32vec2(length / 2, length / 2);

    //Draw row by row from the top left
    for (int32_t y = bottom_left.y; y < bottom_left.y + length; y += 1)
    {
        for (int32_t x = bottom_left.x; x < bottom_left.x + length; x += 1)
        {
            if (centerOrigin)
                DrawPixelCentreOrigin(x, y, color);
            else
                DrawPixel(x, y, color);
        }   
    }
}

void ProjectApplication::RenderScene([[maybe_unused]] double dt)
{
    //RenderSceneOne();
    //RenderSceneTwo();

    RenderSceneThree(dt);
}


void ProjectApplication::BuildSceneOneCommands()
{
    auto DrawLineCommand = [&](glm::i32vec2 start, glm::i32vec2 end, glm::vec4 color)
    {
        std::function<void()> func = 
            std::bind(&ProjectApplication::DrawLineBresenham, this, start, end, color, true);
        return func;
    };


    static constexpr int32_t square_size = 300;

    //A white square example centered around the origin
    renderCommandQueue.push(DrawLineCommand(glm::ivec2(-1 * square_size, -1 * square_size), glm::ivec2(1 * square_size, -1 * square_size), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
    renderCommandQueue.push(DrawLineCommand(glm::ivec2(1 * square_size, -1 * square_size), glm::ivec2(1 * square_size, 1 * square_size), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
    renderCommandQueue.push(DrawLineCommand(glm::ivec2(1 * square_size, 1 * square_size), glm::ivec2(-1 * square_size, 1 * square_size), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
    renderCommandQueue.push(DrawLineCommand(glm::ivec2(-1 * square_size, 1 * square_size), glm::ivec2(-1 * square_size, -1 * square_size), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));

    renderCommandQueue.push(DrawLineCommand(glm::ivec2(-1 * square_size, -1 * square_size), glm::ivec2(1 * square_size, 1 * square_size), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
    renderCommandQueue.push(DrawLineCommand(glm::ivec2(1 * square_size, -1 * square_size), glm::ivec2(-1 * square_size, 1 * square_size), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));


}

void ProjectApplication::RenderSceneOne()
{
    ClearFBO(screen_draw_fbo, clear_screen_color);

    if (is_screen_dirty)
    {
        ClearFBO(pixel_draw_fbo, pixel_clear_screen_color);

        while (!renderCommandQueue.empty())
        {
            renderCommandQueue.front()();
            renderCommandQueue.pop();
        }

        is_screen_dirty = false;
    }

    DrawPixelsToScreen();
}

void ProjectApplication::RenderSceneTwo()
{
    static bool firstRun = true;

    ClearFBO(screen_draw_fbo, clear_screen_color);

    if (firstRun)
    {
        //auto callback = std::bind(&ProjectApplication::BrushControlCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        glfwSetWindowUserPointer(this->_windowHandle, this);
        glfwSetScrollCallback(this->_windowHandle, [](GLFWwindow* window, double xoffset, double yoffset) {
            auto self = static_cast<ProjectApplication*>(glfwGetWindowUserPointer(window));
            if(self != nullptr)
                self->BrushControlCallback(xoffset, yoffset);
            });


        ClearFBO(pixel_draw_fbo, pixel_clear_screen_color);
        firstRun = false;
    }

    double mouseX = 0;
    double mouseY = 0;
    GetMousePosition(mouseX, mouseY);

    if (IsMouseKeyPressed(GLFW_MOUSE_BUTTON_1))
    {
        glfwSetInputMode(_windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(_windowHandle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

        mouseY = windowHeight - mouseY;
        DrawFilledSquare(glm::i32vec2(static_cast<int32_t>(mouseX), static_cast<int32_t>(mouseY)), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), current_brush_length);
        //DrawPixel(static_cast<int32_t>(mouseX), static_cast<int32_t>(mouseY), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

        DrawPixelsToScreen();

    }
    else if (IsMouseKeyPressed(GLFW_MOUSE_BUTTON_2))
    {
        ClearFBO(pixel_draw_fbo, pixel_clear_screen_color);
    }
    else
    {
        glfwSetInputMode(_windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(_windowHandle, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);


        //Ensure the cursor once set to normal is in the same position as the 'disabled cursor's' position.
        glfwSetCursorPos(_windowHandle, mouseX, mouseY); 	
    }
    
    DrawPixelsToScreen();
}


void ProjectApplication::RenderSceneThree([[maybe_unused]] double dt)
{
    ClearFBO(screen_draw_fbo, clear_screen_color);

    //Prototyping just drawing in one pass and storing that first
    //Adapted from Fabriel Gambetta's 'Computer Graphics from Scratch'
    static bool is_first_draw = true;
    if (is_first_draw)
    {
        Timer timer;

        ClearFBO(pixel_draw_fbo, pixel_clear_screen_color);

        //Origin
        static glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
        
        static int32_t canvas_width = (windowWidth);
        static int32_t canvas_height = (windowHeight);
        
        static float viewport_width = 1.0f;
        static float viewport_height = 1.0f;

        static float distance_to_viewport = 1.0f;

        static float inf = std::numeric_limits<float>::max();

        static float t_min = distance_to_viewport;
        static float t_max = inf;

        static glm::vec4 default_draw_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        auto convert_canvas_to_viewport = [&](int32_t x, int32_t y)
        {
            return glm::vec3(
                static_cast<float>(x) * (viewport_width / static_cast<float>(canvas_width)), 
                static_cast<float>(y) * (viewport_height / static_cast<float>(canvas_height)),
                distance_to_viewport);
        };

        struct Sphere 
        {
            glm::vec3 center;
            glm::vec3 color;
            float radius;
        };



        auto intersect_ray_sphere = [&](glm::vec3 origin, glm::vec3 ray, glm::vec3 sphere_center, float radius)
        {
            glm::vec3 CO = origin - sphere_center;

            //solving with quadratic formula
            float a = glm::dot(ray, ray);
            float b = 2 * glm::dot(CO, ray);
            float c = glm::dot(CO, CO) - radius * radius;

            float discrim = b * b - 4 * a * c;

            //x = t1, y = t2
            //Negative values are rejected
            glm::vec2 t_pairs(inf, inf);

            if (discrim < 0)
            {
                return t_pairs;
            }

            t_pairs.x = (-b + sqrt(discrim) / (2 * a));
            t_pairs.y = (-b - sqrt(discrim) / (2 * a));
            return t_pairs;
        };

        Sphere sphere_green{glm::vec3(-2.0f, 0.0f, 4.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f};
        Sphere sphere_red{glm::vec3(0.0f, -1.0f, 3.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f};
        Sphere sphere_blue{glm::vec3(2.0f, 0.0f, 4.0f), glm::vec3(0.0f, 0.0f, 1.0f), 1.0f};

        auto TraceRay = [&](glm::vec3 origin, glm::vec3 ray, std::vector<Sphere>const& sphere_list)
        {
            float closest_t = inf;
            glm::vec4 draw_color = default_draw_color; //Background color

            for (auto const& sphere : sphere_list)
            {
                glm::vec2 t_pairs = intersect_ray_sphere(origin, ray, sphere.center, sphere.radius);

                auto update_closest = [&](float t_value)
                {
                    if (t_value > t_min && t_value < t_max && t_value < closest_t)
                    {
                        closest_t = t_value;
                        draw_color = glm::vec4(sphere.color, 1.0f);
                    }
                };

                update_closest(t_pairs.x);
                update_closest(t_pairs.y);
            }
            return draw_color;
        };

        glm::vec3 origin = glm::vec3(0.0f, 0.0f, 0.0f);
        std::vector<Sphere> spheres{sphere_red, sphere_green, sphere_blue};


        for (int32_t x = -canvas_width / 2; x < canvas_width / 2; x += 1)
        {
            for (int32_t y = -canvas_height / 2; y < canvas_height / 2; y += 1)
            {
                glm::vec3 ray = convert_canvas_to_viewport(x, y);
                DrawPixelCentreOrigin(x, y, TraceRay(origin, ray, spheres));
            }
        }
        is_first_draw = false;

        auto ms = timer.Elapsed_us() / 1000;
        std::cout << "Drawing spheres took " << ms << " ms\n";

    }
    DrawPixelsToScreen();
}



void ProjectApplication::RenderUI([[maybe_unused]] double dt)
{
    ImGui::Begin("Performance");
    {
        ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
        ImGui::Text("Current Brush Length: %d", current_brush_length);
        ImGui::End();
    }
}


void ProjectApplication::BrushControlCallback(double xoffset, double yoffset)
{
    current_brush_length += static_cast<int32_t>(yoffset);
    if (current_brush_length < 0)
        current_brush_length = 1;
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