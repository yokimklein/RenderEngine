#include "report.h"
#include <windows.h>
#include <stdio.h>
#include <cassert>
#include <csignal>

constexpr dword REPORT_STRING_SIZE = 2048;

constexpr wchar_t g_log_level_titles[k_log_level_count][8] =
{
	L"INVALID",
	L"MESSAGE",
	L"WARNING",
	L"-ERROR-"
};

#ifdef PLATFORM_WINDOWS
constexpr uword g_log_level_colours[k_log_level_count] =
{
	15, // Invalid - White
	15, // Message - White
	14, // Warning - Yellow
	12  //  Error  - Red
};
#endif

// TODO: limit input string to 256 characters
// TODO: message formats corrupt after first variable argument
void generate_report(e_log_level log_level, const wchar_t* function_name, const wchar_t* file_path, dword file_line, const wchar_t* message_format, ...)
{
#if _DEBUG
	// TODO: print to screen w/ text rendering

	if (log_level > _log_none && log_level < k_log_level_count)
	{
		// Resolve message format
		wchar_t message_buffer[REPORT_STRING_SIZE] = {};
		va_list list;
		va_start(list, message_format);
		vswprintf_s(message_buffer, REPORT_STRING_SIZE, message_format, list);
		va_end(list);

		wchar_t debug_output_buffer[REPORT_STRING_SIZE] = {};

		if (log_level == _log_error)
		{
			swprintf_s
			(
				debug_output_buffer,
				REPORT_STRING_SIZE,
				L"%ls\r\n%s()\r\n\r\n%ls\r\n\r\nFILE %ls LN %u\r\n-------\r\n",
				g_log_level_titles[log_level],
				function_name,
				message_buffer,
				file_path,
				file_line
			);
		}
		else
		{
			if (log_level > _log_message)
			{
				swprintf_s(debug_output_buffer, REPORT_STRING_SIZE, L"%ls: ", g_log_level_titles[log_level]);
			}
			swprintf_s
			(
				debug_output_buffer,
				REPORT_STRING_SIZE,
				L"%ls%ls() - %ls\r\n",
				debug_output_buffer,
				function_name,
				message_buffer
			);
		}

		// Null terminate string
		debug_output_buffer[REPORT_STRING_SIZE - 1] = 0;

#ifdef PLATFORM_WINDOWS
		// Output to debug output
		OutputDebugStringW(debug_output_buffer);

		// Output to console with log level colour
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), g_log_level_colours[log_level]);
		wprintf(debug_output_buffer);
#endif
		// Display popup on fatal error and terminate
		if (log_level == _log_error)
		{
#ifdef PLATFORM_WINDOWS
			// Set console to appear above game window to display error log
			SetForegroundWindow(GetConsoleWindow());
			// Flash console
			FLASHWINFO fi;
			fi.cbSize = sizeof(FLASHWINFO);
			fi.hwnd = GetConsoleWindow();
			fi.dwFlags = FLASHW_ALL;
			fi.uCount = 30;
			fi.dwTimeout = 0;
			FlashWindowEx(&fi);
			// Display error message
			MessageBoxW(NULL, debug_output_buffer, g_log_level_titles[_log_error], MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_TASKMODAL);
#endif
			__debugbreak();
			exit(EXIT_FAILURE);
		}
	}
#endif
}

//#ifdef PLATFORM_WINDOWS
//LONG NTAPI VexHandler(PEXCEPTION_POINTERS ExceptionInfo)
//{
//	PEXCEPTION_RECORD ExceptionRecord = ExceptionInfo->ExceptionRecord;
//
//	switch (ExceptionRecord->ExceptionCode)
//	{
//	case DBG_PRINTEXCEPTION_WIDE_C:
//	case DBG_PRINTEXCEPTION_C:
//
//		if (ExceptionRecord->NumberParameters >= 2)
//		{
//			ULONG len = (ULONG)ExceptionRecord->ExceptionInformation[0];
//
//			union {
//				ULONG_PTR up;
//				PCWSTR pwz;
//				PCSTR psz;
//			};
//
//			up = ExceptionRecord->ExceptionInformation[1];
//
//			HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
//
//			if (ExceptionRecord->ExceptionCode == DBG_PRINTEXCEPTION_C)
//			{
//				// localized text will be incorrect displayed, if used not CP_OEMCP encoding 
//				// WriteConsoleA(hOut, psz, len, &len, 0);
//
//				// assume CP_ACP encoding
//				if (ULONG n = MultiByteToWideChar(CP_ACP, 0, psz, len, 0, 0))
//				{
//					PWSTR wz = (PWSTR)alloca(n * sizeof(WCHAR));
//
//					if (len = MultiByteToWideChar(CP_ACP, 0, psz, len, wz, n))
//					{
//						pwz = wz;
//					}
//				}
//			}
//
//			if (len)
//			{
//				WriteConsoleW(hOut, pwz, len - 1, &len, 0);
//			}
//
//		}
//		return EXCEPTION_CONTINUE_EXECUTION;
//	}
//
//	return EXCEPTION_CONTINUE_SEARCH;
//}
//#endif

void reporting_init()
{
//#ifdef PLATFORM_WINDOWS
//	AddVectoredExceptionHandler(TRUE, VexHandler);
//#endif
}