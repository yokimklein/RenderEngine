#include "renderer.h"
#include <types.h>
#include <reporting/report.h>
#include <iostream>
#include <fstream>
#include <cassert>
#include <render/api/directx12/helpers.h>
#if _DEBUG
#include <D3d12SDKLayers.h>
#endif
#include <D3Dcompiler.h>
#include <DDSTextureLoader.h>
#include <DirectXHelpers.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>
#include <ResourceUploadBatch.h>
//#include <DirectXMesh.h>
#include <DirectXMeshTangentFrame.cpp> // TODO: this is stupid, but for some reason including DirectXMesh.h isn't finding the function header
#include <BufferHelpers.h>
#include <render/texture.h>
#include <render/model.h>
#include <render/shader.h>
#include <scene/scene.h>
#include <render/imgui_overlay.h>
#include <ImGuizmo.h>
#include <nvapi.h>
#include <comdef.h>

// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp
// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/DXSample.cpp

// Initialise DirectX Graphics Infrastructure Factory
// Provides methods for generating DXGI objects
bool c_renderer_dx12::initialise_factory()
{
    HRESULT hr = S_OK;
    dword dxgi_factory_flags = 0;

#ifdef _DEBUG
    // Enable DirectX debug layer on debug enabled builds
    hr = D3D12GetDebugInterface(IID_PPV_ARGS(&m_dx12_debug));
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    m_dx12_debug->EnableDebugLayer();
    m_dx12_debug->SetEnableGPUBasedValidation(TRUE);
    m_dx12_debug->SetEnableSynchronizedCommandQueueValidation(TRUE);
    
    dxgi_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_factory));
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }

    return K_SUCCESS;
}

#if defined(ENABLE_NVAPI_RAYTRACING_VALIDATION)
// Validation callback
static void __stdcall validation_message_callback(void* user_data, NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY severity, const char* message_code, const char* message, const char* message_details)
{
    switch (severity)
    {
        case NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY_ERROR:
            LOG_ERROR("Ray Tracing Validation Error: [%hs] %hs\n%hs", message_code, message, message_details);
            break;
        case NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY_WARNING:
            LOG_WARNING("Ray Tracing Validation Warning: [%hs] %hs\n%hs", message_code, message, message_details);
            break;
    }
}
#endif

// Get the first available highest performance hardware adapter that supports DX12
bool c_renderer_dx12::initialise_device_adapter()
{
    // TODO?: Enable WARP/use software adapter if no hardware adapter is found on the system
    // Windows Advanced Rasterization Platform - Can run as a graphics device on the CPU
    // Used in testing as it can emulate other GPU devices

    bool adapter_found = false;
    for (dword i = 0; SUCCEEDED(m_factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter))); i++)
    {
        DXGI_ADAPTER_DESC3 adapter_desc;
        m_adapter->GetDesc3(&adapter_desc);

        // Skip software adapters, we want real hardware
        if (adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }
        // Break out if we find a DX12 supported device
        if (SUCCEEDED(D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
        {
            LOG_MESSAGE(L"Adapter found: %ws", adapter_desc.Description);
            adapter_found = true;
            break;
        }
    }

    assert(adapter_found);
    if (!adapter_found)
    {
        LOG_ERROR(L"No suitible adapter found!");
        return K_FAILURE;
    }

    // Create device from found adapter
    HRESULT hr = D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device));
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }

    const bool adapter_valid = m_adapter != nullptr;
    assert(adapter_valid);
    if (!adapter_valid)
    {
        LOG_ERROR(L"Adapter was nullptr!");
        return K_FAILURE;
    }

#if defined(ENABLE_NVAPI_RAYTRACING_VALIDATION)
    if (NvAPI_Initialize() != NVAPI_OK)
    {
        LOG_ERROR("NvAPI failed to initialise!");
        return K_FAILURE;
    }
    if (NvAPI_D3D12_EnableRaytracingValidation(m_device, NVAPI_D3D12_RAYTRACING_VALIDATION_FLAG_NONE) != NVAPI_OK)
    {
        LOG_ERROR("NvAPI failed to enabled RT validation!");
        return K_FAILURE;
    }
    void* nvapiValidationCallbackHandle = nullptr;
    if (NvAPI_D3D12_RegisterRaytracingValidationMessageCallback(m_device, &validation_message_callback, nullptr, &nvapiValidationCallbackHandle) != NVAPI_OK)
    {
        LOG_ERROR("NvAPI failed to register RT validation callback message!");
        return K_FAILURE;
    }
#endif

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    hr = m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
    {
        LOG_ERROR("Raytracing not supported by this device!");
        return K_FAILURE;
    }
    else
    {
        LOG_MESSAGE("Device supports raytracing tier %d", options5.RaytracingTier);
    }
#if defined(_DEBUG)
    hr = D3D12GetDebugInterface(IID_PPV_ARGS(&m_dred_settings));
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    m_dred_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    m_dred_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    m_dred_settings->SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
#endif

    return K_SUCCESS;
}

bool c_renderer_dx12::initialise_command_queue()
{
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    HRESULT hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_command_queue));
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }

    return K_SUCCESS;
}

// TODO: reinitialise swapchain on window resize
bool c_renderer_dx12::initialise_swapchain(const HWND hWnd)
{
    HRESULT hr = S_OK;

    DXGI_MODE_DESC back_buffer_desc = {}; // this is to describe our display mode
    back_buffer_desc.Width = RENDER_GLOBALS.render_bounds.width; // buffer width
    back_buffer_desc.Height = RENDER_GLOBALS.render_bounds.height; // buffer height
    back_buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the buffer (rgba 32 bits, 8 bits for each chanel)

    DXGI_SAMPLE_DESC sample_desc = {};
    sample_desc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)

    DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
    swapchain_desc.BufferCount = FRAME_BUFFER_COUNT; // number of frame buffers, swapped between each frame (double/triple buffering)
    swapchain_desc.BufferDesc = back_buffer_desc; // our back buffer description
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    swapchain_desc.OutputWindow = hWnd; // handle to our window
    swapchain_desc.SampleDesc = sample_desc; // our multi-sampling description
    swapchain_desc.Windowed = true; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps

#ifdef PLATFORM_WINDOWS
    // For windows
    
    // $TODO: Microsoft recommends against using factory->CreateSwapChain, try use CreateSwapChainForHwnd instead?
    // https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd
    IDXGISwapChain* temp_swapchain;
    m_factory->CreateSwapChain(m_command_queue, &swapchain_desc, &temp_swapchain);
    m_swapchain = static_cast<IDXGISwapChain3*>(temp_swapchain);

#else
#error SWAPCHAIN UNDEFINED FOR CURRENT PLATFORM
#endif

    m_frame_index = m_swapchain->GetCurrentBackBufferIndex();

    for (dword i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backbuffers[i]));
        if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
        m_backbuffers[i]->SetName(L"Swapchain backbuffer");
    }

    return K_SUCCESS;
}

