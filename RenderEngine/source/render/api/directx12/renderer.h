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
#include <vector>
#include <dxcapi.h>
#include "nv_helpers_dx12/ShaderBindingTableGenerator.h"
#include <DescriptorHeap.h>

// #DXR
struct s_acceleration_structure_buffers
{
	ID3D12Resource* pScratch;      // Scratch memory for AS builder
	ID3D12Resource* pResult;       // Where the AS is
	ID3D12Resource* pInstanceDesc; // Hold the matrices of the instances
};

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

	bool initialise(const HWND hWnd, c_scene* const scene) override;
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
	// Load a geometry data from a .VBO_I32 file
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
	void update_raytrace(c_scene* const scene);
	void update_raster(c_scene* const scene);

	// Perform post processing pass
	void post_processing(const e_post_processing_passes pass, ID3D12Resource* const texture_resources[], const dword buffer_flags);

	// Set constant buffer view to use for render
	void set_constant_buffer_view(const c_render_target* const target, const e_constant_buffers buffer_type, const dword buffer_index);

	bool m_initialised;
#ifdef _DEBUG
	ID3D12Debug1* m_dx12_debug;
	ID3D12DeviceRemovedExtendedDataSettings* m_dred_settings;
#endif
	IDXGIFactory7* m_factory; // Provides methods for generating DXGI objects
	ID3D12Device5* m_device; // Virtual adapter, representing a GPU
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
	c_shader* m_pbr_shader;
	//c_shader* m_shading_shader;
	c_shader* m_texcam_shader;
	c_shader* m_post_shaders[k_post_processing_passes];

	// $TODO: why do I have one of these per buffered frame but only one command list?
	ID3D12CommandAllocator* m_command_allocators[FRAME_BUFFER_COUNT]; // Allocations of storage for GPU commands (we want enough allocators for each buffer * number of threads (we only have one thread))

	c_shader_input* m_shader_inputs[k_shader_input_count]; // Defines what resources are bound to the graphics pipeline

	ID3D12GraphicsCommandList4* m_command_list; // Encapsulates a list of graphics commands for rendering & instruments command list execution
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




	// == RAYTRACING ==
	bool initialise_raytracing_pipeline();

	s_acceleration_structure_buffers m_top_level_as_buffers;
	std::vector<s_acceleration_structure_buffers> m_bottom_level_as; // Storage for the bottom Level AS

	//nv_helpers_dx12::TopLevelASGenerator m_top_level_as_generator;

	/// Create the acceleration structure of an instance
	///
	/// \param     vVertexBuffers : pair of buffer and vertex count
	/// \return    AccelerationStructureBuffers for TLAS
	void create_bottom_level_as(c_scene* const scene, s_acceleration_structure_buffers& out_buffers, bool update_only = false);

	/// Create the main acceleration structure that holds
	/// all instances of the scene
	/// \param     instances : pair of BLAS and transform
	void create_top_level_as(const std::vector<std::pair<s_acceleration_structure_buffers, DirectX::XMMATRIX>> &instances, bool update_only = false);

	ID3D12RootSignature* create_ray_gen_signature();
	ID3D12RootSignature* create_hit_signature();
	ID3D12RootSignature* create_miss_signature();

	IDxcBlob* m_ray_gen_library;
	IDxcBlob* m_hit_library;
	IDxcBlob* m_miss_library;

	ID3D12RootSignature* m_ray_gen_signature;
	ID3D12RootSignature* m_hit_signature;
	ID3D12RootSignature* m_miss_signature;

	// Ray tracing pipeline state
	ID3D12StateObject* m_rt_state_object;
	// Ray tracing pipeline state properties, retaining the shader identifiers
	// to use in the Shader Binding Table
	ID3D12StateObjectProperties* m_rt_state_object_props;

	//ID3D12Resource* m_output_resource;
	DescriptorHeap* m_srv_uav_heap;
	//ID3D12DescriptorHeap* m_sampler_heap;

	nv_helpers_dx12::ShaderBindingTableGenerator m_sbt_helper;
	ID3D12Resource* m_sbt_storage/*[FRAME_BUFFER_COUNT]*/; // TODO: buffer this again? there's screen tearing going on atm
	qword m_ray_gen_table_size;
	qword m_ray_gen_entry_size;
	qword m_miss_table_size;
	qword m_miss_entry_size;
	qword m_hit_table_size;
	qword m_hit_entry_size;

	c_shader_input* m_ray_gen_input;

	// temp, horrid
	std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> m_texture_handles;

public:
	/// Create all acceleration structures, bottom and top
	bool create_acceleration_structures(c_scene* const scene);

	// Create the raytracing pipeline, associating the shader code to symbol names
	// and to their root signatures, and defining the amount of memory carried by rays (ray payload)
	bool create_raytracing_pipeline();

	// Create the buffer containing the raytracing result (always output in a UAV), and create
	// the heap referencing the resources used by the raytracing, such as the acceleration structure
	void update_shader_resource_heap(c_scene* const scene);

	// Create the shader binding table and indicating which shaders are invoked for each instance in the AS
	bool create_shader_binding_table(c_scene* const scene, bool update_only = false);
};