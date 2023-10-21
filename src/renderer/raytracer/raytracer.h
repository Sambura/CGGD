#pragma once

#include "resource.h"

#include <iostream>
#include <linalg.h>
#include <memory>
#include <omp.h>
#include <random>

using namespace linalg::aliases;

namespace cg::renderer
{
	struct ray
	{
		ray(float3 position, float3 direction) : position(position)
		{
			this->direction = normalize(direction);
		}
		float3 position;
		float3 direction;
	};

	struct payload
	{
		float t;
		float3 bary;
		cg::fcolor color;
	};

	template<typename VB>
	struct triangle
	{
		triangle(const VB& vertex_a, const VB& vertex_b, const VB& vertex_c);

		float3 a;
		float3 b;
		float3 c;

		float3 ba;
		float3 ca;

		float3 na;
		float3 nb;
		float3 nc;

		float3 ambient;
		float3 diffuse;
		float3 emissive;
	};

	template<typename VB>
	inline triangle<VB>::triangle(const VB& vertex_a, const VB& vertex_b, const VB& vertex_c) : 
		a(vertex_a.pos.xyz()), b(vertex_b.pos.xyz()), c(vertex_c.pos.xyz()), 
		ba(vertex_b.pos.xyz() - vertex_a.pos.xyz()), ca(vertex_c.pos.xyz() - vertex_a.pos.xyz()),
		na(vertex_a.norm), nb(vertex_b.norm), nc(vertex_c.norm), 
		ambient(vertex_a.ambient), diffuse(vertex_a.diffuse), emissive(vertex_a.emissive) {}

	template<typename VB>
	class aabb
	{
	public:
		void add_triangle(const triangle<VB> triangle);
		const std::vector<triangle<VB>>& get_triangles() const;
		bool aabb_test(const ray& ray) const;

	protected:
		std::vector<triangle<VB>> triangles;

		float3 aabb_min;
		float3 aabb_max;
	};

	struct light
	{
		float3 position;
		float3 color;
	};

	template<typename VB, typename RT>
	class raytracer
	{
	public:
		raytracer(){};
		~raytracer(){};

		void set_render_target(std::shared_ptr<resource<RT>> in_render_target);
		void clear_render_target(const RT& in_clear_value);
		void set_viewport(size_t in_width, size_t in_height);

		void set_vertex_buffers(std::vector<std::shared_ptr<cg::resource<VB>>> in_vertex_buffers);
		void set_index_buffers(std::vector<std::shared_ptr<cg::resource<unsigned int>>> in_index_buffers);
		void build_acceleration_structure();
		std::vector<aabb<VB>> acceleration_structures;

		void ray_generation(float3 position, float3 direction, float3 right, float3 up, size_t depth, size_t accumulation_num);

		payload trace_ray(const ray& ray, size_t depth, float max_t = 1000.f, float min_t = 0.001f) const;
		payload intersection_shader(const triangle<VB>& triangle, const ray& ray) const;

		std::function<payload(const ray& ray)> miss_shader = nullptr;
		std::function<payload(const ray& ray, payload& payload, const triangle<VB>& triangle, size_t depth)>
				closest_hit_shader = nullptr;
		std::function<payload(const ray& ray, payload& payload, const triangle<VB>& triangle)> any_hit_shader =
				nullptr;

		float2 get_jitter(int frame_id);

	protected:
		std::shared_ptr<cg::resource<RT>> render_target;
		std::shared_ptr<cg::resource<float3>> history;
		std::vector<std::shared_ptr<cg::resource<unsigned int>>> index_buffers;
		std::vector<std::shared_ptr<cg::resource<VB>>> vertex_buffers;
		std::vector<triangle<VB>> triangles;

