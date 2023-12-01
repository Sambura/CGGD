#include "dx12_renderer.h"

#include "utils/com_error_handler.h"
#include "utils/window.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
	
#include <filesystem>


void cg::renderer::dx12_renderer::init() {
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

	view_port = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(settings->width), static_cast<float>(settings->height));
	scissor_rect = CD3DX12_RECT(0, 0, static_cast<LONG>(settings->width), static_cast<LONG>(settings->height));

	load_pipeline();
	load_assets();
}

void cg::renderer::dx12_renderer::destroy() {
	wait_for_gpu();
	CloseHandle(fence_event);
}

void cg::renderer::dx12_renderer::update() {
	auto now = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> duration = now - current_time;
	frame_duration = duration.count();
	current_time = now;

	cb.mwpMatrix = camera->get_dxm_mvp_matrix();
	memcpy(constant_buffer_data_begin, &cb, sizeof(cb));
}

void cg::renderer::dx12_renderer::render() {
	populate_command_list();

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

	THROW_IF_FAILED(swap_chain->Present(0, 0));

	move_to_next_frame();
}

ComPtr<IDXGIFactory4> cg::renderer::dx12_renderer::get_dxgi_factory() {
	UINT dxgi_factory_flags = 0;
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debug_controller;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
		debug_controller->EnableDebugLayer();
		dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
	} 
#endif

	ComPtr<IDXGIFactory4> dxgi_factory;
	THROW_IF_FAILED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dxgi_factory)));

	return dxgi_factory;
}

void cg::renderer::dx12_renderer::initialize_device(ComPtr<IDXGIFactory4>& dxgi_factory) {
	ComPtr<IDXGIAdapter1> hardware_adapter;
	dxgi_factory->EnumAdapters1(0, &hardware_adapter);

#ifdef _DEBUG
	DXGI_ADAPTER_DESC adapter_desc{};
	hardware_adapter->GetDesc(&adapter_desc);
	OutputDebugString(adapter_desc.Description);
	OutputDebugString(L"\n");
#endif

	THROW_IF_FAILED(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
}

void cg::renderer::dx12_renderer::create_direct_command_queue() {
	D3D12_COMMAND_QUEUE_DESC queue_desc{};
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	THROW_IF_FAILED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));
}

void cg::renderer::dx12_renderer::create_swap_chain(ComPtr<IDXGIFactory4>& dxgi_factory) {
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc {};
	swap_chain_desc.BufferCount = frame_number;
	swap_chain_desc.Height = settings->height;
	swap_chain_desc.Width = settings->width;
	swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swap_chain_desc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> temp_swap_chain;
	THROW_IF_FAILED(dxgi_factory->CreateSwapChainForHwnd(
		command_queue.Get(), 
		cg::utils::window::get_hwnd(),
		&swap_chain_desc,
		nullptr,
		nullptr,
		&temp_swap_chain 
	));

	dxgi_factory->MakeWindowAssociation(cg::utils::window::get_hwnd(), DXGI_MWA_NO_ALT_ENTER);
	temp_swap_chain.As(&swap_chain);
	frame_index = swap_chain->GetCurrentBackBufferIndex();
}

void cg::renderer::dx12_renderer::create_render_target_views() {
	rtv_heap.create_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, frame_number);
	for (UINT i = 0; i < frame_number; i++) {
		THROW_IF_FAILED(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_heap.get_cpu_descriptor_handle(i));
		std::wstring name(L"Render target ");
		name += std::to_wstring(i);
		render_targets[i]->SetName(name.c_str());
	}
}

void cg::renderer::dx12_renderer::create_depth_buffer() {
	CD3DX12_RESOURCE_DESC depth_buffer_desc(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		0, settings->width, settings->height,
		1, 1, DXGI_FORMAT_D32_FLOAT,
		1, 0, D3D12_TEXTURE_LAYOUT_UNKNOWN, 
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE
	);

	D3D12_CLEAR_VALUE depth_clear_value{};
	depth_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
	depth_clear_value.DepthStencil.Depth = 1.f;
	depth_clear_value.DepthStencil.Stencil = 0.f;
	
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, &depth_buffer_desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &depth_clear_value, IID_PPV_ARGS(&depth_buffer)
	));

	depth_buffer->SetName(L"Depth buffer");
	dsv_heap.create_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	device->CreateDepthStencilView(depth_buffer.Get(), nullptr, dsv_heap.get_cpu_descriptor_handle());
}

void cg::renderer::dx12_renderer::create_command_allocators() {
	for (auto& command_allocator : command_allocators) {
		THROW_IF_FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
	}
}

void cg::renderer::dx12_renderer::create_command_list() {
	THROW_IF_FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[0].Get(), 
		pipeline_state.Get(), IID_PPV_ARGS(&command_list)));
}

