/*  Sirikata Object Host -- Platform Dependent Definitions
 *  Platform.hpp
 *
 *  Copyright (c) 2009, Ewen Cheslack-Postava and Daniel Reiter Horn
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SIRIKATA_SUBSCRIPTION_PLATFORM_HPP_
#define _SIRIKATA_SUBSCRIPTION_PLATFORM_HPP_

#include <util/Platform.hpp>

#ifndef SIRIKATA_SUBSCRIPTION_EXPORT
# if SIRIKATA_PLATFORM == PLATFORM_WINDOWS
#   if defined(STATIC_LINKED)
#     define SIRIKATA_SUBSCRIPTION_EXPORT
#   else
#     if defined(SIRIKATA_SUBSCRIPTION_BUILD)
#       define SIRIKATA_SUBSCRIPTION_EXPORT __declspec(dllexport)
#     else
#       define SIRIKATA_SUBSCRIPTION_EXPORT __declspec(dllimport)
#     endif
#   endif
#   define SIRIKATA_SUBSCRIPTION_PLUGIN_EXPORT __declspec(dllexport)
# else
#   if defined(__GNUC__) && __GNUC__ >= 4
#     define SIRIKATA_SUBSCRIPTION_EXPORT __attribute__ ((visibility("default")))
#     define SIRIKATA_SUBSCRIPTION_PLUGIN_EXPORT __attribute__ ((visibility("default")))
#   else
#     define SIRIKATA_SUBSCRIPTION_EXPORT
#     define SIRIKATA_SUBSCRIPTION_PLUGIN_EXPORT
#   endif
# endif
#endif

#ifndef SIRIKATA_SUBSCRIPTION_EXPORT_C
# define SIRIKATA_SUBSCRIPTION_EXPORT_C extern "C" SIRIKATA_SUBSCRIPTION_EXPORT
#endif

#ifndef SIRIKATA_SUBSCRIPTION_PLUGIN_EXPORT_C
# define SIRIKATA_SUBSCRIPTION_PLUGIN_EXPORT_C extern "C" SIRIKATA_SUBSCRIPTION_PLUGIN_EXPORT
#endif

#endif //_SIRIKATA_SUBSCRIPTION_PLATFORM_HPP_
