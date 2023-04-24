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
    virtual void AfterCreatedUiContext();
    virtual void BeforeDestroyUiContext();
    virtual bool Initialize();
    virtual bool Load();
    virtual void Unload();
    virtual void RenderScene();
    virtual void RenderUI();
    virtual void Update();

    int windowWidth = 1600;
    int windowHeight = 900;

private:
    GLFWwindow* _windowHandle = nullptr;
    void Render();

};