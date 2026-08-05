/*
 * Windows App Runtime bootstrap tests
 *
 * Copyright 2026 Veedy
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "appmodel.h"

#include "wine/test.h"

static void test_initialize(void)
{
    HRESULT (WINAPI *initialize)(UINT32, const WCHAR *, PACKAGE_VERSION);
    HRESULT (WINAPI *initialize2)(UINT32, const WCHAR *, PACKAGE_VERSION, UINT32);
    void (WINAPI *shutdown)(void);
    PACKAGE_VERSION minimum_version = {{0}};
    HMODULE module;
    HRESULT hr;

    module = LoadLibraryW(L"microsoft.windowsappruntime.bootstrap.dll");
    ok(module != NULL, "Failed to load bootstrap DLL, error %lu.\n", GetLastError());
    if (!module) return;

    initialize = (void *)GetProcAddress(module, "MddBootstrapInitialize");
    initialize2 = (void *)GetProcAddress(module, "MddBootstrapInitialize2");
    shutdown = (void *)GetProcAddress(module, "MddBootstrapShutdown");
    ok(initialize != NULL, "MddBootstrapInitialize is missing.\n");
    ok(initialize2 != NULL, "MddBootstrapInitialize2 is missing.\n");
    ok(shutdown != NULL, "MddBootstrapShutdown is missing.\n");

    if (initialize)
    {
        hr = initialize(0x00010008, NULL, minimum_version);
        ok(hr == S_OK, "MddBootstrapInitialize returned %#lx.\n", hr);
    }
    if (initialize2)
    {
        hr = initialize2(0x00010008, NULL, minimum_version, 0);
        ok(hr == S_OK, "MddBootstrapInitialize2 returned %#lx.\n", hr);
    }
    if (shutdown) shutdown();
    FreeLibrary(module);
}

START_TEST(bootstrap)
{
    test_initialize();
}