// Render Target View (RTV) descriptor heap
bool c_renderer_dx12::initialise_render_target_view()
{   
    HRESULT hr = S_OK;

    m_render_targets[_render_target_deferred] = new c_render_target(m_device, m_shader_inputs[_input_deferred], _render_target_deferred);
    m_render_targets[_render_target_pbr] = new c_render_target(m_device, m_shader_inputs[_input_pbr], _render_target_pbr);
    //m_render_targets[_render_target_shading] = new c_render_target(m_device, m_shader_inputs[_input_shading], _render_target_shading);
    m_render_targets[_render_target_texcams] = new c_render_target(m_device, m_shader_inputs[_input_texcam], _render_target_texcams);

    // $TODO:
    m_render_targets[_render_target_raytrace] = new c_render_target(m_device, m_shader_inputs[_input_deferred], _render_target_deferred);

    for (dword i = k_default_render_target_count; i <= k_render_target_post_reserved; i++)
    {
        m_render_targets[i] = new c_render_target(m_device, m_shader_inputs[_input_post_processing], (e_render_targets)i);
    }

    return K_SUCCESS;
}

bool c_renderer_dx12::initialise_command_allocators()
{
    for (dword i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        HRESULT hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocators[i]));
        if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    }
    return K_SUCCESS;
}

bool c_renderer_dx12::initialise_command_list()
{
    HRESULT hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_command_allocators[m_frame_index], NULL, IID_PPV_ARGS(&m_command_list));
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }

    //hr = m_command_list->Close();
    //if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    //
    //hr = m_command_list->Reset(m_command_allocators[m_frame_index], NULL);
    //if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }

    return K_SUCCESS;
}

bool c_renderer_dx12::initialise_fences()
{
    // Create fence and wait until assets have been uploaded to the GPU
    // Fence - The GPU object we notify when to do work, and the object we wait for the work to be done    
    for (dword i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fences[i]));
        if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
        m_fence_values[i] = 0; // initial fence value
    }

    // create an event handle to use for frame synchronisation
    m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    const bool fence_valid = m_fence_event != nullptr;
    assert(fence_valid);
    if (!fence_valid)
    {
        // just to log a potential error
        HRESULT_VALID(m_device, HRESULT_FROM_WIN32(GetLastError()));
        // return failure anyway, event is already nullptr
        return K_FAILURE;
    }

    return K_SUCCESS;
}

bool c_renderer_dx12::initialise_input_layouts()
{
    HRESULT hr = S_OK;

    // Vertex input layout
    // The input layout is used by the Input Assembler so that it knows how to read the vertex data bound to it.
    // Ensure 16-byte alignment
    constexpr D3D12_INPUT_ELEMENT_DESC full_vertex_input_elements[5] =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",    0, DXGI_FORMAT_R32G32B32_FLOAT,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BINORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    constexpr D3D12_INPUT_ELEMENT_DESC simple_vertex_input_elements[2] =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // DEFERRED SHADER INPUTS
    // Constant buffers - these pointers are the responsibility of c_shader_input to cleanup
    c_constant_buffer* constant_buffers_default[k_deferred_constant_buffer_count] =
    {
        new c_constant_buffer(m_device, _render_pass_deferred, _deferred_constant_buffer_object, sizeof(s_object_cb), D3D12_SHADER_VISIBILITY_VERTEX),
        new c_constant_buffer(m_device, _render_pass_deferred, _deferred_constant_buffer_materials, sizeof(s_material_properties_cb), D3D12_SHADER_VISIBILITY_PIXEL)
    };
    static_assert(_countof(constant_buffers_default) == k_deferred_constant_buffer_count);
    CD3DX12_DESCRIPTOR_RANGE default_texture_range[] = { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, k_default_textures_count, 0 } };
    DXGI_FORMAT deferred_render_target_formats[] =
    {
        // UNORM - Unsigned normalised, will be between 0.0f-1.0f
        DXGI_FORMAT_R8G8B8A8_UNORM, // albedo
        DXGI_FORMAT_R8_UNORM, // roughness
        DXGI_FORMAT_R8_UNORM, // metallic
        DXGI_FORMAT_R16G16B16A16_FLOAT, // normal
        DXGI_FORMAT_R32G32B32A32_FLOAT,  // position
    };
    static_assert(_countof(deferred_render_target_formats) == k_gbuffer_count);
    m_shader_inputs[_input_deferred] = new c_shader_input
    (
        m_device, k_default_textures_count,
        constant_buffers_default, _countof(constant_buffers_default),
        full_vertex_input_elements, _countof(full_vertex_input_elements),
        default_texture_range, _countof(default_texture_range),
        deferred_render_target_formats, _countof(deferred_render_target_formats),
        true, D3D12_COMPARISON_FUNC_LESS
    );

    // PBR SHADER INPUTS
    c_constant_buffer* constant_buffers_lighting[k_lighting_constant_buffer_count] =
    {
        new c_constant_buffer(m_device, _render_pass_pbr, _lighting_constant_buffer_lights, sizeof(s_light_properties_cb), D3D12_SHADER_VISIBILITY_PIXEL)
    };
    static_assert(_countof(constant_buffers_lighting) == k_lighting_constant_buffer_count);
    CD3DX12_DESCRIPTOR_RANGE lighting_texture_range[] = { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, k_lighting_textures_count, 0 } };
    DXGI_FORMAT lighting_render_target_formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM };
    m_shader_inputs[_input_pbr] = new c_shader_input
    (
        m_device, k_lighting_textures_count,
        constant_buffers_lighting, _countof(constant_buffers_lighting),
        simple_vertex_input_elements, _countof(simple_vertex_input_elements),
        lighting_texture_range, _countof(lighting_texture_range),
        lighting_render_target_formats, _countof(lighting_render_target_formats),
        false, D3D12_COMPARISON_FUNC_NONE
    );

    // SHADING SHADER INPUTS
    //CD3DX12_DESCRIPTOR_RANGE shading_texture_range[] = { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, k_shading_textures_count, 0 } };
    //DXGI_FORMAT shading_render_target_formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM };
    //m_shader_inputs[_input_shading] = new c_shader_input
    //(
    //    m_device, k_shading_textures_count,
    //    nullptr, 0,
    //    simple_vertex_input_elements, _countof(simple_vertex_input_elements),
    //    shading_texture_range, _countof(shading_texture_range),
    //    shading_render_target_formats, _countof(shading_render_target_formats),
    //    false, D3D12_COMPARISON_FUNC_NONE
    //);

    // TEXCAM SHADER INPUTS
    // this is a range of descriptors inside a descriptor heap, allows use of resources from multiple heaps
    c_constant_buffer* constant_buffers_texcam[] =
    {
        constant_buffers_default[_deferred_constant_buffer_object] // We can just share the object buffers between passes
        //new c_constant_buffer(m_device, _render_pass_texcam, _texcam_constant_buffer_object, sizeof(s_object_cb), D3D12_SHADER_VISIBILITY_VERTEX)
    };
    static_assert(_countof(constant_buffers_texcam) == k_texcam_constant_buffer_count);
    CD3DX12_DESCRIPTOR_RANGE texcam_texture_range[] = { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, k_texcam_textures_count, 0 } };
    DXGI_FORMAT texcam_render_target_formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM };
    m_shader_inputs[_input_texcam] = new c_shader_input
    (
        m_device, k_texcam_textures_count,
        constant_buffers_texcam, _countof(constant_buffers_texcam),
        full_vertex_input_elements, _countof(full_vertex_input_elements),
        texcam_texture_range, _countof(texcam_texture_range),
        texcam_render_target_formats, _countof(texcam_render_target_formats),
        true, D3D12_COMPARISON_FUNC_LESS_EQUAL // Less equal allows the texcam objects to be redrawn where they are without drawing back over closer geometry
    );
    
    // POST PROCESSING SHADER INPUTS
    c_constant_buffer* constant_buffers_post[] =
    {
        new c_constant_buffer(m_device, _render_pass_post_processing, _post_constant_buffer, sizeof(s_post_parameters_cb), D3D12_SHADER_VISIBILITY_PIXEL)
    };
    static_assert(_countof(constant_buffers_post) == k_post_constant_buffer_count);
    CD3DX12_DESCRIPTOR_RANGE post_texture_range[] = { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, k_post_textures_count, 0 } };
    DXGI_FORMAT post_render_target_formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM };
    m_shader_inputs[_input_post_processing] = new c_shader_input
    (
        m_device, k_post_textures_count,
        constant_buffers_post, _countof(constant_buffers_post),
        simple_vertex_input_elements, _countof(simple_vertex_input_elements),
        post_texture_range, _countof(post_texture_range),
        post_render_target_formats, _countof(post_render_target_formats),
        false, D3D12_COMPARISON_FUNC_NONE
    );

    m_deferred_shader = new c_shader(this, L"assets\\shaders\\default_vs.hlsl", "vs_main", L"assets\\shaders\\deferred.hlsl", "ps_deferred", _input_deferred);
    m_pbr_shader = new c_shader(this, L"assets\\shaders\\screen_quad.hlsl", "vs_screen_quad", L"assets\\shaders\\pbr_raster.hlsl", "ps_deferred_pbr", _input_pbr);
    //m_shading_shader = new c_shader(this, L"assets\\shaders\\screen_quad.hlsl", "vs_screen_quad", L"assets\\shaders\\shading.hlsl", "ps_deferred_shading", _input_shading);
    m_texcam_shader = new c_shader(this, L"assets\\shaders\\default_vs.hlsl", "vs_main", L"assets\\shaders\\texcam.hlsl", "ps_sample_texture", _input_texcam);
    
    m_post_shaders[_post_processing_default] = new c_shader(this, L"assets\\shaders\\screen_quad.hlsl", "vs_screen_quad", L"assets\\shaders\\post_processing.hlsl", "ps_tex_to_screen", _input_post_processing);
    m_post_shaders[_post_processing_blur_horizontal] = new c_shader(this, L"assets\\shaders\\screen_quad.hlsl", "vs_screen_quad", L"assets\\shaders\\gaussian_blur.hlsl", "ps_gaussian_blur_horiz", _input_post_processing);
    m_post_shaders[_post_processing_blur_vertical] = new c_shader(this, L"assets\\shaders\\screen_quad.hlsl", "vs_screen_quad", L"assets\\shaders\\gaussian_blur.hlsl", "ps_gaussian_blur_vert", _input_post_processing);
    m_post_shaders[_post_processing_depth_of_field] = new c_shader(this, L"assets\\shaders\\screen_quad.hlsl", "vs_screen_quad", L"assets\\shaders\\depth_of_field.hlsl", "ps_depth_of_field", _input_post_processing);

    // TODO: ensure input creation was successful before returning success

    return K_SUCCESS;
}

