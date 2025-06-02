#pragma once
#include <types.h>

// this will only call release if an object exists (prevents exceptions calling release on non existant objects)
#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

struct ID3D12Device5;
bool report_hresult(ID3D12Device5* device, dword result, const wchar_t* function_name, const wchar_t* file_path, dword file_line);
#define HRESULT_VALID(device, result) report_hresult(device, result, L"" __FUNCTION__, L"" __FILE__, __LINE__)