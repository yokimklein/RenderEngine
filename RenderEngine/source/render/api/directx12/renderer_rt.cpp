#include "renderer.h"
#include "DXRHelper.h"
#include <nv_helpers_dx12/BottomLevelASGenerator.h>
#include <render/api/directx12/helpers.h>
#include <scene/scene.h>
#include <nv_helpers_dx12/RaytracingPipelineGenerator.h>
#include <nv_helpers_dx12/RootSignatureGenerator.h>

void c_renderer_dx12::update_raytrace(c_scene* const scene)
{
    c_render_target* raytrace_target = m_render_targets[_render_target_raytrace];
    //raytrace_target->begin_render(m_command_list, m_frame_index);

    this->create_acceleration_structures(scene, true);
    this->create_shader_resource_heap(true);
    this->create_shader_binding_table(scene, true);

    // Bind the descriptor heap giving access to the top-level acceleration structure, as well as the raytracing output
    //const dword increment_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    //CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart(), m_frame_index * 3, increment_size);
    std::vector<ID3D12DescriptorHeap*> heaps = { m_srv_uav_heap };
    m_command_list->SetDescriptorHeaps(static_cast<dword>(heaps.size()), heaps.data());
    
    // On the last frame, the raytracing output was used as a copy source, to copy its contents into the render target.
    // Now we need to transition it to a UAV so that the shaders can write in it.
    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(raytrace_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_command_list->ResourceBarrier(1, &transition);

    // Setup the raytracing task
    D3D12_DISPATCH_RAYS_DESC desc = {};
    // The layout of the SBT is as follows: ray generation shader, miss
    // shaders, hit groups. As described in the CreateShaderBindingTable method,
    // all SBT entries of a given type have the same size to allow a fixed stride.

    // The ray generation shaders are always at the beginning of the SBT. 
    uint32_t rayGenerationSectionSizeInBytes = m_sbt_helper.GetRayGenSectionSize();
    desc.RayGenerationShaderRecord.StartAddress = m_sbt_storage[m_frame_index]->GetGPUVirtualAddress();
    desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

    // The miss shaders are in the second SBT section, right after the ray generation shader.
    // We have one miss shader for the camera rays and one for the shadow rays, so this section has a size of 2*m_sbtEntrySize.
    // We also indicate the stride between the two miss shaders, which is the size of a SBT entry
    uint32_t missSectionSizeInBytes = m_sbt_helper.GetMissSectionSize();
    desc.MissShaderTable.StartAddress = m_sbt_storage[m_frame_index]->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
    desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
    desc.MissShaderTable.StrideInBytes = m_sbt_helper.GetMissEntrySize();

    // The hit groups section start after the miss shaders. In this sample we have one 1 hit group for the triangle
    uint32_t hitGroupsSectionSize = m_sbt_helper.GetHitGroupSectionSize();
    desc.HitGroupTable.StartAddress = m_sbt_storage[m_frame_index]->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
    desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
    desc.HitGroupTable.StrideInBytes = m_sbt_helper.GetHitGroupEntrySize();

    // Dimensions of the image to render, identical to a kernel launch dimension
    desc.Width = RENDER_GLOBALS.render_bounds.width;
    desc.Height = RENDER_GLOBALS.render_bounds.height;
    desc.Depth = 1;

    // Bind the raytracing pipeline
    m_command_list->SetPipelineState1(m_rt_state_object);
    // Dispatch the rays and write to the raytracing output
    m_command_list->DispatchRays(&desc);

    // The raytracing output needs to be copied to the actual render target used
    // for display. For this, we need to transition the raytracing output from a
    // UAV to a copy source, and the render target buffer to a copy destination.
    // We can then do the actual copy, before transitioning the render target
    // buffer into a render target, that will be then used to display the image
    transition = CD3DX12_RESOURCE_BARRIER::Transition(raytrace_target->get_frame_resource(0, m_frame_index), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_command_list->ResourceBarrier(1, &transition);
}

s_acceleration_structure_buffers c_renderer_dx12::create_bottom_level_as(std::vector<std::pair<ID3D12Resource*, uint32_t>> v_vertex_buffers, std::vector<std::pair<ID3D12Resource*, uint32_t>> v_index_buffers)
{
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

    // Adding all vertex buffers and not transforming their position.
    // for (const auto &buffer : v_vertex_buffers) {
    for (size_t i = 0; i < v_vertex_buffers.size(); i++)
    {
        if (i < v_index_buffers.size() && v_index_buffers[i].second > 0)
        {
            bottomLevelAS.AddVertexBuffer(v_vertex_buffers[i].first, 0, v_vertex_buffers[i].second, sizeof(vertex), v_index_buffers[i].first, 0, v_index_buffers[i].second, nullptr, 0, true);
        }
        else
        {
            bottomLevelAS.AddVertexBuffer(v_vertex_buffers[i].first, 0, v_vertex_buffers[i].second, sizeof(vertex), 0, 0);
        }
    }

    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 resultSizeInBytes = 0;

    bottomLevelAS.ComputeASBufferSizes(m_device, false, &scratchSizeInBytes, &resultSizeInBytes);

    // Once the sizes are obtained, the application is responsible for allocating
    // the necessary buffers. Since the entire generation will be done on the GPU,
    // we can directly allocate those on the default heap
    s_acceleration_structure_buffers buffers;
    buffers.pScratch = nv_helpers_dx12::CreateBuffer(
        m_device, scratchSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON,
        nv_helpers_dx12::kDefaultHeapProps);
    buffers.pResult = nv_helpers_dx12::CreateBuffer(
        m_device, resultSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nv_helpers_dx12::kDefaultHeapProps);

    // Build the acceleration structure. Note that this call integrates a barrier
    // on the generated AS, so that it can be used to compute a top-level AS right
    // after this method.
    bottomLevelAS.Generate(m_command_list, buffers.pScratch, buffers.pResult, false, nullptr);

    return buffers;
}

//-----------------------------------------------------------------------------
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
//
void c_renderer_dx12::create_top_level_as(const std::vector<std::pair<ID3D12Resource*, DirectX::XMMATRIX>>& instances, bool update_only) // pair of bottom level AS and matrix of the instance
{
    if (!update_only)
    {
        // Gather all the instances into the builder helper
        for (size_t i = 0; i < instances.size(); i++)
        {
            m_top_level_as_generator.AddInstance(instances[i].first, instances[i].second, static_cast<dword>(i), static_cast<dword>(0));
        }

        // As for the bottom-level AS, the building the AS requires some scratch space
        // to store temporary data in addition to the actual AS. In the case of the
        // top-level AS, the instance descriptors also need to be stored in GPU
        // memory. This call outputs the memory requirements for each (scratch,
        // results, instance descriptors) so that the application can allocate the
        // corresponding memory
        UINT64 scratchSize, resultSize, instanceDescsSize;

        m_top_level_as_generator.ComputeASBufferSizes(m_device, true, &scratchSize, &resultSize, &instanceDescsSize);

        // Create the scratch and result buffers. Since the build is all done on GPU,
        // those can be allocated on the default heap
        m_top_level_as_buffers.pScratch = nv_helpers_dx12::CreateBuffer(
            m_device, scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nv_helpers_dx12::kDefaultHeapProps);
        m_top_level_as_buffers.pResult = nv_helpers_dx12::CreateBuffer(
            m_device, resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            nv_helpers_dx12::kDefaultHeapProps);

        // The buffer describing the instances: ID, shader binding information,
        // matrices ... Those will be copied into the buffer by the helper through
        // mapping, so the buffer has to be allocated on the upload heap.
        m_top_level_as_buffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
            m_device, instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
    }

    // After all the buffers are allocated, or if only an update is required, we
    // can build the acceleration structure. Note that in the case of the update
    // we also pass the existing AS as the 'previous' AS, so that it can be
    // refitted in place.
    m_top_level_as_generator.Generate(m_command_list, m_top_level_as_buffers.pScratch, m_top_level_as_buffers.pResult, m_top_level_as_buffers.pInstanceDesc, update_only, m_top_level_as_buffers.pResult);
}

//-----------------------------------------------------------------------------
//
// Combine the BLAS and TLAS builds to construct the entire acceleration
// structure required to raytrace the scene
//
void c_renderer_dx12::create_acceleration_structures(c_scene* const scene, bool update_only)
{
    if (!update_only)
    {
        HRESULT hr = m_command_list->Reset(m_command_allocators[m_frame_index], NULL);
        if (!HRESULT_VALID(hr)) { return; }
    }

    // Build the bottom AS from the Triangle vertex buffer
    std::vector<c_scene_object*> objects = *scene->get_objects();
    for (dword object_index = 0; object_index < objects.size(); object_index++)
    {
        c_scene_object* object = objects[object_index];
        if (update_only)
        {
            matrix4x4 object_matrix = object->m_transform.build_matrix();
            m_instances[object_index].second = XMLoadFloat4x4((XMFLOAT4X4*)&object_matrix);
        }
        else
        {
            s_acceleration_structure_buffers bottomLevelBuffers = create_bottom_level_as
            (
                { { object->get_model()->get_resources()->vertex_buffer, object->get_model()->get_resources()->vertex_count } },
                { { object->get_model()->get_resources()->index_buffer, object->get_model()->get_resources()->index_count } }
            );

            matrix4x4 object_matrix = object->m_transform.build_matrix();
            m_instances.push_back({ bottomLevelBuffers.pResult, XMLoadFloat4x4((XMFLOAT4X4*)&object_matrix) });

            // Store the AS buffers. The rest of the buffers will be released once we exit the function
            m_bottom_level_as.push_back(bottomLevelBuffers);
        }
    }

    create_top_level_as(m_instances, update_only);

    if (!update_only)
    {
        // Flush the command list and wait for it to finish
        m_command_list->Close();
        ID3D12CommandList* ppCommandLists[] = { m_command_list };
        m_command_queue->ExecuteCommandLists(1, ppCommandLists);
        m_fence_values[m_frame_index]++;
        m_command_queue->Signal(m_fences[m_frame_index], m_fence_values[m_frame_index]);

        m_fences[m_frame_index]->SetEventOnCompletion(m_fence_values[m_frame_index], m_fence_event);
        WaitForSingleObject(m_fence_event, INFINITE);

        // Once the command list is finished executing, reset it to be reused for
        // rendering
        //hr = m_command_list->Reset(m_command_allocators[m_frame_index], NULL);
        //if (!HRESULT_VALID(hr)) { return; }
    }
}

//-----------------------------------------------------------------------------
// The ray generation shader needs to access 2 resources: the raytracing output
// and the top-level acceleration structure
//
ID3D12RootSignature* c_renderer_dx12::create_ray_gen_signature()
{
    nv_helpers_dx12::RootSignatureGenerator rsc;
    rsc.AddHeapRangesParameter
    ({
        { 0 /*u0*/, 1 /*1 descriptor */, 0 /*use the implicit register space 0*/, D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/, 0 /*heap slot where the UAV is defined*/ },
        { 0 /*t0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 1 },
        { 0 /*b0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV /* Object buffer */, 2}
    });

    return rsc.Generate(m_device, true);
}

//-----------------------------------------------------------------------------
// The hit shader communicates only through the ray payload, and therefore does
// not require any resources
//
ID3D12RootSignature* c_renderer_dx12::create_hit_signature()
{
    nv_helpers_dx12::RootSignatureGenerator rsc;
    rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0 /*t0*/); // vertices
    rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1 /*t1*/); // indices
    return rsc.Generate(m_device, true);
}

