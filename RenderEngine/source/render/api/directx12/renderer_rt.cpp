#include "renderer.h"
#include "DXRHelper.h"
#include <render/api/directx12/helpers.h>
#include <scene/scene.h>
#include <nv_helpers_dx12/RaytracingPipelineGenerator.h>
#include <nv_helpers_dx12/RootSignatureGenerator.h>
#include <render\texture.h>
#include <reporting\report.h>
#include <DirectXHelpers.h>
#include <BufferHelpers.h>

bool c_renderer_dx12::initialise_raytracing_pipeline()
{
    if (!this->create_raytracing_pipeline()) { return K_FAILURE; }

    // Create a SRV/UAV/CBV descriptor heap. We need 2 entries - 1 UAV for the raytracing output and 1 SRV for the TLAS
    // FRAME0: Output UAV, Top level AS, Camera CB
    // FRAME1: Output UAV, Top level AS, Camera CB
    // FRAME2: Output UAV, Top level AS, Camera CB
    // Textures
    // $TODO: clean this up on destruction
    m_srv_uav_heap = new DescriptorHeap(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, FRAME_BUFFER_COUNT * 3 + 4);

    return K_SUCCESS;
}

void c_renderer_dx12::update_raytrace(c_scene* const scene)
{
    c_render_target* raytrace_target = m_render_targets[_render_target_raytrace];
    //raytrace_target->begin_render(m_command_list, m_frame_index);

    this->update_shader_resource_heap(scene);
    this->create_shader_binding_table(scene, true);

    // Bind the descriptor heap giving access to the top-level acceleration structure, as well as the raytracing output
    //const dword increment_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    //CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart(), m_frame_index * 3, increment_size);
    std::vector<ID3D12DescriptorHeap*> heaps = { m_srv_uav_heap->Heap()};
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

//-----------------------------------------------------------------------------
//
// Combine the BLAS and TLAS builds to construct the entire acceleration
// structure required to raytrace the scene
//
bool c_renderer_dx12::create_acceleration_structures(c_scene* const scene)
{
    // TODO: crashing on update with too much geometry
    // TODO: handle updates
    // TODO: design around calling reset & upload

    // TODO: don't do this here
    HRESULT hr = m_command_list->Reset(m_command_allocators[m_frame_index], NULL);
    if (!HRESULT_VALID(hr)) { return K_FAILURE; }

    // Build the bottom AS from the Triangle vertex buffer
    std::vector<c_scene_object*> objects = *scene->get_objects();
    for (dword object_index = 0; object_index < objects.size(); object_index++)
    {
        c_scene_object* object = objects[object_index];
        const s_geometry_resources* geometry = object->get_model()->get_resources();

        D3D12_RAYTRACING_GEOMETRY_DESC geometry_desc = {};
        geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry_desc.Triangles.VertexBuffer.StartAddress = geometry->vertex_buffer->GetGPUVirtualAddress(); // If this doesn't work, get the address from the vertex buffer view
        geometry_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(vertex);
        geometry_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometry_desc.Triangles.VertexCount = geometry->vertex_count;
        geometry_desc.Triangles.IndexBuffer = geometry->index_buffer->GetGPUVirtualAddress();
        geometry_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        geometry_desc.Triangles.IndexCount = geometry->index_count;
        geometry_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        // Get the size requirements for the scratch and AS buffers
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.NumDescs = 1;
        inputs.pGeometryDescs = &geometry_desc;
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
        m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
        s_acceleration_structure_buffers bottom_level_buffer = {};
        hr = CreateUAVBuffer(m_device, info.ScratchDataSizeInBytes, &bottom_level_buffer.pScratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if (!HRESULT_VALID(hr)) { return K_FAILURE; }
        hr = CreateUAVBuffer(m_device, info.ResultDataMaxSizeInBytes, &bottom_level_buffer.pResult, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        if (!HRESULT_VALID(hr)) { return K_FAILURE; }

        // Create the bottom-level AS
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc = {};
        as_desc.Inputs = inputs;
        as_desc.DestAccelerationStructureData = bottom_level_buffer.pResult->GetGPUVirtualAddress();
        as_desc.ScratchAccelerationStructureData = bottom_level_buffer.pScratch->GetGPUVirtualAddress();
        m_command_list->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);

        // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
        CD3DX12_RESOURCE_BARRIER blas_uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(bottom_level_buffer.pResult);
        m_command_list->ResourceBarrier(1, &blas_uav_barrier);

        m_bottom_level_as.push_back(bottom_level_buffer);
    }

    // Create top level acceleration structure
    // First, get the size of the TLAS buffers and create them
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = objects.size();
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers
    hr = CreateUAVBuffer(m_device, info.ScratchDataSizeInBytes, &m_top_level_as_buffers.pScratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (!HRESULT_VALID(hr)) { return K_FAILURE; }
    hr = CreateUAVBuffer(m_device, info.ResultDataMaxSizeInBytes, &m_top_level_as_buffers.pResult, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
    if (!HRESULT_VALID(hr)) { return K_FAILURE; }
    //qword tlas_size = info.ResultDataMaxSizeInBytes;
    
    // Initialize the instance desc. We only have a single instance
    // The instance desc should be inside a buffer, create and map the buffer
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Alignment = 0;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.Height = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * objects.size();

    CD3DX12_HEAP_PROPERTIES upload_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    hr = m_device->CreateCommittedResource(&upload_heap_properties, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_top_level_as_buffers.pInstanceDesc));
    if (!HRESULT_VALID(hr)) { return K_FAILURE; }

    D3D12_RAYTRACING_INSTANCE_DESC* raytracing_instance_descs;
    m_top_level_as_buffers.pInstanceDesc->Map(0, nullptr, (void**)&raytracing_instance_descs);
    ZeroMemory(raytracing_instance_descs, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * objects.size());

    //D3D12_RAYTRACING_INSTANCE_DESC* raytracing_instance_descs = new D3D12_RAYTRACING_INSTANCE_DESC[objects.size()];
    //memset(raytracing_instance_descs, 0, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * objects.size());
    
    //raytracing_instance_desc.InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
    //raytracing_instance_desc.InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
    //raytracing_instance_desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

    for (dword object_index = 0; object_index < objects.size(); object_index++)
    {
        c_scene_object* object = objects[object_index];

        raytracing_instance_descs[object_index].InstanceID = object_index;
        raytracing_instance_descs[object_index].InstanceContributionToHitGroupIndex = 0;
        raytracing_instance_descs[object_index].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        matrix4x4 object_matrix = objects[object_index]->m_transform.build_matrix();
        XMMATRIX xm_matrix = XMLoadFloat4x4((XMFLOAT4X4*)&object_matrix);
        xm_matrix = XMMatrixTranspose(xm_matrix);
        memcpy(raytracing_instance_descs[object_index].Transform, &xm_matrix, sizeof(raytracing_instance_descs[object_index].Transform));
        raytracing_instance_descs[object_index].AccelerationStructure = m_bottom_level_as[object_index].pResult->GetGPUVirtualAddress();
        raytracing_instance_descs[object_index].InstanceMask = 0xFF;
    }

    // The instance desc should be inside a buffer, create and map the buffer
    //hr = CreateUploadBuffer(m_device, &raytracing_instance_descs, objects.size(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC), &m_top_level_as_buffers.pInstanceDesc);
    //if (!HRESULT_VALID(hr)) { return K_FAILURE; }
    // Unmap
    m_top_level_as_buffers.pInstanceDesc->Unmap(0, nullptr);

    // Create the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc = {};
    as_desc.Inputs = inputs;
    as_desc.Inputs.InstanceDescs = m_top_level_as_buffers.pInstanceDesc->GetGPUVirtualAddress();
    as_desc.DestAccelerationStructureData = m_top_level_as_buffers.pResult->GetGPUVirtualAddress();
    as_desc.ScratchAccelerationStructureData = m_top_level_as_buffers.pScratch->GetGPUVirtualAddress();

    m_command_list->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    CD3DX12_RESOURCE_BARRIER tlas_uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_top_level_as_buffers.pResult);
    m_command_list->ResourceBarrier(1, &tlas_uav_barrier);

    // TODO: don't do this here
    // Flush the command list and wait for it to finish
    m_command_list->Close();
    ID3D12CommandList* ppCommandLists[] = { m_command_list };
    m_command_queue->ExecuteCommandLists(1, ppCommandLists);
    m_fence_values[m_frame_index]++;
    m_command_queue->Signal(m_fences[m_frame_index], m_fence_values[m_frame_index]);

    m_fences[m_frame_index]->SetEventOnCompletion(m_fence_values[m_frame_index], m_fence_event);
    WaitForSingleObject(m_fence_event, INFINITE);

    //delete[] raytracing_instance_descs;

    return K_SUCCESS;
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
        { 0 /*b0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV /* Object/Camera buffer */, 2}
    });

    return rsc.Generate(m_device, true);
}

