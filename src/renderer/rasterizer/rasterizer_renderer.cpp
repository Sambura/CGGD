#include "rasterizer_renderer.h"

#include "utils/resource_utils.h"

std::chrono::steady_clock::time_point __pet_start_time;

#define PRINT_EXECUTION_TIME(name, stmts) \
	__pet_start_time = std::chrono::high_resolution_clock::now(); \
	stmts \
	std::cout << name << ": " << \
		(static_cast<std::chrono::duration<float, std::milli>>( \
			std::chrono::high_resolution_clock::now() - __pet_start_time) \
		).count() << " ms.\n";

void cg::renderer::rasterization_renderer::init()
{
	// TODO Lab: 1.06 Add depth buffer in `cg::renderer::rasterization_renderer`
	
	rasterizer = std::make_shared<cg::renderer::rasterizer<cg::vertex, cg::ucolor>>();
	rasterizer->set_viewport(settings->width, settings->height);
	render_target = std::make_shared<cg::resource<cg::ucolor>>(settings->width, settings->height);
	rasterizer->set_render_target(render_target);

	model = std::make_shared<cg::world::model>();
	model->load_obj(settings->model_path);

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

void cg::renderer::rasterization_renderer::render()
{
	float4x4 matrix = mul(
		camera->get_projection_matrix(), 
		camera->get_view_matrix(),
		model->get_world_matrix()
	);
	rasterizer->vertex_shader = [&](float4 vertex, cg::vertex data) {
		float4 processed = mul(matrix, vertex);
		return std::make_pair(processed, data);
	};
	rasterizer->pixel_shader = [](cg::vertex data, float z) {
		return data.ambient;
	};

	PRINT_EXECUTION_TIME("Clear time", 
		rasterizer->clear_render_target({15, 15, 15});
	);

	PRINT_EXECUTION_TIME("Draw time",
		auto& vertices = model->get_vertex_buffers();
		auto& indices = model->get_index_buffers();

		for (size_t mesh_idx = 0; mesh_idx < indices.size(); mesh_idx++) {
			rasterizer->set_vertex_buffer(vertices[mesh_idx]);
			rasterizer->set_index_buffer(indices[mesh_idx]);
			rasterizer->draw(indices[mesh_idx]->get_number_of_elements(), 0);
		}
	);

	// save render target as an image at `settings->result_path`
	cg::utils::save_resource(*render_target, settings->result_path);
}

void cg::renderer::rasterization_renderer::destroy() {}

void cg::renderer::rasterization_renderer::update() {}