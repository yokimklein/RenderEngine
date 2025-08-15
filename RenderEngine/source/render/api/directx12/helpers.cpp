#include "helpers.h"
#include <reporting/report.h>
#include <comdef.h>
#include <d3d12.h>

bool report_hresult(ID3D12Device5* device, dword result, const wchar_t* function_name, const wchar_t* file_path, dword file_line)
{
    if (FAILED(result))
    {
#ifdef _DEBUG

        if (result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DEVICE_REMOVED)
        {
            HRESULT removed_reason = device->GetDeviceRemovedReason();
            _com_error removed_error(removed_reason);
            LOG_WARNING("DEVICE HUNG OR REMOVED: 0x%08X\n%ls", removed_reason, removed_error.ErrorMessage());

            //ID3D12DeviceRemovedExtendedData1* dred;
            //HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&dred));
            //if (FAILED(hr))
            //{
            //    LOG_ERROR("Failed to query DRED! HRESULT: 0x%08X", hr);
            //    return K_FAILURE;
            //}
            //
            //D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 auto_breadcrumbs_output;
            //hr = dred->GetAutoBreadcrumbsOutput1(&auto_breadcrumbs_output);
            //if (FAILED(hr))
            //{
            //    LOG_ERROR("GetAutoBreadcrumbsOutput1 failed! HRESULT: 0x%08X", hr);
            //    return K_FAILURE;
            //}
            //
            //D3D12_DRED_PAGE_FAULT_OUTPUT page_fault_output;
            //hr = dred->GetPageFaultAllocationOutput(&page_fault_output);
            //if (FAILED(hr))
            //{
            //    LOG_ERROR("GetPageFaultAllocationOutput failed! HRESULT: 0x%08X", hr);
            //    return K_FAILURE;
            //}
        }

        _com_error err(result);
        generate_report(_log_error, function_name, file_path, file_line, L"BAD HRESULT: 0x%08X\n%ls", static_cast<ubyte>(result), err.ErrorMessage());

        assert(!FAILED(result));
#endif
        return K_FAILURE;
    }
    return K_SUCCESS;
}