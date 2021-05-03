/*************************************************************************/
/*  crash_handler_windows.h                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef CRASH_HANDLER_WINDOWS_H
#define CRASH_HANDLER_WINDOWS_H

#include "thirdparty/crashpad/crashpad/client/crashpad_client.h"
#include "thirdparty/crashpad/crashpad/client/settings.h"
#include "thirdparty/crashpad/crashpad/third_party/mini_chromium/mini_chromium/base/files/file_path.h"

#include <windows.h>

// Crash handler exception only enabled with MSVC
#if defined(DEBUG_ENABLED) && defined(MSVC)
#define CRASH_HANDLER_EXCEPTION 1
extern DWORD CrashHandlerException(EXCEPTION_POINTERS *ep);
#endif

static crashpad::CrashpadClient client;
static bool disable_crash_reporter;

class String;

class CrashHandler {
	bool disabled = false;

	base::FilePath database;
	// Path to the out-of-process handler executable
	base::FilePath handler;

public:
	void initialize();
	void initialize_crashpad(String p_crashpad_handler_path, String p_crashpad_server);

	void disable();
	bool is_disabled() const { return disabled; };

	CrashHandler() {}
	~CrashHandler() {}
};

#endif // CRASH_HANDLER_WINDOWS_H
