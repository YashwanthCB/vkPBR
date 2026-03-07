#pragma once

#include "Global.h"

#include <string>
#include <map>

struct Entity
{
    glm::mat4 model;
    glm::vec4 albedo;
    float metallic;
    float roughness;
};

struct Scene
{
    glm::vec3 cameraPosition;
    std::map<std::string, Entity> entities;
};


