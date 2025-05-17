#include "constants.h"
#include <reporting/report.h>
#include <stdlib.h>
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif

const char* const get_gbuffer_name(const e_gbuffers buffer_type)
{
    constexpr const char* k_gbuffer_names[] =
    {
        "Albedo",
        "Specular Material",
        "Normals",
        "Position",
        "Emissive Material",
        "Ambient Material",
        "Diffuse Material",
        "Diffuse Lighting",
        "Specular Lighting",
        "Ambient Lighting",
        "Depth"
    };
    static_assert(_countof(k_gbuffer_names) == k_gbuffer_count + k_light_buffer_count + 1);

    if (!IN_RANGE_COUNT(buffer_type, _gbuffer_albedo, k_gbuffer_count + k_light_buffer_count + 1))
    {
        LOG_WARNING(L"tried to get name for invalid gbuffer type [%d]!", buffer_type);
        return "Invalid GBuffer";
    }

    return k_gbuffer_names[buffer_type];
}

s_render_globals RENDER_GLOBALS;
void init_render_globals(qword hwnd)
{
#ifdef PLATFORM_WINDOWS
    RECT rect;
    GetClientRect((HWND)hwnd, &rect);

    RENDER_GLOBALS.hwnd = hwnd;
    RENDER_GLOBALS.render_bounds.width = rect.right - rect.left;
    RENDER_GLOBALS.render_bounds.height = rect.bottom - rect.top;
    RENDER_GLOBALS.window_pos.x = static_cast<float>(rect.left);
    RENDER_GLOBALS.window_pos.y = static_cast<float>(rect.top);
#else
#error RENDER BOUNDS NOT SET FOR PLATFORM!
#endif
}

void update_render_globals()
{
#ifdef PLATFORM_WINDOWS
    RECT rect;
    GetClientRect((HWND)RENDER_GLOBALS.hwnd, &rect);

    RENDER_GLOBALS.render_bounds.width = rect.right - rect.left;
    RENDER_GLOBALS.render_bounds.height = rect.bottom - rect.top;
    RENDER_GLOBALS.window_pos.x = static_cast<float>(rect.left);
    RENDER_GLOBALS.window_pos.y = static_cast<float>(rect.top);
#else
#error RENDER BOUNDS NOT SET FOR PLATFORM!
#endif
}