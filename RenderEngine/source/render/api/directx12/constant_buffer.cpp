#include "constant_buffer.h"
#include <d3dx12.h>
#include <BufferHelpers.h>
#include <render/api/directx12/helpers.h>
#include <reporting/report.h>

// debug only function
const wchar_t* const get_constant_buffer_name(const e_render_pass render_pass, const e_constant_buffers buffer_type)
{
    if (!IN_RANGE_COUNT(render_pass, 0, k_render_pass_count))
    {
        LOG_WARNING(L"tried to get name with invalid render pass [%d]!", render_pass);
        return L"Unknown Constant Buffer";
    }

    switch (render_pass)
    {
        case _render_pass_deferred:
        {
            if (!IN_RANGE_COUNT(buffer_type, 0, k_deferred_constant_buffer_count)) break;

            constexpr const wchar_t* k_default_buffer_names[k_deferred_constant_buffer_count] =
            {
                L"Object Constant Buffer",
                L"Material Properties Constant Buffer"
            };
            static_assert(_countof(k_default_buffer_names) == k_deferred_constant_buffer_count);
            return k_default_buffer_names[buffer_type];
        }
        case _render_pass_pbr:
        {
            if (!IN_RANGE_COUNT(buffer_type, 0, k_lighting_constant_buffer_count)) break;

            constexpr const wchar_t* k_lighting_buffer_names[] =
            {
                L"Light Properties Constant Buffer"
            };
            static_assert(_countof(k_lighting_buffer_names) == k_lighting_constant_buffer_count);
            return k_lighting_buffer_names[buffer_type];
        }
        case _render_pass_texcam:
        {
            if (!IN_RANGE_COUNT(buffer_type, 0, k_texcam_constant_buffer_count)) break;

            //constexpr const wchar_t* k_texcam_buffer_names[] =
            //{
            //};
            //static_assert(_countof(k_texcam_buffer_names) == k_texcam_constant_buffer_count);
            //return k_texcam_buffer_names[buffer_type];
            break;
        }
        case _render_pass_post_processing:
        {
            if (!IN_RANGE_COUNT(buffer_type, 0, k_post_constant_buffer_count)) break;

            constexpr const wchar_t* k_post_buffer_names[] =
            {
                L"Post Parameters Constant Buffer"
            };
            static_assert(_countof(k_post_buffer_names) == k_post_constant_buffer_count);
            return k_post_buffer_names[buffer_type];
        }

        case _render_pass_raytrace:
        {
            if (!IN_RANGE_COUNT(buffer_type, 0, 1)) break;

            constexpr const wchar_t* k_rt_buffer_names[] =
            {
                L"RT Constant Buffer"
            };
            static_assert(_countof(k_rt_buffer_names) == 1);
            return k_rt_buffer_names[buffer_type];
        }
    }

    LOG_WARNING(L"tried to get name for invalid buffer type [%d] for render pass [%d]!", buffer_type, render_pass);
    return L"Unknown Constant Buffer";
}

