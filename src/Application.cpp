#include "Application.h"

Application::Application()
{
    assert(glfwInit() == GL_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "vkPBR", NULL, NULL);
}

Application::~Application()
{
    delete renderer;
}

void Application::Initialize()
{
    Scene scene;
    scene.cameraPosition = glm::vec3{0.0f, -0.8f, 8.0f};
    
    Entity icoSphere;
    icoSphere.metallic = 0.9f;
    icoSphere.roughness = 0.4f;
    icoSphere.albedo = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};
    icoSphere.model = glm::mat4{1.0f};
    icoSphere.model = glm::translate(icoSphere.model, glm::vec3(-2.5f, 0.0f, 0.0f));
    icoSphere.model = glm::scale(icoSphere.model, glm::vec3(2.0f));
    
    Entity sphere;
    
    sphere.metallic = .4f;
    sphere.roughness = 0.8f;
    sphere.albedo = glm::vec4{0.05f, 0.7f, 0.05f, 1.0f};
    
    sphere.model = glm::mat4{1.0f};
    sphere.model = glm::translate(sphere.model, glm::vec3(2.5f, 0.0f, 0.0f));
    sphere.model = glm::scale(sphere.model, glm::vec3(1.5f));
    
    scene.entities.insert({std::string("ico.obj"), icoSphere});
    scene.entities.insert({std::string("sphere.obj"), sphere});
    
    renderer = new RendererPBR(window, scene);
}

void Application::Update()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        renderer->Draw();
        glfwSwapBuffers(window);
    }
}
