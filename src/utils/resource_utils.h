#pragma once

#include "resource.h"

#include <filesystem>


namespace cg::utils
{
	void open_file_with_system_app(const std::filesystem::path& filepath);

	void save_resource(cg::resource<cg::ucolor>& render_target, const std::filesystem::path& filepath);
	void save_resource(cg::resource<float>& depth_buffer, const std::filesystem::path& filepath);
}