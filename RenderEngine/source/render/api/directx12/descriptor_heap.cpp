#include "descriptor_heap.h"
#include <reporting/report.h>
#include <render/api/directx12/helpers.h>
#include <d3dx12.h>

c_descriptor_heap::c_descriptor_heap(ID3D12Device5* const device, const wchar_t* name, const D3D12_DESCRIPTOR_HEAP_DESC heap_description)
    : m_maximum_allocations(heap_description.NumDescriptors)
    , m_allocated(0)
    , m_size(device->GetDescriptorHandleIncrementSize(heap_description.Type))
{
    HRESULT hr = S_OK;
    hr = device->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&m_descriptor_heap));
    if (!HRESULT_VALID(device, hr))
    {
        LOG_ERROR(L"Descriptor heap creation failed!");
        return;
    }
    m_descriptor_heap->SetName(name);
}

c_descriptor_heap::~c_descriptor_heap()
{
    SAFE_RELEASE(m_descriptor_heap);
}

bool c_descriptor_heap::allocate(dword* const out_index)
{
    if (!IN_RANGE_COUNT(m_allocated, 0, m_maximum_allocations))
    {
        LOG_ERROR(L"exceeded maximum [%d] allocations!", m_maximum_allocations);
        return K_FAILURE;
    }

    const dword index = m_allocated;
    m_allocated++;
    if (out_index != nullptr)
    {
        *out_index = index;
    }
    return K_SUCCESS;
}

const D3D12_CPU_DESCRIPTOR_HANDLE c_descriptor_heap::get_cpu_handle(const dword index) const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), index, m_size);
    return descriptor_handle;
}

const D3D12_GPU_DESCRIPTOR_HANDLE c_descriptor_heap::get_gpu_handle(const dword index) const
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE descriptor_handle(m_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), index, m_size);
    return descriptor_handle;
}