bool c_renderer_dx12::create_shader(const wchar_t* vs_path, const char* vs_name, const wchar_t* ps_path, const char* ps_name, const e_shader_input input_type, s_shader_resources* out_resources)
{
    HRESULT hr = S_OK;

    assert(vs_path != nullptr && vs_name != nullptr && ps_path != nullptr && ps_name != nullptr && out_resources != nullptr);
    if (vs_path == nullptr || vs_name == nullptr || ps_path == nullptr || ps_name == nullptr || out_resources == nullptr)
    {
        LOG_WARNING(L"invalid arguments in call! aborting");
        *out_resources = {};
        return K_FAILURE;
    }

    // create vertex and pixel shaders
    // 
    // when debugging, we can compile the shader files at runtime.
    // but for release versions, we can compile the hlsl shaders
    // with fxc.exe to create .cso files, which contain the shader
    // bytecode. We can load the .cso files at runtime to get the
    // shader bytecode, which of course is faster than compiling
    // them at runtime
#ifdef _DEBUG
    // Enable better shader debugging with the graphics debugging tools.
    constexpr dword compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    constexpr dword compile_flags = 0;
#endif
    ID3DBlob* vertex_shader = nullptr;
    ID3DBlob* pixel_shader = nullptr;
    ID3D12PipelineState* pipeline_state = nullptr;
    ID3DBlob* error = nullptr;
    hr = D3DCompileFromFile(vs_path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, vs_name, "vs_5_0", compile_flags, 0, &vertex_shader, &error);
    if (hr != S_OK)
    {
        if (error != nullptr)
        {
            LOG_ERROR(L"%hs", (char*)error->GetBufferPointer());
        }
        HRESULT_VALID(m_device, hr);
        return K_FAILURE;
    }
    // fill out a shader bytecode structure, which is basically just a pointer
    // to the shader bytecode and the size of the shader bytecode
    D3D12_SHADER_BYTECODE vs_bytecode = {};
    vs_bytecode.BytecodeLength = vertex_shader->GetBufferSize();
    vs_bytecode.pShaderBytecode = vertex_shader->GetBufferPointer();

    hr = D3DCompileFromFile(ps_path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, ps_name, "ps_5_0", compile_flags, 0, &pixel_shader, &error);
    if (hr != S_OK)
    {
        if (error != nullptr)
        {
            LOG_ERROR(L"%hs", (char*)error->GetBufferPointer());
        }
        HRESULT_VALID(m_device, hr);
        return K_FAILURE;
    }
    // fill out shader bytecode structure for pixel shader
    D3D12_SHADER_BYTECODE ps_bytecode = {};
    ps_bytecode.BytecodeLength = pixel_shader->GetBufferSize();
    ps_bytecode.pShaderBytecode = pixel_shader->GetBufferPointer();

    // Graphics Pipeline State Object (PSO)
    // In a real application, you will have many pso's. for each different shader
    // or different combinations of shaders, different blend states or different rasterizer states,
    // different topology types (point, line, triangle, patch), or a different number
    // of render targets you will need a pso
    
    // VS is the only required shader for a pso. You might be wondering when a case would be where
    // you only set the VS. It's possible that you have a pso that only outputs data with the stream
    // output, and not on a render target, which means you would not need anything after the stream
    // output.

    DXGI_SAMPLE_DESC sample_desc = {};
    sample_desc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)

    c_shader_input* shader_input = m_shader_inputs[input_type];

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {}; // a structure to define a pso
    pso_desc.InputLayout = shader_input->get_input_layout(); // the structure describing our input layout
    pso_desc.pRootSignature = shader_input->get_root_signature(); // the root signature that describes the input data this pso needs
    pso_desc.VS = vs_bytecode; // structure describing where to find the vertex shader bytecode and how large it is
    pso_desc.PS = ps_bytecode; // same as VS but for pixel shader
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing
    for (dword i = 0; i < shader_input->m_render_target_count; i++)
    {
        pso_desc.RTVFormats[i] = shader_input->get_render_target_format(i); // format of the render target
    }
    pso_desc.SampleDesc = sample_desc; // must be the same sample description as the swapchain and depth/stencil buffer
    pso_desc.SampleMask = UINT_MAX; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; // Cull front instead of back for right-handed coordinate models
    pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state
    pso_desc.NumRenderTargets = shader_input->m_render_target_count;
    pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
    if (shader_input->m_uses_depth_buffer)
    {
        pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pso_desc.DepthStencilState.DepthEnable = TRUE;
        pso_desc.DepthStencilState.DepthFunc = shader_input->m_depth_comparison_func;
    }
    else
    {
        pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        pso_desc.DepthStencilState.DepthEnable = FALSE;
        pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_NONE;
    }

    // create the pso
    hr = m_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state));
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }

    out_resources->vertex_shader = vertex_shader;
    out_resources->pixel_shader = pixel_shader;
    out_resources->pipeline_state = pipeline_state;

    return K_SUCCESS;
}