//-----------------------------------------------------------------------------
// The miss shader communicates only through the ray payload, and therefore
// does not require any resources
//
ID3D12RootSignature* c_renderer_dx12::create_miss_signature()
{
    nv_helpers_dx12::RootSignatureGenerator rsc;
    return rsc.Generate(m_device, true);
}

//-----------------------------------------------------------------------------
//
// The raytracing pipeline binds the shader code, root signatures and pipeline
// characteristics in a single structure used by DXR to invoke the shaders and
// manage temporary memory during raytracing
//
//
void c_renderer_dx12::create_raytracing_pipeline()
{
    nv_helpers_dx12::RayTracingPipelineGenerator pipeline(m_device);

    // The pipeline contains the DXIL code of all the shaders potentially executed
    // during the raytracing process. This section compiles the HLSL code into a
    // set of DXIL libraries. We chose to separate the code in several libraries
    // by semantic (ray generation, hit, miss) for clarity. Any code layout can be
    // used.
    m_ray_gen_library = nv_helpers_dx12::CompileShaderLibrary(L"assets/shaders/raytrace/ray_gen.hlsl");
    m_miss_library = nv_helpers_dx12::CompileShaderLibrary(L"assets/shaders/raytrace/miss.hlsl");
    m_hit_library = nv_helpers_dx12::CompileShaderLibrary(L"assets/shaders/raytrace/hit.hlsl");

    // In a way similar to DLLs, each library is associated with a number of
    // exported symbols. This
    // has to be done explicitly in the lines below. Note that a single library
    // can contain an arbitrary number of symbols, whose semantic is given in HLSL
    // using the [shader("xxx")] syntax
    pipeline.AddLibrary(m_ray_gen_library, { L"ray_gen" });
    pipeline.AddLibrary(m_miss_library, { L"miss" });
    pipeline.AddLibrary(m_hit_library, { L"closest_hit" });

    // To be used, each DX12 shader needs a root signature defining which
    // parameters and buffers will be accessed.
    m_ray_gen_signature = create_ray_gen_signature();
    m_miss_signature = create_miss_signature();
    m_hit_signature = create_hit_signature();

    // 3 different shaders can be invoked to obtain an intersection: an
    // intersection shader is called
    // when hitting the bounding box of non-triangular geometry. This is beyond
    // the scope of this tutorial. An any-hit shader is called on potential
    // intersections. This shader can, for example, perform alpha-testing and
    // discard some intersections. Finally, the closest-hit program is invoked on
    // the intersection point closest to the ray origin. Those 3 shaders are bound
    // together into a hit group.
    
    // Note that for triangular geometry the intersection shader is built-in. An
    // empty any-hit shader is also defined by default, so in our simple case each
    // hit group contains only the closest hit shader. Note that since the
    // exported symbols are defined above the shaders can be simply referred to by
    // name.
    
    // Hit group for the triangles, with a shader simply interpolating vertex
    // colors
    pipeline.AddHitGroup(L"hit_group", L"closest_hit");

    // The following section associates the root signature to each shader. Note
    // that we can explicitly show that some shaders share the same root signature
    // (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
    // to as hit groups, meaning that the underlying intersection, any-hit and
    // closest-hit shaders share the same root signature.
    pipeline.AddRootSignatureAssociation(m_ray_gen_signature, {L"ray_gen"});
    pipeline.AddRootSignatureAssociation(m_miss_signature, {L"miss"});
    pipeline.AddRootSignatureAssociation(m_hit_signature, {L"hit_group"});

    // The payload size defines the maximum size of the data carried by the rays,
    // ie. the the data
    // exchanged between shaders, such as the HitInfo structure in the HLSL code.
    // It is important to keep this value as low as possible as a too high value
    // would result in unnecessary memory consumption and cache trashing.
    pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance

    // Upon hitting a surface, DXR can provide several attributes to the hit. In
    // our sample we just use the barycentric coordinates defined by the weights
    // u,v of the last two vertices of the triangle. The actual barycentrics can
    // be obtained using float3 barycentrics = float3(1.f-u-v, u, v);
    pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates

    // The raytracing process can shoot rays from existing hit points, resulting
    // in nested TraceRay calls. Our sample code traces only primary rays, which
    // then requires a trace depth of 1. Note that this recursion depth should be
    // kept to a minimum for best performance. Path tracing algorithms can be
    // easily flattened into a simple loop in the ray generation.
    pipeline.SetMaxRecursionDepth(1);

    // Compile the pipeline for execution on the GPU
    m_rt_state_object = pipeline.Generate();

    // Cast the state object into a properties object, allowing to later access
    // the shader pointers by name
    HRESULT hr = m_rt_state_object->QueryInterface(IID_PPV_ARGS(&m_rt_state_object_props));
    if (!HRESULT_VALID(hr)) { return; }
}