void cg::renderer::dx12_renderer::load_pipeline() {
	ComPtr<IDXGIFactory4> dxgi_factory = get_dxgi_factory();
	initialize_device(dxgi_factory);
	create_direct_command_queue();
	create_swap_chain(dxgi_factory);
	create_render_target_views();
	create_depth_buffer();
}

D3D12_STATIC_SAMPLER_DESC cg::renderer::dx12_renderer::get_sampler_descriptor() {
	D3D12_STATIC_SAMPLER_DESC sampler_desc{};
	return sampler_desc;
}

void cg::renderer::dx12_renderer::create_root_signature(const D3D12_STATIC_SAMPLER_DESC* sampler_descriptors, UINT num_sampler_descriptors) {
	CD3DX12_ROOT_PARAMETER1 root_parameters[1];
	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];

	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rs_feature_data{};
	rs_feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rs_feature_data, sizeof(rs_feature_data)))) {
		rs_feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rs_desc{};
	rs_desc.Init_1_1(
		_countof(root_parameters), 
		root_parameters, 
		num_sampler_descriptors, 
		sampler_descriptors,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> signature, error;
	if (HRESULT result = FAILED(D3DX12SerializeVersionedRootSignature(&rs_desc, rs_feature_data.HighestVersion, &signature, &error))) {
		OutputDebugStringA((char*) error->GetBufferPointer());
		THROW_IF_FAILED(result);
	}

	THROW_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
}

std::filesystem::path cg::renderer::dx12_renderer::get_shader_path(const std::string& shader_name) {
	WCHAR buffer[MAX_PATH];
	GetModuleFileName(nullptr, buffer, MAX_PATH);
	return std::filesystem::path(buffer).parent_path() / shader_name;
}

ComPtr<ID3DBlob> cg::renderer::dx12_renderer::compile_shader(const std::filesystem::path& shader_path, const std::string& entrypoint, const std::string& target)
{
	ComPtr<ID3DBlob> shader, error;
	UINT compile_flags = 0;

#ifdef _DEBUG
	compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG

	HRESULT res = D3DCompileFromFile(shader_path.wstring().c_str(), nullptr, nullptr, entrypoint.c_str(),
		target.c_str(), compile_flags, 0, &shader, &error);

	if (FAILED(res)) {
		OutputDebugStringA((char*) error->GetBufferPointer());
		THROW_IF_FAILED(res);
	}

	return shader;
}

void cg::renderer::dx12_renderer::create_pso(const std::string& shader_name) {
	ComPtr<ID3DBlob> vshader = compile_shader(get_shader_path(shader_name), "VSMain", "vs_5_0");
	ComPtr<ID3DBlob> pshader = compile_shader(get_shader_path(shader_name), "PSMain", "ps_5_0");
	
	D3D12_INPUT_ELEMENT_DESC element_descs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, norm), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertex, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, ambient), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, diffuse), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, emissive), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
	pso_desc.InputLayout = { element_descs, _countof(element_descs) };
	pso_desc.pRootSignature = root_signature.Get();
	pso_desc.VS = CD3DX12_SHADER_BYTECODE(vshader.Get());
	pso_desc.PS = CD3DX12_SHADER_BYTECODE(pshader.Get());
	pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pso_desc.RasterizerState.FrontCounterClockwise = TRUE;
	pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pso_desc.DepthStencilState.DepthEnable = TRUE;
	pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pso_desc.DepthStencilState.StencilEnable = FALSE;
	pso_desc.SampleMask = UINT_MAX;
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso_desc.NumRenderTargets = 1;
	pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pso_desc.SampleDesc.Count = 1;

	THROW_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));
}

void cg::renderer::dx12_renderer::create_resource_on_upload_heap(ComPtr<ID3D12Resource>& resource, UINT size, const std::wstring& name)
{
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	));
	
	if (!name.empty()) resource->SetName(name.c_str());
}

void cg::renderer::dx12_renderer::create_resource_on_default_heap(ComPtr<ID3D12Resource>& resource, UINT size, 
	const std::wstring& name, D3D12_RESOURCE_DESC* resource_descriptor)
{
	if (resource_descriptor == nullptr) {
		resource_descriptor = &CD3DX12_RESOURCE_DESC::Buffer(size);
	}

	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		resource_descriptor,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	));

	if (!name.empty()) resource->SetName(name.c_str());
}

void cg::renderer::dx12_renderer::copy_data(const void* buffer_data, UINT buffer_size, ComPtr<ID3D12Resource>& destination_resource)
{
	UINT8* buffer_data_begin;
	CD3DX12_RANGE read_range(0, 0);
	THROW_IF_FAILED(destination_resource->Map(0, &read_range, reinterpret_cast<void**>(&buffer_data_begin)));
	memcpy(buffer_data_begin, buffer_data, buffer_size);
	destination_resource->Unmap(0, 0);
}