qword c_renderer_dx12::get_gbuffer_textureid(e_gbuffers gbuffer_type) const
{
    return m_gbuffer_gpu_handles[gbuffer_type].ptr;
}

bool c_renderer_dx12::upload_geometry(const dword vertex_size, const void* const vertices, const dword vertices_size, const dword indices[], const dword indices_size, s_geometry_resources* const out_resources)
{
    const bool vertex_upload_result = this->upload_vertex_buffer(vertex_size, vertices, vertices_size, out_resources);
    const bool index_upload_result = this->upload_index_buffer(indices, indices_size, out_resources);

    const bool upload_successful = vertex_upload_result && index_upload_result;
    assert(upload_successful);

    return upload_successful;
}

bool c_renderer_dx12::create_geometry(vertex vertices[], dword vertices_size, dword indices[], dword indices_size, s_geometry_resources* const out_resources)
{
    HRESULT hr = S_OK;
    const dword face_count = indices_size / sizeof(dword) / 3; // IMPORTANT: ASSUMES ALL MODELS ARE TRIANGULATED TO GET ACCURATE FACE COUNT
    const dword vertex_count = vertices_size / sizeof(vertex);

    // Retrieve positions, normals and texcoords and copy into sequential arrays for ComputeTangentFrame
    vector3d* positions = new vector3d[vertex_count];
    for (dword i = 0; i < vertex_count; i++)
    {
        positions[i] = vertices[i].vertex.position;
    }

    vector3d* normals = new vector3d[vertex_count];
    for (dword i = 0; i < vertex_count; i++)
    {
        normals[i] = vertices[i].vertex.normal;
    }

    point2d* texcoords = new point2d[vertex_count];
    for (dword i = 0; i < vertex_count; i++)
    {
        texcoords[i] = vertices[i].vertex.tex_coord;
    }

    vector3d* tangents = new vector3d[vertex_count];
    vector3d* binormals = new vector3d[vertex_count];
    hr = ComputeTangentFrame((uint32_t*)indices, face_count, (XMFLOAT3*)positions, (XMFLOAT3*)normals, (XMFLOAT2*)texcoords, vertex_count, (XMFLOAT3*)tangents, (XMFLOAT3*)binormals);
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    delete[] positions;
    delete[] normals;
    delete[] texcoords;
    for (dword i = 0; i < vertex_count; i++)
    {
        vertices[i].tangent = tangents[i];
        vertices[i].bitangent = binormals[i];
    }
    delete[] tangents;
    delete[] binormals;

    this->upload_geometry(sizeof(vertex), vertices, vertices_size, indices, indices_size, out_resources);
    out_resources->vertex_count = vertex_count;

    return true;
}

bool c_renderer_dx12::create_simple_geometry(simple_vertex vertices[], dword vertices_size, s_geometry_resources* const out_resources)
{
    const bool result = this->upload_vertex_buffer(sizeof(simple_vertex), vertices, vertices_size, out_resources);

    return result;
}

// Loads right-handed models (engine is left handed)
bool c_renderer_dx12::load_model(const wchar_t* const file_path, s_geometry_resources* const out_resources)
{
    assert(out_resources != nullptr);

    std::ifstream vbo_file(file_path, std::ifstream::in | std::ifstream::binary);
    if (vbo_file.fail())
    {
        LOG_WARNING(L"failed to load file %s!", file_path);
        
        // Initialise data to default, nullptrs and 0 counts so we can continue beyond this failure
        s_geometry_resources empty_resources = {};
        *out_resources = empty_resources;
        return false;
    }

    dword vertex_count = 0;
    dword index_count = 0;

    vbo_file.read(reinterpret_cast<char*>(&vertex_count), sizeof(dword));
    if (vertex_count == 0)
    {
        LOG_WARNING(L"vertex count for VBO_I32 %s was 0! stopping load", file_path);
        s_geometry_resources empty_resources = {};
        *out_resources = empty_resources;
        return false;
    }

    vbo_file.read(reinterpret_cast<char*>(&index_count), sizeof(dword));
    if (index_count == 0)
    {
        LOG_WARNING(L"index count for VBO_I32 %s was 0! stopping load", file_path);
        s_geometry_resources empty_resources = {};
        *out_resources = empty_resources;
        return false;
    }

    vertex_part* vertices = new vertex_part[vertex_count];
    vbo_file.read(reinterpret_cast<char*>(vertices), sizeof(vertex_part) * vertex_count);

    dword* indices32 = new dword[index_count];
    vbo_file.read(reinterpret_cast<char*>(indices32), sizeof(dword) * index_count);

    vertex* full_vertices = new vertex[vertex_count];
    for (dword i = 0; i < vertex_count; i++)
    {
        // Convert from right to left handed
        vertices[i].position.z *= -1.0f;
        vertices[i].tex_coord.x = 1.0f - vertices[i].tex_coord.x;
        vertices[i].tex_coord.y = 1.0f - vertices[i].tex_coord.y;
        vertices[i].normal.z *= -1.0f;

        full_vertices[i].vertex = vertices[i];
    }

    const bool geometry_loaded = this->create_geometry(full_vertices, vertex_count * sizeof(vertex), indices32, index_count * sizeof(dword), out_resources);
    assert(geometry_loaded);

    // free memory
    delete[] vertices;
    delete[] indices32;
    delete[] full_vertices;

    return geometry_loaded;
}

bool c_renderer_dx12::initialise_default_geometry()
{
    // TODO: add cube mesh to here? also move to c_renderer

    simple_vertex vertices[] =
    {
        { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } }, // top left
        { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } }, // bottom left
        { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } }, // top right
        { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } }, // bottom right
    };

    const bool creation_succeeded = this->create_simple_geometry(vertices, sizeof(vertices), &m_screen_quad);
    return creation_succeeded;
}

