#include "rasterizer_renderer.h"

#include "utils/resource_utils.h"


void cg::renderer::rasterization_renderer::init()
{
	// TODO Lab: 1.03 Adjust `cg::renderer::rasterization_renderer` class to consume `cg::world::model`
	// TODO Lab: 1.04 Setup an instance of camera `cg::world::camera` class in `cg::renderer::rasterization_renderer`
	// TODO Lab: 1.06 Add depth buffer in `cg::renderer::rasterization_renderer`

	rasterizer = std::make_shared<cg::renderer::rasterizer<cg::vertex, cg::ucolor>>();
	rasterizer->set_viewport(settings->width, settings->height);
	render_target = std::make_shared<cg::resource<cg::ucolor>>(settings->width, settings->height);
	rasterizer->set_render_target(render_target);
}

void cg::renderer::rasterization_renderer::render()
{
	// TODO Lab: 1.04 Implement `vertex_shader` lambda for the instance of `cg::renderer::rasterizer`
	// TODO Lab: 1.05 Implement `pixel_shader` lambda for the instance of `cg::renderer::rasterizer`
	// TODO Lab: 1.03 Adjust `cg::renderer::rasterization_renderer` class to consume `cg::world::model`

	auto start_time = std::chrono::high_resolution_clock::now();
	rasterizer->clear_render_target({255, 0, 0});
	auto end_time = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> render_time = end_time - start_time;
	std::cout << "Render time: " << render_time.count() << " ms.\n";

	cg::utils::save_resource(*render_target, settings->result_path);
}

void cg::renderer::rasterization_renderer::destroy() {}

void cg::renderer::rasterization_renderer::update() {}