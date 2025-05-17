#pragma once
#include <d3d12.h> // TODO: reduce reliance on this header by replacing handles with generic pointer types?
#include <types.h>

class c_descriptor_heap
{
public:
	c_descriptor_heap(ID3D12Device* const device, const wchar_t* name, const D3D12_DESCRIPTOR_HEAP_DESC heap_description);
	~c_descriptor_heap();

	bool allocate(dword* const out_index);
	const D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(const dword index = 0) const;
	const D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(const dword index = 0) const;
	ID3D12DescriptorHeap* const get_heap() const { return m_descriptor_heap; };
	inline const dword get_maximum_allocations() const { return m_maximum_allocations; };

private:
	const dword m_maximum_allocations;
	dword m_allocated;
	const dword m_size;
	ID3D12DescriptorHeap* m_descriptor_heap;
};