bool c_renderer_dx12::upload_vertex_buffer(const dword vertex_size, const void* const vertices, const dword vertices_size, s_geometry_resources* const out_resources)
{
    HRESULT hr = S_OK;

    const bool arguments_valid = vertex_size > 0 && vertices != nullptr && vertices_size > 0;
    assert(arguments_valid);
    if (!arguments_valid)
    {
        LOG_WARNING(L"invalid args! aborting");
        out_resources->vertex_buffer = nullptr;
        out_resources->vertex_buffer_view = {};
        return K_FAILURE;
    }

    // Create default heap
    // default heap is memory on the GPU. Only the GPU has access to this memory
    // To get data into this heap, we will have to upload the data using an upload heap
    hr = CreateUAVBuffer(m_device, vertices_size, &out_resources->vertex_buffer, D3D12_RESOURCE_STATE_COMMON);
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    out_resources->vertex_buffer->SetName(L"Vertex Buffer Resource Heap");

    // setup vertex data subresource
    D3D12_SUBRESOURCE_DATA vertex_data = {};
    vertex_data.pData = vertices; // pointer to our vertex array
    vertex_data.RowPitch = vertices_size; // size of all our triangle vertex data
    vertex_data.SlicePitch = vertices_size; // also the size of our triangle vertex data

    // Upload resources to the GPU
    ResourceUploadBatch resource_upload(m_device);
    resource_upload.Begin();
    resource_upload.Upload(out_resources->vertex_buffer, 0, &vertex_data, 1);
    resource_upload.Transition(out_resources->vertex_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    // wait for upload thread to terminate
    std::future<void> upload_thread = resource_upload.End(m_command_queue);
    upload_thread.wait();

    // Vertex buffer view
    out_resources->vertex_buffer_view.BufferLocation = out_resources->vertex_buffer->GetGPUVirtualAddress();
    out_resources->vertex_buffer_view.StrideInBytes = vertex_size;
    out_resources->vertex_buffer_view.SizeInBytes = vertices_size;

    return K_SUCCESS;
}

bool c_renderer_dx12::upload_index_buffer(const dword indices[], const dword indices_size, s_geometry_resources* const out_resources)
{
    HRESULT hr = S_OK;

    const bool arguments_valid = indices != nullptr && indices_size > 0;
    assert(arguments_valid);
    if (!arguments_valid)
    {
        LOG_WARNING(L"invalid args! aborting");
        out_resources->index_buffer = nullptr;
        out_resources->index_buffer_view = {};
        out_resources->index_count = 0;
        return K_FAILURE;
    }

    // create default heap to hold index buffer on GPU
    hr = CreateUAVBuffer(m_device, indices_size, &out_resources->index_buffer, D3D12_RESOURCE_STATE_COMMON);
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    out_resources->index_buffer->SetName(L"Index Buffer Resource Heap");

    // setup index data subresource
    D3D12_SUBRESOURCE_DATA index_data = {};
    index_data.pData = indices; // pointer to our index array
    index_data.RowPitch = indices_size; // size of all our index buffer
    index_data.SlicePitch = indices_size; // also the size of our index buffer

    // Upload resources to the GPU
    ResourceUploadBatch resource_upload(m_device);
    resource_upload.Begin();
    resource_upload.Upload(out_resources->index_buffer, 0, &index_data, 1);
    resource_upload.Transition(out_resources->index_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    // wait for upload thread to terminate
    std::future<void> upload_thread = resource_upload.End(m_command_queue);
    upload_thread.wait();

    // Init index buffer view
    out_resources->index_buffer_view.BufferLocation = out_resources->index_buffer->GetGPUVirtualAddress();
    out_resources->index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
    out_resources->index_buffer_view.SizeInBytes = indices_size;
    out_resources->index_count = indices_size / sizeof(dword);

    return K_SUCCESS;
}

bool c_renderer_dx12::load_texture(const e_texture_type texture_type, const wchar_t* const file_path, s_texture_resources* const out_resources)
{
    HRESULT hr = S_OK;
    const bool valid_arguments = file_path != nullptr && out_resources != nullptr;
    assert(valid_arguments);
    if (!valid_arguments)
    {
        LOG_WARNING(L"invalid arguments in call! aborting");
        return K_FAILURE;
    }

    // create DDS texture and upload to the GPU using helper class
    ID3D12Resource* texture_resource;
    ResourceUploadBatch resource_upload(m_device);
    resource_upload.Begin();
    hr = CreateDDSTextureFromFile(m_device, resource_upload, file_path, &texture_resource);
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }
    texture_resource->SetName(L"Texture Buffer Resource Heap");
    std::future<void> upload_thread = resource_upload.End(m_command_queue);
    // wait for upload thread to terminate
    upload_thread.wait();

    out_resources->resource = texture_resource;

    return K_SUCCESS;
}

bool c_renderer_dx12::upload_assets()
{
    HRESULT hr = S_OK;
    // Now we execute the command list to upload the initial assets (triangle data)
    hr = m_command_list->Close();
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }

    ID3D12CommandList* pp_command_lists[] = { m_command_list };
    m_command_queue->ExecuteCommandLists(_countof(pp_command_lists), pp_command_lists);

    // increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
    m_fence_values[m_frame_index]++;
    hr = m_command_queue->Signal(m_fences[m_frame_index], m_fence_values[m_frame_index]);
    if (!HRESULT_VALID(m_device, hr)) { return K_FAILURE; }

    return K_SUCCESS;
}

bool c_renderer_dx12::initialise_imgui(const HWND hWnd)
{
    HRESULT hr = S_OK;
    // Setup Dear ImGui context

    const dword descriptor_count = k_gbuffer_count + k_light_buffer_count + 2; // + depth & imgui font texture
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = descriptor_count;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_imgui_descriptor_heap = new c_descriptor_heap(m_device, L"ImGUI heap", desc);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    const bool win32_init_succeeded = ImGui_ImplWin32_Init(hWnd);
    assert(win32_init_succeeded);
    if (!win32_init_succeeded) { return K_FAILURE; }

    // Allocate indices for gbuffers, light buffers & imgui fonts
    for (dword i = 0; i < descriptor_count; i++)
    {
        const bool allocation = m_imgui_descriptor_heap->allocate(nullptr);
        assert(allocation == K_SUCCESS);
        if (allocation == K_FAILURE) { return K_FAILURE; }
    }

    dword font_index = descriptor_count - 1;
    const bool dx12_init_succeeded = ImGui_ImplDX12_Init(m_device, FRAME_BUFFER_COUNT, DXGI_FORMAT_R8G8B8A8_UNORM, m_imgui_descriptor_heap->get_heap(),
        m_imgui_descriptor_heap->get_cpu_handle(font_index), m_imgui_descriptor_heap->get_gpu_handle(font_index));
    assert(dx12_init_succeeded);
    if (!dx12_init_succeeded) { return K_FAILURE; }

    return K_SUCCESS;
}