		size_t width = 1920;
		size_t height = 1080;
	};

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::set_render_target(std::shared_ptr<resource<RT>> in_render_target) {
		render_target = in_render_target;
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::set_viewport(size_t in_width, size_t in_height) {
		width = in_width;
		height = in_height;
		history = std::make_shared<cg::resource<float3>>(width, height);
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::clear_render_target(const RT& in_clear_value) {
		for (size_t i = 0; i < render_target->get_number_of_elements(); i++) {
			render_target->item(i) = in_clear_value;
			history->item(i) = float3{ 0.f };
		}
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::set_vertex_buffers(std::vector<std::shared_ptr<cg::resource<VB>>> in_vertex_buffers) {
		vertex_buffers = in_vertex_buffers;
	}

	template<typename VB, typename RT>
	void raytracer<VB, RT>::set_index_buffers(std::vector<std::shared_ptr<cg::resource<unsigned int>>> in_index_buffers) {
		index_buffers = in_index_buffers;
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::build_acceleration_structure()
	{
		for (size_t i = 0; i < index_buffers.size(); i++) {
			aabb<VB> aabb;
			for (size_t vi = 0; vi < index_buffers[i]->get_number_of_elements();) {
				aabb.add_triangle({
					vertex_buffers[i]->item(index_buffers[i]->item(vi++)), 
					vertex_buffers[i]->item(index_buffers[i]->item(vi++)), 
					vertex_buffers[i]->item(index_buffers[i]->item(vi++))
				});
			}
			acceleration_structures.push_back(aabb);
		}
	}

	template<typename VB, typename RT>
	inline void raytracer<VB, RT>::ray_generation(
			float3 position, float3 direction,
			float3 right, float3 up, size_t depth, size_t accumulation_num)
	{
		for (size_t frame_id = 0; frame_id < accumulation_num; frame_id++) {
			std::cout << "Tracing " << frame_id + 1 << "/" << accumulation_num << " frame\n";
			float2 jitter = get_jitter(frame_id);

			#pragma omp parallel for
			for (int x = 0; x < static_cast<int>(width); x++) {
				for (int y = 0; y < static_cast<int>(height); y++) {
					float u = (2.f * x + jitter.x) / static_cast<float>(width) - 1;
					float v = (2.f * y + jitter.y) / static_cast<float>(height) - 1;
					u *= static_cast<float>(width) / static_cast<float>(height);

					float3 primary_direction = direction + right * u - up * v;
					ray primary_ray(position, primary_direction);
					payload payload = trace_ray(primary_ray, depth);

					history->item(x, y) += payload.color / accumulation_num;
				}
			}
		}

		#pragma omp parallel for
		for (int i = 0; i < static_cast<int>(history->get_number_of_elements()); i++) {
			render_target->item(i) = cg::from_fcolor(history->item(i));
		}
	}

	template<typename VB, typename RT>
	inline payload raytracer<VB, RT>::trace_ray(
			const ray& ray, size_t depth, float max_t, float min_t) const
	{
		if (depth-- == 0) return miss_shader(ray);

		payload closest_intersection {};
		const triangle<VB>* closest_triangle = nullptr;
		closest_intersection.t = max_t;

		for (auto& aabb : acceleration_structures) {
			if (!aabb.aabb_test(ray)) continue;

			for (auto& triangle : aabb.get_triangles()) {
				payload payload = intersection_shader(triangle, ray);

				if (payload.t >= min_t && closest_intersection.t > payload.t) {
					closest_intersection = payload;
					closest_triangle = &triangle;
					if (any_hit_shader) return any_hit_shader(ray, payload, triangle);
				}
			}
		}

		if (closest_triangle && closest_hit_shader)
			return closest_hit_shader(ray, closest_intersection, *closest_triangle, depth);

		return miss_shader(ray);
	}

	template<typename VB, typename RT>
	inline payload raytracer<VB, RT>::intersection_shader(const triangle<VB>& triangle, const ray& ray) const {
		payload payload {};
		payload.t = -1;
		constexpr float tolerance = 1e-8f;

		float3 pvec = cross(ray.direction, triangle.ca);
		float determinant = dot(pvec, triangle.ba);
		if (determinant > -tolerance && determinant < tolerance) return payload;
		float inv_det = 1 / determinant;
		
		float3 tvec = ray.position - triangle.a;
		float u = dot(tvec, pvec) * inv_det;
		if (u < 0 || u > 1) return payload;
		float3 qvec = cross(tvec, triangle.ba);
		float v = dot(ray.direction, qvec) * inv_det;
		if (v < 0 || v + u > 1) return payload;
		float w = 1 - v - u;

		payload.t = dot(triangle.ca, qvec) * inv_det;
		payload.bary = float3{ w, u, v }; // ????

		return payload;
	}

	template<typename VB, typename RT>
	float2 raytracer<VB, RT>::get_jitter(int frame_id) {
		float2 result{ -0.5f };
		
		constexpr int base_x = 2;
		constexpr int base_y = 3;

		float inv_base = 1.f / base_x;
		float fraction = inv_base;

		for(int index = frame_id + 1; index > 0; index /= base_x) {
			result.x += (index % base_x) * fraction;
			fraction *= inv_base;
		}

		inv_base = 1.f / base_y;
		fraction = inv_base;

		for(int index = frame_id + 1; index > 0; index /= base_y) {
			result.y += (index % base_y) * fraction;
			fraction *= inv_base;
		}

		return result;
	}


	template<typename VB>
	inline void aabb<VB>::add_triangle(const triangle<VB> triangle) {
		if (triangles.empty()) aabb_min = aabb_max = triangle.a;

		triangles.push_back(triangle);
		aabb_max = max(aabb_max, max(triangle.c, max(triangle.b, triangle.a)));
		aabb_min = min(aabb_min, min(triangle.c, min(triangle.b, triangle.a)));
	}

	template<typename VB>
	inline const std::vector<triangle<VB>>& aabb<VB>::get_triangles() const { return triangles; }

	template<typename VB>
	inline bool aabb<VB>::aabb_test(const ray& ray) const {
		float3 inv_ray_dir = 1 / ray.direction;
		float3 t0 = (aabb_max - ray.position) * inv_ray_dir;
		float3 t1 = (aabb_min - ray.position) * inv_ray_dir;

		float3 t_min = min(t0, t1);
		float3 t_max = max(t0, t1);
		return maxelem(t_min) <= maxelem(t_max);	
	}

} // namespace cg::renderer