c_constant_buffer::c_constant_buffer(ID3D12Device5* const device, const e_render_pass render_pass,
    const e_constant_buffers buffer_type, const dword buffer_struct_size, const D3D12_SHADER_VISIBILITY visibility)
    : m_buffer_struct_size(buffer_struct_size)
    , m_buffer_aligned_size((buffer_struct_size + 255) & ~255)
    , m_visibility(visibility)
{
    // ensure size doesn't exceed alignment (this should never happen)
    const bool buffer_aligned = m_buffer_struct_size <= m_buffer_aligned_size;
    assert(buffer_aligned);
    if (!buffer_aligned)
    {
        LOG_ERROR(L"constant buffer was not aligned! [size: %d] [aligned: %d]", m_buffer_struct_size, m_buffer_aligned_size);
        return;
    }

    // create the constant buffer resource heap
    // We will update the constant buffer one or more times per frame, so we will use only an upload heap
    // unlike previously we used an upload heap to upload the vertex and index data, and then copied over
    // to a default heap. If you plan to use a resource for more than a couple frames, it is usually more
    // efficient to copy to a default heap where it stays on the gpu. In this case, our constant buffer
    // will be modified and uploaded at least once per frame, so we only use an upload heap

    // first we will create a resource heap (upload heap) for each frame for the cubes constant buffers
    // As you can see, we are allocating 64KB for each resource we create. Buffer resource heaps must be
    // an alignment of 64KB. We are creating 3 resources, one for each frame. Each constant buffer is 
    // only a 4x4 matrix of floats in this tutorial. So with a float being 4 bytes, we have 
    // 16 floats in one constant buffer, and we will store 2 constant buffers in each
    // heap, one for each cube, thats only 64x2 bits, or 128 bits we are using for each
    // resource, and each resource must be at least 64KB (65536 bits)

    HRESULT hr = S_OK;
    CD3DX12_RANGE read_range(0, 0);
    for (dword frame_index = 0; frame_index < FRAME_BUFFER_COUNT; frame_index++)
    {
        // 64KiB constant buffer - Must be a multiple of 64KiB for single-textures and constant buffers
        hr = CreateUploadBuffer(device, nullptr, 64, 1024, &m_upload_buffers[frame_index]);
        if (!HRESULT_VALID(device, hr))
        {
            LOG_WARNING(L"upload buffer creation failed! setting nullptr");
            m_upload_buffers[frame_index] = nullptr;
        }
        hr = m_upload_buffers[frame_index]->Map(0, &read_range, reinterpret_cast<void**>(&m_gpu_address[frame_index]));
        if (!HRESULT_VALID(device, hr))
        {
            m_gpu_address[frame_index] = nullptr;
        }
        m_upload_buffers[frame_index]->SetName(get_constant_buffer_name(render_pass, buffer_type));
    }
}

c_constant_buffer::~c_constant_buffer()
{
    for (dword frame_index = 0; frame_index < FRAME_BUFFER_COUNT; frame_index++)
    {
        SAFE_RELEASE(m_upload_buffers[frame_index]);
    };
}

void c_constant_buffer::set_data(const void* const buffer, const dword frame_index, const dword sub_index)
{
    const bool gpu_address_invalid = m_gpu_address == nullptr;
    assert(!gpu_address_invalid);
    if (gpu_address_invalid)
    {
        LOG_WARNING(L"failed to set constant buffer data! gpu address was invalid!");
        return;
    }

    const bool invalid_arguments = buffer == nullptr || !IN_RANGE_COUNT(frame_index, 0, FRAME_BUFFER_COUNT);
    assert(!invalid_arguments);
    if (invalid_arguments)
    {
        LOG_WARNING(L"failed to set constant buffer data! arguments were invalid");
        return;
    }

    // copy our ConstantBuffer instance to the mapped constant buffer resource
    // Because of the constant read alignment requirements, constant buffer views must be 256 bit aligned. Our buffers are smaller than 256 bits,
    // so we need to add spacing between the two buffers, so that the second buffer starts at 256 bits from the beginning of the resource heap.
    memcpy(m_gpu_address[frame_index] + (m_buffer_aligned_size * sub_index), buffer, m_buffer_struct_size);
}

D3D12_GPU_VIRTUAL_ADDRESS c_constant_buffer::get_gpu_address(const dword frame_index, const dword buffer_index)
{
    // TODO: validate buffer index - store max value in class?
    const bool invalid_frame_index = !IN_RANGE_COUNT(frame_index, 0, FRAME_BUFFER_COUNT);
    assert(!invalid_frame_index);
    if (invalid_frame_index)
    {
        LOG_WARNING(L"invalid frame index!");
        return NULL;
    }
    ID3D12Resource* upload_buffer = m_upload_buffers[frame_index];
    const bool invalid_upload_buffer = upload_buffer == nullptr;
    assert(!invalid_upload_buffer);
    if (invalid_upload_buffer)
    {
        LOG_WARNING(L"invalid upload buffer!");
        return NULL;
    }

    D3D12_GPU_VIRTUAL_ADDRESS gpu_address = upload_buffer->GetGPUVirtualAddress() + (m_buffer_aligned_size * buffer_index);
    return gpu_address;
}