c_renderer_dx12::~c_renderer_dx12()
{
    // wait for the gpu to finish all frames
    for (int i = 0; i < FRAME_BUFFER_COUNT; ++i)
    {
        m_frame_index = i;
        this->wait_for_previous_frame();
    }

    if (m_fence_event != nullptr)
    {
        CloseHandle(m_fence_event);
    }

    // get swapchain out of full screen before exiting
    BOOL fs = false;
    if (HRESULT_VALID(m_device, m_swapchain->GetFullscreenState(&fs, nullptr)))
    {
        m_swapchain->SetFullscreenState(false, nullptr);
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    delete m_imgui_descriptor_heap;

    SAFE_RELEASE(m_device);
    SAFE_RELEASE(m_swapchain);
    SAFE_RELEASE(m_command_queue);
    SAFE_RELEASE(m_command_list);
    SAFE_RELEASE(m_screen_quad.vertex_buffer);
    SAFE_RELEASE(m_screen_quad.index_buffer);

    for (dword i = 0; i < k_render_target_count; i++)
    {
        if (m_render_targets[i] != nullptr)
        {
            delete m_render_targets[i];
        }
    }
    for (dword i = 0; i < FRAME_BUFFER_COUNT; ++i)
    {
        SAFE_RELEASE(m_backbuffers[i]);
        SAFE_RELEASE(m_command_allocators[i]);
        SAFE_RELEASE(m_fences[i]);
    };
    
    for (dword i = 0; i < k_shader_input_count; i++)
    {
        delete m_shader_inputs[i];
    }

    delete m_deferred_shader;
    delete m_pbr_shader;
    delete m_texcam_shader;
    for (dword i = 0; i < k_post_processing_passes; i++)
    {
        delete m_post_shaders[i];
    }
}

// TODO: decouple arg from windows
bool c_renderer_dx12::initialise(const HWND hWnd, c_scene* const scene)
{
    if (m_initialised)
    {
        LOG_WARNING(L"DirectX12 renderer already initialised!");
        return K_FAILURE;
    }

    // DirectX Graphics Infrastructure Factory
    // Provides methods for generating DXGI objects
    if (!this->initialise_factory()) { return K_FAILURE; }

    // Get the first available highest performance hardware adapter that supports DX12
    if (!this->initialise_device_adapter()) { return K_FAILURE; }

    // Command queue
    if (!this->initialise_command_queue()) { return K_FAILURE; }

    // Swapchain
    if (!this->initialise_swapchain(hWnd)) { return K_FAILURE; }

    // Disable DXGI responding to alt + enter for toggling fullscreen
    //if (!HRESULT_VALID(m_device, m_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER))) return false;

    // Command allocators
    if (!this->initialise_command_allocators()) { return K_FAILURE; }

    // Create command list
    if (!this->initialise_command_list()) { return K_FAILURE; }
    
    // Create fence and wait until assets have been uploaded to the GPU
    // Fence - The GPU object we notify when to do work, and the object we wait for the work to be done    
    if (!this->initialise_fences()) { return K_FAILURE; }

    // Define which resources are bound to the graphics pipeline
    if (!this->initialise_input_layouts()) { return K_FAILURE; }

    // Render Target View (RTV) Descriptor Heaps (Back buffers)
    if (!this->initialise_render_target_view()) { return K_FAILURE; }

    // Default geometry
    if (!this->initialise_default_geometry()) { return K_FAILURE; }

    // Raytacing pipeline
    if (!this->initialise_raytracing_pipeline()) { return K_FAILURE; }
    //this->create_acceleration_structures(scene);
    //if (!this->create_shader_binding_table(scene, false)) { return K_FAILURE; }

    // Execute the command list to upload the initial assets
    if (!this->upload_assets()) { return K_FAILURE; }

    // Fill out the Viewport
    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(RENDER_GLOBALS.render_bounds.width), static_cast<float>(RENDER_GLOBALS.render_bounds.height));

    // Fill out a scissor rect
    m_scissor_rect = CD3DX12_RECT(0, 0, static_cast<int32>(RENDER_GLOBALS.render_bounds.width), static_cast<int32>(RENDER_GLOBALS.render_bounds.height));

    // Setup Dear ImGui context
    if (!this->initialise_imgui(hWnd)) { return K_FAILURE; }

    m_initialised = true;

    return K_SUCCESS;
}

