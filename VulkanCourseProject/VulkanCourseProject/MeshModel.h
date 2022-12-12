#pragma once

#include <vector>
#include <string>

#include <glm/glm.hpp>
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Mesh.h"


class VulkanRenderer;

class MeshModel
{
public:
	MeshModel(const std::vector<Mesh>& meshes, glm::mat4 model);

	size_t GetMeshCount() const;
	const Mesh& GetMesh(size_t index) const;
	const glm::mat4& GetModel() const;

	void SetModel(const glm::mat4& model);

	void Destroy();

	static MeshModel LoadModel(const std::string& directory, const std::string& fileName, VulkanRenderer* renderer);

private:
	std::vector<Mesh> meshes_;
	glm::mat4 model_;

	static std::vector<std::string> LoadMaterials(const aiScene* scene);
	static std::vector<Mesh> LoadNode(const aiNode* node, const aiScene* scene, std::vector<int> matToTex, VulkanRenderer* renderer);
	static Mesh LoadMesh(const aiMesh* mesh, const aiScene* scene, std::vector<int> matToTex, VulkanRenderer* renderer);
};


inline size_t MeshModel::GetMeshCount() const
{
	return meshes_.size();
}

inline const Mesh& MeshModel::GetMesh(size_t index) const
{
	if (index >= meshes_.size())
	{
		throw std::runtime_error("Attempted to access invalid Mesh index.");
	}

	return meshes_[index];
}

inline const glm::mat4& MeshModel::GetModel() const
{
	return model_;
}


inline void MeshModel::SetModel(const glm::mat4& model)
{
	model_ = model;
}
