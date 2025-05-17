#pragma once
#include <types.h>

enum e_log_level
{
	_log_none,

	// logs message only
	_log_message, // MESSAGE c_class::method(): log message
	// prints file info w/ message
	_log_warning, // WARNING c_class::method(): log warning
	// AT FILE c:\file\code.cpp ln156
	// prints file info & stops execution with error popup w/ message
	_log_error,   // -ERROR- c_class::method(): log error
	// AT FILE c:\file\code.cpp ln156
	k_log_level_count
};

// outputs normal formatted message
#define LOG_MESSAGE(message_format, ...) generate_report(_log_message, L"" __FUNCTION__, L"" __FILE__, __LINE__, L"" message_format, __VA_ARGS__)
// outputs formatted message with WARNING
#define LOG_WARNING(message_format, ...) generate_report(_log_warning, L"" __FUNCTION__, L"" __FILE__, __LINE__, L"" message_format, __VA_ARGS__)
// outputs formatted message with ERROR, pauses execution & displays message in a popup, stopping execution once closed
#define LOG_ERROR(message_format, ...) generate_report(_log_error, L"" __FUNCTION__, L"" __FILE__, __LINE__, L"" message_format, __VA_ARGS__)
void generate_report(e_log_level log_level, const wchar_t* function_name, const wchar_t* file_path, dword file_line, const wchar_t* message_format, ...);
void reporting_init();