/*************************************************************************/
/*  cacheserv_defines.h                                                  */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#ifndef CACHESERV_DEFINES_H
#define CACHESERV_DEFINES_H

#define CS_PAGE_SIZE 0x4000
#define CS_CACHE_SIZE (CS_PAGE_SIZE * 64)
#define CS_MEM_VAL_BAD ~0
#define CS_NUM_FRAMES ((CS_CACHE_SIZE) / (CS_PAGE_SIZE))
#define CS_FIFO_THRESH_DEFAULT 8
#define CS_LRU_THRESH_DEFAULT 8
#define CS_KEEP_THRESH_DEFAULT 8
#define CS_LEN_UNSPECIFIED 0xFADEFADEFADEFADE

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

// The number of bytes after the previous page offset for the offset a.
#define CS_PARTIAL_SIZE(a) ((a) % CS_PAGE_SIZE)

// Extract offset from GUID by masking the range.
#define CS_GET_FILE_OFFSET_FROM_GUID(guid) ((guid)&0x000000FFFFFFFFFF)
#define CS_GET_GUID_FROM_FILE_OFFSET(offset, guid_prefix) ((guid_prefix) | offset)

// Round off to the previous page offset.
#define CS_GET_PAGE(a) ((a)-CS_PARTIAL_SIZE(a))

#define CS_GET_CACHE_POLICY_FN(fns, policy) (this->*(fns[policy]))

#define CS_GET_LENGTH_IN_PAGES(length) (((length) / CS_PAGE_SIZE) + CS_PARTIAL_SIZE(length))

#define ERR_COND_MSG_ACTION(m_cond, m_message, m_action)                                                                                                     \
	{                                                                                                                                                        \
		if (unlikely(m_cond)) {                                                                                                                              \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, (String("Condition ' " _STR(m_cond) " ' is true. Message: ") + m_message).utf8().get_data()); \
			m_action                                                                                                                                         \
		}                                                                                                                                                    \
	}

#define ERR_COND_ACTION(m_cond, m_action)                                                                  \
	{                                                                                                      \
		if (unlikely(m_cond)) {                                                                            \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' " _STR(m_cond) " ' is true."); \
			m_action                                                                                       \
		}                                                                                                  \
	}

#ifndef ERR_FAIL_COND_MSG
#define ERR_FAIL_COND_MSG(m_cond, m_msg)                                                                   \
	{                                                                                                      \
		if (unlikely(m_cond)) {                                                                            \
			ERR_EXPLAIN(m_msg);                                                                            \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' " _STR(m_cond) " ' is true."); \
			return;                                                                                        \
		}                                                                                                  \
		_err_error_exists = false;                                                                         \
	}
#endif

#ifndef CRASH_COND_MSG
#define CRASH_COND_MSG(m_cond, m_msg)                                                                             \
	{                                                                                                             \
		if (unlikely(m_cond)) {                                                                                   \
			ERR_EXPLAIN(m_msg);                                                                                   \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "FATAL: Condition ' " _STR(m_cond) " ' is true."); \
			GENERATE_TRAP                                                                                         \
		}                                                                                                         \
	}
#endif

#ifndef ERR_FAIL_COND_V_MSG
#define ERR_FAIL_COND_V_MSG(m_cond, m_retval, m_msg)                                                                                 \
	{                                                                                                                                \
		if (unlikely(m_cond)) {                                                                                                      \
			ERR_EXPLAIN(m_msg);                                                                                                      \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' " _STR(m_cond) " ' is true. returned: " _STR(m_retval)); \
			return m_retval;                                                                                                         \
		}                                                                                                                            \
		_err_error_exists = false;                                                                                                   \
	}

#endif

#endif // CACHESERV_DEFINES_H
