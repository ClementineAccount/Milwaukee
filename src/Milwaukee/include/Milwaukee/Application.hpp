#pragma once
#include <cstdint>

struct GLFWwindow;

class Application
{
public:
    void Run();

protected:
    void Close();
    bool IsKeyPressed(int32_t key);
    bool IsKeyRelease(int32_t key);

    bool IsMouseKeyPressed(int32_t key);
    void GetMousePosition(double& mouseX, double& mouseY);

    virtual void AfterCreatedUiContext();
    virtual void BeforeDestroyUiContext();
    virtual bool Initialize();
    virtual bool Load();
    virtual void Unload();
    virtual void RenderScene(double dt);
    virtual void RenderUI(double dt);
    virtual void Update(double dt);

    int windowWidth = 1600;
    int windowHeight = 900;

    int windowWidth_half = windowWidth / 2;
    int windowHeight_half = windowHeight / 2;

    GLFWwindow* _windowHandle = nullptr;



private:
  
    void Render(double dt);

};