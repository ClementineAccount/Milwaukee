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


Canvas::Canvas(int32_t width, int32_t height, int32_t origin_x, int32_t origin_y)
{
    this->width = width;
    this->height = height;
    this->origin_x = origin_x;
    this->origin_y = origin_y;

    canvas_color_buffer.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    ClearCanvas();
}

void Canvas::ClearCanvas(glm::vec4 color)
{
    std::fill(canvas_color_buffer.begin(), canvas_color_buffer.end(), color);
}

void Canvas::DrawCanvasToFBO(DrawFrameBuffer& FBO) const
{
    assert(FBO.width >= this->width && FBO.height >= this->height);
    assert(origin_x >= 0 && origin_x < FBO.width && origin_y >= 0 && origin_y < FBO.height);


    //Possible for canvas to be smaller than the fbo
    glTextureSubImage2D(FBO.tex_id, 0, origin_x, origin_y, this->width, this->height, GL_RGBA, GL_FLOAT, canvas_color_buffer.data());
}


DrawFrameBuffer::DrawFrameBuffer(int32_t width, int32_t height)
{
    this->width = width;
    this->height = height;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);

    glTextureParameteri(tex_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTextureParameteri(tex_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTextureParameteri(tex_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(tex_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTextureStorage2D(tex_id, 1, GL_RGBA8, width, height);

    glCreateFramebuffers(1, &fbo_id);
    glNamedFramebufferTexture(fbo_id, GL_COLOR_ATTACHMENT0, tex_id, 0);

    //glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    //glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void DrawFrameBuffer::DrawPixel(int32_t x, int32_t y, glm::vec4 color, int32_t x_offset, int32_t y_offset)
{
    glTextureSubImage2D(tex_id, 0, x + x_offset, y + y_offset, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(color));
}


void ProjectApplication::AfterCreatedUiContext()
{
}

void ProjectApplication::BeforeDestroyUiContext()
{

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

    size_t draw_framebuffer_width_scale_inverse = 3;
    size_t draw_framebuffer_height_inverse = 3;

    size_t draw_framebuffer_width = windowWidth / draw_framebuffer_width_scale_inverse;
    size_t draw_framebuffer_height = windowHeight / draw_framebuffer_height_inverse;

    draw_framebuffer = std::make_unique<DrawFrameBuffer>(draw_framebuffer_width, draw_framebuffer_height);

    size_t canvas_width_scale_inverse = 3;
    size_t canvas_height_inverse = 3;

    size_t canvas_width = draw_framebuffer_width / canvas_width_scale_inverse;
    size_t canvas_height = draw_framebuffer_height / canvas_height_inverse;

    //half as we center the origin in middle of canvas
    size_t canvas_origin_x = draw_framebuffer_width / 2 -  canvas_width / 2;
    size_t canvas_origin_y = draw_framebuffer_height / 2 - canvas_height / 2;

    draw_canvas = std::make_unique<Canvas>(canvas_width, canvas_height, canvas_origin_x, canvas_origin_y);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    currently_binded_fbo = 0;

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

    glBlitNamedFramebuffer(draw_framebuffer.get()->fbo_id, screen_draw_fbo, 0, 0, draw_framebuffer.get()->width, draw_framebuffer.get()->height, 0, 0, windowWidth, windowHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void Canvas::DrawPixel(int32_t x, int32_t y, glm::vec4 color)
{
    static int32_t x_offset = width / 2;
    static int32_t y_offset = height / 2;

    x = x + x_offset;
    y = y + y_offset;

    //Bounds check. Simply don't draw if out of bounds
    if (x < 0 || y  < 0)
        return;

    if (x >= width || y >= height)
        return;

    canvas_color_buffer[static_cast<size_t>(y) * static_cast<size_t>(width) + x] = color;
}

void ProjectApplication::DrawPixel(int32_t x, int32_t y, glm::vec4 color, int32_t x_offset, int32_t y_offset)
{
    //To Do: can have this call a function pointer to the current DrawPixel function
    draw_framebuffer.get()->DrawPixel(x, y, color, x_offset, y_offset);
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

void ProjectApplication::DrawFilledSquareCanvas(glm::i32vec2 center, glm::vec4 color, int32_t length)
{
    //Start from the top right possible
    glm::i32vec2 bottom_left = center - glm::i32vec2(length / 2, length / 2);

    //Draw row by row from the top left
    for (int32_t y = bottom_left.y; y < bottom_left.y + length; y += 1)
    {
        for (int32_t x = bottom_left.x; x < bottom_left.x + length; x += 1)
        {
            draw_canvas.get()->DrawPixel(x, y, color);
        }   
    }
}

void ProjectApplication::RenderScene([[maybe_unused]] double dt)
{
    //RenderSceneOne();
    //RenderSceneTwo();
    //FillScreenBenchmarks();
    //RenderSceneThree();

    //RenderSpheresDelay(dt);
    RenderSpheresRealTime(dt);

    elapsed_time_seconds += dt;
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
        ClearFBO(draw_framebuffer.get()->fbo_id, draw_framebuffer.get()->clear_color);

        while (!renderCommandQueue.empty())
        {
            renderCommandQueue.front()();
            renderCommandQueue.pop();
        }

        is_screen_dirty = false;
        //DrawCanvasToFBO();
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

        ClearFBO(draw_framebuffer.get()->fbo_id, draw_framebuffer.get()->clear_color);
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

    }
    else if (IsMouseKeyPressed(GLFW_MOUSE_BUTTON_2))
    {
        ClearFBO(draw_framebuffer.get()->fbo_id, draw_framebuffer.get()->clear_color);
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

void ProjectApplication::FillScreenBenchmarks()
{
    ClearFBO(screen_draw_fbo, clear_screen_color);
    ClearFBO(draw_framebuffer.get()->fbo_id, draw_framebuffer.get()->clear_color);
    static bool is_first_draw = true;
    if (is_first_draw)
    {
        static glm::vec4 clear_color{1.0f, 1.0f, 1.0f, 1.0f};
        {
            Timer timer;
            static int32_t canvas_width = (draw_canvas.get()->width);
            static int32_t canvas_height = (draw_canvas.get()->height);


            for (int32_t x = -canvas_width / 2; x < canvas_width / 2; x += 1)
            {
                for (int32_t y = -canvas_height / 2; y < canvas_height / 2; y += 1)
                {
                    DrawPixelCentreOrigin(x, y, clear_color);

                    //draw_framebuffer->DrawPixel(x, y, default_draw_color, windowWidth_half, windowHeight_half);
                    //draw_canvas->DrawPixel(x, y, default_draw_color);
                    //draw_canvas->DrawPixel(x, y, TraceRay(origin, ray, spheres));
                }
            }
            //DrawPixelsToScreen();

            auto ms = timer.Elapsed_us() / 1000;
            std::cout << "Drawing directly FBO texture took: " << ms << " ms\n";
        }

        {
            Timer timer;
            static int32_t canvas_width = (draw_canvas.get()->width);
            static int32_t canvas_height = (draw_canvas.get()->height);

            for (int32_t x = -canvas_width / 2; x < canvas_width / 2; x += 1)
            {
                for (int32_t y = -canvas_height / 2; y < canvas_height / 2; y += 1)
                {
                    draw_canvas->DrawPixel(x, y, clear_color);
                }
            }

           /* draw_canvas->DrawCanvasToFBO(*draw_framebuffer);
            DrawPixelsToScreen();*/

            auto ms = timer.Elapsed_us() / 1000;
            std::cout << "Drawing to canvas buffer took: " << ms << " ms\n";
        }
        is_first_draw = false;
    }
    DrawPixelsToScreen();
}


void ProjectApplication::RenderSceneThree()
{
    ClearFBO(screen_draw_fbo, clear_screen_color);

    //Prototyping just drawing in one pass and storing that first
    //Adapted from Fabriel Gambetta's 'Computer Graphics from Scratch'
    static bool is_first_draw = true;
    if (is_first_draw)
    {
        Timer timer;

        ClearFBO(draw_framebuffer.get()->fbo_id, draw_framebuffer.get()->clear_color);

        //Origin
        static glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
        
        static int32_t canvas_width = (draw_canvas.get()->width);
        static int32_t canvas_height = (draw_canvas.get()->height);
        
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

                //DrawPixelCentreOrigin(x, y, default_draw_color);

                //draw_framebuffer->DrawPixel(x, y, default_draw_color, windowWidth_half, windowHeight_half);
                //draw_canvas->DrawPixel(x, y, default_draw_color);
                draw_canvas->DrawPixel(x, y, TraceRay(origin, ray, spheres));
            }
        }
        is_first_draw = false;

        auto ms = timer.Elapsed_us() / 1000;
        std::cout << "Drawing spheres took " << ms << " ms\n";

        draw_canvas->DrawCanvasToFBO(*draw_framebuffer);
    }
    DrawPixelsToScreen();
}

void ProjectApplication::RenderSpheresDelay(double dt)
{

    static glm::vec4 default_draw_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    ClearFBO(screen_draw_fbo, clear_screen_color);
    static bool is_first_frame = true;
    if (is_first_frame)
    {
        draw_canvas->ClearCanvas(default_draw_color);
        ClearFBO(draw_framebuffer.get()->fbo_id, clear_screen_color);
        is_first_frame = false;
    }

    //Origin
    static glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
    static int32_t canvas_width = (draw_canvas.get()->width);
    static int32_t canvas_height = (draw_canvas.get()->height);
    static float viewport_width = 1.0f;
    static float viewport_height = 1.0f;
    static float distance_to_viewport = 1.0f;
    static float inf = std::numeric_limits<float>::max();
    static float t_min = distance_to_viewport;
    static float t_max = inf;

    
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

    static Sphere sphere_green{glm::vec3(-2.0f, 0.0f, 4.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f};
    static Sphere sphere_red{glm::vec3(0.0f, -1.0f, 3.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f};
    static Sphere sphere_blue{glm::vec3(2.0f, 0.0f, 4.0f), glm::vec3(0.0f, 0.0f, 1.0f), 1.0f};

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

    static int curr_x = -canvas_width / 2;
    static int curr_y = -canvas_height / 2;

    static float time_between_draw = 0.0f;
    static float elapsed_draw_time = 0.0f;


    static int num_rays_per_frame = canvas_width;
    int current_ray_count = 0;

    elapsed_draw_time += dt;
    if (elapsed_draw_time > time_between_draw)
    {
        elapsed_draw_time = 0.0f;
        //if (curr_x < (canvas_width / 2))
        //{
        //    if (curr_y < canvas_height / 2)
        //    {
        //        glm::vec3 ray = convert_canvas_to_viewport(curr_x, curr_y);
        //        DrawPixelCentreOrigin(curr_x, curr_y, TraceRay(origin, ray, spheres));
        //        curr_y += 1;
        //    }
        //    else
        //    {
        //        curr_y = -canvas_height / 2;
        //        curr_x += 1;
        //    }
        //}

        while (current_ray_count < num_rays_per_frame)
        {
            current_ray_count += 1;
            if (curr_y < (canvas_height / 2))
            {
                if (curr_x < canvas_width / 2)
                {
                    glm::vec3 ray = convert_canvas_to_viewport(curr_x, curr_y);
                    DrawPixelCentreOrigin(curr_x, curr_y, TraceRay(origin, ray, spheres));
                    //draw_canvas->DrawPixel(curr_x, curr_y, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                    curr_x += 1;
                }
                else
                {
                    curr_x = -canvas_width / 2;
                    curr_y += 1;
                }
            }
        }
    }
    //draw_canvas->DrawCanvasToFBO(*draw_framebuffer);
    DrawPixelsToScreen();
}


void ProjectApplication::RenderSpheresRealTime(double dt)
{
    static glm::vec4 default_draw_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    ClearFBO(screen_draw_fbo, clear_screen_color);
    static bool is_first_frame = true;
    if (is_first_frame)
    {
        draw_canvas->ClearCanvas(default_draw_color);
        ClearFBO(draw_framebuffer.get()->fbo_id, clear_screen_color);
        is_first_frame = false;
    }

    //Origin
    static glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
    static int32_t canvas_width = (draw_canvas.get()->width);
    static int32_t canvas_height = (draw_canvas.get()->height);
    static float viewport_width = 1.0f;
    static float viewport_height = 1.0f;
    static float distance_to_viewport = 1.0f;
    static float inf = std::numeric_limits<float>::max();
    static float t_min = distance_to_viewport;
    static float t_max = inf;


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


    static glm::vec3 red_sphere_center = glm::vec3(0.0f, -1.0f, 3.0f);
    red_sphere_center.y += 1.0f * dt;

    static glm::vec3 blue_sphere_center = glm::vec3(2.0f, 0.0f, 4.0f);
    blue_sphere_center.x -= 1.0f * dt;

    Sphere sphere_green{glm::vec3(-2.0f, 0.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f};
    Sphere sphere_red{red_sphere_center, glm::vec3(1.0f, 0.0f, 0.0f), 1.0f};
    Sphere sphere_blue{blue_sphere_center, glm::vec3(0.0f, 0.0f, 1.0f), 1.0f};

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

    static int curr_x = -canvas_width / 2;
    static int curr_y = -canvas_height / 2;

    static float time_between_draw = 0.0f;
    static float elapsed_draw_time = 0.0f;


    static int num_rays_per_frame = canvas_width;
    int current_ray_count = 0;

    elapsed_draw_time += dt;
    if (elapsed_draw_time > time_between_draw)
    {
        elapsed_draw_time = 0.0f;
        for (int32_t x = -canvas_width / 2; x < canvas_width / 2; x += 1)
        {
            for (int32_t y = -canvas_height / 2; y < canvas_height / 2; y += 1)
            {
                glm::vec3 ray = convert_canvas_to_viewport(x, y);
                draw_canvas->DrawPixel(x, y, TraceRay(origin, ray, spheres));
            }
        }
    }


    draw_canvas->DrawCanvasToFBO(*draw_framebuffer);
    DrawPixelsToScreen();
}



void ProjectApplication::RenderUI([[maybe_unused]] double dt)
{
    ImGui::Begin("Performance");
    {
        ImGui::Text("Framerate: %.0f Hertz", 1 / dt);
        ImGui::Text("Elapsed Real Time in Seconds (Footage may be sped up): %.3f", elapsed_time_seconds);
        //ImGui::Text("Current Brush Length: %d", current_brush_length);
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