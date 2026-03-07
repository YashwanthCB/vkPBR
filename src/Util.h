#pragma once

#include <vector>
#include <string>

void InitAssimp();
std::vector<char> ReadFile(const char* fileName);
std::vector<struct VertexData> LoadModel(std::string fileName);