#include "render_target.h"
#include <reporting/report.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <render/api/directx12/descriptor_heap.h>
#include <render/api/directx12/helpers.h>
#include <render/api/directx12/shader_input.h>
#include <render/api/directx12/constant_buffer.h>
#include <render/shader.h>
#include <DirectXHelpers.h>

const wchar_t* const get_render_target_name(const e_render_targets target_type)
{
	constexpr const wchar_t* k_render_target_names[] =
	{
		L"Deferred RT",
		L"PBR RT",
		//L"Shading RT",
		L"Texture Camera RT",

        L"Raytace RT",

		L"Default Post RT",
		L"Blur Horizontal Post RT",
		L"Blur Vertical Post RT",
		L"Depth of Field Post RT"
	};
    static_assert(_countof(k_render_target_names) == k_render_target_count);

	if (!IN_RANGE_COUNT(target_type, _render_target_deferred, k_render_target_count))
	{
		LOG_WARNING(L"tried to get name for invalid target type [%d]!", target_type);
		return L"Invalid Render Target";
	}

    return k_render_target_names[target_type];
}

c_render_target::c_render_target(ID3D12Device5* const device, c_shader_input* const shader_input, const e_render_targets target_type)
    : m_target_type(target_type)
    , m_device(device)
    , m_render_target_view_heap(nullptr)
    , m_render_target_buffers()
    , m_depth_stencil_heap(nullptr)
    , m_depth_stencil_buffers()
    , m_shader_input(shader_input)
    , m_shader_texture_heap(nullptr)
{
    HRESULT hr = S_OK;
    const bool valid_arguments = device != nullptr && IN_RANGE_COUNT(target_type, 0, k_render_target_count);
    assert(valid_arguments);
    if (!valid_arguments)
    {
        LOG_ERROR(L"invalid args! aborting");
        return;
    }

    const dword buffered_target_count = FRAME_BUFFER_COUNT * m_shader_input->m_render_target_count;

    // Descriptor heaps - A descriptor is a block of data describing an object to the GPU, in a GPU specific format
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview
    D3D12_DESCRIPTOR_HEAP_DESC rtv_descriptor_heap = {};
    rtv_descriptor_heap.NumDescriptors = buffered_target_count; // number of descriptors for this heap
    // This heap is a render target view heap
    // This heap will not be directly referenced by the shaders (not shader visible), as this will store the output from the pipeline
    // otherwise we would set the heap's flag to D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
    rtv_descriptor_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_descriptor_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    m_render_target_view_heap = new c_descriptor_heap(m_device, L"Render Target View Heap", rtv_descriptor_heap);

    CD3DX12_HEAP_PROPERTIES heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    // Create a RTV for each buffer
    m_render_target_buffers = new ID3D12Resource*[buffered_target_count];
    for (dword frame_buffer_index = 0; frame_buffer_index < FRAME_BUFFER_COUNT; frame_buffer_index++)
    {
        for (dword render_target_index = 0; render_target_index < m_shader_input->m_render_target_count; render_target_index++)
        {
            const dword resource_index = render_target_index + (frame_buffer_index * m_shader_input->m_render_target_count);
            const D3D12_CLEAR_VALUE clear_value =
            {
                m_shader_input->get_render_target_format(render_target_index),
                { CLEAR_COLOUR.r, CLEAR_COLOUR.g, CLEAR_COLOUR.b, CLEAR_COLOUR.a }
            };
            D3D12_RESOURCE_DESC texture2d_desc = CD3DX12_RESOURCE_DESC::Tex2D
            (
                m_shader_input->get_render_target_format(render_target_index),
                RENDER_GLOBALS.render_bounds.width,
                RENDER_GLOBALS.render_bounds.height,
                1,
                1,
                1,
                0,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS // $TODO: only specify this for RT
            );
            // create render target buffers
            hr = m_device->CreateCommittedResource
            (
                &heap_properties,
                D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
                &texture2d_desc,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                &clear_value,
                IID_PPV_ARGS(&m_render_target_buffers[resource_index])
            );
            if (!HRESULT_VALID(device, hr))
            {
                LOG_ERROR(L"Render target buffer resource failed to create!");
                return;
            }
            m_render_target_buffers[resource_index]->SetName(get_render_target_name(target_type));

            dword view_heap_index = 0;
            if (m_render_target_view_heap->allocate(&view_heap_index) == K_SUCCESS)
            {
                m_device->CreateRenderTargetView(m_render_target_buffers[resource_index], nullptr, m_render_target_view_heap->get_cpu_handle(view_heap_index));
            }
        }
    }

    // Setup depth buffer if set to use
    if (m_shader_input->m_uses_depth_buffer)
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, buffered_target_count, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
        m_depth_stencil_heap = new c_descriptor_heap(m_device, L"Depth/Stencil Resource Heap", dsv_heap_desc);

        D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
        depth_stencil_desc.Format = DXGI_FORMAT_D32_FLOAT;
        depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depth_optimized_clear_value = {};
        depth_optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
        depth_optimized_clear_value.DepthStencil.Depth = 1.0f;
        depth_optimized_clear_value.DepthStencil.Stencil = 0;

        CD3DX12_HEAP_PROPERTIES tex_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC depth_tex_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D
        (
            DXGI_FORMAT_D32_FLOAT, RENDER_GLOBALS.render_bounds.width, RENDER_GLOBALS.render_bounds.height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        );


        for (dword frame_buffer_index = 0; frame_buffer_index < FRAME_BUFFER_COUNT; frame_buffer_index++)
        {
            hr = m_device->CreateCommittedResource(
                &tex_heap_properties,
                D3D12_HEAP_FLAG_NONE,
                &depth_tex_resource_desc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &depth_optimized_clear_value,
                IID_PPV_ARGS(&m_depth_stencil_buffers[frame_buffer_index])
            );
            if (!HRESULT_VALID(device, hr))
            {
                LOG_ERROR(L"depth/stencil buffer failed to create!");
                return;
            }
            m_depth_stencil_buffers[frame_buffer_index]->SetName(L"Depth/Stencil View");

            dword depth_heap_index = 0;
            if (m_depth_stencil_heap->allocate(&depth_heap_index) == K_SUCCESS)
            {
                m_device->CreateDepthStencilView(m_depth_stencil_buffers[frame_buffer_index], &depth_stencil_desc, m_depth_stencil_heap->get_cpu_handle(depth_heap_index));
            }
        }
    }

    // Create texture descriptor heap with shader resource views IN ORDER
    m_shader_texture_heap = new c_descriptor_heap
    (
        m_device,
        L"Render Target Shader Texture Heap",
        {
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, // type
            shader_input->m_texture_count * MAXIMUM_TEXTURE_SETS, // descriptor count
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, // flags
            0 // node mask
        }
    );
}

