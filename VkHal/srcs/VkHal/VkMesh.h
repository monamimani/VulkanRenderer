#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace VkHal
{
struct Vertex
{
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  static auto getBindingDescription()
  {
    vk::VertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = vk::VertexInputRate::eVertex;

    return bindingDesc;
  }

  static auto getAttributesDescription()
  {
    std::array<vk::VertexInputAttributeDescription, 3> attributesDesc = {};
    attributesDesc[0].binding = 0;
    attributesDesc[0].location = 0;
    attributesDesc[0].format = vk::Format::eR32G32B32Sfloat;
    attributesDesc[0].offset = offsetof(Vertex, pos);

    attributesDesc[1].binding = 0;
    attributesDesc[1].location = 1;
    attributesDesc[1].format = vk::Format::eR32G32B32Sfloat;
    attributesDesc[1].offset = offsetof(Vertex, color);

    attributesDesc[2].binding = 0;
    attributesDesc[2].location = 2;
    attributesDesc[2].format = vk::Format::eR32G32Sfloat;
    attributesDesc[2].offset = offsetof(Vertex, texCoord);

    return attributesDesc;
  }
};

class Mesh
{
public:
  Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
      : m_vertices{vertices}
      , m_indices{indices}
  {
  }

  std::vector<Vertex> m_vertices;
  std::vector<uint32_t> m_indices;
};

class VkMesh
{
  // something like that , approximatively
  VkMesh(Mesh mesh);
  vk::UniqueDeviceMemory m_vertexBufferMemory;
  vk::UniqueBuffer m_vertexBuffer;

  vk::UniqueDeviceMemory m_indexBufferMemory;
  vk::UniqueBuffer m_indexBuffer;
};

struct UniformBufferObject
{
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

// const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, //
//                                      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},  //
//                                      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

//const std::vector<Vertex> vertices = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, //
//                                      {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, //
//                                      {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, //
//                                      {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
//
//                                      {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, //
//                                      {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, //
//                                      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, //
//                                      {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}};

//const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

Mesh processMesh(const aiMesh* mesh, const aiScene* scene)
{
  std::vector<Vertex> vertices;
  vertices.reserve(mesh->mNumVertices);
  std::vector<uint32_t> indices;

  bool hasFirstSetOfUV = mesh->mTextureCoords[0] != nullptr;

  for (size_t i = 0; i < mesh->mNumVertices; i++)
  {
    const auto& aiVertex = mesh->mVertices[i];
    const auto& aiNormal = mesh->mNormals[i];

    Vertex vertex{};
    vertex.pos = {aiVertex.x, aiVertex.y, aiVertex.z};

    if (hasFirstSetOfUV)
    {
      const auto& aiUV = mesh->mTextureCoords[0][i];
      vertex.texCoord = {aiUV.x, aiUV.y};
    }
    else
    {
      vertex.texCoord = {0.0f, 0.0f};
    }
    vertices.push_back(vertex);
  }

  for (size_t i = 0; i < mesh->mNumFaces; i++)
  {
    const auto& face = mesh->mFaces[i];
    for (size_t j = 0; j < face.mNumIndices; j++)
    {
      indices.push_back(face.mIndices[j]);
    }
  }

  return Mesh{vertices, indices};
}

class MeshLoader
{
public:
  void loadModel(std::filesystem::path path)
  {
    // https://learnopengl.com/Model-Loading/Assimp
    // https://learnopengl.com/Model-Loading/Model
    Assimp::Importer import;
    auto scene = import.ReadFile(path.u8string().c_str(), aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
      throw std::runtime_error(std::string("ERROR::ASSIMP::") + import.GetErrorString());
      return;
    }

    meshes.reserve(scene->mNumMeshes);
    processNode(scene->mRootNode, scene);
    vertices = meshes[0].m_vertices;
    indices = meshes[0].m_indices;
  }

  auto getVertices() const
  {
    return vertices;
  }

  auto getIndices() const
  {
    return indices;
  }

private:
  void processNode(aiNode* node, const aiScene* scene)
  {
    // process all the node's meshes (if any)
    for (size_t i = 0; i < node->mNumMeshes; i++)
    {
      const auto mesh = scene->mMeshes[node->mMeshes[i]];
      meshes.push_back(processMesh(mesh, scene));
    }

    // then do the same for each of its children
    for (size_t i = 0; i < node->mNumChildren; i++)
    {
      processNode(node->mChildren[i], scene);
    }
  }

  std::vector<Mesh> meshes;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
};
}