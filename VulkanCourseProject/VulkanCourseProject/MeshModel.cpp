#include "MeshModel.h"

#include "VulkanRenderer.h"


MeshModel::MeshModel(const std::vector<Mesh>& meshes, glm::mat4 model) : meshes_(meshes), model_(model)
{
}


void MeshModel::Destroy()
{
	for (Mesh& mesh : meshes_)
	{
		mesh.Destroy();
	}
}


MeshModel MeshModel::LoadModel(const std::string& directory, const std::string& fileName, VulkanRenderer* renderer)
{
	//const std::string base_filename = fileName.substr(fileName.find_last_of("/\\") + 1);
	//std::string::size_type const p(base_filename.find_last_of('.'));
	//const std::string file_without_extension = base_filename.substr(0, p);
	//const std::string filePath = "../models/" + file_without_extension + "/" + fileName;
	const std::string filePath = "../models/" + directory + "/" + fileName;

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(filePath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
	if (!scene)
	{
		throw std::runtime_error("Failed to load model.");
	}

	std::vector<std::string> textureNames = LoadMaterials(scene);

	std::vector<int> matToTex(textureNames.size());
	for (size_t i = 0; i < matToTex.size(); i++)
	{
		if (textureNames[i].empty())
		{
			matToTex[i] = 0;
		}
		else
		{
			const std::string texturePath = "../models/" + directory + "/" + textureNames[i];
			matToTex[i] = renderer->CreateTexture(texturePath, false);
		}
	}

	std::vector<Mesh> meshes = LoadNode(scene->mRootNode, scene, matToTex, renderer);

	return MeshModel(meshes, glm::mat4(1.0f));
}


std::vector<std::string> MeshModel::LoadMaterials(const aiScene* scene)
{
	std::vector<std::string> textures(scene->mNumMaterials);

	for (size_t i = 0; i < scene->mNumMaterials; i++)
	{
		textures[i] = "";

		const aiMaterial* material = scene->mMaterials[i];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			aiString path;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == aiReturn_SUCCESS)
			{
				const std::string fullPath = std::string(path.data);
				const std::string fileName = fullPath.substr(fullPath.rfind("\\") + 1);

				textures[i] = fileName;
			}
		}
	}

	return textures;
}

std::vector<Mesh> MeshModel::LoadNode(const aiNode* node, const aiScene* scene, std::vector<int> matToTex, VulkanRenderer* renderer)
{
	std::vector<Mesh> meshes;

	for (size_t i = 0; i < node->mNumMeshes; i++)
	{
		meshes.push_back(LoadMesh(scene->mMeshes[node->mMeshes[i]], scene, matToTex, renderer));
	}

	for (size_t i = 0; i < node->mNumChildren; i++)
	{
		std::vector<Mesh> childMeshes = LoadNode(node->mChildren[i], scene, matToTex, renderer);
		meshes.insert(meshes.end(), childMeshes.begin(), childMeshes.end());
	}

	return meshes;
}

Mesh MeshModel::LoadMesh(const aiMesh* mesh, const aiScene* scene, std::vector<int> matToTex, VulkanRenderer* renderer)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.resize(mesh->mNumVertices);
	for (size_t i = 0; i < vertices.size(); i++)
	{
		vertices[i].position = glm::vec3(
			mesh->mVertices[i].x,
			mesh->mVertices[i].y,
			mesh->mVertices[i].z
		);

		vertices[i].color = glm::vec3(1.0f);

		if (mesh->mTextureCoords[0])
		{
			vertices[i].texCoords = glm::vec2(
				mesh->mTextureCoords[0][i].x,
				mesh->mTextureCoords[0][i].y
			);
		}
		else
		{
			vertices[i].texCoords = glm::vec2(0.0f);
		}
	}

	for (size_t i = 0; i < mesh->mNumFaces; i++)
	{
		const aiFace& face = mesh->mFaces[i];

		for (size_t j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	return Mesh(renderer->mainDevice.physicalDevice, renderer->mainDevice.logicalDevice, renderer->graphicsQueue_, 
		renderer->graphicsCommandPool_, vertices, indices, matToTex[mesh->mMaterialIndex]);
}
