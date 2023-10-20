#include "raytracer_renderer.h"

#include "utils/resource_utils.h"

#include <iostream>

std::chrono::steady_clock::time_point __pet_start_time;

#define PRINT_EXECUTION_TIME(name, stmts) \
	__pet_start_time = std::chrono::high_resolution_clock::now(); \
	stmts \
	std::cout << name << ": " << \
		(static_cast<std::chrono::duration<float, std::milli>>( \
			std::chrono::high_resolution_clock::now() - __pet_start_time) \
		).count() << " ms.\n";

void cg::renderer::ray_tracing_renderer::init() {
	model = std::make_shared<cg::world::model>(settings->model_path);

	raytracer = std::make_shared<cg::renderer::raytracer<cg::vertex, cg::ucolor>>();
	raytracer->set_viewport(settings->width, settings->height);
	render_target = std::make_shared<cg::resource<cg::ucolor>>(settings->width, settings->height);
	raytracer->set_render_target(render_target);
	raytracer->set_vertex_buffers(model->get_vertex_buffers());
	raytracer->set_index_buffers(model->get_index_buffers());

	shadow_raytracer = std::make_shared<cg::renderer::raytracer<cg::vertex, cg::ucolor>>();

	lights.push_back({
		float3{0, 1.58f, -0.03f}, // position
		float3{0.78f} // color
	});

	camera = std::make_shared<cg::world::camera>();
	camera->set_height(static_cast<float>(settings->height));
	camera->set_width(static_cast<float>(settings->width));
	camera->set_position(float3{settings->camera_position[0], settings->camera_position[1], settings->camera_position[2]});
	camera->set_theta(settings->camera_theta);
	camera->set_phi(settings->camera_phi);
	camera->set_field_of_view(settings->camera_angle_of_view);
	camera->set_z_near(settings->camera_z_near);
	camera->set_z_far(settings->camera_z_far);
}

void cg::renderer::ray_tracing_renderer::destroy() {}

void cg::renderer::ray_tracing_renderer::update() {}

cg::renderer::payload skybox_shader(const cg::renderer::ray& ray) {
	cg::renderer::payload payload{}; 
	payload.color = cg::fcolor{0.f, 0.f, ray.direction.y / 2 + 0.5f}; 
	return payload; 
}

void cg::renderer::ray_tracing_renderer::render() {
	// TODO Lab: 2.06 (Bonus) Adjust `closest_hit_shader` for Monte-Carlo light tracing
	
	// TODO: Take angle of view into account

	raytracer->miss_shader = skybox_shader;
	raytracer->closest_hit_shader = [&](const ray& ray, payload& payload, const triangle<cg::vertex>& triangle, size_t depth) {
		payload.color = triangle.emissive;
		float3 position = ray.position + ray.direction * payload.t;
		float3 normal = payload.bary.x * triangle.na + payload.bary.y * triangle.nb + payload.bary.z * triangle.nc;
		
		for (auto& light : lights) {
			cg::renderer::ray to_light(position, light.position - position);
			cg::renderer::payload shadow = shadow_raytracer->trace_ray(to_light, 1, length(light.position - position));
			if (shadow.t > 0) continue;
			payload.color += light.color * triangle.diffuse * std::max(0.f, dot(normal, to_light.direction));
		}
		
		return payload;
	};
	
	shadow_raytracer->miss_shader = [](const ray& ray) { 
		payload payload{};
		payload.t = -1;
		return payload;
	};
	shadow_raytracer->any_hit_shader = [&](const ray& ray, payload& payload, const triangle<cg::vertex>& triangle) {
		return payload; 
	};

	raytracer->build_acceleration_structure();
	shadow_raytracer->acceleration_structures = raytracer->acceleration_structures;
	raytracer->clear_render_target({0, 0, 0});

	PRINT_EXECUTION_TIME("Ray tracing time",
		raytracer->ray_generation(
				camera->get_position(), 
				camera->get_forward(), 
				camera->get_right(), 
				camera->get_up(), 
				settings->raytracing_depth, 
				settings->accumulation_num
		);
	);

	cg::utils::save_resource(*render_target, settings->result_path);
	if (settings->show_render) cg::utils::open_file_with_system_app(settings->result_path);
}