void cg::renderer::dx12_renderer::copy_data(const void* buffer_data, const UINT buffer_size, ComPtr<ID3D12Resource>& destination_resource, 
	ComPtr<ID3D12Resource>& intermediate_resource, D3D12_RESOURCE_STATES state_after, int row_pitch, int slice_pitch)
{
	D3D12_SUBRESOURCE_DATA data{};
	data.pData = buffer_data;
	data.RowPitch = row_pitch ? row_pitch : buffer_size;
	data.SlicePitch = slice_pitch ? slice_pitch : buffer_size;
	
	UpdateSubresources(command_list.Get(), destination_resource.Get(), intermediate_resource.Get(), 0, 0, 1, &data);

	command_list->ResourceBarrier(
		1, &CD3DX12_RESOURCE_BARRIER::Transition(destination_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, state_after)
	);
}

D3D12_VERTEX_BUFFER_VIEW cg::renderer::dx12_renderer::create_vertex_buffer_view(const ComPtr<ID3D12Resource>& vertex_buffer, const UINT vertex_buffer_size)
{
	D3D12_VERTEX_BUFFER_VIEW view{};
	view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
	view.StrideInBytes = sizeof(vertex);
	view.SizeInBytes = vertex_buffer_size;
	
	return view;
}

D3D12_INDEX_BUFFER_VIEW cg::renderer::dx12_renderer::create_index_buffer_view(const ComPtr<ID3D12Resource>& index_buffer, const UINT index_buffer_size)
{
	D3D12_INDEX_BUFFER_VIEW view{};
	view.BufferLocation = index_buffer->GetGPUVirtualAddress();
	view.SizeInBytes = index_buffer_size;
	view.Format = DXGI_FORMAT_R32_UINT;
	
	return view;
}

void cg::renderer::dx12_renderer::create_shader_resource_view(const ComPtr<ID3D12Resource>& texture, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handler)
{
}

void cg::renderer::dx12_renderer::create_constant_buffer_view(const ComPtr<ID3D12Resource>& buffer, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handler)
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc {};
	cbv_desc.BufferLocation = buffer->GetGPUVirtualAddress();
	cbv_desc.SizeInBytes = (sizeof(cb) + 255) & ~255;
	device->CreateConstantBufferView(&cbv_desc, cpu_handler);
}

void cg::renderer::dx12_renderer::load_assets()
{
	create_root_signature(nullptr, 0);
	create_pso("shaders.hlsl");
	create_command_allocators();
	create_command_list();

	cbv_srv_heap.create_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	auto& model_vertex_buffers = model->get_vertex_buffers();
	auto& model_index_buffers = model->get_index_buffers();

	upload_vertex_buffers.resize(model_vertex_buffers.size());
	upload_index_buffers.resize(model_index_buffers.size());
	vertex_buffers.resize(model_vertex_buffers.size());
	vertex_buffer_views.resize(model_vertex_buffers.size());
	index_buffers.resize(model_index_buffers.size());
	index_buffer_views.resize(model_index_buffers.size());

	for (size_t i = 0; i < model_vertex_buffers.size(); i++) {
		auto vertex_buffer_data = model_vertex_buffers[i];
		const UINT vertex_buffer_size = static_cast<UINT>(vertex_buffer_data->get_size_in_bytes());
		std::wstring vertex_buffer_name(L"Vertex buffer ");
		vertex_buffer_name += std::to_wstring(i);

		create_resource_on_default_heap(vertex_buffers[i], vertex_buffer_size, vertex_buffer_name);
		create_resource_on_upload_heap(upload_vertex_buffers[i], vertex_buffer_size);
		copy_data(
			vertex_buffer_data->get_data(), vertex_buffer_size, vertex_buffers[i], 
			upload_vertex_buffers[i], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		vertex_buffer_views[i] = create_vertex_buffer_view(vertex_buffers[i], vertex_buffer_size);
	}

	for (size_t i = 0; i < model_index_buffers.size(); i++) {
		auto index_buffer_data = model_index_buffers[i];
		const UINT index_buffer_size = static_cast<UINT>(index_buffer_data->get_size_in_bytes());
		std::wstring index_buffer_name(L"Index buffer ");
		index_buffer_name += std::to_wstring(i);

		create_resource_on_default_heap(index_buffers[i], index_buffer_size, index_buffer_name);
		create_resource_on_upload_heap(upload_index_buffers[i], index_buffer_size);
		copy_data(
			index_buffer_data->get_data(), index_buffer_size, index_buffers[i],
			upload_index_buffers[i], D3D12_RESOURCE_STATE_INDEX_BUFFER);
		index_buffer_views[i] = create_index_buffer_view(index_buffers[i], index_buffer_size);
	}

	std::wstring const_buffer_name(L"Constant buffer");
	create_resource_on_upload_heap(constant_buffer, 64 * 1024, const_buffer_name);
	copy_data(&cb, sizeof(cb), constant_buffer);
	CD3DX12_RANGE read_range(0, 0);
	THROW_IF_FAILED(constant_buffer->Map(0, &read_range, reinterpret_cast<void**>(&constant_buffer_data_begin)));
	create_constant_buffer_view(constant_buffer, cbv_srv_heap.get_cpu_descriptor_handle());

	THROW_IF_FAILED(command_list->Close());
	ID3D12CommandList* command_lists[] = {command_list.Get()};
	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);
	THROW_IF_FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr) THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));

	wait_for_gpu();
}

