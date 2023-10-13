#pragma once

#include "resource.h"

#include <functional>
#include <iostream>
#include <linalg.h>
#include <limits>
#include <memory>


using namespace linalg::aliases;

static constexpr float DEFAULT_DEPTH = std::numeric_limits<float>::max();

namespace cg::renderer
{
	template<typename VB, typename RT>
	class rasterizer
	{
	public:
		rasterizer(){};
		virtual ~rasterizer(){};
		void set_render_target(
				std::shared_ptr<resource<RT>> in_render_target,
				std::shared_ptr<resource<float>> in_depth_buffer = nullptr);
		void clear_render_target(
				const RT& in_clear_value, const float in_depth = DEFAULT_DEPTH);

		void set_vertex_buffer(std::shared_ptr<resource<VB>> in_vertex_buffer);
		void set_index_buffer(std::shared_ptr<resource<unsigned int>> in_index_buffer);

		void set_viewport(size_t in_width, size_t in_height);

		void draw(size_t num_vertexes, size_t vertex_offset);

		std::function<std::pair<float4, VB>(float4 vertex, VB vertex_data)> vertex_shader;
		std::function<cg::fcolor(const VB& vertex_data, const float z)> pixel_shader;

	protected:
		std::shared_ptr<cg::resource<VB>> vertex_buffer;
		std::shared_ptr<cg::resource<unsigned int>> index_buffer;
		std::shared_ptr<cg::resource<RT>> render_target;
		std::shared_ptr<cg::resource<float>> depth_buffer;

		size_t width = 1920;
		size_t height = 1080;

		float edge_function(float2 a, float2 b, float2 c);
		bool z_test(float z, size_t x, size_t y);
	};

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::set_render_target(
			std::shared_ptr<resource<RT>> in_render_target,
			std::shared_ptr<resource<float>> in_depth_buffer)
	{
		if (in_render_target) render_target = in_render_target;
		if (in_depth_buffer) depth_buffer = in_depth_buffer;
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::set_viewport(size_t in_width, size_t in_height)
	{
		width = in_width;
		height = in_height;
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::clear_render_target(
			const RT& in_clear_value, const float in_depth)
	{
		for (size_t i = 0; i < render_target->get_number_of_elements(); i++) {
			render_target->item(i) = in_clear_value;
		}

		if (depth_buffer) {
			for (size_t i = 0; i < depth_buffer->get_number_of_elements(); i++) {
				depth_buffer->item(i) = in_depth;
			}
		}
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::set_vertex_buffer(std::shared_ptr<resource<VB>> in_vertex_buffer) {
		vertex_buffer = in_vertex_buffer;
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::set_index_buffer(std::shared_ptr<resource<unsigned int>> in_index_buffer) {
		index_buffer = in_index_buffer;
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::draw(size_t num_vertexes, size_t vertex_offset) {
		size_t vertex_id = vertex_offset;
		while (vertex_id < vertex_offset + num_vertexes) {
			// Assume we only work with triangles
			std::vector<VB> vertices {
				vertex_buffer->item(index_buffer->item(vertex_id++)),
				vertex_buffer->item(index_buffer->item(vertex_id++)),
				vertex_buffer->item(index_buffer->item(vertex_id++))
			};

			// apply some coordinate transformations + vertex shader to the triangle
			for (auto& vertex : vertices) {
				auto data = vertex_shader(float4{ vertex.pos, 1 }, vertex);
				float4 processed_position = data.first;
				vertex.pos = processed_position.xyz() / processed_position.w;
				vertex.pos.x = (vertex.pos.x + 1) * width / 2;
				vertex.pos.y = (-vertex.pos.y + 1) * height / 2;
				vertex.depth = processed_position.z;
			}
			
			float2 vertex_a = vertices[0].pos.xy();
			float2 vertex_b = vertices[1].pos.xy();
			float2 vertex_c = vertices[2].pos.xy();

			// calculate bounding box
			int2 min_coord { 0, 0 };
			int2 max_coord { static_cast<int>(width), static_cast<int>(height) };
			int2 min_vertex { floor(min(vertex_a, min(vertex_b, vertex_c))) };
			int2 max_vertex { ceil(max(vertex_a, max(vertex_b, vertex_c))) };
			uint2 bounding_box_begin { clamp(min_vertex, min_coord, max_coord) };
			uint2 bounding_box_end { clamp(max_vertex, min_coord, max_coord) };

			// precalculated values
			float triangle_edge = edge_function(vertex_a, vertex_b, vertex_c);
			float avgZ = (vertices[0].pos.z + vertices[1].pos.z + vertices[2].pos.z) / 3;
			float z1 = (vertices[0].pos.z - avgZ) / triangle_edge;
			float z2 = (vertices[1].pos.z - avgZ) / triangle_edge;
			float z3 = (vertices[2].pos.z - avgZ) / triangle_edge;
			float depth1 = vertices[0].depth / triangle_edge;
			float depth2 = vertices[1].depth / triangle_edge;
			float depth3 = vertices[2].depth / triangle_edge;

			// Iterating over pixels in the bounding box
			for (size_t x = bounding_box_begin.x; x < bounding_box_end.x; x++) {
				for (size_t y = bounding_box_begin.y; y < bounding_box_end.y; y++) {
					float2 point { static_cast<float>(x), static_cast<float>(y)};
					// edge values determine, whether the pixel belongs to the triangle
					float edge1 = edge_function(vertex_a, vertex_b, point);
					float edge2 = edge_function(vertex_b, vertex_c, point);
					float edge3 = edge_function(vertex_c, vertex_a, point);

					if (edge1 < 0 || edge2 < 0 || edge3 < 0) continue;

					float z = edge2 * z1 + edge3 * z2 + edge1 * z3 + avgZ;
					if (z < 0 || z > 1) continue; // near/far camera clipping
					if (!z_test(z, x, y)) continue;
					
					float zDepth = edge2 * depth1 + edge3 * depth2 + edge1 * depth3;
					cg::fcolor pixel_result = pixel_shader(vertices[0], zDepth);
					render_target->item(x, y) = cg::from_fcolor(pixel_result);
					if (depth_buffer) depth_buffer->item(x, y) = z;
				}
			}
		}
	}
	
	template<typename VB, typename RT>
	inline float
	rasterizer<VB, RT>::edge_function(float2 a, float2 b, float2 c) {
		return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
	}

	// Depth test. Things closer to camera have lower depth value.
	template<typename VB, typename RT>
	inline bool rasterizer<VB, RT>::z_test(float z, size_t x, size_t y) {
		return !depth_buffer || (depth_buffer->item(x, y) > z);
	}

} // namespace cg::renderer