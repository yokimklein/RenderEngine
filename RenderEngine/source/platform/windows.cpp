#include "windows.h"
#include <cassert>
#include <stdio.h>
#include <reporting/report.h>
#include <game/game.h>
#include <backends/imgui_impl_win32.h>
#include <render/render.h>
#include <scene/scene.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    static bool right_mouse_down = false;
    switch (uMsg)
    {
        case WM_SIZE:
        {
            update_render_globals();
            if (g_scene != nullptr)
            {
                g_scene->m_camera->set_resolution(RENDER_GLOBALS.render_bounds);
            }
            break;
        }
        case WM_KEYDOWN:
            switch (wParam)
            {
                case 27:
                    //running = false;
                    PostQuitMessage(0);
                    break;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_RBUTTONDOWN:
            right_mouse_down = true;
            break;
        case WM_RBUTTONUP:
            right_mouse_down = false;
            break;
        case WM_MOUSEMOVE:
        {
            if (!right_mouse_down)
                break;

            // Get the dimensions of the window
            RECT rect;
            GetClientRect(hwnd, &rect);

            // Calculate the center position of the window
            POINT window_center;
            window_center.x = (rect.right - rect.left) / 2;
            window_center.y = (rect.bottom - rect.top) / 2;

            // Convert the client area point to screen coordinates
            ClientToScreen(hwnd, &window_center);

            // Get the current cursor position
            POINTS mouse_pos = MAKEPOINTS(lParam);
            POINT cursor_pos = { mouse_pos.x, mouse_pos.y };
            ClientToScreen(hwnd, &cursor_pos);

            // Calculate the delta from the window center
            POINT delta;
            delta.x = cursor_pos.x - window_center.x;
            delta.y = cursor_pos.y - window_center.y;

            // Update the camera with the delta
            // (You may need to convert POINT to POINTS or use the deltas as is)
            update_mouse(delta.x, delta.y);

            // Recenter the cursor
            SetCursorPos(window_center.x, window_center.y);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    // Create console
#ifdef _DEBUG
    FILE* f;
    AllocConsole();
    freopen_s(&f, "CONOUT$", "w", stdout);
#endif

    // Window creation
    WNDCLASS wnd = {};
    wnd.lpfnWndProc = WindowProc;
    wnd.hInstance = hInstance;
    wnd.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClass(&wnd))
        return E_FAIL;

    //AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowEx
    (
        0,                      // Default extended window style
        WINDOW_CLASS_NAME,      // Unique window class name
        WINDOW_TITLE,           // Window title
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // Window style
        CW_USEDEFAULT,          // Initial X position
        CW_USEDEFAULT,          // Initial Y position
        CW_USEDEFAULT,     // Window Width
        CW_USEDEFAULT,     // Window Height
        NULL,                   // Parent window
        NULL,                   // Menu / Child window identifier
        hInstance,              // Module instance
        NULL                    // lpParam
    );

    // Exit on failure
    if (hwnd == NULL)
    {
        LOG_ERROR(L"CreateWindowEx returned NULL HWND!");
        return E_FAIL;
    }

    ShowWindow(hwnd, SW_SHOWMAXIMIZED); // Force maximised window on launch, previously nCmdShow

    main_win((qword)hwnd);

    return S_OK;
}