#include "shader_input.h"
#include <d3dx12.h>
#include <reporting/report.h>
#include <render/api/directx12/helpers.h>
#include <render/api/directx12/constant_buffer.h>

c_shader_input::c_shader_input(ID3D12Device* const device, const dword textures_count,
    c_constant_buffer* const constant_buffers[], const dword constant_buffer_count,
    const D3D12_INPUT_ELEMENT_DESC input_desc[], const dword input_element_count,
    const D3D12_DESCRIPTOR_RANGE texture_ranges[], const dword texture_range_count,
    const DXGI_FORMAT render_target_formats[], const dword render_target_count,
    const bool use_depth_buffer, const D3D12_COMPARISON_FUNC depth_comparison_func)
    : m_constant_buffer_count(constant_buffer_count)
    , m_root_parameter_count(constant_buffer_count + 1) // constant buffers + texture table
    , m_textures_root_index(m_root_parameter_count - 1) // TODO: this indexing sucks, assumes the last parameter is the texture table
    , m_texture_count(textures_count)
    , m_render_target_count(render_target_count)
    , m_render_target_formats(new DXGI_FORMAT[m_render_target_count])
    , m_uses_depth_buffer(use_depth_buffer)
    , m_depth_comparison_func(use_depth_buffer ? depth_comparison_func : D3D12_COMPARISON_FUNC_NONE)
{
    const bool valid_constant_buffers = constant_buffer_count > 0 ? constant_buffers != nullptr : true; // can provide no cbuffers
    const bool valid_input_desc = input_element_count > 0 && input_desc != nullptr; // must supply at least 1 input desc
    const bool valid_texture_ranges = texture_range_count > 0 ? texture_ranges != nullptr : true; // can provide no textures
    const bool valid_render_targets = IN_RANGE_INCLUSIVE(render_target_count, 1, 8) && render_target_formats != nullptr; // must supply at least 1 render target to a max of 8 in dx12
    const bool valid_arguments = device != nullptr && valid_constant_buffers && valid_input_desc && valid_texture_ranges && valid_render_targets;
    assert(valid_arguments);
    if (!valid_arguments)
    {
        LOG_ERROR(L"invalid arguments!");
        return;
    }

    HRESULT hr = S_OK;

    m_constant_buffers = new c_constant_buffer*[m_constant_buffer_count];
    for (dword buffer_index = 0; buffer_index < m_constant_buffer_count; buffer_index++)
    {
        if (constant_buffers[buffer_index] != nullptr)
        {
            m_constant_buffers[buffer_index] = constant_buffers[buffer_index];
        }
        else
        {
            LOG_WARNING(L"Supplied constant buffer %d was nullptr!", buffer_index);
            m_constant_buffers[buffer_index] = nullptr;
        }
    }

    // Input layout
    m_input_layout = {};
    m_input_layout.NumElements = input_element_count;
    m_input_layout.pInputElementDescs = input_desc;

    // Root signature - Defines what types of resources are bound to the graphics pipeline
    // Resources eg. vertex/pixel shader, constant buffer
    D3D12_ROOT_PARAMETER* root_parameters = new D3D12_ROOT_PARAMETER[m_root_parameter_count]();

    // Setup constant buffer parameters
    for (dword buffer_index = 0; buffer_index < m_constant_buffer_count; buffer_index++)
    {
        // Root descriptors are descriptors inlined in the root arguments - made to contain descriptors that are accessed most often (like constant buffers updated each frame)
        // They have 1 level of indirection (one extra object is queried in order to access them) unlike root constants, making them slightly slower
        D3D12_ROOT_DESCRIPTOR descriptor;
        descriptor.RegisterSpace = 0;
        descriptor.ShaderRegister = buffer_index;

        // create a root parameter and fill it out
        root_parameters[buffer_index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // this is a constant buffer view root descriptor
        root_parameters[buffer_index].Descriptor = descriptor; // this is the root descriptor for this root parameter
        root_parameters[buffer_index].ShaderVisibility = m_constant_buffers[buffer_index]->get_visibility();
    }

    // create a descriptor table - this currently contains our single texture group
    D3D12_ROOT_DESCRIPTOR_TABLE descriptor_table;
    descriptor_table.NumDescriptorRanges = texture_range_count; // we only have one range
    descriptor_table.pDescriptorRanges = texture_ranges; // the pointer to the beginning of our ranges array

    // fill out the parameter for our descriptor table. Remember it's a good idea to sort parameters by frequency of change. Our constant
    // buffer will be changed multiple times per frame, while our descriptor table will not be changed at all (in this tutorial)
    root_parameters[m_textures_root_index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
    root_parameters[m_textures_root_index].DescriptorTable = descriptor_table; // this is our descriptor table for this root parameter
    root_parameters[m_textures_root_index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // our pixel shader will be the only shader accessing this parameter for now

    // create a static sampler
    CD3DX12_STATIC_SAMPLER_DESC samplers[1] =
    {
        (
            0, // register
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            0.0f, // mip LOD bias
            0, // max anisotropy
            D3D12_COMPARISON_FUNC_NEVER,
            D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
            0.0f, // min LOD
            D3D12_FLOAT32_MAX, // max LOD
            D3D12_SHADER_VISIBILITY_PIXEL,
            0 // register space
        )
    };

    CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init
    (
        m_root_parameter_count,
        root_parameters, // a pointer to the beginning of our root parameters array
        _countof(samplers),
        samplers, // a pointer to our static samplers (array)
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // we can deny shader stages here for better performance
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
    );
    ID3DBlob* signature;
    ID3DBlob* error; // a buffer holding the error data if any
    hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (hr == S_OK)
    {
        hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_root_signature));
        if (!HRESULT_VALID(hr))
        {
            m_root_signature = nullptr;
        }
    }
    else
    {
        if (error != nullptr)
        {
            LOG_ERROR(L"%hs", (char*)error->GetBufferPointer());
        }
        HRESULT_VALID(hr);
        return;
    }
    delete[] root_parameters;

    for (dword i = 0; i < m_render_target_count; i++)
    {
        m_render_target_formats[i] = render_target_formats[i];
    }
}

c_shader_input::~c_shader_input()
{
    for (dword buffer_index = 0; buffer_index < m_constant_buffer_count; buffer_index++)
    {
        if (m_constant_buffers[buffer_index] != nullptr)
        {
            delete m_constant_buffers[buffer_index];
        }
    }
    delete[] m_constant_buffers;
    SAFE_RELEASE(m_root_signature);
    delete[] m_render_target_formats;
}