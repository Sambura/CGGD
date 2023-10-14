#include "rasterizer_renderer.h"

#include "utils/resource_utils.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::chrono::steady_clock::time_point __pet_start_time;

#define PRINT_EXECUTION_TIME(name, stmts) \
	__pet_start_time = std::chrono::high_resolution_clock::now(); \
	stmts \
	std::cout << name << ": " << \
		(static_cast<std::chrono::duration<float, std::milli>>( \
			std::chrono::high_resolution_clock::now() - __pet_start_time) \
		).count() << " ms.\n";

void cg::renderer::rasterization_renderer::init() {
	rasterizer = std::make_shared<cg::renderer::rasterizer<cg::vertex, cg::ucolor>>();
	rasterizer->set_viewport(settings->width, settings->height);
	render_target = std::make_shared<cg::resource<cg::ucolor>>(settings->width, settings->height);

	if (!settings->disable_depth)
		depth_buffer = std::make_shared<cg::resource<float>>(settings->width, settings->height);
	
	rasterizer->set_render_target(render_target, depth_buffer);

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

typedef std::function<cg::fcolor (const cg::vertex&, void*)> PixelShader;
typedef std::function<cg::fcolor(float x, float y)> sampler2D;

cg::fcolor empty_pixel_shader(const cg::vertex& vertex, void* data) { return {0,0,0}; }
cg::fcolor ambient_pixel_shader(const cg::vertex& vertex, void* data) { return vertex.ambient; }

PixelShader depth_pixel_shader(float bias, float fade) {
	return [bias, fade](const cg::vertex& vertex, void* data) {
		float z = vertex.pos.z * vertex.pos.w;
		return vertex.ambient * (1 + std::clamp(bias - fade * z, -1.f, 0.f));
	};
}

PixelShader fog_pixel_shader(float bias, float fade) {
	return [bias, fade](const cg::vertex& vertex, void* data) {
		float z = vertex.pos.z * vertex.pos.w;
		return vertex.ambient + cg::fcolor{0.7f} * std::clamp(fade * z - bias, 0.f, 1.f);
	};
}

cg::fcolor texture_pixel_shader(const cg::vertex& vertex, void* data) { 
	if (data == nullptr) return vertex.ambient;

	sampler2D* tex = (sampler2D*) data;
	cg::fcolor pixelColor = (*tex)(vertex.uv.x, vertex.uv.y);
	return clamp(pixelColor + vertex.ambient, 0.f, 1.f);
}

sampler2D get_texture_sampler_nn(const unsigned char* texture, int w, int h, int ch_count) {
	if (texture == nullptr) return [] (float x, float y) { return cg::fcolor{1}; };

	return [texture, w, h, ch_count](float x, float y) { 
		// C++ modulus operator strikes again (it doesn't work with negative numbers)
		int x_pixel = (static_cast<int>(x * w) % w + w) % w;
		int y_pixel = (static_cast<int>(y * h) % h + h) % h;
		int index = (x_pixel + (h - y_pixel - 1) * w) * ch_count;
		return cg::from_ucolor(cg::ucolor {
			texture[index + 0], 
			texture[index + 1], 
			texture[index + 2]
		}); 
	};
}

void cg::renderer::rasterization_renderer::render()
{
	float4x4 matrix = mul(
		camera->get_projection_matrix(), 
		camera->get_view_matrix(),
		model->get_world_matrix()
	);
	rasterizer->vertex_shader = [&](cg::vertex vertex) {
		vertex.pos = mul(matrix, vertex.pos);
		return vertex;
	};

	auto it = settings->extra_options.find("--lps_fade");
	float fade = (it != settings->extra_options.end()) ? std::stof(it->second) : 0.1f;
	it = settings->extra_options.find("--lps_bias");
	float bias = (it != settings->extra_options.end()) ? std::stof(it->second) : fade * length(camera->get_position() - 0.5f);
	bool zshader = settings->extra_options.find("--zshader") != settings->extra_options.end();
	bool fogshader = settings->extra_options.find("--fogshader") != settings->extra_options.end();

	rasterizer->pixel_shader = zshader ? depth_pixel_shader(bias, fade) : (fogshader ? fog_pixel_shader(bias, fade) : texture_pixel_shader);

	PRINT_EXECUTION_TIME("Clear time", 
		rasterizer->clear_render_target({0, 0, 0});
	);

	PRINT_EXECUTION_TIME("Draw time", 
		auto& vertices = model->get_vertex_buffers();
		auto& indices = model->get_index_buffers();
		auto& textures = model->get_per_shape_texture_files();

		for (size_t mesh_idx = 0; mesh_idx < indices.size(); mesh_idx++) {
			rasterizer->set_vertex_buffer(vertices[mesh_idx]);
			rasterizer->set_index_buffer(indices[mesh_idx]);
			auto& path = textures[mesh_idx];
			int w;
			int h;
			int _;
			unsigned char *data = stbi_load(path.string().c_str(), &w, &h, &_, 3);

			sampler2D texSampler = get_texture_sampler_nn(data, w, h, 3);
			rasterizer->draw(indices[mesh_idx]->get_number_of_elements(), 0, data ? &texSampler : nullptr);
			if (data) stbi_image_free(data);
		}
	);

	// save render target as an image at `settings->result_path`
	cg::utils::save_resource(*render_target, settings->result_path);
	if (!settings->depth_result_path.empty() && depth_buffer)
		cg::utils::save_resource(*depth_buffer, settings->depth_result_path);
	if (settings->show_render) cg::utils::open_file_with_system_app(settings->result_path);
}

void cg::renderer::rasterization_renderer::destroy() {}

void cg::renderer::rasterization_renderer::update() {}