//-----------------------------------------------------------------------------
// The hit shader communicates only through the ray payload, and therefore does
// not require any resources
//
ID3D12RootSignature* c_renderer_dx12::create_hit_signature()
{
    ID3D12RootSignature* root_signature;

    D3D12_ROOT_PARAMETER root_parameters[3] = {};

    // Vertices
    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    root_parameters[0].Descriptor.RegisterSpace = 0;
    root_parameters[0].Descriptor.ShaderRegister = 0; // t0
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    // Indices
    root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    root_parameters[1].Descriptor.RegisterSpace = 0;
    root_parameters[1].Descriptor.ShaderRegister = 1; // t1
    root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    // Textures
    D3D12_ROOT_DESCRIPTOR_TABLE descriptor_table;
    CD3DX12_DESCRIPTOR_RANGE texture_ranges[] = { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, k_default_textures_count, 2 } }; // t2
    descriptor_table.NumDescriptorRanges = _countof(texture_ranges); // we only have one range
    descriptor_table.pDescriptorRanges = texture_ranges; // the pointer to the beginning of our ranges array
    root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[2].DescriptorTable = descriptor_table;
    root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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
        _countof(root_parameters),
        root_parameters, // a pointer to the beginning of our root parameters array
        _countof(samplers),
        samplers, // a pointer to our static samplers (array)
        D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE
    );

    ID3DBlob* signature;
    ID3DBlob* error; // a buffer holding the error data if any
    HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (hr == S_OK)
    {
        hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature));
        if (!HRESULT_VALID(hr))
        {
            LOG_ERROR(L"failed to create root signature!");
        }
        else
        {
            return root_signature;
        }
    }
    return nullptr;

    //nv_helpers_dx12::RootSignatureGenerator rsc;
    //// TODO: these have a maximum of 65k, which may be causing issues?
    //rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0 /*t0*/); // vertices
    //rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1 /*t1*/); // indices
    //rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 2 /*t2*/); // textures
    //
    //rsc.AddHeapRangesParameter
    //({
    //    //{ 2 /*t2*/, 4, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 },      // Albedo texture
    //    { 0 /*s0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1 }   // Sampler
    //});
    //return rsc.Generate(m_device, true);
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
bool c_renderer_dx12::create_raytracing_pipeline()
{
    // Need 10 subobjects:
    //  1 for the DXIL library
    //  1 for hit-group
    //  2 for RayGen root-signature (root-signature and the subobject association)
    //  2 for miss shader root-signature (signature and association)
    //  2 for hit shader root-signature (signature and association)
    //  2 for shader config (shared between all programs. 1 for the config, 1 for association)
    //  1 for pipeline config
    //  1 for the global root signature
    //D3D12_STATE_SUBOBJECT subobjects[12];
    //uint32_t index = 0;

    // Create the DXIL library
    //DxilLibrary dxilLib = createDxilLibrary();
    //subobjects[index++] = dxilLib.stateSubobject; // 0 Library





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
    if (!HRESULT_VALID(hr)) { return K_FAILURE; }

    return K_SUCCESS;
}

