#pragma once
#include <types.h>
#include <render/render.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>
#include <render/api/directx12/constant_buffer.h>
#include <render/api/directx12/render_target.h>
#include <render/api/directx12/descriptor_heap.h>
#include <render/api/directx12/shader_input.h>
#include <render/model.h>

// TODO: root_parameters.h
// TODO: post_processing.h

enum e_root_parameters_default
{
	// DO NOT MOVE THIS, CONSTANT BUFFERS MUST BE FIRST ROOT PARAMETERS, TEXTURE TABLE AFTER
	_default_root_parameter_textures = k_deferred_constant_buffer_count,

	k_default_root_parameters_count
};
enum e_root_parameters_post_processing
{
	// DO NOT MOVE THIS, CONSTANT BUFFERS MUST BE FIRST ROOT PARAMETERS, TEXTURE TABLE AFTER
	_post_root_parameter_textures = k_post_constant_buffer_count,

	k_post_root_parameters_count
};

class c_shader;
class c_renderer_dx12 : public c_renderer
{	
public:
	~c_renderer_dx12();

	bool initialise(const HWND hWnd) override;
	void render_frame(c_scene* const scene, dword fps_counter) override;
	void set_object_constant_buffer(const s_object_cb& cbuffer, const dword object_index) override;
	void set_material_constant_buffer(const s_material_properties_cb& cbuffer, const dword object_index) override;
	void set_lights_constant_buffer(const s_light_properties_cb& cbuffer) override;
	void set_post_constant_buffer(const s_post_parameters_cb& cbuffer) override;

	// Halts the thread until the previous frame has finished execution
	bool wait_for_previous_frame() override;
	// Load a texture from a .DDS file into out_resources stored in material's descriptor heap
	bool load_texture(const e_texture_type texture_type, const wchar_t* const file_path, s_texture_resources* const out_resources) override;
	// create a mesh from vertex & index buffers and upload the data to the GPU
	bool create_geometry(vertex vertices[], dword vertices_size, dword indices[], dword indices_size, s_geometry_resources* const out_resources) override;
	// create a mesh from a simple mesh & index buffer
	bool create_simple_geometry(simple_vertex vertices[], dword vertices_size, s_geometry_resources* const out_resources);
	// Load a geometry data from a .VBO file
	bool load_model(const wchar_t* const file_path, s_geometry_resources* const out_resources) override;
	// Load a vertex & pixel shader from a .hlsl file
	bool create_shader(const wchar_t* vs_path, const char* vs_name, const wchar_t* ps_path, const char* ps_name, const e_shader_input input_type, s_shader_resources* out_resources) override;
	// Get the ImGUI gbuffer texture ID
	qword get_gbuffer_textureid(e_gbuffers gbuffer_type) const override;

private:
	// Initialisation methods used by initialise()
	bool initialise_factory();
	bool initialise_device_adapter();
	bool initialise_command_queue();
	bool initialise_swapchain(const HWND hWnd);
	bool initialise_render_target_view();
	bool initialise_command_allocators();
	bool initialise_command_list();
	bool initialise_fences();
	bool initialise_input_layouts();
	bool initialise_default_geometry();
	bool initialise_imgui(const HWND hWnd);

	// Upload pending assets on the command queue after initialisation
	bool upload_assets();

	// Upload vertex data to buffer stored in out_resources
	bool upload_vertex_buffer(const dword vertex_size, const void* const vertices, const dword vertices_size, s_geometry_resources* const out_resources);
	
	// Upload index data to buffer stored in out_resources
	bool upload_index_buffer(const dword indices[], const dword indices_size, s_geometry_resources* const out_resources);
	
	// Upload vertex & index data to buffers in out_resources
	bool upload_geometry(const dword vertex_size, const void* const vertices, const dword vertices_size, const dword indices[], const dword indices_size, s_geometry_resources* const out_resources);

	// Update pipeline prior to render
	void update_pipeline(c_scene* const scene, dword fps_counter);

	// Perform post processing pass
	void post_processing(const e_post_processing_passes pass, ID3D12Resource* const texture_resources[], const dword buffer_flags);

	// Set constant buffer view to use for render
	void set_constant_buffer_view(const c_render_target* const target, const e_constant_buffers buffer_type, const dword buffer_index);

	bool m_initialised;
#ifdef _DEBUG
	ID3D12Debug* m_dx12_debug;
#endif
	IDXGIFactory7* m_factory; // Provides methods for generating DXGI objects
	ID3D12Device* m_device; // Virtual adapter, representing a GPU
	IDXGIAdapter4* m_adapter; // Display subsystem, representing one or more GPUs
	ID3D12CommandQueue* m_command_queue; // Provides methods for submitting command lists to the GPU
	IDXGISwapChain3* m_swapchain; // Alternating display surfaces (back-buffer & front-buffer)
	ID3D12Resource* m_backbuffers[FRAME_BUFFER_COUNT]; // swapchain backbuffers
	
	// Descriptors describe an object to the GPU

	// Render targets - TODO: move heaps to c_render_target
	c_render_target* m_render_targets[k_render_target_count]; // Render targets

	s_geometry_resources m_screen_quad; // Screen quad used to draw rendered scene texture to

	// TODO: TEMPORARY, MOVE THIS!!
	c_shader* m_deferred_shader;
	c_shader* m_lighting_shader;
	c_shader* m_shading_shader;
	c_shader* m_texcam_shader;
	c_shader* m_post_shaders[k_post_processing_passes];

	ID3D12CommandAllocator* m_command_allocators[FRAME_BUFFER_COUNT]; // Allocations of storage for GPU commands (we want enough allocators for each buffer * number of threads (we only have one thread))

	c_shader_input* m_shader_inputs[k_shader_input_count]; // Defines what resources are bound to the graphics pipeline

	ID3D12GraphicsCommandList* m_command_list; // Encapsulates a list of graphics commands for rendering & instruments command list execution
	CD3DX12_VIEWPORT m_viewport; // Viewports for rasterisation
	CD3DX12_RECT m_scissor_rect;

	// "Designate a descriptor from your descriptor heap for Dear ImGui to use internally for its font texture's SRV"
	c_descriptor_heap* m_imgui_descriptor_heap;
	D3D12_GPU_DESCRIPTOR_HANDLE m_gbuffer_gpu_handles[k_gbuffer_count + k_light_buffer_count + 1]; // + depth

	// Synchronisation objects
	dword m_frame_index; // Current frame index on the swapchain
	HANDLE m_fence_event; // Frame synchronisation event handle
	ID3D12Fence* m_fences[FRAME_BUFFER_COUNT]; // The GPU object we notify when to do work, and the object we wait for the work to be done
	qword m_fence_values[FRAME_BUFFER_COUNT]; // Updating value which can signal when work is done
};