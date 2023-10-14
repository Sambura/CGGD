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

	using ucolor = byte3;
	using fcolor = float3;

	inline ucolor from_fcolor(fcolor color) { return static_cast<ucolor>(clamp(color * 255, 0, 255)); };
	inline fcolor from_ucolor(ucolor color) { return static_cast<fcolor>(static_cast<fcolor>(color) / 255); };

	struct vertex
	{
		float4 pos;
		float3 norm;
		float2 uv;
		fcolor ambient;
		fcolor diffuse;
		fcolor emissive;
	};

} // namespace cg