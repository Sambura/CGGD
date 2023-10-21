#pragma once

#include "resource.h"

#include <filesystem>

extern std::chrono::steady_clock::time_point __pet_start_time;

#define PRINT_EXECUTION_TIME(name, stmts) \
	__pet_start_time = std::chrono::high_resolution_clock::now(); \
	stmts \
	std::cout << name << ": " << \
		(static_cast<std::chrono::duration<float, std::milli>>( \
			std::chrono::high_resolution_clock::now() - __pet_start_time) \
		).count() << " ms.\n";

namespace cg::utils
{
	void open_file_with_system_app(const std::filesystem::path& filepath);

	void save_resource(cg::resource<cg::ucolor>& render_target, const std::filesystem::path& filepath);
	void save_resource(cg::resource<float>& depth_buffer, const std::filesystem::path& filepath);
}