//-----------------------------------------------------------------------------
//
// Create the main heap used by the shaders, which will give access to the
// raytracing output and the top-level acceleration structure
//
// $TODO: textures per object!
void c_renderer_dx12::update_shader_resource_heap(c_scene* const scene)
{
    const dword increment_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Get a handle to the heap memory on the CPU side, to be able to write the descriptors directly
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srv_uav_heap->Heap()->GetCPUDescriptorHandleForHeapStart(), m_frame_index * 3, increment_size);
    
    // Create the UAV. Based on the root signature we created it is the first entry. The Create*View methods write the view information directly into srvHandle
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

    //srvHandle.ptr += increment_size;

    srvHandle.InitOffsetted(m_srv_uav_heap->Heap()->GetCPUDescriptorHandleForHeapStart(), FRAME_BUFFER_COUNT * 3, increment_size);
    std::vector<c_scene_object*> objects = *scene->get_objects();
    CreateShaderResourceView(m_device, (ID3D12Resource*)objects[0]->get_material()->get_texture(0)->get_resources()->resource, srvHandle);
    srvHandle.ptr += increment_size;
    CreateShaderResourceView(m_device, (ID3D12Resource*)objects[0]->get_material()->get_texture(1)->get_resources()->resource, srvHandle);
    srvHandle.ptr += increment_size;
    CreateShaderResourceView(m_device, (ID3D12Resource*)objects[0]->get_material()->get_texture(2)->get_resources()->resource, srvHandle);
    srvHandle.ptr += increment_size;
    CreateShaderResourceView(m_device, (ID3D12Resource*)objects[0]->get_material()->get_texture(3)->get_resources()->resource, srvHandle);
    srvHandle.ptr += increment_size;


    // FOR EACH OBJECT
    //std::vector<c_scene_object*> objects = *scene->get_objects();
    //for (dword object_index = 0; object_index < /*objects.size()*/1; object_index++)
    //{
    //    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle(m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart(), object_index, increment_size);
    //    CreateShaderResourceView(m_device, (ID3D12Resource*)objects[object_index]->get_material()->get_texture(0)->get_resources()->resource, descriptor_handle);
    //
    //    CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart(), object_index, increment_size);
    //    m_texture_handles.push_back(texHandle);
    //
    //    srvHandle.ptr += increment_size;
    //}
    
    //D3D12_SAMPLER_DESC samplerDesc = {};
    //samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    //samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    //samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    //samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    //
    //CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(m_sampler_heap->GetCPUDescriptorHandleForHeapStart(), 0, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));
    //m_device->CreateSampler(&samplerDesc, samplerHandle);
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
bool c_renderer_dx12::create_shader_binding_table(c_scene* const scene, bool update_only)
{
    // The SBT helper class collects calls to Add*Program.  If called several
    // times, the helper must be emptied before re-adding shaders.
    m_sbt_helper.Reset();

    // The pointer to the beginning of the heap is the only parameter required by
    // shaders without root parameters
    const dword increment_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle(m_srv_uav_heap->Heap()->GetGPUDescriptorHandleForHeapStart(), m_frame_index * 3, increment_size);

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
    // Offset descriptor heap to texture table
    srvUavHeapHandle.InitOffsetted(m_srv_uav_heap->Heap()->GetGPUDescriptorHandleForHeapStart(), FRAME_BUFFER_COUNT * 3, increment_size);
    for (dword object_index = 0; object_index < objects.size(); object_index++)
    {
        std::vector<void*> geometry_data;

        geometry_data.push_back((void*)objects[object_index]->get_model()->get_resources()->vertex_buffer->GetGPUVirtualAddress());
        geometry_data.push_back((void*)objects[object_index]->get_model()->get_resources()->index_buffer->GetGPUVirtualAddress());
        geometry_data.push_back((void*)srvUavHeapHandle.ptr); // texture table

        m_sbt_helper.AddHitGroup(L"hit_group", geometry_data);
    }

    // Compute the size of the SBT given the number of shaders and their parameters
    uint32_t sbt_size = m_sbt_helper.ComputeSBTSize();

    // Create the SBT on the upload heap. This is required as the helper will use
    // mapping to write the SBT contents. After the SBT compilation it could be
    // copied to the default heap for performance.
    if (!update_only)
    {
        for (dword i = 0; i < FRAME_BUFFER_COUNT; i++)
        {
            m_sbt_storage[i] = nv_helpers_dx12::CreateBuffer(m_device, sbt_size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
            if (!m_sbt_storage[i])
            {
                LOG_ERROR("Could not allocate the shader binding table! Failed for frame index %d", i);
                return K_FAILURE;
            }
        }
    }
    // Compile the SBT from the shader and parameters info
    m_sbt_helper.Generate(m_sbt_storage[m_frame_index], m_rt_state_object_props);

    return K_SUCCESS;
}


// Code graveyard
/*
// $TODO: REMOVE THIS!
void c_renderer_dx12::create_bottom_level_as(c_scene* const scene, s_acceleration_structure_buffers& out_buffers, bool update_only)
{
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

    // Adding all vertex buffers and not transforming their position.
    for (c_scene_object* object : *scene->get_objects())
    {
        const s_geometry_resources* geometry = object->get_model()->get_resources();
        bottomLevelAS.AddVertexBuffer(geometry->vertex_buffer, 0, geometry->vertex_count, sizeof(vertex), geometry->index_buffer, 0, geometry->index_count, nullptr, 0, true);
    }

    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 resultSizeInBytes = 0;

    bottomLevelAS.ComputeASBufferSizes(m_device, true, &scratchSizeInBytes, &resultSizeInBytes);

    if (!update_only)
    {
        // Once the sizes are obtained, the application is responsible for allocating
        // the necessary buffers. Since the entire generation will be done on the GPU,
        // we can directly allocate those on the default heap
        //s_acceleration_structure_buffers buffers;
        out_buffers.pScratch = nv_helpers_dx12::CreateBuffer(
            m_device, scratchSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nv_helpers_dx12::kDefaultHeapProps);
        out_buffers.pResult = nv_helpers_dx12::CreateBuffer(
            m_device, resultSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            nv_helpers_dx12::kDefaultHeapProps);
    }
    // Build the acceleration structure. Note that this call integrates a barrier
    // on the generated AS, so that it can be used to compute a top-level AS right
    // after this method.
    bottomLevelAS.Generate(m_command_list, out_buffers.pScratch, out_buffers.pResult, update_only, update_only ? out_buffers.pResult : nullptr);

    //return buffers;
}

//-----------------------------------------------------------------------------
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
//
void c_renderer_dx12::create_top_level_as(const std::vector<std::pair<s_acceleration_structure_buffers, DirectX::XMMATRIX>>& instances, bool update_only) // pair of bottom level AS and matrix of the instance
{
    if (!update_only)
    {
        // Gather all the instances into the builder helper
        for (size_t i = 0; i < instances.size(); i++)
        {
            m_top_level_as_generator.AddInstance(instances[i].first.pResult, instances[i].second, static_cast<dword>(i), static_cast<dword>(i));
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
    m_top_level_as_generator.Generate(m_command_list, m_top_level_as_buffers.pScratch, m_top_level_as_buffers.pResult, m_top_level_as_buffers.pInstanceDesc, update_only, update_only ? m_top_level_as_buffers.pResult : nullptr);
}
*/