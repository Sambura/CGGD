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
	// TODO Lab: 2.03 Add light information to `lights` array of `ray_tracing_renderer`
	// TODO Lab: 2.04 Initialize `shadow_raytracer` in `ray_tracing_renderer`

	raytracer = std::make_shared<cg::renderer::raytracer<cg::vertex, cg::ucolor>>();
	raytracer->set_viewport(settings->width, settings->height);
	render_target = std::make_shared<cg::resource<cg::ucolor>>(settings->width, settings->height);
	raytracer->set_render_target(render_target);

	model = std::make_shared<cg::world::model>(settings->model_path);

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

void cg::renderer::ray_tracing_renderer::render() {
	// TODO Lab: 2.02 Add `closest_hit_shader` to `raytracer` class to return diffuse color
	// TODO Lab: 2.03 Adjust `closest_hit_shader` of `raytracer` to implement Lambertian shading model
	// TODO Lab: 2.04 Define `any_hit_shader` and `miss_shader` for `shadow_raytracer`
	// TODO Lab: 2.04 Adjust `closest_hit_shader` of `raytracer` to cast shadows rays and to ignore occluded lights
	// TODO Lab: 2.05 Adjust `ray_tracing_renderer` class to build the acceleration structure
	// TODO Lab: 2.06 (Bonus) Adjust `closest_hit_shader` for Monte-Carlo light tracing

	raytracer->miss_shader = [](const ray& ray) { payload payload{}; payload.color = cg::fcolor{0.f, 0.f, ray.direction.y / 2 + 0.5f}; return payload; };

	raytracer->clear_render_target({0, 0, 0});

	PRINT_EXECUTION_TIME("Ray tracing time:",
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