#pragma once

#include "resource.h"

#include <filesystem>


namespace cg::utils
{
	void save_resource(cg::resource<cg::ucolor>& render_target, const std::filesystem::path& filepath);
}