//-----------------------------------------------------------------------------
//
// Allocate the buffer holding the raytracing output, with the same size as the
// output image
//
void c_renderer_dx12::create_raytracing_output_buffer()
{
    //D3D12_RESOURCE_DESC resDesc = {};
    //resDesc.DepthOrArraySize = 1;
    //resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    //// The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB
    //// formats cannot be used with UAVs. For accuracy we should convert to sRGB
    //// ourselves in the shader
    //resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //
    //resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    //resDesc.Width = RENDER_GLOBALS.render_bounds.width;
    //resDesc.Height = RENDER_GLOBALS.render_bounds.height;
    //resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    //resDesc.MipLevels = 1;
    //resDesc.SampleDesc.Count = 1;
    //HRESULT hr = m_device->CreateCommittedResource
    //(
    //    &nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
    //    D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_output_resource)
    //);
}

//-----------------------------------------------------------------------------
//
// Create the main heap used by the shaders, which will give access to the
// raytracing output and the top-level acceleration structure
//
void c_renderer_dx12::create_shader_resource_heap(bool update_only)
{
    // Create a SRV/UAV/CBV descriptor heap. We need 2 entries - 1 UAV for the
    // raytracing output and 1 SRV for the TLAS
    if (!update_only)
    {
        m_srv_uav_heap = nv_helpers_dx12::CreateDescriptorHeap(m_device, 3 * FRAME_BUFFER_COUNT, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
    }

    const dword increment_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Get a handle to the heap memory on the CPU side, to be able to write the
    // descriptors directly
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart(), m_frame_index * 3, increment_size);
    
    // Create the UAV. Based on the root signature we created it is the first
    // entry. The Create*View methods write the view information directly into
    // srvHandle
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_device->CreateUnorderedAccessView(m_render_targets[_render_target_raytrace]->get_frame_resource(0, m_frame_index), nullptr, &uavDesc, srvHandle);

    // Add the Top Level AS SRV right after the raytracing output buffer
    srvHandle.ptr += increment_size;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_top_level_as_buffers.pResult->GetGPUVirtualAddress();
    // Write the acceleration structure view in the heap
    m_device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

    // #DXR Extra: Perspective Camera
    // Add the constant buffer for the camera after the TLAS
    srvHandle.ptr += increment_size;

    // Describe and create a constant buffer view for the camera
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    // $TODO: decouple shader input & constant buffers
    // $TODO: refactor shader input classes to be more dynamic
    c_constant_buffer* objbuffer = m_shader_inputs[_input_deferred]->get_constant_buffer(_deferred_constant_buffer_object);
    cbvDesc.BufferLocation = objbuffer->get_gpu_address(m_frame_index, 0); // $TODO: per object? we don't actually use the object world matrix so we should be fine
    cbvDesc.SizeInBytes = objbuffer->get_buffer_size();
    m_device->CreateConstantBufferView(&cbvDesc, srvHandle);
}