void c_renderer_dx12::update_pipeline(c_scene* const scene, dword fps_counter)
{
    HRESULT hr = S_OK;

    // We have to wait for the gpu to finish with the command allocator before we reset it
    const bool wait_succeeded = this->wait_for_previous_frame();
    assert(wait_succeeded);
    if (!wait_succeeded) { return; }

    // we can only reset an allocator once the gpu is done with it
    // resetting an allocator frees the memory that the command list was stored in
    hr = m_command_allocators[m_frame_index]->Reset();
    if (!HRESULT_VALID(m_device, hr)) { return; }

    // reset the command list. by resetting the command list we are putting it into
    // a recording state so we can start recording commands into the command allocator.
    // the command allocator that we reference here may have multiple command lists
    // associated with it, but only one can be recording at any time. Make sure
    // that any other command lists associated to this command allocator are in
    // the closed state (not recording).
    // Here you will pass an initial pipeline state object as the second parameter,
    // but in this tutorial we are only clearing the rtv, and do not actually need
    // anything but an initial default pipeline, which is what we get by setting
    // the second parameter to NULL
    hr = m_command_list->Reset(m_command_allocators[m_frame_index], NULL);
    if (!HRESULT_VALID(m_device, hr)) { return; }
    // here we start recording commands into the commandList (which all the commands will be stored in the commandAllocator)

    m_command_list->RSSetViewports(1, &m_viewport); // set the viewports
    m_command_list->RSSetScissorRects(1, &m_scissor_rect); // set the scissor rects

    // Raster pipeline
    if (m_raster)
    {
        this->update_raster(scene);
    }
    else
    {
        this->update_raytrace(scene);
    }

    // Draw ImGUI
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::BeginFrame();

    if (m_raster)
    {
        // Make the gbuffers available for ImGUI
        for (dword i = 0; i < k_gbuffer_count; i++)
        {
            // TODO: is calling this several times a frame going to cause a memory leak?
            CreateShaderResourceView(m_device, m_render_targets[_render_target_deferred]->get_frame_resource(i, m_frame_index), m_imgui_descriptor_heap->get_cpu_handle(i));
            m_gbuffer_gpu_handles[i] = m_imgui_descriptor_heap->get_gpu_handle(i);
        }
        //for (dword i = k_gbuffer_count; i < k_gbuffer_count + k_light_buffer_count; i++)
        //{
        //    // Ditto above
        //    dword target_index = i - k_gbuffer_count;
        //    CreateShaderResourceView(m_device, lighting_target->get_frame_resource(target_index, m_frame_index), m_imgui_descriptor_heap->get_cpu_handle(i));
        //    m_gbuffer_gpu_handles[i] = m_imgui_descriptor_heap->get_gpu_handle(i);
        //}
        {
            // SRV for depth buffer
            D3D12_SHADER_RESOURCE_VIEW_DESC depth_srv = {};
            depth_srv.Format = DXGI_FORMAT_R32_FLOAT;
            depth_srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            depth_srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            depth_srv.Texture2D.MipLevels = 1;

            dword index = k_gbuffer_count + k_light_buffer_count;
            ID3D12Resource* texture_resource = m_render_targets[_render_target_deferred]->get_depth_resource(m_frame_index);
            m_device->CreateShaderResourceView(texture_resource, &depth_srv, m_imgui_descriptor_heap->get_cpu_handle(index));
            m_gbuffer_gpu_handles[index] = m_imgui_descriptor_heap->get_gpu_handle(index);
        }
    }
    else
    {
        // Start raster render for ImGUI
        m_render_targets[_render_target_raytrace]->begin_render(m_command_list, m_frame_index, false);
    }

    imgui_overlay(scene, this, fps_counter);

    c_render_target* final_target;
    if (m_raster)
    {
        final_target = m_render_targets[k_render_target_final_raster];
    }
    else
    {
        final_target = m_render_targets[k_render_target_final_raytrace];
    }

    ID3D12DescriptorHeap* imgui_descriptor_heaps[] = { m_imgui_descriptor_heap->get_heap() };
    m_command_list->SetDescriptorHeaps(_countof(imgui_descriptor_heaps), imgui_descriptor_heaps);
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_command_list);

    // prepare final frame to copy into swapchain
    TransitionResource(m_command_list, final_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    ID3D12Resource* final_frame = final_target->get_frame_resource(0, m_frame_index);
    // copy final frame to swapchain buffer and get ready to present
    m_command_list->CopyResource(m_backbuffers[m_frame_index], final_frame);
    TransitionResource(m_command_list, final_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(m_command_list, m_backbuffers[m_frame_index], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

    hr = m_command_list->Close();
    if (!HRESULT_VALID(m_device, hr)) { return; }
}

void c_renderer_dx12::update_raster(c_scene* const scene)
{
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // set the primitive topology

    // Deferred pass
    c_render_target* deferred_target = m_render_targets[_render_target_deferred];
    deferred_target->begin_render(m_command_list, m_frame_index);
    dword object_index = 0;
    std::vector<c_scene_object*> texcam_objects;
    std::vector<dword> texcam_object_scene_indices;
    for (c_scene_object* const object : *scene->get_objects())
    {
        // Assign textures from material
        const c_material* const material = object->get_material();

        // Render textures will eventually be overdrawn, but on the first pass they will use their material
        if (material->m_properties.m_render_texture)
        {
            texcam_objects.push_back(object);
            texcam_object_scene_indices.push_back(object_index);
        }

        for (dword i = 0; i < material->get_maximum_textures(); i++)
        {
            c_render_texture* const texture = material->get_texture(i);
            e_texture_type texture_type = texture->get_type();
            const s_texture_resources* const texture_resources = texture->get_resources();
            deferred_target->assign_texture((ID3D12Resource*)texture_resources->resource, texture_type, object_index);
        }
        deferred_target->begin_draw(m_command_list, m_deferred_shader, object_index);

        // Per-object constant buffers (materials & transforms)
        this->set_constant_buffer_view(deferred_target, _deferred_constant_buffer_object, object_index);
        this->set_constant_buffer_view(deferred_target, _deferred_constant_buffer_materials, object_index);

        // Set geometry buffers & draw
        const s_geometry_resources* const geometry_resources = object->get_model()->get_resources();
        m_command_list->IASetVertexBuffers(0, 1, &geometry_resources->vertex_buffer_view); // set the vertex buffer (using the vertex buffer view)
        m_command_list->IASetIndexBuffer(&geometry_resources->index_buffer_view);
        m_command_list->DrawIndexedInstanced(geometry_resources->index_count, 1, 0, 0, 0);

        object_index++;
    }

    // PBR + Lighting pass
    c_render_target* pbr_target = m_render_targets[_render_target_pbr];
    pbr_target->begin_render(m_command_list, m_frame_index);
    e_gbuffers pbr_gbuffers[k_lighting_textures_count] = { _gbuffer_position, _gbuffer_normal, _gbuffer_albedo, _gbuffer_roughness, _gbuffer_metallic };
    for (dword i = 0; i < k_lighting_textures_count; i++)
    {
        ID3D12Resource* deferred_texture = deferred_target->get_frame_resource(pbr_gbuffers[i], m_frame_index);
        pbr_target->assign_texture(deferred_texture, (e_texture_type)i, 0); // TODO: TRANSITION RESOURCE?
    }
    pbr_target->begin_draw(m_command_list, m_pbr_shader, 0);
    this->set_constant_buffer_view(pbr_target, _lighting_constant_buffer_lights, 0);
    // Draw screen quad
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_command_list->IASetVertexBuffers(0, 1, &m_screen_quad.vertex_buffer_view); // set the vertex buffer (using the vertex buffer view)
    m_command_list->DrawInstanced(4, 1, 0, 0);

    //// Shading pass
    //c_render_target* shading_target = m_render_targets[_render_target_shading];
    //shading_target->begin_render(m_command_list, m_frame_index);
    //e_light_buffers shading_light_buffers[k_light_buffer_count] = { _light_buffer_ambient, _light_buffer_diffuse, _light_buffer_specular };
    //for (dword i = 0; i < _texture_shading_albedo; i++)
    //{
    //    ID3D12Resource* lighting_texture = lighting_target->get_frame_resource(shading_light_buffers[i], m_frame_index);
    //    shading_target->assign_texture(lighting_texture, (e_texture_type)i, 0); // TODO: TRANSITION RESOURCE?
    //}
    //e_gbuffers shading_gbuffers[k_shading_textures_count - _texture_shading_albedo] = { _gbuffer_albedo, _gbuffer_emissive };
    //for (dword i = _texture_shading_albedo; i < k_shading_textures_count; i++)
    //{
    //    ID3D12Resource* deferred_texture = deferred_target->get_frame_resource(shading_gbuffers[i - _texture_shading_albedo], m_frame_index);
    //    shading_target->assign_texture(deferred_texture, (e_texture_type)i, 0); // TODO: TRANSITION RESOURCE?
    //}
    //shading_target->begin_draw(m_command_list, m_shading_shader, 0);
    //// Draw screen quad
    //m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    //m_command_list->IASetVertexBuffers(0, 1, &m_screen_quad.vertex_buffer_view); // set the vertex buffer (using the vertex buffer view)
    //m_command_list->DrawInstanced(4, 1, 0, 0);

    // TODO: TEXCAM IS Z FIGHTING SINCE ADDING DEFERRED RENDERING - Might have something to dow ith frame resource set index 0? I'm very tired right now and probably missing something obvious
    // Texture camera objects
    // copy lighting pass target buffer into tex cam first and don't clear
    c_render_target* texcam_target = m_render_targets[_render_target_texcams];
    // Copy rendered frame into texcam view
    TransitionResource(m_command_list, pbr_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE); // transition to copy
    TransitionResource(m_command_list, texcam_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST); // transition to copy
    m_command_list->CopyResource(texcam_target->get_frame_resource(0, m_frame_index), pbr_target->get_frame_resource(0, m_frame_index));
    TransitionResource(m_command_list, pbr_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // return to target
    TransitionResource(m_command_list, texcam_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    // Copy deferred depth buffer into texcam view
    TransitionResource(m_command_list, deferred_target->get_depth_resource(m_frame_index), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE);
    TransitionResource(m_command_list, texcam_target->get_depth_resource(m_frame_index), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_DEST);
    m_command_list->CopyResource(texcam_target->get_depth_resource(m_frame_index), deferred_target->get_depth_resource(m_frame_index));
    TransitionResource(m_command_list, deferred_target->get_depth_resource(m_frame_index), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    TransitionResource(m_command_list, texcam_target->get_depth_resource(m_frame_index), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // TODO: SET TOPOLOGY IN SHADER INPUT
    texcam_target->begin_render(m_command_list, m_frame_index, false);
    // Re-draw all tex camera objects with rendered scene as the only input texture
    dword texcam_index = 0;
    for (const c_scene_object* const object : texcam_objects)
    {
        dword object_scene_index = texcam_object_scene_indices[texcam_index];
        texcam_target->assign_texture(pbr_target->get_frame_resource(0, m_frame_index), _texture_cam_render_target, texcam_index);
        texcam_target->begin_draw(m_command_list, m_texcam_shader, texcam_index);
        this->set_constant_buffer_view(texcam_target, _texcam_constant_buffer_object, object_scene_index);

        const s_geometry_resources* const geometry_resources = object->get_model()->get_resources();
        m_command_list->IASetVertexBuffers(0, 1, &geometry_resources->vertex_buffer_view); // set the vertex buffer (using the vertex buffer view)
        m_command_list->IASetIndexBuffer(&geometry_resources->index_buffer_view);
        m_command_list->DrawIndexedInstanced(geometry_resources->index_count, 1, 0, 0, 0);

        texcam_index++;
    }
    TransitionResource(m_command_list, pbr_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Post processing
    TransitionResource(m_command_list, texcam_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    {
        ID3D12Resource* texture_resources[] = { texcam_target->get_frame_resource(0, m_frame_index), nullptr, nullptr };
        static_assert(_countof(texture_resources) == k_post_textures_count);
        c_flags<e_constant_buffers, dword, k_post_constant_buffer_count> enabled_cbuffers;
        enabled_cbuffers.set(_post_constant_buffer, true);
        this->post_processing(_post_processing_default, texture_resources, enabled_cbuffers);
    }
    {
        e_render_targets default_target = (e_render_targets)(_post_processing_default + k_default_render_target_count);
        ID3D12Resource* texture_resources[] = { m_render_targets[default_target]->get_frame_resource(0, m_frame_index), nullptr, nullptr };
        static_assert(_countof(texture_resources) == k_post_textures_count);
        c_flags<e_constant_buffers, dword, k_post_constant_buffer_count> enabled_cbuffers;
        enabled_cbuffers.set(_post_constant_buffer, true);
        this->post_processing(_post_processing_blur_horizontal, texture_resources, enabled_cbuffers);
    }
    {
        e_render_targets blurh_target = (e_render_targets)(_post_processing_blur_horizontal + k_default_render_target_count);
        ID3D12Resource* texture_resources[] = { m_render_targets[blurh_target]->get_frame_resource(0, m_frame_index), nullptr, nullptr };
        static_assert(_countof(texture_resources) == k_post_textures_count);
        c_flags<e_constant_buffers, dword, k_post_constant_buffer_count> enabled_cbuffers;
        enabled_cbuffers.set(_post_constant_buffer, true);
        this->post_processing
        (
            _post_processing_blur_vertical,
            texture_resources,
            enabled_cbuffers
        );
    }
    {
        e_render_targets default_target = (e_render_targets)(_post_processing_default + k_default_render_target_count);
        e_render_targets blur_target = (e_render_targets)(_post_processing_blur_vertical + k_default_render_target_count);
        ID3D12Resource* texture_resources[] =
        {
            m_render_targets[default_target]->get_frame_resource(0, m_frame_index),
            m_render_targets[_render_target_deferred]->get_depth_resource(m_frame_index),
            m_render_targets[blur_target]->get_frame_resource(0, m_frame_index)
        };
        static_assert(_countof(texture_resources) == k_post_textures_count);
        c_flags<e_constant_buffers, dword, k_post_constant_buffer_count> enabled_cbuffers;
        enabled_cbuffers.set(_post_constant_buffer, true);
        this->post_processing(_post_processing_depth_of_field, texture_resources, enabled_cbuffers);
    }
    TransitionResource(m_command_list, texcam_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void c_renderer_dx12::set_constant_buffer_view(const c_render_target* const target, const e_constant_buffers buffer_type, const dword buffer_index)
{
    c_constant_buffer* const constant_buffer = target->get_shader_input()->get_constant_buffer(buffer_type);
    D3D12_GPU_VIRTUAL_ADDRESS gpu_address = constant_buffer->get_gpu_address(m_frame_index, buffer_index);
    m_command_list->SetGraphicsRootConstantBufferView(buffer_type, gpu_address);
}

void c_renderer_dx12::post_processing(const e_post_processing_passes pass, ID3D12Resource* const texture_resources[], const dword buffer_flags)
{
    const bool arguments_valid = IN_RANGE_INCLUSIVE(pass, 0, k_post_processing_passes) && texture_resources != nullptr;
    assert(arguments_valid);
    if (!arguments_valid)
    {
        LOG_WARNING(L"invalid arguments! aborting post processing pass");
        return;
    }

    c_flags<e_constant_buffers, dword, k_post_constant_buffer_count> enabled_cbuffers = buffer_flags;
    c_render_target* post_target = m_render_targets[k_default_render_target_count + pass];

    post_target->begin_render(m_command_list, m_frame_index);

    for (dword i = 0; i < k_post_textures_count; i++)
    {
        ID3D12Resource* const resource = texture_resources[i];
        if (resource != nullptr)
        {
            post_target->assign_texture(resource, (e_texture_type)i, pass);
        }
    }

    post_target->begin_draw(m_command_list, m_post_shaders[pass], pass);
    
    for (dword i = 0; i < k_post_constant_buffer_count; i++)
    {
        if (enabled_cbuffers.test((e_constant_buffers)i))
        {
            this->set_constant_buffer_view(post_target, (e_constant_buffers)i, 0);
        }
    }

    // Draw triangle strip screen quad
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_command_list->IASetVertexBuffers(0, 1, &m_screen_quad.vertex_buffer_view); // set the vertex buffer (using the vertex buffer view)
    m_command_list->DrawInstanced(4, 1, 0, 0);
}

void c_renderer_dx12::render_frame(c_scene* const scene, dword fps_counter)
{
    HRESULT hr = S_OK;

    this->update_pipeline(scene, fps_counter);

    // create an array of command lists (only one command list here)
    ID3D12CommandList* command_lists[] = { m_command_list };

    // execute the array of command lists
    m_command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

    // this command goes in at the end of our command queue. we will know when our command queue 
    // has finished because the fence value will be set to "fenceValue" from the GPU since the command
    // queue is being executed on the GPU
    hr = m_command_queue->Signal(m_fences[m_frame_index], m_fence_values[m_frame_index]);
    if (!HRESULT_VALID(m_device, hr)) { return; }

    // present the current backbuffer
    hr = m_swapchain->Present(0, 0);

    if (hr == DXGI_ERROR_DEVICE_HUNG)
    {
        HRESULT removal_reason = m_device->GetDeviceRemovedReason();
        HRESULT_VALID(m_device, removal_reason);
    }

    if (!HRESULT_VALID(m_device, hr)) { return; }
}

bool c_renderer_dx12::wait_for_previous_frame()
{
    HRESULT hr = S_OK;
    bool wait_success = K_SUCCESS;

    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.
    m_frame_index = m_swapchain->GetCurrentBackBufferIndex();

    // if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
    // the command queue since it has not reached the "commandQueue->Signal(fence, fenceValue)" command
    if (m_fences[m_frame_index]->GetCompletedValue() < m_fence_values[m_frame_index])
    {
        // we have the fence create an event which is signaled once the fence's current value is "fenceValue"
        hr = m_fences[m_frame_index]->SetEventOnCompletion(m_fence_values[m_frame_index], m_fence_event);
        if (!HRESULT_VALID(m_device, hr))
        {
            wait_success = K_FAILURE;
        }

        // We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
        // has reached "fenceValue", we know the command queue has finished executing
        WaitForSingleObject(m_fence_event, INFINITE);

#if defined(ENABLE_NVAPI_RAYTRACING_VALIDATION)
        NvAPI_D3D12_FlushRaytracingValidationMessages(m_device);
#endif
    }

    // increment fenceValue for next frame
    m_fence_values[m_frame_index]++;

    return wait_success;
}

// TODO: store constant buffer class in c_renderer so we don't have to use a bunch of duplicate methods like this
void c_renderer_dx12::set_object_constant_buffer(const s_object_cb& cbuffer, const dword object_index)
{
    // This sets the object cb for the texcam pass too as they are shared
    m_shader_inputs[_input_deferred]->get_constant_buffer(_deferred_constant_buffer_object)->set_data(&cbuffer, m_frame_index, object_index);
}
void c_renderer_dx12::set_material_constant_buffer(const s_material_properties_cb& cbuffer, const dword object_index)
{
    m_shader_inputs[_input_deferred]->get_constant_buffer(_deferred_constant_buffer_materials)->set_data(&cbuffer, m_frame_index, object_index);
}
void c_renderer_dx12::set_lights_constant_buffer(const s_light_properties_cb& cbuffer)
{
    m_shader_inputs[_input_pbr]->get_constant_buffer(_lighting_constant_buffer_lights)->set_data(&cbuffer, m_frame_index, 0);
}
void c_renderer_dx12::set_post_constant_buffer(const s_post_parameters_cb& cbuffer)
{
    m_shader_inputs[_input_post_processing]->get_constant_buffer(_post_constant_buffer)->set_data(&cbuffer, m_frame_index, 0);
}