void cg::renderer::dx12_renderer::populate_command_list() {
	THROW_IF_FAILED(command_allocators[frame_index]->Reset());
	THROW_IF_FAILED(command_list->Reset(command_allocators[frame_index].Get(), pipeline_state.Get()));

	command_list->SetGraphicsRootSignature(root_signature.Get());
	ID3D12DescriptorHeap* heaps[] = { cbv_srv_heap.get() };
	command_list->SetDescriptorHeaps(_countof(heaps), heaps);
	command_list->SetGraphicsRootDescriptorTable(0, cbv_srv_heap.get_gpu_descriptor_handle(0));
	command_list->RSSetViewports(1, &view_port);
	command_list->RSSetScissorRects(1, &scissor_rect);
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_RESOURCE_BARRIER begin_barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(render_targets[frame_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
	};
	command_list->ResourceBarrier(_countof(begin_barriers), begin_barriers);

	command_list->OMSetRenderTargets(1, &rtv_heap.get_cpu_descriptor_handle(frame_index), FALSE, &dsv_heap.get_cpu_descriptor_handle());
	const float clear_color[] = { 0.f, 0.f, 0.f, 1.f };
	command_list->ClearRenderTargetView(rtv_heap.get_cpu_descriptor_handle(frame_index), clear_color, 0, nullptr);
	command_list->ClearDepthStencilView(dsv_heap.get_cpu_descriptor_handle(), D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

	for (size_t s = 0; s < model->get_vertex_buffers().size(); s++) {
		command_list->IASetVertexBuffers(0, 1, &vertex_buffer_views[s]);
		command_list->IASetIndexBuffer(&index_buffer_views[s]);
		command_list->DrawIndexedInstanced(static_cast<UINT>(model->get_index_buffers()[s]->get_number_of_elements()), 1, 0, 0, 0);
	}

	D3D12_RESOURCE_BARRIER end_barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(render_targets[frame_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)
	};
	command_list->ResourceBarrier(_countof(end_barriers), end_barriers);
	
	THROW_IF_FAILED(command_list->Close());
}

void cg::renderer::dx12_renderer::move_to_next_frame() {
	const UINT64 current_fence_value = fence_values[frame_index];
	THROW_IF_FAILED(command_queue->Signal(fence.Get(), current_fence_value));
	frame_index = swap_chain->GetCurrentBackBufferIndex();
	if (fence->GetCompletedValue() < fence_values[frame_index]) {
		THROW_IF_FAILED(fence->SetEventOnCompletion(fence_values[frame_index], fence_event));
		WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
	}
	fence_values[frame_index] = current_fence_value + 1;
}

void cg::renderer::dx12_renderer::wait_for_gpu() {
	THROW_IF_FAILED(command_queue->Signal(fence.Get(), fence_values[frame_index]));
	THROW_IF_FAILED(fence->SetEventOnCompletion(fence_values[frame_index], fence_event));
	WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
	fence_values[frame_index]++;
}

void cg::renderer::descriptor_heap::create_heap(ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT number, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
	heap_desc.NumDescriptors = number;
	heap_desc.Type = type;
	heap_desc.Flags = flags;
	
	THROW_IF_FAILED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)));
	descriptor_size = device->GetDescriptorHandleIncrementSize(type);
}

D3D12_CPU_DESCRIPTOR_HANDLE cg::renderer::descriptor_heap::get_cpu_descriptor_handle(UINT index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), descriptor_size);
}

D3D12_GPU_DESCRIPTOR_HANDLE cg::renderer::descriptor_heap::get_gpu_descriptor_handle(UINT index) const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), descriptor_size);
}

ID3D12DescriptorHeap* cg::renderer::descriptor_heap::get() const { return heap.Get(); }
