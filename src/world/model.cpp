#define TINYOBJLOADER_IMPLEMENTATION

#include "model.h"

#include "utils/error_handler.h"

#include <linalg.h>


using namespace linalg::aliases;
using namespace cg::world;

cg::world::model::model() {}

cg::world::model::model(const std::filesystem::path& model_path) {
	load_obj(model_path);
}

cg::world::model::~model() {}

void cg::world::model::load_obj(const std::filesystem::path& model_path)
{
	tinyobj::ObjReaderConfig readerConfig;
	readerConfig.mtl_search_path = model_path.parent_path().string();
	readerConfig.triangulate = true;

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(model_path.string(), readerConfig)) {
		if (!reader.Error().empty()) THROW_ERROR(reader.Error());
	}

	allocate_buffers(reader.GetShapes());
	fill_buffers(
		reader.GetShapes(), 
		reader.GetAttrib(), 
		reader.GetMaterials(), 
		model_path.parent_path()
	);
}

void model::allocate_buffers(const std::vector<tinyobj::shape_t>& shapes)
{
	for (const auto& shape : shapes) {
		unsigned int vertex_buffer_size = 0;
		unsigned int idx_size = 0;
		std::map<std::tuple<int, int, int>, unsigned int> index_map;
		const auto& mesh = shape.mesh;

		for (unsigned char fv : mesh.num_face_vertices) {
			for (size_t start = idx_size; idx_size - start < fv; idx_size++) {
				tinyobj::index_t idx = mesh.indices[idx_size];
				auto idx_tuple = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);
				if (index_map.find(idx_tuple) == index_map.end()) {
					index_map[idx_tuple] = vertex_buffer_size++;
				}
			}
		}

		vertex_buffers.push_back(std::make_shared<cg::resource<cg::vertex>>(vertex_buffer_size));
		index_buffers.push_back(std::make_shared<cg::resource<unsigned int>>(idx_size));
	}

	textures.resize(shapes.size());
}

float3 get_vert_pos(const tinyobj::attrib_t& attrib, const tinyobj::index_t index) {
	return float3 {
		attrib.vertices[3 * index.vertex_index],
		attrib.vertices[3 * index.vertex_index + 1],
		attrib.vertices[3 * index.vertex_index + 2],
	};
}

float3 get_vert_norm(const tinyobj::attrib_t& attrib, const tinyobj::index_t index) {
	return float3 {
		attrib.normals[3 * index.normal_index],
		attrib.normals[3 * index.normal_index + 1],
		attrib.normals[3 * index.normal_index + 2],
	};
}

float2 get_vert_uv(const tinyobj::attrib_t& attrib, const tinyobj::index_t index) {
	return float2 {
		attrib.texcoords[2 * index.texcoord_index],
		attrib.texcoords[2 * index.texcoord_index + 1],
	};
}

float3 cast_tiny_real3(const tinyobj::real_t color[3]) {
	return float3 { color[0], color[1], color[2] };
}

float3 cg::world::model::compute_normal(const tinyobj::attrib_t& attrib, const tinyobj::mesh_t& mesh, size_t index_offset)
{
	float3 a = get_vert_pos(attrib, mesh.indices[index_offset + 0]);
	float3 b = get_vert_pos(attrib, mesh.indices[index_offset + 1]);
	float3 c = get_vert_pos(attrib, mesh.indices[index_offset + 2]);

	return normalize(cross(b - a, c - a));
}

void model::fill_vertex_data(cg::vertex& vertex, const tinyobj::attrib_t& attrib, const tinyobj::index_t idx, 
		const float3 computed_normal, const tinyobj::material_t material)
{
	vertex.pos = get_vert_pos(attrib, idx);
	vertex.norm = (idx.normal_index < 0) ? computed_normal : get_vert_norm(attrib, idx);
	vertex.uv = (idx.texcoord_index < 0) ? float2{0, 0} : get_vert_uv(attrib, idx);
	vertex.ambient = cast_tiny_real3(material.ambient);
	vertex.diffuse = cast_tiny_real3(material.diffuse);
	vertex.emissive = cast_tiny_real3(material.emission);
}

void model::fill_buffers(const std::vector<tinyobj::shape_t>& shapes, const tinyobj::attrib_t& attrib, 
		const std::vector<tinyobj::material_t>& materials, const std::filesystem::path& base_folder)
{
	for (size_t s = 0; s < shapes.size(); s++) {
		size_t index = 0;
		unsigned int vertex_buffer_id = 0;
		unsigned int index_buffer_id = 0;
		std::map<std::tuple<int, int, int>, unsigned int> index_map;
		const auto& mesh = shapes[s].mesh;

		for (size_t f = 0; f < mesh.num_face_vertices.size(); f++) {
			float3 normal = compute_normal(attrib, mesh, index);
			
			for (size_t start = index; index - start < mesh.num_face_vertices[f]; index++) {
				tinyobj::index_t idx = mesh.indices[index];
				auto idx_tuple = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);
				if (index_map.find(idx_tuple) == index_map.end()) {
					cg::vertex& vertex = vertex_buffers[s]->item(vertex_buffer_id);
					const auto& material = materials[mesh.material_ids[f]];
					fill_vertex_data(vertex, attrib, idx, normal, material);
					index_map[idx_tuple] = vertex_buffer_id++;
				}
				index_buffers[s]->item(index_buffer_id++) = index_map[idx_tuple];
			}
		}

		if (!materials[mesh.material_ids[0]].diffuse_texname.empty()) {
			textures[s] = base_folder / materials[mesh.material_ids[0]].diffuse_texname;
		}
	}
}

const std::vector<std::shared_ptr<cg::resource<cg::vertex>>>&
cg::world::model::get_vertex_buffers() const { return vertex_buffers; }

const std::vector<std::shared_ptr<cg::resource<unsigned int>>>&
cg::world::model::get_index_buffers() const { return index_buffers; }

const std::vector<std::filesystem::path>& cg::world::model::get_per_shape_texture_files() const { return textures; }

const float4x4 cg::world::model::get_world_matrix() const {
	return float4x4 {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1}
	};
}
