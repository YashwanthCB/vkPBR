#pragma once
#include "RendererPBR.h"

class Application
{
public:
    Application();
    ~Application();
    void Initialize();
    void Update();
private:
    RendererPBR* renderer;
    Scene scene;
    GLFWwindow* window;
};