c_render_target::~c_render_target()
{
    // TODO: somehow clean up from original heap? need a descriptor heap handler class
    // There's probably one in DXTK I could use
    for (size_t i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        SAFE_RELEASE(m_render_target_buffers[i]);
        SAFE_RELEASE(m_depth_stencil_buffers[i]);
    }
    delete[] m_render_target_buffers;
    delete m_render_target_view_heap;
    delete m_depth_stencil_heap;
    delete m_shader_texture_heap;
}

void c_render_target::begin_render(ID3D12GraphicsCommandList* const command_list, const dword frame_index, const bool clear_buffers)
{
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = { NULL };
    if (m_shader_input->m_uses_depth_buffer && m_depth_stencil_heap != nullptr)
    {
        dsv_handle = m_depth_stencil_heap->get_cpu_handle(frame_index);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE* rtv_handles = new D3D12_CPU_DESCRIPTOR_HANDLE[m_shader_input->m_render_target_count]{};
    for (dword render_target_index = 0; render_target_index < m_shader_input->m_render_target_count; render_target_index++)
    {
        const dword resource_index = render_target_index + (frame_index * m_shader_input->m_render_target_count);
        rtv_handles[render_target_index] = m_render_target_view_heap->get_cpu_handle(resource_index);

        // Clear render buffer by filling it with CLEAR_COLOUR
        if (clear_buffers)
        {
            command_list->ClearRenderTargetView(rtv_handles[render_target_index], CLEAR_COLOUR.values, 0, nullptr);
        }
    }
    // set the render target for the output merger stage (the output of the pipeline)
    command_list->OMSetRenderTargets(m_shader_input->m_render_target_count, rtv_handles, TRUE, dsv_handle.ptr != NULL ? &dsv_handle : nullptr);
    delete[] rtv_handles;

    // clear the depth/stencil buffer
    if (clear_buffers && dsv_handle.ptr != NULL)
    {
        command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    command_list->SetGraphicsRootSignature(m_shader_input->get_root_signature());
}

void c_render_target::assign_texture(ID3D12Resource* const texture_resource, const e_texture_type texture_index, const dword set_index)
{
    const bool invalid_resource = texture_resource == nullptr;
    assert(!invalid_resource);
    if (invalid_resource)
    {
        LOG_ERROR(L"nullptr texture resource supplied!");
        return;
    }
    const dword texture_count = m_shader_input->m_texture_count;
    const bool texture_index_invalid = !IN_RANGE_COUNT(texture_index, 0, texture_count);
    assert(!texture_index_invalid);
    if (texture_index_invalid)
    {
        LOG_ERROR(L"out of bounds texture index [%d] supplied! max: [%d]", texture_index, texture_count);
        return;
    }
    const bool set_index_invalid = !IN_RANGE_COUNT(set_index, 0, MAXIMUM_TEXTURE_SETS);
    assert(!set_index_invalid);
    if (set_index_invalid)
    {
        LOG_ERROR(L"out of bounds texture set index [%d] supplied! max: [%d]", set_index, MAXIMUM_TEXTURE_SETS);
        return;
    }

    const dword handle_index = texture_index + (set_index * texture_count);

    const dword maximum_allocations = m_shader_texture_heap->get_maximum_allocations();
    const bool handle_index_invalid = !IN_RANGE_COUNT(handle_index, 0, maximum_allocations);
    assert(!handle_index_invalid);
    if (handle_index_invalid)
    {
        LOG_ERROR(L"out of bounds handle index [%d] supplied! max: [%d]", handle_index, maximum_allocations);
        return;
    }

    // Reinterpret depth format as R32
    if (texture_resource->GetDesc().Format == DXGI_FORMAT_D32_FLOAT)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC depth_srv = {};
        depth_srv.Format = DXGI_FORMAT_R32_FLOAT;
        depth_srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        depth_srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        depth_srv.Texture2D.MipLevels = 1;

        // Ditto below
        m_device->CreateShaderResourceView(texture_resource, &depth_srv, m_shader_texture_heap->get_cpu_handle(handle_index));
    }
    else
    {
        // TODO: is calling this several times a frame going to cause a memory leak?
        CreateShaderResourceView(m_device, texture_resource, m_shader_texture_heap->get_cpu_handle(handle_index));
    }
}

void c_render_target::begin_draw(ID3D12GraphicsCommandList* const command_list, const c_shader* const shader, const dword set_index)
{
    command_list->SetPipelineState((ID3D12PipelineState*)shader->get_resources()->pipeline_state);
    ID3D12DescriptorHeap* const shader_texture_heap = m_shader_texture_heap->get_heap();
    command_list->SetDescriptorHeaps(1, &shader_texture_heap); // Only one heap type can be set at a time

    const dword handle_offset = set_index * m_shader_input->m_texture_count;
    command_list->SetGraphicsRootDescriptorTable(m_shader_input->m_textures_root_index, m_shader_texture_heap->get_gpu_handle(handle_offset));
}

ID3D12Resource* const c_render_target::get_frame_resource(const dword target_index, const dword frame_index) const
{
    assert(IN_RANGE_COUNT(target_index, 0, m_shader_input->m_render_target_count));
    assert(IN_RANGE_COUNT(frame_index, 0, FRAME_BUFFER_COUNT));

    const dword resource_index = target_index + (frame_index * m_shader_input->m_render_target_count);
    return m_render_target_buffers[resource_index];
}

D3D12_GPU_DESCRIPTOR_HANDLE c_render_target::get_texture_handle(const dword texture_index, const dword set_index) const
{
    assert(IN_RANGE_COUNT(texture_index, 0, m_shader_input->m_texture_count));
    assert(IN_RANGE_COUNT(set_index, 0, MAXIMUM_TEXTURE_SETS));

    const dword resource_index = texture_index + (set_index * m_shader_input->m_render_target_count);
    return m_shader_texture_heap->get_gpu_handle(resource_index);
}