//-----------------------------------------------------------------------------
//
// The Shader Binding Table (SBT) is the cornerstone of the raytracing setup:
// this is where the shader resources are bound to the shaders, in a way that
// can be interpreted by the raytracer on GPU. In terms of layout, the SBT
// contains a series of shader IDs with their resource pointers. The SBT
// contains the ray generation shader, the miss shaders, then the hit groups.
// Using the helper class, those can be specified in arbitrary order.
//
void c_renderer_dx12::create_shader_binding_table(c_scene* const scene, bool update_only)
{
    // The SBT helper class collects calls to Add*Program.  If called several
    // times, the helper must be emptied before re-adding shaders.
    m_sbt_helper.Reset();

    // The pointer to the beginning of the heap is the only parameter required by
    // shaders without root parameters
    const dword increment_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle(m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart(), m_frame_index * 3, increment_size);

    // The helper treats both root parameter pointers and heap pointers as void*,
    // while DX12 uses the
    // D3D12_GPU_DESCRIPTOR_HANDLE to define heap pointers. The pointer in this
    // struct is a UINT64, which then has to be reinterpreted as a pointer.
    auto heapPointer = reinterpret_cast<void*>(srvUavHeapHandle.ptr);

    // The ray generation only uses heap data
    m_sbt_helper.AddRayGenerationProgram(L"ray_gen", { heapPointer });

    // The miss and hit shaders do not access any external resources: instead they
    // communicate their results through the ray payload
    m_sbt_helper.AddMissProgram(L"miss", {});

    // Adding the triangle hit shader
    std::vector<c_scene_object*> objects = *scene->get_objects();
    std::vector<void*> geometry_data;
    for (c_scene_object* const object : *scene->get_objects())
    {
        geometry_data.push_back((void*)object->get_model()->get_resources()->vertex_buffer->GetGPUVirtualAddress());
        geometry_data.push_back((void*)object->get_model()->get_resources()->index_buffer->GetGPUVirtualAddress());
    }
    m_sbt_helper.AddHitGroup(L"hit_group", geometry_data);

    // Compute the size of the SBT given the number of shaders and their parameters
    uint32_t sbtSize = m_sbt_helper.ComputeSBTSize();

    // Create the SBT on the upload heap. This is required as the helper will use
    // mapping to write the SBT contents. After the SBT compilation it could be
    // copied to the default heap for performance.
    if (!update_only)
    {
        for (dword i = 0; i < FRAME_BUFFER_COUNT; i++)
        {
            m_sbt_storage[i] = nv_helpers_dx12::CreateBuffer(m_device, sbtSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
        }
    }
    if (!m_sbt_storage[m_frame_index])
    {
        throw std::logic_error("Could not allocate the shader binding table");
    }
    // Compile the SBT from the shader and parameters info
    m_sbt_helper.Generate(m_sbt_storage[m_frame_index], m_rt_state_object_props);
}