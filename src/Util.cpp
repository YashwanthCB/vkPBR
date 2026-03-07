#include "Util.h"

#include "Global.h"
#include "RendererTypes.h"

#include <vector>
#include <fstream>
#include <cassert>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/DefaultLogger.hpp>


Assimp::Importer importer;

std::vector<char> ReadFile(const char* fileName)
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);
    assert(file.is_open());
    
    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);        
    file.close();
    
    return buffer;        
}

void InitAssimp()
{
    Assimp::Logger::LogSeverity severity = Assimp::Logger::VERBOSE;
    Assimp::DefaultLogger::create("",severity, aiDefaultLogStream_STDOUT);
    Assimp::DefaultLogger::create("assimp_log.txt",severity, aiDefaultLogStream_FILE);
    Assimp::DefaultLogger::get()->info("this is my info-call");
   
}

std::vector<VertexData> LoadModel(std::string fileName)
{
    std::vector<VertexData> vertexData;
    
    const aiScene* scene = importer.ReadFile(fileName, aiProcessPreset_TargetRealtime_Quality);
    
    for (int sceneIndex = 0; sceneIndex < scene->mNumMeshes; sceneIndex++)
    {
        aiMesh* mesh = scene->mMeshes[sceneIndex];
        
        for (int faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++)
        {
            const struct aiFace* face = &mesh->mFaces[faceIndex];
            
            for (int index = 0; index < face->mNumIndices; index++)
            {
                int vertexIndex = face->mIndices[index];
                auto v = mesh->mVertices[vertexIndex];
                auto n = mesh->mNormals[vertexIndex];
                
                VertexData perVertexData;
                
                perVertexData.position = glm::vec3(v.x, v.y, v.z);
                perVertexData.normal = glm::vec3(n.x, n.y, n.z);
                vertexData.emplace_back(perVertexData);
            }
        }
    }
    
    return vertexData;
}
