#pragma once

#include "utils/error_handler.h"

#include <algorithm>
#include <linalg.h>
#include <vector>


using namespace linalg::aliases;

namespace cg
{
	template<typename T>
	class resource
	{
	public:
		resource(size_t size);
		resource(size_t x_size, size_t y_size);
		virtual ~resource();

		const T* get_data();
		T& item(size_t item);
		T& item(size_t x, size_t y);

		size_t get_size_in_bytes() const;
		size_t get_number_of_elements() const;
		size_t get_stride() const;

	private:
		std::vector<T> data;
		const size_t item_size = sizeof(T);
		const size_t stride;
	};

	template<typename T>
	inline resource<T>::resource(size_t size) : data(size), stride(0) {}
	
	template<typename T>
	inline resource<T>::resource(size_t x_size, size_t y_size) : data(x_size * y_size), stride(x_size) {}
	
	template<typename T>
	inline resource<T>::~resource() {}

	template<typename T>
	inline const T* resource<T>::get_data() { return data.data(); }

	template<typename T>
	inline T& resource<T>::item(size_t item) { return data.at(item); /* data[item]; */ }

	template<typename T>
	inline T& resource<T>::item(size_t x, size_t y) { return data.at(x + stride * y); /* data[x + stride * y]; */ }

	template<typename T>
	inline size_t resource<T>::get_size_in_bytes() const { return item_size * data.size(); }

	template<typename T>
	inline size_t resource<T>::get_number_of_elements() const { return data.size(); }

	template<typename T>
	inline size_t resource<T>::get_stride() const { return stride; }

	struct color
	{
		static color from_float3(const float3& in) { return color{ in.x, in.y, in.z }; }
		float3 to_float3() const { return float3{r, g, b}; }

		float r;
		float g;
		float b;
	};

	struct unsigned_color
	{
		static unsigned_color from_color(const color& color)
		{
			return unsigned_color{
				static_cast<uint8_t>(std::clamp(color.r, 0.f, 1.f) * 255), 
				static_cast<uint8_t>(std::clamp(color.g, 0.f, 1.f) * 255), 
				static_cast<uint8_t>(std::clamp(color.b, 0.f, 1.f) * 255)
			};
		};

		static unsigned_color from_float3(const float3& color) {
			float3 clamped = clamp(color, 0, 1) * 255;

			return unsigned_color{ 
				static_cast<uint8_t>(clamped.x), 
				static_cast<uint8_t>(clamped.y),
				static_cast<uint8_t>(clamped.z),
			};
		}

		float3 to_float3() const { 
			return float3{ 
				static_cast<float>(r), 
				static_cast<float>(g), 
				static_cast<float>(b), 
			} / 255.f; 
		};

		uint8_t r;
		uint8_t g;
		uint8_t b;
	};

	using ucolor = byte3;
	using fcolor = float3;

	struct vertex
	{
		float3 pos;
		float3 norm;
		float2 uv;
		fcolor ambient;
		fcolor diffuse;
		fcolor emissive;
	};

} // namespace cg