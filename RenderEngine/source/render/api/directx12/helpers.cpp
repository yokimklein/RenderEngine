#include "helpers.h"
#include <reporting/report.h>
#include <comdef.h>

bool report_hresult(dword result, const wchar_t* function_name, const wchar_t* file_path, dword file_line)
{
    if (FAILED(result))
    {
#ifdef _DEBUG
        _com_error err(result);
        generate_report(_log_error, function_name, file_path, file_line, L"BAD HRESULT: 0x%08X\n%ls", static_cast<ubyte>(result), err.ErrorMessage());
        assert(!FAILED(result));
#endif
        return K_FAILURE;
    }
    return K_SUCCESS;
}