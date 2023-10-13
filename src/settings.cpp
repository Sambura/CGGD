#include "settings.h"

#include "utils/error_handler.h"

#include <cxxopts.hpp>
#include <iostream>

using namespace cg;

std::shared_ptr<settings> cg::settings::parse_settings(int argc, char** argv)
{
	std::shared_ptr<cg::settings> settings = std::make_shared<cg::settings>();

	cxxopts::Options options(argv[0], "Computer graphics in Game development");
	options.allow_unrecognised_options();

	auto add_options = options.add_options();
	add_options("height", "Render target height", cxxopts::value<unsigned>()->default_value("1080"));
	add_options("width", "Render target width", cxxopts::value<unsigned>()->default_value("1920"));
	add_options("model_path", "Path to OBJ model", cxxopts::value<std::filesystem::path>()->default_value("models/cube.obj"));
	add_options("camera_position", "Camera position", cxxopts::value<std::vector<float>>()->default_value("0.0,1.0,5.0"));
	add_options("camera_theta", "Camera polar angle", cxxopts::value<float>()->default_value("0.0"));
	add_options("camera_phi", "Camera azimuth angle", cxxopts::value<float>()->default_value("0.0"));
	add_options("camera_angle_of_view", "Camera angle of view", cxxopts::value<float>()->default_value("60.0"));
	add_options("camera_z_near", "Minimum expected depth", cxxopts::value<float>()->default_value("0.001"));
	add_options("camera_z_far", "Maximum expected depth", cxxopts::value<float>()->default_value("100.0"));
	add_options("disable_depth", "Disables depth buffer", cxxopts::value<bool>()->default_value("false"));
	add_options("result_path", "Path to resulted image", cxxopts::value<std::filesystem::path>()->default_value("result.png"));
	// apparently empty default value is illegal in this library 
	add_options("depth_export_path", "Exports the raw depth map as a binary file", cxxopts::value<std::filesystem::path>()->default_value("~~~~~~~~~~"));
	add_options("raytracing_depth", "Maximum number of traces rays", cxxopts::value<unsigned>()->default_value("1"));
	add_options("accumulation_num", "Number of accumulated frames", cxxopts::value<unsigned>()->default_value("1"));
	add_options("h,help", "Print usage");

	auto result = options.parse(argc, argv);

	if (result.count("help"))
	{
		THROW_ERROR(options.help());
	}

	settings->height = result["height"].as<unsigned>();
	settings->width = result["width"].as<unsigned>();
	settings->model_path = result["model_path"].as<std::filesystem::path>();
	settings->camera_position = result["camera_position"].as<std::vector<float>>();
	settings->camera_theta = result["camera_theta"].as<float>();
	settings->camera_phi = result["camera_phi"].as<float>();
	settings->camera_angle_of_view = result["camera_angle_of_view"].as<float>();
	settings->camera_z_near = result["camera_z_near"].as<float>();
	settings->camera_z_far = result["camera_z_far"].as<float>();
	settings->disable_depth = result["disable_depth"].as<bool>();
	settings->result_path = result["result_path"].as<std::filesystem::path>();
	settings->depth_result_path = result["depth_export_path"].as<std::filesystem::path>();
	if (settings->depth_result_path == "~~~~~~~~~~") settings->depth_result_path = "";
	settings->raytracing_depth = result["raytracing_depth"].as<unsigned>();
	settings->accumulation_num = result["accumulation_num"].as<unsigned>();

	const cxxopts::OptionNames& extras = result.unmatched();
	for (size_t i = 0; i < extras.size(); i++) {
		const std::string& option = extras[i];
		if (option[0] == '-') {
			if (i + 1 < extras.size() && (extras[i + 1][0] != '-' || (extras[i + 1].size() > 1 && std::isdigit(extras[i + 1][1])))) {
				settings->extra_options[option] = extras[++i];
				continue;
			}

			settings->extra_options[option] = "";
		} else {
			std::cerr << "Warning: unknown option: " << option << '\n';
		}
	}

	return settings;
}
