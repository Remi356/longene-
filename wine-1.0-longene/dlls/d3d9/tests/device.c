/*
 * Copyright (C) 2006 Vitaliy Margolen
 * Copyright (C) 2006 Chris Robinson
 * Copyright (C) 2006-2007 Stefan D?singer(For CodeWeavers)
 * Copyright 2007 Henri Verbeet
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

#define COBJMACROS
#include <d3d9.h>
#include <dxerr9.h>
#include "wine/test.h"

static IDirect3D9 *(WINAPI *pDirect3DCreate9)(UINT);

static int get_refcount(IUnknown *object)
{
    IUnknown_AddRef( object );
    return IUnknown_Release( object );
}

#define CHECK_CALL(r,c,d,rc) \
    if (SUCCEEDED(r)) {\
        int tmp1 = get_refcount( (IUnknown *)d ); \
        int rc_new = rc; \
        ok(tmp1 == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, tmp1); \
    } else {\
        trace("%s failed: %s\n", c, DXGetErrorString9(r)); \
    }

#define CHECK_RELEASE(obj,d,rc) \
    if (obj) { \
        int tmp1, rc_new = rc; \
        IUnknown_Release( obj ); \
        tmp1 = get_refcount( (IUnknown *)d ); \
        ok(tmp1 == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, tmp1); \
    }

#define CHECK_REFCOUNT(obj,rc) \
    { \
        int rc_new = rc; \
        int count = get_refcount( (IUnknown *)obj ); \
        ok(count == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, count); \
    }

#define CHECK_RELEASE_REFCOUNT(obj,rc) \
    { \
        int rc_new = rc; \
        int count = IUnknown_Release( (IUnknown *)obj ); \
        ok(count == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, count); \
    }

#define CHECK_ADDREF_REFCOUNT(obj,rc) \
    { \
        int rc_new = rc; \
        int count = IUnknown_AddRef( (IUnknown *)obj ); \
        ok(count == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, count); \
    }

#define CHECK_SURFACE_CONTAINER(obj,iid,expected) \
    { \
        void *container_ptr = (void *)0x1337c0d3; \
        hr = IDirect3DSurface9_GetContainer(obj, &iid, &container_ptr); \
        ok(SUCCEEDED(hr) && container_ptr == expected, "GetContainer returned: hr %#x, container_ptr %p. " \
            "Expected hr %#x, container_ptr %p\n", hr, container_ptr, S_OK, expected); \
        if (container_ptr && container_ptr != (void *)0x1337c0d3) IUnknown_Release((IUnknown *)container_ptr); \
    }

static void check_mipmap_levels(
    IDirect3DDevice9* device, 
    int width, int height, int count) 
{

    IDirect3DBaseTexture9* texture = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture( device, width, height, 0, 0, 
        D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, (IDirect3DTexture9**) &texture, NULL );
       
    if (SUCCEEDED(hr)) {
        DWORD levels = IDirect3DBaseTexture9_GetLevelCount(texture);
        ok(levels == count, "Invalid level count. Expected %d got %u\n", count, levels);
    } else 
        trace("CreateTexture failed: %s\n", DXGetErrorString9(hr));

    if (texture) IUnknown_Release( texture );
}

static void test_mipmap_levels(void)
{

    HRESULT               hr;
    HWND                  hwnd = NULL;

    IDirect3D9            *pD3d = NULL;
    IDirect3DDevice9      *pDevice = NULL;
    D3DPRESENT_PARAMETERS d3dpp;
    D3DDISPLAYMODE        d3ddm;
 
    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(SUCCEEDED(hr), "Failed to create IDirect3D9Device (%s)\n", DXGetErrorString9(hr));
    if (FAILED(hr)) goto cleanup;

    check_mipmap_levels(pDevice, 32, 32, 6);
    check_mipmap_levels(pDevice, 256, 1, 9);
    check_mipmap_levels(pDevice, 1, 256, 9);
    check_mipmap_levels(pDevice, 1, 1, 1);

    cleanup:
    if (pD3d)     IUnknown_Release( pD3d );
    if (pDevice)  IUnknown_Release( pDevice );
    DestroyWindow( hwnd );
}

static void test_swapchain(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D9                  *pD3d               = NULL;
    IDirect3DDevice9            *pDevice            = NULL;
    IDirect3DSwapChain9         *swapchain0         = NULL;
    IDirect3DSwapChain9         *swapchain1         = NULL;
    IDirect3DSwapChain9         *swapchain2         = NULL;
    IDirect3DSwapChain9         *swapchain3         = NULL;
    IDirect3DSwapChain9         *swapchainX         = NULL;
    IDirect3DSurface9           *backbuffer         = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;

    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferCount  = 0;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == S_OK || hr == D3DERR_NOTAVAILABLE,
       "Failed to create IDirect3D9Device (%s)\n", DXGetErrorString9(hr));
    if (FAILED(hr)) goto cleanup;

    /* Check if the back buffer count was modified */
    ok(d3dpp.BackBufferCount == 1, "The back buffer count in the presentparams struct is %d\n", d3dpp.BackBufferCount);

    /* Get the implicit swapchain */
    hr = IDirect3DDevice9_GetSwapChain(pDevice, 0, &swapchain0);
    ok(SUCCEEDED(hr), "Failed to get the impicit swapchain (%s)\n", DXGetErrorString9(hr));
    if(swapchain0) IDirect3DSwapChain9_Release(swapchain0);

    /* Check if there is a back buffer */
    hr = IDirect3DSwapChain9_GetBackBuffer(swapchain0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    ok(SUCCEEDED(hr), "Failed to get the back buffer (%s)\n", DXGetErrorString9(hr));
    ok(backbuffer != NULL, "The back buffer is NULL\n");
    if(backbuffer) IDirect3DSurface9_Release(backbuffer);

    /* Try to get a nonexistent swapchain */
    hr = IDirect3DDevice9_GetSwapChain(pDevice, 1, &swapchainX);
    ok(hr == D3DERR_INVALIDCALL, "GetSwapChain on an nonexistent swapchain returned (%s)\n", DXGetErrorString9(hr));
    ok(swapchainX == NULL, "Swapchain 1 is %p\n", swapchainX);
    if(swapchainX) IDirect3DSwapChain9_Release(swapchainX);

    /* Create a bunch of swapchains */
    d3dpp.BackBufferCount = 0;
    hr = IDirect3DDevice9_CreateAdditionalSwapChain(pDevice, &d3dpp, &swapchain1);
    ok(SUCCEEDED(hr), "Failed to create a swapchain (%s)\n", DXGetErrorString9(hr));
    ok(d3dpp.BackBufferCount == 1, "The back buffer count in the presentparams struct is %d\n", d3dpp.BackBufferCount);

    d3dpp.BackBufferCount  = 1;
    hr = IDirect3DDevice9_CreateAdditionalSwapChain(pDevice, &d3dpp, &swapchain2);
    ok(SUCCEEDED(hr), "Failed to create a swapchain (%s)\n", DXGetErrorString9(hr));

    d3dpp.BackBufferCount  = 2;
    hr = IDirect3DDevice9_CreateAdditionalSwapChain(pDevice, &d3dpp, &swapchain3);
    ok(SUCCEEDED(hr), "Failed to create a swapchain (%s)\n", DXGetErrorString9(hr));
    if(SUCCEEDED(hr)) {
        /* Swapchain 3, created with backbuffercount 2 */
        backbuffer = (void *) 0xdeadbeef;
        hr = IDirect3DSwapChain9_GetBackBuffer(swapchain3, 0, 0, &backbuffer);
        ok(SUCCEEDED(hr), "Failed to get the 1st back buffer (%s)\n", DXGetErrorString9(hr));
        ok(backbuffer != NULL && backbuffer != (void *) 0xdeadbeef, "The back buffer is %p\n", backbuffer);
        if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface9_Release(backbuffer);

        backbuffer = (void *) 0xdeadbeef;
        hr = IDirect3DSwapChain9_GetBackBuffer(swapchain3, 1, 0, &backbuffer);
        ok(SUCCEEDED(hr), "Failed to get the 2nd back buffer (%s)\n", DXGetErrorString9(hr));
        ok(backbuffer != NULL && backbuffer != (void *) 0xdeadbeef, "The back buffer is %p\n", backbuffer);
        if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface9_Release(backbuffer);

        backbuffer = (void *) 0xdeadbeef;
        hr = IDirect3DSwapChain9_GetBackBuffer(swapchain3, 2, 0, &backbuffer);
        ok(hr == D3DERR_INVALIDCALL, "GetBackBuffer returned %s\n", DXGetErrorString9(hr));
        ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
        if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface9_Release(backbuffer);

        backbuffer = (void *) 0xdeadbeef;
        hr = IDirect3DSwapChain9_GetBackBuffer(swapchain3, 3, 0, &backbuffer);
        ok(FAILED(hr), "Failed to get the back buffer (%s)\n", DXGetErrorString9(hr));
        ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
        if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface9_Release(backbuffer);
    }

    /* Check the back buffers of the swapchains */
    /* Swapchain 1, created with backbuffercount 0 */
    hr = IDirect3DSwapChain9_GetBackBuffer(swapchain1, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    ok(SUCCEEDED(hr), "Failed to get the back buffer (%s)\n", DXGetErrorString9(hr));
    ok(backbuffer != NULL, "The back buffer is NULL (%s)\n", DXGetErrorString9(hr));
    if(backbuffer) IDirect3DSurface9_Release(backbuffer);

    backbuffer = (void *) 0xdeadbeef;
    hr = IDirect3DSwapChain9_GetBackBuffer(swapchain1, 1, 0, &backbuffer);
    ok(FAILED(hr), "Failed to get the back buffer (%s)\n", DXGetErrorString9(hr));
    ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
    if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface9_Release(backbuffer);

    /* Swapchain 2 - created with backbuffercount 1 */
    backbuffer = (void *) 0xdeadbeef;
    hr = IDirect3DSwapChain9_GetBackBuffer(swapchain2, 0, 0, &backbuffer);
    ok(SUCCEEDED(hr), "Failed to get the back buffer (%s)\n", DXGetErrorString9(hr));
    ok(backbuffer != NULL && backbuffer != (void *) 0xdeadbeef, "The back buffer is %p\n", backbuffer);
    if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface9_Release(backbuffer);

    backbuffer = (void *) 0xdeadbeef;
    hr = IDirect3DSwapChain9_GetBackBuffer(swapchain2, 1, 0, &backbuffer);
    ok(hr == D3DERR_INVALIDCALL, "GetBackBuffer returned %s\n", DXGetErrorString9(hr));
    ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
    if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface9_Release(backbuffer);

    backbuffer = (void *) 0xdeadbeef;
    hr = IDirect3DSwapChain9_GetBackBuffer(swapchain2, 2, 0, &backbuffer);
    ok(FAILED(hr), "Failed to get the back buffer (%s)\n", DXGetErrorString9(hr));
    ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
    if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface9_Release(backbuffer);

    /* Try getSwapChain on a manually created swapchain
     * it should fail, apparently GetSwapChain only returns implicit swapchains
     */
    swapchainX = (void *) 0xdeadbeef;
    hr = IDirect3DDevice9_GetSwapChain(pDevice, 1, &swapchainX);
    ok(hr == D3DERR_INVALIDCALL, "Failed to get the second swapchain (%s)\n", DXGetErrorString9(hr));
    ok(swapchainX == NULL, "The swapchain pointer is %p\n", swapchainX);
    if(swapchainX && swapchainX != (void *) 0xdeadbeef ) IDirect3DSwapChain9_Release(swapchainX);

    cleanup:
    if(swapchain1) IDirect3DSwapChain9_Release(swapchain1);
    if(swapchain2) IDirect3DSwapChain9_Release(swapchain2);
    if(swapchain3) IDirect3DSwapChain9_Release(swapchain3);
    if(pDevice) IDirect3DDevice9_Release(pDevice);
    if(pD3d) IDirect3DDevice9_Release(pD3d);
    DestroyWindow( hwnd );
}

/* Shared between two functions */
static const DWORD simple_vs[] = {0xFFFE0101,       /* vs_1_1               */
    0x0000001F, 0x80000000, 0x900F0000,             /* dcl_position0 v0     */
    0x00000009, 0xC0010000, 0x90E40000, 0xA0E40000, /* dp4 oPos.x, v0, c0   */
    0x00000009, 0xC0020000, 0x90E40000, 0xA0E40001, /* dp4 oPos.y, v0, c1   */
    0x00000009, 0xC0040000, 0x90E40000, 0xA0E40002, /* dp4 oPos.z, v0, c2   */
    0x00000009, 0xC0080000, 0x90E40000, 0xA0E40003, /* dp4 oPos.w, v0, c3   */
    0x0000FFFF};                                    /* END                  */

static void test_refcount(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D9                  *pD3d               = NULL;
    IDirect3DDevice9            *pDevice            = NULL;
    IDirect3DVertexBuffer9      *pVertexBuffer      = NULL;
    IDirect3DIndexBuffer9       *pIndexBuffer       = NULL;
    IDirect3DVertexDeclaration9 *pVertexDeclaration = NULL;
    IDirect3DVertexShader9      *pVertexShader      = NULL;
    IDirect3DPixelShader9       *pPixelShader       = NULL;
    IDirect3DCubeTexture9       *pCubeTexture       = NULL;
    IDirect3DTexture9           *pTexture           = NULL;
    IDirect3DVolumeTexture9     *pVolumeTexture     = NULL;
    IDirect3DVolume9            *pVolumeLevel       = NULL;
    IDirect3DSurface9           *pStencilSurface    = NULL;
    IDirect3DSurface9           *pOffscreenSurface  = NULL;
    IDirect3DSurface9           *pRenderTarget      = NULL;
    IDirect3DSurface9           *pRenderTarget2     = NULL;
    IDirect3DSurface9           *pRenderTarget3     = NULL;
    IDirect3DSurface9           *pTextureLevel      = NULL;
    IDirect3DSurface9           *pBackBuffer        = NULL;
    IDirect3DStateBlock9        *pStateBlock        = NULL;
    IDirect3DStateBlock9        *pStateBlock1       = NULL;
    IDirect3DSwapChain9         *pSwapChain         = NULL;
    IDirect3DQuery9             *pQuery             = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    int                          refcount = 0, tmp;

    D3DVERTEXELEMENT9 decl[] =
    {
	D3DDECL_END()
    };
    static DWORD simple_ps[] = {0xFFFF0101,                                     /* ps_1_1                       */
        0x00000051, 0xA00F0001, 0x3F800000, 0x00000000, 0x00000000, 0x00000000, /* def c1 = 1.0, 0.0, 0.0, 0.0  */
        0x00000042, 0xB00F0000,                                                 /* tex t0                       */
        0x00000008, 0x800F0000, 0xA0E40001, 0xA0E40000,                         /* dp3 r0, c1, c0               */
        0x00000005, 0x800F0000, 0x90E40000, 0x80E40000,                         /* mul r0, v0, r0               */
        0x00000005, 0x800F0000, 0xB0E40000, 0x80E40000,                         /* mul r0, t0, r0               */
        0x0000FFFF};                                                            /* END                          */


    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == S_OK || hr == D3DERR_NOTAVAILABLE,
       "Failed to create IDirect3D9Device (%s)\n", DXGetErrorString9(hr));
    if (FAILED(hr)) goto cleanup;

    refcount = get_refcount( (IUnknown *)pDevice );
    ok(refcount == 1, "Invalid device RefCount %d\n", refcount);

    /**
     * Check refcount of implicit surfaces and implicit swapchain. Findings:
     *   - the container is the device OR swapchain
     *   - they hold a reference to the device
     *   - they are created with a refcount of 0 (Get/Release returns original refcount)
     *   - they are not freed if refcount reaches 0.
     *   - the refcount is not forwarded to the container.
     */
    hr = IDirect3DDevice9_GetSwapChain(pDevice, 0, &pSwapChain);
    CHECK_CALL( hr, "GetSwapChain", pDevice, ++refcount);
    if (pSwapChain)
    {
        CHECK_REFCOUNT( pSwapChain, 1);

        hr = IDirect3DDevice9_GetRenderTarget(pDevice, 0, &pRenderTarget);
        CHECK_CALL( hr, "GetRenderTarget", pDevice, ++refcount);
        CHECK_REFCOUNT( pSwapChain, 1);
        if(pRenderTarget)
        {
            CHECK_SURFACE_CONTAINER( pRenderTarget, IID_IDirect3DSwapChain9, pSwapChain);
            CHECK_REFCOUNT( pRenderTarget, 1);

            CHECK_ADDREF_REFCOUNT(pRenderTarget, 2);
            CHECK_REFCOUNT(pDevice, refcount);
            CHECK_RELEASE_REFCOUNT(pRenderTarget, 1);
            CHECK_REFCOUNT(pDevice, refcount);

            hr = IDirect3DDevice9_GetRenderTarget(pDevice, 0, &pRenderTarget);
            CHECK_CALL( hr, "GetRenderTarget", pDevice, refcount);
            CHECK_REFCOUNT( pRenderTarget, 2);
            CHECK_RELEASE_REFCOUNT( pRenderTarget, 1);
            CHECK_RELEASE_REFCOUNT( pRenderTarget, 0);
            CHECK_REFCOUNT( pDevice, --refcount);

            /* The render target is released with the device, so AddRef with refcount=0 is fine here. */
            CHECK_ADDREF_REFCOUNT(pRenderTarget, 1);
            CHECK_REFCOUNT(pDevice, ++refcount);
            CHECK_RELEASE_REFCOUNT(pRenderTarget, 0);
            CHECK_REFCOUNT(pDevice, --refcount);
        }

        /* Render target and back buffer are identical. */
        hr = IDirect3DDevice9_GetBackBuffer(pDevice, 0, 0, 0, &pBackBuffer);
        CHECK_CALL( hr, "GetBackBuffer", pDevice, ++refcount);
        if(pBackBuffer)
        {
            CHECK_RELEASE_REFCOUNT(pBackBuffer, 0);
            ok(pRenderTarget == pBackBuffer, "RenderTarget=%p and BackBuffer=%p should be the same.\n",
            pRenderTarget, pBackBuffer);
            pBackBuffer = NULL;
        }
        CHECK_REFCOUNT( pDevice, --refcount);

        hr = IDirect3DDevice9_GetDepthStencilSurface(pDevice, &pStencilSurface);
        CHECK_CALL( hr, "GetDepthStencilSurface", pDevice, ++refcount);
        CHECK_REFCOUNT( pSwapChain, 1);
        if(pStencilSurface)
        {
            CHECK_SURFACE_CONTAINER( pStencilSurface, IID_IDirect3DDevice9, pDevice);
            CHECK_REFCOUNT( pStencilSurface, 1);

            CHECK_ADDREF_REFCOUNT(pStencilSurface, 2);
            CHECK_REFCOUNT(pDevice, refcount);
            CHECK_RELEASE_REFCOUNT(pStencilSurface, 1);
            CHECK_REFCOUNT(pDevice, refcount);

            CHECK_RELEASE_REFCOUNT( pStencilSurface, 0);
            CHECK_REFCOUNT( pDevice, --refcount);

            /* The stencil surface is released with the device, so AddRef with refcount=0 is fine here. */
            CHECK_ADDREF_REFCOUNT(pStencilSurface, 1);
            CHECK_REFCOUNT(pDevice, ++refcount);
            CHECK_RELEASE_REFCOUNT(pStencilSurface, 0);
            CHECK_REFCOUNT(pDevice, --refcount);
            pStencilSurface = NULL;
        }

        CHECK_RELEASE_REFCOUNT( pSwapChain, 0);
        CHECK_REFCOUNT( pDevice, --refcount);

        /* The implicit swapchwin is released with the device, so AddRef with refcount=0 is fine here. */
        CHECK_ADDREF_REFCOUNT(pSwapChain, 1);
        CHECK_REFCOUNT(pDevice, ++refcount);
        CHECK_RELEASE_REFCOUNT(pSwapChain, 0);
        CHECK_REFCOUNT(pDevice, --refcount);
        pSwapChain = NULL;
    }

    /* Buffers */
    hr = IDirect3DDevice9_CreateIndexBuffer( pDevice, 16, 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &pIndexBuffer, NULL );
    CHECK_CALL( hr, "CreateIndexBuffer", pDevice, ++refcount );
    if(pIndexBuffer)
    {
        tmp = get_refcount( (IUnknown *)pIndexBuffer );

        hr = IDirect3DDevice9_SetIndices(pDevice, pIndexBuffer);
        CHECK_CALL( hr, "SetIndices", pIndexBuffer, tmp);
        hr = IDirect3DDevice9_SetIndices(pDevice, NULL);
        CHECK_CALL( hr, "SetIndices", pIndexBuffer, tmp);
    }

    hr = IDirect3DDevice9_CreateVertexBuffer( pDevice, 16, 0, D3DFVF_XYZ, D3DPOOL_DEFAULT, &pVertexBuffer, NULL );
    CHECK_CALL( hr, "CreateVertexBuffer", pDevice, ++refcount );
    if(pVertexBuffer)
    {
        IDirect3DVertexBuffer9 *pVBuf = (void*)~0;
        UINT offset = ~0;
        UINT stride = ~0;

        tmp = get_refcount( (IUnknown *)pVertexBuffer );

        hr = IDirect3DDevice9_SetStreamSource(pDevice, 0, pVertexBuffer, 0, 3 * sizeof(float));
        CHECK_CALL( hr, "SetStreamSource", pVertexBuffer, tmp);
        hr = IDirect3DDevice9_SetStreamSource(pDevice, 0, NULL, 0, 0);
        CHECK_CALL( hr, "SetStreamSource", pVertexBuffer, tmp);

        hr = IDirect3DDevice9_GetStreamSource(pDevice, 0, &pVBuf, &offset, &stride);
        ok(SUCCEEDED(hr), "GetStreamSource did not succeed with NULL stream!\n");
        ok(pVBuf==NULL, "pVBuf not NULL (%p)!\n", pVBuf);
        ok(stride==3*sizeof(float), "stride not 3 floats (got %u)!\n", stride);
        ok(offset==0, "offset not 0 (got %u)!\n", offset);
    }
    /* Shaders */
    hr = IDirect3DDevice9_CreateVertexDeclaration( pDevice, decl, &pVertexDeclaration );
    CHECK_CALL( hr, "CreateVertexDeclaration", pDevice, ++refcount );
    hr = IDirect3DDevice9_CreateVertexShader( pDevice, simple_vs, &pVertexShader );
    CHECK_CALL( hr, "CreateVertexShader", pDevice, ++refcount );
    hr = IDirect3DDevice9_CreatePixelShader( pDevice, simple_ps, &pPixelShader );
    CHECK_CALL( hr, "CreatePixelShader", pDevice, ++refcount );
    /* Textures */
    hr = IDirect3DDevice9_CreateTexture( pDevice, 32, 32, 3, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pTexture, NULL );
    CHECK_CALL( hr, "CreateTexture", pDevice, ++refcount );
    if (pTexture)
    {
        tmp = get_refcount( (IUnknown *)pTexture );

        /* SetTexture should not increase refcounts */
        hr = IDirect3DDevice9_SetTexture(pDevice, 0, (IDirect3DBaseTexture9 *) pTexture);
        CHECK_CALL( hr, "SetTexture", pTexture, tmp);
        hr = IDirect3DDevice9_SetTexture(pDevice, 0, NULL);
        CHECK_CALL( hr, "SetTexture", pTexture, tmp);

        /* This should not increment device refcount */
        hr = IDirect3DTexture9_GetSurfaceLevel( pTexture, 1, &pTextureLevel );
        CHECK_CALL( hr, "GetSurfaceLevel", pDevice, refcount );
        /* But should increment texture's refcount */
        CHECK_REFCOUNT( pTexture, tmp+1 );
        /* Because the texture and surface refcount are identical */
        if (pTextureLevel)
        {
            CHECK_REFCOUNT        ( pTextureLevel, tmp+1 );
            CHECK_ADDREF_REFCOUNT ( pTextureLevel, tmp+2 );
            CHECK_REFCOUNT        ( pTexture     , tmp+2 );
            CHECK_RELEASE_REFCOUNT( pTextureLevel, tmp+1 );
            CHECK_REFCOUNT        ( pTexture     , tmp+1 );
            CHECK_RELEASE_REFCOUNT( pTexture     , tmp   );
            CHECK_REFCOUNT        ( pTextureLevel, tmp   );
        }
    }
    hr = IDirect3DDevice9_CreateCubeTexture( pDevice, 32, 0, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pCubeTexture, NULL );
    CHECK_CALL( hr, "CreateCubeTexture", pDevice, ++refcount );
    hr = IDirect3DDevice9_CreateVolumeTexture( pDevice, 32, 32, 2, 0, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pVolumeTexture, NULL );
    CHECK_CALL( hr, "CreateVolumeTexture", pDevice, ++refcount );
    if (pVolumeTexture)
    {
        tmp = get_refcount( (IUnknown *)pVolumeTexture );

        /* This should not increment device refcount */
        hr = IDirect3DVolumeTexture9_GetVolumeLevel(pVolumeTexture, 0, &pVolumeLevel);
        CHECK_CALL( hr, "GetVolumeLevel", pDevice, refcount );
        /* But should increment volume texture's refcount */
        CHECK_REFCOUNT( pVolumeTexture, tmp+1 );
        /* Because the volume texture and volume refcount are identical */
        if (pVolumeLevel)
        {
            CHECK_REFCOUNT        ( pVolumeLevel  , tmp+1 );
            CHECK_ADDREF_REFCOUNT ( pVolumeLevel  , tmp+2 );
            CHECK_REFCOUNT        ( pVolumeTexture, tmp+2 );
            CHECK_RELEASE_REFCOUNT( pVolumeLevel  , tmp+1 );
            CHECK_REFCOUNT        ( pVolumeTexture, tmp+1 );
            CHECK_RELEASE_REFCOUNT( pVolumeTexture, tmp   );
            CHECK_REFCOUNT        ( pVolumeLevel  , tmp   );
        }
    }
    /* Surfaces */
    hr = IDirect3DDevice9_CreateDepthStencilSurface( pDevice, 32, 32, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &pStencilSurface, NULL );
    CHECK_CALL( hr, "CreateDepthStencilSurface", pDevice, ++refcount );
    CHECK_REFCOUNT( pStencilSurface, 1 );
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface( pDevice, 32, 32, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pOffscreenSurface, NULL );
    CHECK_CALL( hr, "CreateOffscreenPlainSurface", pDevice, ++refcount );
    CHECK_REFCOUNT( pOffscreenSurface, 1 );
    hr = IDirect3DDevice9_CreateRenderTarget( pDevice, 32, 32, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pRenderTarget3, NULL );
    CHECK_CALL( hr, "CreateRenderTarget", pDevice, ++refcount );
    CHECK_REFCOUNT( pRenderTarget3, 1 );
    /* Misc */
    hr = IDirect3DDevice9_CreateStateBlock( pDevice, D3DSBT_ALL, &pStateBlock );
    CHECK_CALL( hr, "CreateStateBlock", pDevice, ++refcount );
    hr = IDirect3DDevice9_CreateAdditionalSwapChain( pDevice, &d3dpp, &pSwapChain );
    CHECK_CALL( hr, "CreateAdditionalSwapChain", pDevice, ++refcount );
    if(pSwapChain)
    {
        /* check implicit back buffer */
        hr = IDirect3DSwapChain9_GetBackBuffer(pSwapChain, 0, 0, &pBackBuffer);
        CHECK_CALL( hr, "GetBackBuffer", pDevice, ++refcount);
        CHECK_REFCOUNT( pSwapChain, 1);
        if(pBackBuffer)
        {
            CHECK_SURFACE_CONTAINER( pBackBuffer, IID_IDirect3DSwapChain9, pSwapChain);
            CHECK_REFCOUNT( pBackBuffer, 1);
            CHECK_RELEASE_REFCOUNT( pBackBuffer, 0);
            CHECK_REFCOUNT( pDevice, --refcount);

            /* The back buffer is released with the swapchain, so AddRef with refcount=0 is fine here. */
            CHECK_ADDREF_REFCOUNT(pBackBuffer, 1);
            CHECK_REFCOUNT(pDevice, ++refcount);
            CHECK_RELEASE_REFCOUNT(pBackBuffer, 0);
            CHECK_REFCOUNT(pDevice, --refcount);
            pBackBuffer = NULL;
        }
        CHECK_REFCOUNT( pSwapChain, 1);
    }
    hr = IDirect3DDevice9_CreateQuery( pDevice, D3DQUERYTYPE_EVENT, &pQuery );
    CHECK_CALL( hr, "CreateQuery", pDevice, ++refcount );

    hr = IDirect3DDevice9_BeginStateBlock( pDevice );
    CHECK_CALL( hr, "BeginStateBlock", pDevice, refcount );
    hr = IDirect3DDevice9_EndStateBlock( pDevice, &pStateBlock1 );
    CHECK_CALL( hr, "EndStateBlock", pDevice, ++refcount );

    /* The implicit render target is not freed if refcount reaches 0.
     * Otherwise GetRenderTarget would re-allocate it and the pointer would change.*/
    hr = IDirect3DDevice9_GetRenderTarget(pDevice, 0, &pRenderTarget2);
    CHECK_CALL( hr, "GetRenderTarget", pDevice, ++refcount);
    if(pRenderTarget2)
    {
        CHECK_RELEASE_REFCOUNT(pRenderTarget2, 0);
        ok(pRenderTarget == pRenderTarget2, "RenderTarget=%p and RenderTarget2=%p should be the same.\n",
           pRenderTarget, pRenderTarget2);
        CHECK_REFCOUNT( pDevice, --refcount);
        pRenderTarget2 = NULL;
    }
    pRenderTarget = NULL;

cleanup:
    CHECK_RELEASE(pDevice,              pDevice, --refcount);

    /* Buffers */
    CHECK_RELEASE(pVertexBuffer,        pDevice, --refcount);
    CHECK_RELEASE(pIndexBuffer,         pDevice, --refcount);
    /* Shaders */
    CHECK_RELEASE(pVertexDeclaration,   pDevice, --refcount);
    CHECK_RELEASE(pVertexShader,        pDevice, --refcount);
    CHECK_RELEASE(pPixelShader,         pDevice, --refcount);
    /* Textures */
    CHECK_RELEASE(pTextureLevel,        pDevice, --refcount);
    CHECK_RELEASE(pCubeTexture,         pDevice, --refcount);
    CHECK_RELEASE(pVolumeTexture,       pDevice, --refcount);
    /* Surfaces */
    CHECK_RELEASE(pStencilSurface,      pDevice, --refcount);
    CHECK_RELEASE(pOffscreenSurface,    pDevice, --refcount);
    CHECK_RELEASE(pRenderTarget3,       pDevice, --refcount);
    /* Misc */
    CHECK_RELEASE(pStateBlock,          pDevice, --refcount);
    CHECK_RELEASE(pSwapChain,           pDevice, --refcount);
    CHECK_RELEASE(pQuery,               pDevice, --refcount);
    /* This will destroy device - cannot check the refcount here */
    if (pStateBlock1)         CHECK_RELEASE_REFCOUNT( pStateBlock1, 0);

    if (pD3d)                 CHECK_RELEASE_REFCOUNT( pD3d, 0);

    DestroyWindow( hwnd );
}

static void test_cursor(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D9                  *pD3d               = NULL;
    IDirect3DDevice9            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    CURSORINFO                   info;
    IDirect3DSurface9 *cursor = NULL;
    HCURSOR cur;

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    hr = GetCursorInfo(&info);
    cur = info.hCursor;

    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == S_OK || hr == D3DERR_NOTAVAILABLE,
       "Failed to create IDirect3D9Device (%s)\n", DXGetErrorString9(hr));
    if (FAILED(hr)) goto cleanup;

    IDirect3DDevice9_CreateOffscreenPlainSurface(pDevice, 32, 32, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &cursor, 0);
    ok(cursor != NULL, "IDirect3DDevice9_CreateOffscreenPlainSurface failed with %08x\n", hr);

    /* Initially hidden */
    hr = IDirect3DDevice9_ShowCursor(pDevice, TRUE);
    ok(hr == FALSE, "IDirect3DDevice9_ShowCursor returned %08x\n", hr);

    /* Not enabled without a surface*/
    hr = IDirect3DDevice9_ShowCursor(pDevice, TRUE);
    ok(hr == FALSE, "IDirect3DDevice9_ShowCursor returned %08x\n", hr);

    /* Fails */
    hr = IDirect3DDevice9_SetCursorProperties(pDevice, 0, 0, NULL);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_SetCursorProperties returned %08x\n", hr);

    hr = IDirect3DDevice9_SetCursorProperties(pDevice, 0, 0, cursor);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetCursorProperties returned %08x\n", hr);

    IDirect3DSurface9_Release(cursor);

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    hr = GetCursorInfo(&info);
    ok(hr != 0, "GetCursorInfo returned %08x\n", hr);
    ok(info.flags & CURSOR_SHOWING, "The gdi cursor is hidden (%08x)\n", info.flags);
    ok(info.hCursor == cur, "The cursor handle is %p\n", info.hCursor); /* unchanged */

    /* Still hidden */
    hr = IDirect3DDevice9_ShowCursor(pDevice, TRUE);
    ok(hr == FALSE, "IDirect3DDevice9_ShowCursor returned %08x\n", hr);

    /* Enabled now*/
    hr = IDirect3DDevice9_ShowCursor(pDevice, TRUE);
    ok(hr == TRUE, "IDirect3DDevice9_ShowCursor returned %08x\n", hr);

    /* GDI cursor unchanged */
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    hr = GetCursorInfo(&info);
    ok(hr != 0, "GetCursorInfo returned %08x\n", hr);
    ok(info.flags & CURSOR_SHOWING, "The gdi cursor is hidden (%08x)\n", info.flags);
    ok(info.hCursor == cur, "The cursor handle is %p\n", info.hCursor); /* unchanged */

cleanup:
    if(pDevice) IDirect3D9_Release(pDevice);
    if(pD3d) IDirect3D9_Release(pD3d);
    DestroyWindow( hwnd );
}

static void test_reset(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D9                  *pD3d               = NULL;
    IDirect3DDevice9            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm, d3ddm2;
    D3DVIEWPORT9                 vp;
    DWORD                        width, orig_width = GetSystemMetrics(SM_CXSCREEN);
    DWORD                        height, orig_height = GetSystemMetrics(SM_CYSCREEN);
    IDirect3DSwapChain9          *pSwapchain;
    IDirect3DSurface9            *surface;
    IDirect3DTexture9            *texture;
    IDirect3DVertexShader9       *shader;
    BOOL                         support_800x600 = FALSE;
    UINT                         i;
    D3DLOCKED_RECT               lockrect;

    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = FALSE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight  = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;

    for(i = 0; i < IDirect3D9_GetAdapterModeCount(pD3d, D3DADAPTER_DEFAULT, d3ddm.Format); i++) {
        ZeroMemory( &d3ddm2, sizeof(d3ddm2) );
        hr = IDirect3D9_EnumAdapterModes(pD3d, D3DADAPTER_DEFAULT, d3ddm.Format, i, &d3ddm2);
        ok(hr == D3D_OK, "IDirect3D9_EnumAdapterModes returned %#x\n", hr);

        if(d3ddm2.Width == 800 && d3ddm2.Height == 600) {
            support_800x600 = TRUE;
        }
        /* We use them as invalid modes */
        if((d3ddm2.Width == 801 && d3ddm2.Height == 600) ||
           (d3ddm2.Width == 32 && d3ddm2.Height == 32)) {
            skip("This system supports a screen resolution of %dx%d, not running mode tests\n",
                 d3ddm2.Width, d3ddm2.Height);
            goto cleanup;
        }
    }
    if(!support_800x600) {
        skip("Mode 800x600 not supported, skipping mode tests\n");
        goto cleanup;
    }

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );

    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D9_CreateDevice returned %#x\n", hr);
        goto cleanup;
    }
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after creation returned %#x\n", hr);

    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
    ok(width == 800, "Screen width is %d\n", width);
    ok(height == 600, "Screen height is %d\n", height);

    hr = IDirect3DDevice9_GetViewport(pDevice, &vp);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetViewport failed with %s\n", DXGetErrorString9(hr));
    if(SUCCEEDED(hr))
    {
        ok(vp.X == 0, "D3DVIEWPORT->X = %d\n", vp.X);
        ok(vp.Y == 0, "D3DVIEWPORT->Y = %d\n", vp.Y);
        ok(vp.Width == 800, "D3DVIEWPORT->Width = %d\n", vp.Width);
        ok(vp.Height == 600, "D3DVIEWPORT->Height = %d\n", vp.Height);
        ok(vp.MinZ == 0, "D3DVIEWPORT->MinZ = %f\n", vp.MinZ);
        ok(vp.MaxZ == 1, "D3DVIEWPORT->MaxZ = %f\n", vp.MaxZ);
    }
    vp.X = 10;
    vp.X = 20;
    vp.MinZ = 2;
    vp.MaxZ = 3;
    hr = IDirect3DDevice9_SetViewport(pDevice, &vp);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetViewport failed with %s\n", DXGetErrorString9(hr));

    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed         = FALSE;
    d3dpp.BackBufferWidth  = 640;
    d3dpp.BackBufferHeight  = 480;
    d3dpp.BackBufferFormat = d3ddm.Format;
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);

    ZeroMemory(&vp, sizeof(vp));
    hr = IDirect3DDevice9_GetViewport(pDevice, &vp);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetViewport failed with %s\n", DXGetErrorString9(hr));
    if(SUCCEEDED(hr))
    {
        ok(vp.X == 0, "D3DVIEWPORT->X = %d\n", vp.X);
        ok(vp.Y == 0, "D3DVIEWPORT->Y = %d\n", vp.Y);
        ok(vp.Width == 640, "D3DVIEWPORT->Width = %d\n", vp.Width);
        ok(vp.Height == 480, "D3DVIEWPORT->Height = %d\n", vp.Height);
        ok(vp.MinZ == 0, "D3DVIEWPORT->MinZ = %f\n", vp.MinZ);
        ok(vp.MaxZ == 1, "D3DVIEWPORT->MaxZ = %f\n", vp.MaxZ);
    }

    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
    ok(width == 640, "Screen width is %d\n", width);
    ok(height == 480, "Screen height is %d\n", height);

    hr = IDirect3DDevice9_GetSwapChain(pDevice, 0, &pSwapchain);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetSwapChain returned %s\n", DXGetErrorString9(hr));
    if(SUCCEEDED(hr))
    {
        ZeroMemory(&d3dpp, sizeof(d3dpp));
        hr = IDirect3DSwapChain9_GetPresentParameters(pSwapchain, &d3dpp);
        ok(hr == D3D_OK, "IDirect3DSwapChain9_GetPresentParameters returned %s\n", DXGetErrorString9(hr));
        if(SUCCEEDED(hr))
        {
            ok(d3dpp.BackBufferWidth == 640, "Back buffer width is %d\n", d3dpp.BackBufferWidth);
            ok(d3dpp.BackBufferHeight == 480, "Back buffer height is %d\n", d3dpp.BackBufferHeight);
        }
        IDirect3DSwapChain9_Release(pSwapchain);
    }

    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed         = TRUE;
    d3dpp.BackBufferWidth  = 400;
    d3dpp.BackBufferHeight  = 300;
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);

    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
    ok(width == orig_width, "Screen width is %d\n", width);
    ok(height == orig_height, "Screen height is %d\n", height);

    ZeroMemory(&vp, sizeof(vp));
    hr = IDirect3DDevice9_GetViewport(pDevice, &vp);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetViewport failed with %s\n", DXGetErrorString9(hr));
    if(SUCCEEDED(hr))
    {
        ok(vp.X == 0, "D3DVIEWPORT->X = %d\n", vp.X);
        ok(vp.Y == 0, "D3DVIEWPORT->Y = %d\n", vp.Y);
        ok(vp.Width == 400, "D3DVIEWPORT->Width = %d\n", vp.Width);
        ok(vp.Height == 300, "D3DVIEWPORT->Height = %d\n", vp.Height);
        ok(vp.MinZ == 0, "D3DVIEWPORT->MinZ = %f\n", vp.MinZ);
        ok(vp.MaxZ == 1, "D3DVIEWPORT->MaxZ = %f\n", vp.MaxZ);
    }

    hr = IDirect3DDevice9_GetSwapChain(pDevice, 0, &pSwapchain);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetSwapChain returned %s\n", DXGetErrorString9(hr));
    if(SUCCEEDED(hr))
    {
        ZeroMemory(&d3dpp, sizeof(d3dpp));
        hr = IDirect3DSwapChain9_GetPresentParameters(pSwapchain, &d3dpp);
        ok(hr == D3D_OK, "IDirect3DSwapChain9_GetPresentParameters returned %s\n", DXGetErrorString9(hr));
        if(SUCCEEDED(hr))
        {
            ok(d3dpp.BackBufferWidth == 400, "Back buffer width is %d\n", d3dpp.BackBufferWidth);
            ok(d3dpp.BackBufferHeight == 300, "Back buffer height is %d\n", d3dpp.BackBufferHeight);
        }
        IDirect3DSwapChain9_Release(pSwapchain);
    }

    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed         = TRUE;
    d3dpp.BackBufferWidth  = 400;
    d3dpp.BackBufferHeight  = 300;

    /* _Reset fails if there is a resource in the default pool */
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDevice, 16, 16, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &surface, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateOffscreenPlainSurface returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3DERR_DEVICENOTRESET, "IDirect3DDevice9_TestCooperativeLevel after a failed reset returned %#x\n", hr);
    IDirect3DSurface9_Release(surface);
    /* Reset again to get the device out of the lost state */
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);

    /* Scratch, sysmem and managed pools are fine */
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDevice, 16, 16, D3DFMT_R5G6B5, D3DPOOL_SCRATCH, &surface, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateOffscreenPlainSurface returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);
    IDirect3DSurface9_Release(surface);

    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDevice, 16, 16, D3DFMT_R5G6B5, D3DPOOL_SYSTEMMEM, &surface, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateOffscreenPlainSurface returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);
    IDirect3DSurface9_Release(surface);

    /* Will a sysmem or scratch survive while locked */
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDevice, 16, 16, D3DFMT_R5G6B5, D3DPOOL_SYSTEMMEM, &surface, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateOffscreenPlainSurface returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DSurface9_LockRect(surface, &lockrect, NULL, D3DLOCK_DISCARD);
    ok(hr == D3D_OK, "IDirect3DSurface9_LockRect returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);
    IDirect3DSurface9_UnlockRect(surface);
    IDirect3DSurface9_Release(surface);

    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDevice, 16, 16, D3DFMT_R5G6B5, D3DPOOL_SCRATCH, &surface, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateOffscreenPlainSurface returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DSurface9_LockRect(surface, &lockrect, NULL, D3DLOCK_DISCARD);
    ok(hr == D3D_OK, "IDirect3DSurface9_LockRect returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);
    IDirect3DSurface9_UnlockRect(surface);
    IDirect3DSurface9_Release(surface);

    hr = IDirect3DDevice9_CreateTexture(pDevice, 16, 16, 0, 0, D3DFMT_R5G6B5, D3DPOOL_MANAGED, &texture, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);
    IDirect3DTexture9_Release(texture);

    /* A reference held to an implicit surface causes failures as well */
    hr = IDirect3DDevice9_GetBackBuffer(pDevice, 0, 0, D3DBACKBUFFER_TYPE_MONO, &surface);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetBackBuffer returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3DERR_DEVICENOTRESET, "IDirect3DDevice9_TestCooperativeLevel after a failed reset returned %#x\n", hr);
    IDirect3DSurface9_Release(surface);
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_TestCooperativeLevel after a successful reset returned %#x\n", hr);

    /* Shaders are fine as well */
    hr = IDirect3DDevice9_CreateVertexShader(pDevice, simple_vs, &shader);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateVertexShader returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3D_OK, "IDirect3DDevice9_Reset failed with %s\n", DXGetErrorString9(hr));
    IDirect3DVertexShader9_Release(shader);

    /* Try setting invalid modes */
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed         = FALSE;
    d3dpp.BackBufferWidth  = 32;
    d3dpp.BackBufferHeight  = 32;
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_Reset to w=32, h=32, windowed=FALSE failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3DERR_DEVICENOTRESET, "IDirect3DDevice9_TestCooperativeLevel after a failed reset returned %#x\n", hr);

    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed         = FALSE;
    d3dpp.BackBufferWidth  = 801;
    d3dpp.BackBufferHeight  = 600;
    hr = IDirect3DDevice9_Reset(pDevice, &d3dpp);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_Reset to w=801, h=600, windowed=FALSE failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_TestCooperativeLevel(pDevice);
    ok(hr == D3DERR_DEVICENOTRESET, "IDirect3DDevice9_TestCooperativeLevel after a failed reset returned %#x\n", hr);

cleanup:
    if(pD3d) IDirect3D9_Release(pD3d);
    if(pDevice) IDirect3D9_Release(pDevice);
}

/* Test adapter display modes */
static void test_display_modes(void)
{
    D3DDISPLAYMODE dmode;
    IDirect3D9 *pD3d;

    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    if(!pD3d) return;

#define TEST_FMT(x,r) do { \
    HRESULT res = IDirect3D9_EnumAdapterModes(pD3d, 0, (x), 0, &dmode); \
    ok(res==(r), "EnumAdapterModes("#x") did not return "#r" (got %s)!\n", DXGetErrorString9(res)); \
} while(0)

    TEST_FMT(D3DFMT_R8G8B8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A8R8G8B8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_X8B8G8R8, D3DERR_INVALIDCALL);
    /* D3DFMT_R5G6B5 */
    TEST_FMT(D3DFMT_X1R5G5B5, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A1R5G5B5, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A4R4G4B4, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_R3G3B2, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A8R3G3B2, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_X4R4G4B4, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A2B10G10R10, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A8B8G8R8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_X8B8G8R8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_G16R16, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A2R10G10B10, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A16B16G16R16, D3DERR_INVALIDCALL);

    TEST_FMT(D3DFMT_A8P8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_P8, D3DERR_INVALIDCALL);

    TEST_FMT(D3DFMT_L8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A8L8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A4L4, D3DERR_INVALIDCALL);

    TEST_FMT(D3DFMT_V8U8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_L6V5U5, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_X8L8V8U8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_Q8W8V8U8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_V16U16, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A2W10V10U10, D3DERR_INVALIDCALL);

    TEST_FMT(D3DFMT_UYVY, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_YUY2, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_DXT1, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_DXT2, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_DXT3, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_DXT4, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_DXT5, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_MULTI2_ARGB8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_G8R8_G8B8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_R8G8_B8G8, D3DERR_INVALIDCALL);

    TEST_FMT(D3DFMT_D16_LOCKABLE, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_D32, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_D15S1, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_D24S8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_D24X8, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_D24X4S4, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_D16, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_L16, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_D32F_LOCKABLE, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_D24FS8, D3DERR_INVALIDCALL);

    TEST_FMT(D3DFMT_VERTEXDATA, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_INDEX16, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_INDEX32, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_Q16W16V16U16, D3DERR_INVALIDCALL);
    /* Floating point formats */
    TEST_FMT(D3DFMT_R16F, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_G16R16F, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A16B16G16R16F, D3DERR_INVALIDCALL);

    /* IEEE formats */
    TEST_FMT(D3DFMT_R32F, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_G32R32F, D3DERR_INVALIDCALL);
    TEST_FMT(D3DFMT_A32B32G32R32F, D3DERR_INVALIDCALL);

    TEST_FMT(D3DFMT_CxV8U8, D3DERR_INVALIDCALL);

    TEST_FMT(0, D3DERR_INVALIDCALL);

    IDirect3D9_Release(pD3d);
}

static void test_scene(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D9                  *pD3d               = NULL;
    IDirect3DDevice9            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    IDirect3DSurface9            *pSurface1 = NULL, *pSurface2 = NULL, *pSurface3 = NULL, *pRenderTarget = NULL;
    IDirect3DSurface9            *pBackBuffer = NULL, *pDepthStencil = NULL;
    RECT rect = {0, 0, 128, 128};
    D3DCAPS9                     caps;

    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight  = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE, "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!pDevice)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    /* Get the caps, they will be needed to tell if an operation is supposed to be valid */
    memset(&caps, 0, sizeof(caps));
    hr = IDirect3DDevice9_GetDeviceCaps(pDevice, &caps);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetCaps failed with %s\n", DXGetErrorString9(hr));
    if(FAILED(hr)) goto cleanup;

    /* Test an EndScene without beginscene. Should return an error */
    hr = IDirect3DDevice9_EndScene(pDevice);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_EndScene returned %s\n", DXGetErrorString9(hr));

    /* Test a normal BeginScene / EndScene pair, this should work */
    hr = IDirect3DDevice9_BeginScene(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_BeginScene failed with %s\n", DXGetErrorString9(hr));
    if(SUCCEEDED(hr))
    {
        hr = IDirect3DDevice9_EndScene(pDevice);
        ok(hr == D3D_OK, "IDirect3DDevice9_EndScene failed with %s\n", DXGetErrorString9(hr));
    }

    /* Test another EndScene without having begun a new scene. Should return an error */
    hr = IDirect3DDevice9_EndScene(pDevice);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_EndScene returned %s\n", DXGetErrorString9(hr));

    /* Two nested BeginScene and EndScene calls */
    hr = IDirect3DDevice9_BeginScene(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_BeginScene failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_BeginScene(pDevice);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_BeginScene returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_EndScene(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_EndScene failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_EndScene(pDevice);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_EndScene returned %s\n", DXGetErrorString9(hr));

    /* Create some surfaces to test stretchrect between the scenes */
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDevice, 128, 128, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pSurface1, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateOffscreenPlainSurface failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(pDevice, 128, 128, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pSurface2, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateOffscreenPlainSurface failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_CreateDepthStencilSurface(pDevice, 800, 600, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, FALSE, &pSurface3, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateDepthStencilSurface failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_CreateRenderTarget(pDevice, 128, 128, d3ddm.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &pRenderTarget, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateRenderTarget failed with %s\n", DXGetErrorString9(hr));

    hr = IDirect3DDevice9_GetBackBuffer(pDevice, 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetBackBuffer failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_GetDepthStencilSurface(pDevice, &pDepthStencil);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetBackBuffer failed with %s\n", DXGetErrorString9(hr));

    /* First make sure a simple StretchRect call works */
    if(pSurface1 && pSurface2) {
        hr = IDirect3DDevice9_StretchRect(pDevice, pSurface1, NULL, pSurface2, NULL, 0);
        ok( hr == D3D_OK, "IDirect3DDevice9_StretchRect failed with %s\n", DXGetErrorString9(hr));
    }
    if(pBackBuffer && pRenderTarget) {
        hr = IDirect3DDevice9_StretchRect(pDevice, pBackBuffer, &rect, pRenderTarget, NULL, 0);
        ok( hr == D3D_OK, "IDirect3DDevice9_StretchRect failed with %s\n", DXGetErrorString9(hr));
    }
    if(pDepthStencil && pSurface3) {
        HRESULT expected;
        if(0) /* Disabled for now because it crashes in wine */ {
            expected = caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES ? D3D_OK : D3DERR_INVALIDCALL;
            hr = IDirect3DDevice9_StretchRect(pDevice, pDepthStencil, NULL, pSurface3, NULL, 0);
            ok( hr == expected, "IDirect3DDevice9_StretchRect returned %s, expected %s\n", DXGetErrorString9(hr), DXGetErrorString9(expected));
        }
    }

    /* Now try it in a BeginScene - EndScene pair. Seems to be allowed in a beginScene - Endscene pair
     * width normal surfaces, render targets and depth stencil surfaces.
     */
    hr = IDirect3DDevice9_BeginScene(pDevice);
    ok( hr == D3D_OK, "IDirect3DDevice9_BeginScene failed with %s\n", DXGetErrorString9(hr));

    if(pSurface1 && pSurface2)
    {
        hr = IDirect3DDevice9_StretchRect(pDevice, pSurface1, NULL, pSurface2, NULL, 0);
        ok( hr == D3D_OK, "IDirect3DDevice9_StretchRect failed with %s\n", DXGetErrorString9(hr));
    }
    if(pBackBuffer && pRenderTarget)
    {
        hr = IDirect3DDevice9_StretchRect(pDevice, pBackBuffer, &rect, pRenderTarget, NULL, 0);
        ok( hr == D3D_OK, "IDirect3DDevice9_StretchRect failed with %s\n", DXGetErrorString9(hr));
    }
    if(pDepthStencil && pSurface3)
    {
        /* This is supposed to fail inside a BeginScene - EndScene pair. */
        hr = IDirect3DDevice9_StretchRect(pDevice, pDepthStencil, NULL, pSurface3, NULL, 0);
        ok( hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_StretchRect returned %s, expected D3DERR_INVALIDCALL\n", DXGetErrorString9(hr));
    }

    hr = IDirect3DDevice9_EndScene(pDevice);
    ok( hr == D3D_OK, "IDirect3DDevice9_EndScene failed with %s\n", DXGetErrorString9(hr));

    /* Does a SetRenderTarget influence BeginScene / EndScene ?
     * Set a new render target, then see if it started a new scene. Flip the rt back and see if that maybe
     * ended the scene. Expected result is that the scene is not affected by SetRenderTarget
     */
    hr = IDirect3DDevice9_SetRenderTarget(pDevice, 0, pRenderTarget);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetRenderTarget failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_BeginScene(pDevice);
    ok( hr == D3D_OK, "IDirect3DDevice9_BeginScene failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_SetRenderTarget(pDevice, 0, pBackBuffer);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetRenderTarget failed with %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_EndScene(pDevice);
    ok( hr == D3D_OK, "IDirect3DDevice9_EndScene failed with %s\n", DXGetErrorString9(hr));

cleanup:
    if(pRenderTarget) IDirect3DSurface9_Release(pRenderTarget);
    if(pDepthStencil) IDirect3DSurface9_Release(pDepthStencil);
    if(pBackBuffer) IDirect3DSurface9_Release(pBackBuffer);
    if(pSurface1) IDirect3DSurface9_Release(pSurface1);
    if(pSurface2) IDirect3DSurface9_Release(pSurface2);
    if(pSurface3) IDirect3DSurface9_Release(pSurface3);
    if(pD3d) IDirect3D9_Release(pD3d);
    if(pDevice) IDirect3D9_Release(pDevice);
    if(hwnd) DestroyWindow(hwnd);
}

static void test_limits(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D9                  *pD3d               = NULL;
    IDirect3DDevice9            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    IDirect3DTexture9           *pTexture           = NULL;
    int i;

    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight  = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE, "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!pDevice)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    hr = IDirect3DDevice9_CreateTexture(pDevice, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexture, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture failed with %s\n", DXGetErrorString9(hr));
    if(!pTexture) goto cleanup;

    /* There are 16 pixel samplers. We should be able to access all of them */
    for(i = 0; i < 16; i++) {
        hr = IDirect3DDevice9_SetTexture(pDevice, i, (IDirect3DBaseTexture9 *) pTexture);
        ok(hr == D3D_OK, "IDirect3DDevice9_SetTexture for sampler %d failed with %s\n", i, DXGetErrorString9(hr));
        hr = IDirect3DDevice9_SetTexture(pDevice, i, NULL);
        ok(hr == D3D_OK, "IDirect3DDevice9_SetTexture for sampler %d failed with %s\n", i, DXGetErrorString9(hr));
        hr = IDirect3DDevice9_SetSamplerState(pDevice, i, D3DSAMP_SRGBTEXTURE, TRUE);
        ok(hr == D3D_OK, "IDirect3DDevice9_SetSamplerState for sampler %d failed with %s\n", i, DXGetErrorString9(hr));
    }

    /* Now test all 8 textures stage states */
    for(i = 0; i < 8; i++) {
        hr = IDirect3DDevice9_SetTextureStageState(pDevice, i, D3DTSS_COLOROP, D3DTOP_ADD);
        ok(hr == D3D_OK, "IDirect3DDevice9_SetTextureStageState for texture %d failed with %s\n", i, DXGetErrorString9(hr));
    }

    /* Investigations show that accessing higher samplers / textures stage states does not return an error either. Writing
     * to too high samplers(approximately sampler 40) causes memory corruption in windows, so there is no bounds checking
     * but how do I test that?
     */
cleanup:
    if(pTexture) IDirect3DTexture9_Release(pTexture);
    if(pD3d) IDirect3D9_Release(pD3d);
    if(pDevice) IDirect3D9_Release(pDevice);
    if(hwnd) DestroyWindow(hwnd);
}

static void test_depthstenciltest(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D9                  *pD3d               = NULL;
    IDirect3DDevice9            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    IDirect3DSurface9           *pDepthStencil           = NULL;
    IDirect3DSurface9           *pDepthStencil2          = NULL;
    D3DZBUFFERTYPE               state;

    pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight  = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE, "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!pDevice)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    hr = IDirect3DDevice9_GetDepthStencilSurface(pDevice, &pDepthStencil);
    ok(hr == D3D_OK && pDepthStencil != NULL, "IDirect3DDevice9_GetDepthStencilSurface failed with %s\n", DXGetErrorString9(hr));

    /* Try to clear */
    hr = IDirect3DDevice9_Clear(pDevice, 0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice9_Clear failed with %s\n", DXGetErrorString9(hr));

    hr = IDirect3DDevice9_SetDepthStencilSurface(pDevice, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetDepthStencilSurface failed with %s\n", DXGetErrorString9(hr));

    /* Check if the set buffer is returned on a get. WineD3D had a bug with that once, prevent it from coming back */
    hr = IDirect3DDevice9_GetDepthStencilSurface(pDevice, &pDepthStencil2);
    ok(hr == D3DERR_NOTFOUND && pDepthStencil2 == NULL, "IDirect3DDevice9_GetDepthStencilSurface failed with %s\n", DXGetErrorString9(hr));
    if(pDepthStencil2) IDirect3DSurface9_Release(pDepthStencil2);

    /* This left the render states untouched! */
    hr = IDirect3DDevice9_GetRenderState(pDevice, D3DRS_ZENABLE, &state);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetRenderState failed with %s\n", DXGetErrorString9(hr));
    ok(state == D3DZB_TRUE, "D3DRS_ZENABLE is %s\n", state == D3DZB_FALSE ? "D3DZB_FALSE" : (state == D3DZB_TRUE ? "D3DZB_TRUE" : "D3DZB_USEW"));
    hr = IDirect3DDevice9_GetRenderState(pDevice, D3DRS_ZWRITEENABLE, &state);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetRenderState failed with %s\n", DXGetErrorString9(hr));
    ok(state == TRUE, "D3DRS_ZWRITEENABLE is %s\n", state ? "TRUE" : "FALSE");
    hr = IDirect3DDevice9_GetRenderState(pDevice, D3DRS_STENCILENABLE, &state);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetRenderState failed with %s\n", DXGetErrorString9(hr));
    ok(state == FALSE, "D3DRS_STENCILENABLE is %s\n", state ? "TRUE" : "FALSE");
    hr = IDirect3DDevice9_GetRenderState(pDevice, D3DRS_STENCILWRITEMASK, &state);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetRenderState failed with %s\n", DXGetErrorString9(hr));
    ok(state == 0xffffffff, "D3DRS_STENCILWRITEMASK is 0x%08x\n", state);

    /* This is supposed to fail now */
    hr = IDirect3DDevice9_Clear(pDevice, 0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0, 0);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_Clear failed with %s\n", DXGetErrorString9(hr));

    hr = IDirect3DDevice9_SetRenderState(pDevice, D3DRS_ZENABLE, D3DZB_FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetRenderState failed with %s\n", DXGetErrorString9(hr));

    hr = IDirect3DDevice9_SetDepthStencilSurface(pDevice, pDepthStencil);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetDepthStencilSurface failed with %s\n", DXGetErrorString9(hr));

    hr = IDirect3DDevice9_GetRenderState(pDevice, D3DRS_ZENABLE, &state);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetRenderState failed with %s\n", DXGetErrorString9(hr));
    ok(state == D3DZB_FALSE, "D3DRS_ZENABLE is %s\n", state == D3DZB_FALSE ? "D3DZB_FALSE" : (state == D3DZB_TRUE ? "D3DZB_TRUE" : "D3DZB_USEW"));

    /* Now it works again */
    hr = IDirect3DDevice9_Clear(pDevice, 0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice9_Clear failed with %s\n", DXGetErrorString9(hr));

    if(pDepthStencil) IDirect3DSurface9_Release(pDepthStencil);
    if(pDevice) IDirect3D9_Release(pDevice);

    /* Now see if autodepthstencil disable is honored. First, without a format set */
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight  = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE, "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!pDevice)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    pDepthStencil = NULL;
    hr = IDirect3DDevice9_GetDepthStencilSurface(pDevice, &pDepthStencil);
    ok(hr == D3DERR_NOTFOUND && pDepthStencil == NULL, "IDirect3DDevice9_GetDepthStencilSurface returned %s, surface = %p\n", DXGetErrorString9(hr), pDepthStencil);
    if(pDepthStencil) {
        IDirect3DSurface9_Release(pDepthStencil);
        pDepthStencil = NULL;
    }

    /* Check the depth test state */
    hr = IDirect3DDevice9_GetRenderState(pDevice, D3DRS_ZENABLE, &state);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetRenderState failed with %s\n", DXGetErrorString9(hr));
    ok(state == D3DZB_FALSE, "D3DRS_ZENABLE is %s\n", state == D3DZB_FALSE ? "D3DZB_FALSE" : (state == D3DZB_TRUE ? "D3DZB_TRUE" : "D3DZB_USEW"));

    if(pDevice) IDirect3D9_Release(pDevice);

    /* Next, try EnableAutoDepthStencil FALSE with a depth stencil format set */
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight  = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE, "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!pDevice)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    pDepthStencil = NULL;
    hr = IDirect3DDevice9_GetDepthStencilSurface(pDevice, &pDepthStencil);
    ok(hr == D3DERR_NOTFOUND && pDepthStencil == NULL, "IDirect3DDevice9_GetDepthStencilSurface returned %s, surface = %p\n", DXGetErrorString9(hr), pDepthStencil);
    if(pDepthStencil) {
        IDirect3DSurface9_Release(pDepthStencil);
        pDepthStencil = NULL;
    }

    hr = IDirect3DDevice9_GetRenderState(pDevice, D3DRS_ZENABLE, &state);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetRenderState failed with %s\n", DXGetErrorString9(hr));
    ok(state == D3DZB_FALSE, "D3DRS_ZENABLE is %s\n", state == D3DZB_FALSE ? "D3DZB_FALSE" : (state == D3DZB_TRUE ? "D3DZB_TRUE" : "D3DZB_USEW"));

cleanup:
    if(pDepthStencil) IDirect3DSurface9_Release(pDepthStencil);
    if(pD3d) IDirect3D9_Release(pD3d);
    if(pDevice) IDirect3D9_Release(pDevice);
    if(hwnd) DestroyWindow(hwnd);
}

/* Test what happens when IDirect3DDevice9_DrawIndexedPrimitive is called without a valid index buffer set. */
static void test_draw_indexed(void)
{
    static const struct {
        float position[3];
        DWORD color;
    } quad[] = {
        {{-1.0f, -1.0f, 0.0f}, 0xffff0000},
        {{-1.0f,  1.0f, 0.0f}, 0xffff0000},
        {{ 1.0f,  1.0f, 0.0f}, 0xffff0000},
        {{ 1.0f, -1.0f, 0.0f}, 0xffff0000},
    };
    WORD indices[] = {0, 1, 2, 3, 0, 2};

    static const D3DVERTEXELEMENT9 decl_elements[] = {
        {0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,    D3DDECLUSAGE_COLOR, 0},
        D3DDECL_END()
    };

    IDirect3DVertexDeclaration9 *vertex_declaration = NULL;
    IDirect3DVertexBuffer9 *vertex_buffer = NULL;
    IDirect3DIndexBuffer9 *index_buffer = NULL;
    D3DPRESENT_PARAMETERS present_parameters;
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d9;
    HRESULT hr;
    HWND hwnd;
    void *ptr;

    hwnd = CreateWindow("static", "d3d9_test",
            0, 0, 0, 10, 10, 0, 0, 0, 0);
    if (!hwnd)
    {
        skip("Failed to create window\n");
        return;
    }

    d3d9 = pDirect3DCreate9(D3D_SDK_VERSION);
    if (!d3d9)
    {
        skip("Failed to create IDirect3D9 object\n");
        goto cleanup;
    }

    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = hwnd;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;

    hr = IDirect3D9_CreateDevice(d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device);
    if (FAILED(hr) || !device)
    {
        skip("Failed to create device\n");
        goto cleanup;
    }

    hr = IDirect3DDevice9_CreateVertexDeclaration(device, decl_elements, &vertex_declaration);
    ok(SUCCEEDED(hr), "CreateVertexDeclaration failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_SetVertexDeclaration(device, NULL);
    ok(SUCCEEDED(hr), "SetVertexDeclaration failed (0x%08x)\n", hr);

    hr = IDirect3DDevice9_CreateVertexBuffer(device, sizeof(quad), 0, 0, D3DPOOL_DEFAULT, &vertex_buffer, NULL);
    ok(SUCCEEDED(hr), "CreateVertexBuffer failed (0x%08x)\n", hr);
    hr = IDirect3DVertexBuffer9_Lock(vertex_buffer, 0, 0, &ptr, D3DLOCK_DISCARD);
    ok(SUCCEEDED(hr), "Lock failed (0x%08x)\n", hr);
    memcpy(ptr, quad, sizeof(quad));
    hr = IDirect3DVertexBuffer9_Unlock(vertex_buffer);
    ok(SUCCEEDED(hr), "Unlock failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_SetStreamSource(device, 0, vertex_buffer, 0, sizeof(*quad));
    ok(SUCCEEDED(hr), "SetStreamSource failed (0x%08x)\n", hr);

    hr = IDirect3DDevice9_CreateIndexBuffer(device, sizeof(indices), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &index_buffer, NULL);
    ok(SUCCEEDED(hr), "CreateIndexBuffer failed (0x%08x)\n", hr);
    hr = IDirect3DIndexBuffer9_Lock(index_buffer, 0, 0, &ptr, D3DLOCK_DISCARD);
    ok(SUCCEEDED(hr), "Lock failed (0x%08x)\n", hr);
    memcpy(ptr, indices, sizeof(indices));
    hr = IDirect3DIndexBuffer9_Unlock(index_buffer);
    ok(SUCCEEDED(hr), "Unlock failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState D3DRS_LIGHTING failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed (0x%08x)\n", hr);

    /* NULL index buffer. Should fail */
    hr = IDirect3DDevice9_SetIndices(device, NULL);
    ok(SUCCEEDED(hr), "SetIndices failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_DrawIndexedPrimitive(device, D3DPT_TRIANGLELIST, 0 /* BaseVertexIndex */, 0 /* MinIndex */,
            4 /* NumVerts */, 0 /* StartIndex */, 2 /*PrimCount */);
    ok(hr == D3DERR_INVALIDCALL, "DrawIndexedPrimitive returned 0x%08x, expected D3DERR_INVALIDCALL (0x%08x)\n",
            hr, D3DERR_INVALIDCALL);

    /* Valid index buffer, NULL vertex declaration. Should fail */
    hr = IDirect3DDevice9_SetIndices(device, index_buffer);
    ok(SUCCEEDED(hr), "SetIndices failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_DrawIndexedPrimitive(device, D3DPT_TRIANGLELIST, 0 /* BaseVertexIndex */, 0 /* MinIndex */,
            4 /* NumVerts */, 0 /* StartIndex */, 2 /*PrimCount */);
    ok(hr == D3DERR_INVALIDCALL, "DrawIndexedPrimitive returned 0x%08x, expected D3DERR_INVALIDCALL (0x%08x)\n",
            hr, D3DERR_INVALIDCALL);

    /* Valid index buffer and vertex declaration. Should succeed */
    hr = IDirect3DDevice9_SetVertexDeclaration(device, vertex_declaration);
    ok(SUCCEEDED(hr), "SetVertexDeclaration failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_DrawIndexedPrimitive(device, D3DPT_TRIANGLELIST, 0 /* BaseVertexIndex */, 0 /* MinIndex */,
            4 /* NumVerts */, 0 /* StartIndex */, 2 /*PrimCount */);
    ok(SUCCEEDED(hr), "DrawIndexedPrimitive failed (0x%08x)\n", hr);

    hr = IDirect3DDevice9_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed (0x%08x)\n", hr);

    hr = IDirect3DDevice9_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (0x%08x)\n", hr);

    IDirect3DVertexBuffer9_Release(vertex_buffer);
    IDirect3DIndexBuffer9_Release(index_buffer);
    IDirect3DVertexDeclaration9_Release(vertex_declaration);

cleanup:
    if (d3d9) IDirect3D9_Release(d3d9);
    if (device) IDirect3DDevice9_Release(device);
    if (hwnd) DestroyWindow(hwnd);
}

static void test_null_stream(void)
{
    IDirect3DVertexBuffer9 *buffer = NULL;
    D3DPRESENT_PARAMETERS present_parameters;
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d9;
    HWND hwnd;
    HRESULT hr;
    IDirect3DVertexShader9 *shader = NULL;
    IDirect3DVertexDeclaration9 *decl = NULL;
    DWORD shader_code[] = {
        0xfffe0101,                             /* vs_1_1           */
        0x0000001f, 0x80000000, 0x900f0000,     /* dcl_position v0  */
        0x00000001, 0xc00f0000, 0x90e40000,     /* mov oPos, v0     */
        0x0000ffff                              /* end              */
    };
    static const D3DVERTEXELEMENT9 decl_elements[] = {
        {0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,    D3DDECLUSAGE_COLOR, 0},
        D3DDECL_END()
    };

    d3d9 = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(d3d9 != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d9 || !hwnd) goto cleanup;

    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = hwnd;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;

    hr = IDirect3D9_CreateDevice( d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE, "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!device)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    hr = IDirect3DDevice9_CreateVertexShader(device, shader_code, &shader);
    if(FAILED(hr)) {
        skip("No vertex shader support\n");
        goto cleanup;
    }
    hr = IDirect3DDevice9_CreateVertexDeclaration(device, decl_elements, &decl);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_CreateVertexDeclaration failed (0x%08x)\n", hr);
    if (!SUCCEEDED(hr)) {
	skip("Vertex declaration handling not possible.\n");
	goto cleanup;
    }
    hr = IDirect3DDevice9_CreateVertexBuffer(device, 12 * sizeof(float), 0, 0, D3DPOOL_MANAGED, &buffer, NULL);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_CreateVertexBuffer failed (0x%08x)\n", hr);
    if (!SUCCEEDED(hr)) {
	skip("Vertex buffer handling not possible.\n");
	goto cleanup;
    }

    hr = IDirect3DDevice9_SetStreamSource(device, 0, buffer, 0, sizeof(float) * 3);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_SetStreamSource failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_SetStreamSource(device, 1, NULL, 0, 0);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_SetStreamSource failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_SetVertexShader(device, shader);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_SetVertexShader failed (0x%08x)\n", hr);
    hr = IDirect3DDevice9_SetVertexDeclaration(device, decl);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_SetVertexDeclaration failed (0x%08x)\n", hr);

    hr = IDirect3DDevice9_BeginScene(device);
    ok(hr == D3D_OK, "IDirect3DDevice9_BeginScene failed (0x%08x)\n", hr);
    if(SUCCEEDED(hr)) {
        hr = IDirect3DDevice9_DrawPrimitive(device, D3DPT_POINTLIST, 0, 1);
        ok(SUCCEEDED(hr), "IDirect3DDevice9_DrawPrimitive failed (0x%08x)\n", hr);

        hr = IDirect3DDevice9_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice9_EndScene failed (0x%08x)\n", hr);
    }

    IDirect3DDevice9_SetStreamSource(device, 0, NULL, 0, 0);
    IDirect3DDevice9_SetVertexShader(device, NULL);
    IDirect3DDevice9_SetVertexDeclaration(device, NULL);

cleanup:
    if(decl) IDirect3DVertexDeclaration9_Release(decl);
    if(shader) IDirect3DVertexShader9_Release(shader);
    if(device) IDirect3DDevice9_Release(device);
    if(d3d9) IDirect3D9_Release(d3d9);
}

static inline const char *debug_d3dpool(D3DPOOL pool) {
    switch(pool) {
        case D3DPOOL_DEFAULT: return "D3DPOOL_DEFAULT";
        case D3DPOOL_SYSTEMMEM: return "D3DPOOL_SYSTEMMEM";
        case D3DPOOL_SCRATCH: return "D3DPOOL_SCRATCH";
        case D3DPOOL_MANAGED: return "D3DPOOL_MANAGED";
        default:
        return "unknown pool";
    }
}

static void test_vertex_buffer_alignment(void)
{
    IDirect3DVertexBuffer9 *buffer = NULL;
    D3DPRESENT_PARAMETERS present_parameters;
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d9;
    HWND hwnd;
    HRESULT hr;
    D3DPOOL pools[] = {D3DPOOL_DEFAULT, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH, D3DPOOL_MANAGED};
    DWORD sizes[] = {1, 4, 16, 17, 32, 33, 64, 65, 1024, 1025, 1048576, 1048577};
    unsigned int i, j;
    void *data;

    d3d9 = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(d3d9 != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d9 || !hwnd) goto cleanup;

    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = hwnd;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;

    hr = IDirect3D9_CreateDevice( d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE, "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!device)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    for(i = 0; i < (sizeof(sizes) / sizeof(sizes[0])); i++) {
        for(j = 0; j < (sizeof(pools) / sizeof(pools[0])); j++) {
            hr = IDirect3DDevice9_CreateVertexBuffer(device, sizes[i], 0, 0, pools[j], &buffer, NULL);
            if(pools[j] == D3DPOOL_SCRATCH) {
                ok(hr == D3DERR_INVALIDCALL, "Creating a D3DPOOL_SCRATCH buffer returned (0x%08x)\n", hr);
            } else {
                ok(SUCCEEDED(hr), "IDirect3DDevice9_CreateVertexBuffer failed (0x%08x). Pool = %s, size %d\n", hr,
                   debug_d3dpool(pools[j]), sizes[i]);
            }
            if(FAILED(hr)) continue;

            hr = IDirect3DVertexBuffer9_Lock(buffer, 0, 0, (void **) &data, 0);
            ok(SUCCEEDED(hr), "IDirect3DVertexBuffer9_Lock failed (0x%08x)\n", hr);
            ok(((DWORD_PTR) data & 31) == 0, "Vertex buffer start address is not 32 byte aligned(size: %d, pool: %s, data: %p)\n",
               sizes[i], debug_d3dpool(pools[j]), data);
            hr = IDirect3DVertexBuffer9_Unlock(buffer);
            ok(SUCCEEDED(hr), "IDirect3DVertexBuffer9_Unlock failed (0x%08x)\n", hr);

            if(buffer) IDirect3DVertexBuffer9_Release(buffer);
        }
    }

    cleanup:
    if(d3d9) IDirect3D9_Release(d3d9);
}

static void test_lights(void)
{
    D3DPRESENT_PARAMETERS present_parameters;
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d9;
    HWND hwnd;
    HRESULT hr;
    unsigned int i;
    BOOL enabled;
    D3DCAPS9 caps;

    d3d9 = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(d3d9 != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d9 || !hwnd) goto cleanup;

    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = hwnd;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;

    hr = IDirect3D9_CreateDevice( d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, &present_parameters, &device );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE || hr == D3DERR_INVALIDCALL,
       "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!device)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    memset(&caps, 0, sizeof(caps));
    hr = IDirect3DDevice9_GetDeviceCaps(device, &caps);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetDeviceCaps failed with %s\n", DXGetErrorString9(hr));

    for(i = 1; i <= caps.MaxActiveLights; i++) {
        hr = IDirect3DDevice9_LightEnable(device, i, TRUE);
        ok(hr == D3D_OK, "Enabling light %u failed with %s\n", i, DXGetErrorString9(hr));
        hr = IDirect3DDevice9_GetLightEnable(device, i, &enabled);
        ok(hr == D3D_OK, "GetLightEnable on light %u failed with %s\n", i, DXGetErrorString9(hr));
        ok(enabled, "Light %d is %s\n", i, enabled ? "enabled" : "disabled");
    }

    /* TODO: Test the rendering results in this situation */
    hr = IDirect3DDevice9_LightEnable(device, i + 1, TRUE);
    ok(hr == D3D_OK, "Enabling one light more than supported returned %s\n", DXGetErrorString9(hr));
    hr = IDirect3DDevice9_GetLightEnable(device, i + 1, &enabled);
    ok(hr == D3D_OK, "GetLightEnable on light %u failed with %s\n", i + 1, DXGetErrorString9(hr));
    ok(enabled, "Light %d is %s\n", i + 1, enabled ? "enabled" : "disabled");
    hr = IDirect3DDevice9_LightEnable(device, i + 1, FALSE);
    ok(hr == D3D_OK, "Disabling the additional returned %s\n", DXGetErrorString9(hr));

    for(i = 1; i <= caps.MaxActiveLights; i++) {
        hr = IDirect3DDevice9_LightEnable(device, i, FALSE);
        ok(hr == D3D_OK, "Disabling light %u failed with %s\n", i, DXGetErrorString9(hr));
    }

    cleanup:
    if(device) IDirect3DDevice9_Release(device);
    if(d3d9) IDirect3D9_Release(d3d9);
}

static void test_set_stream_source(void)
{
    D3DPRESENT_PARAMETERS present_parameters;
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d9;
    HWND hwnd;
    HRESULT hr;
    IDirect3DVertexBuffer9 *pVertexBuffer = NULL;

    d3d9 = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(d3d9 != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d9 || !hwnd) goto cleanup;

    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = hwnd;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;

    hr = IDirect3D9_CreateDevice( d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE || hr == D3DERR_INVALIDCALL,
       "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
    if(!device)
    {
        hr = IDirect3D9_CreateDevice( d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hwnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device );
        ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE, "IDirect3D9_CreateDevice failed with %s\n", DXGetErrorString9(hr));
        if(!device)
        {
            skip("Failed to create a d3d device\n");
            goto cleanup;
        }
    }

    hr = IDirect3DDevice9_CreateVertexBuffer( device, 512, 0, 0, D3DPOOL_DEFAULT, &pVertexBuffer, NULL );
    ok(hr == D3D_OK, "Failed to create a vertex buffer, hr = %s\n", DXGetErrorString9(hr));
    if (SUCCEEDED(hr)) {
        /* Some cards(Geforce 7400 at least) accept non-aligned offsets, others(radeon 9000 verified) reject it,
         * so accept both results. Wine currently rejects this to be able to optimize the vbo conversion, but writes
         * a WARN
         */
        hr = IDirect3DDevice9_SetStreamSource(device, 0, pVertexBuffer, 0, 32);
        ok(hr == D3D_OK, "Failed to set the stream source, offset 0, hr = %s\n",
           DXGetErrorString9(hr));
        hr = IDirect3DDevice9_SetStreamSource(device, 0, pVertexBuffer, 1, 32);
        ok(hr == D3DERR_INVALIDCALL || hr == D3D_OK, "Unexpected result when setting the stream source, offset 1, hr = %s\n",
           DXGetErrorString9(hr));
        hr = IDirect3DDevice9_SetStreamSource(device, 0, pVertexBuffer, 2, 32);
        ok(hr == D3DERR_INVALIDCALL || hr == D3D_OK, "Unexpected result when setting the stream source, offset 2, hr = %s\n",
           DXGetErrorString9(hr));
        hr = IDirect3DDevice9_SetStreamSource(device, 0, pVertexBuffer, 3, 32);
        ok(hr == D3DERR_INVALIDCALL || hr == D3D_OK, "Unexpected result when setting the stream source, offset 3, hr = %s\n",
           DXGetErrorString9(hr));
        hr = IDirect3DDevice9_SetStreamSource(device, 0, pVertexBuffer, 4, 32);
        ok(hr == D3D_OK, "Failed to set the stream source, offset 4, hr = %s\n",
          DXGetErrorString9(hr));
    }
    /* Try to set the NULL buffer with an offset and stride 0 */
    hr = IDirect3DDevice9_SetStreamSource(device, 0, NULL, 0, 0);
    ok(hr == D3D_OK, "Failed to set the stream source, offset 0, hr = %s\n",
       DXGetErrorString9(hr));
    hr = IDirect3DDevice9_SetStreamSource(device, 0, NULL, 1, 0);
    ok(hr == D3DERR_INVALIDCALL || hr == D3D_OK, "Unexpected result when setting the stream source, offset 1, hr = %s\n",
       DXGetErrorString9(hr));
    hr = IDirect3DDevice9_SetStreamSource(device, 0, NULL, 2, 0);
    ok(hr == D3DERR_INVALIDCALL || hr == D3D_OK, "Unexpected result when setting the stream source, offset 2, hr = %s\n",
       DXGetErrorString9(hr));
    hr = IDirect3DDevice9_SetStreamSource(device, 0, NULL, 3, 0);
    ok(hr == D3DERR_INVALIDCALL || hr == D3D_OK, "Unexpected result when setting the stream source, offset 3, hr = %s\n",
       DXGetErrorString9(hr));
    hr = IDirect3DDevice9_SetStreamSource(device, 0, NULL, 4, 0);
    ok(hr == D3D_OK, "Failed to set the stream source, offset 4, hr = %s\n",
       DXGetErrorString9(hr));

    hr = IDirect3DDevice9_SetStreamSource(device, 0, NULL, 0, 0);
    ok(hr == D3D_OK, "Failed to set the stream source, offset 4, hr = %s\n", DXGetErrorString9(hr));

    if(pVertexBuffer) IDirect3DDevice9_Release(pVertexBuffer);
cleanup:
    if(device) IDirect3DDevice9_Release(device);
    if(d3d9) IDirect3D9_Release(d3d9);
}

struct formats {
    D3DFORMAT DisplayFormat;
    D3DFORMAT BackBufferFormat;
    BOOL shouldPass;
};

struct formats r5g6b5_format_list[] =
{
    { D3DFMT_R5G6B5, D3DFMT_R5G6B5, TRUE },
    { D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, FALSE },
    { D3DFMT_R5G6B5, D3DFMT_A1R5G5B5, FALSE },
    { D3DFMT_R5G6B5, D3DFMT_X8R8G8B8, FALSE },
    { D3DFMT_R5G6B5, D3DFMT_A8R8G8B8, FALSE },
    { 0, 0, 0}
};

struct formats x1r5g5b5_format_list[] =
{
    { D3DFMT_X1R5G5B5, D3DFMT_R5G6B5, FALSE },
    { D3DFMT_X1R5G5B5, D3DFMT_X1R5G5B5, TRUE },
    { D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, TRUE },
    { D3DFMT_X1R5G5B5, D3DFMT_X8R8G8B8, FALSE },
    { D3DFMT_X1R5G5B5, D3DFMT_A8R8G8B8, FALSE },

    /* A1R5G5B5 should not be usable as a display format, it is backbuffer-only */
    { D3DFMT_A1R5G5B5, D3DFMT_R5G6B5, FALSE },
    { D3DFMT_A1R5G5B5, D3DFMT_X1R5G5B5, FALSE },
    { D3DFMT_A1R5G5B5, D3DFMT_A1R5G5B5, FALSE },
    { D3DFMT_A1R5G5B5, D3DFMT_X8R8G8B8, FALSE },
    { D3DFMT_A1R5G5B5, D3DFMT_A8R8G8B8, FALSE },
    { 0, 0, 0}
};

struct formats x8r8g8b8_format_list[] =
{
    { D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, FALSE },
    { D3DFMT_X8R8G8B8, D3DFMT_X1R5G5B5, FALSE },
    { D3DFMT_X8R8G8B8, D3DFMT_A1R5G5B5, FALSE },
    { D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE },
    { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, TRUE },

    /* A1R8G8B8 should not be usable as a display format, it is backbuffer-only */
    { D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, FALSE },
    { D3DFMT_A8R8G8B8, D3DFMT_X1R5G5B5, FALSE },
    { D3DFMT_A8R8G8B8, D3DFMT_A1R5G5B5, FALSE },
    { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, FALSE },
    { D3DFMT_A8R8G8B8, D3DFMT_A8R8G8B8, FALSE },
    { 0, 0, 0}
};

static void test_display_formats()
{
    /* Direct3D9 offers 4 display formats R5G6B5, X1R5G5B5, X8R8G8B8 and A2R10G10B10.
     * Next to these there are 6 different backbuffer formats. Only a fixed number of
     * combinations are possible in FULLSCREEN mode. In windowed mode more combinations are
     * allowed due to depth conversion and this is likely driver dependent.
     * This test checks which combinations are possible in fullscreen mode and this should not be driver dependent.
     * TODO: handle A2R10G10B10 but what hardware supports it? Parhelia? It is very rare. */

    UINT Adapter = D3DADAPTER_DEFAULT;
    D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;
    int i, nmodes;
    HRESULT hr;

    IDirect3D9 *d3d9 = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(d3d9 != NULL, "Failed to create IDirect3D9 object\n");
    if(!d3d9) return;

    nmodes = IDirect3D9_GetAdapterModeCount(d3d9, D3DADAPTER_DEFAULT, D3DFMT_R5G6B5);
    if(!nmodes) {
        skip("Display format R5G6B5 not supported, skipping\n");
    } else {
        trace("Testing display format R5G6B5\n");
        for(i=0; r5g6b5_format_list[i].DisplayFormat != 0; i++)
        {
            hr = IDirect3D9_CheckDeviceType(d3d9, Adapter, DeviceType, r5g6b5_format_list[i].DisplayFormat, r5g6b5_format_list[i].BackBufferFormat, FALSE);

            if(r5g6b5_format_list[i].shouldPass)
                ok(hr == D3D_OK, "format %d %d didn't pass with hr=%#08x\n", r5g6b5_format_list[i].DisplayFormat, r5g6b5_format_list[i].BackBufferFormat, hr);
            else
                ok(hr != D3D_OK, "format %d %d didn't pass while it was expected to\n", r5g6b5_format_list[i].DisplayFormat, r5g6b5_format_list[i].BackBufferFormat);
        }
    }

    nmodes = IDirect3D9_GetAdapterModeCount(d3d9, D3DADAPTER_DEFAULT, D3DFMT_X1R5G5B5);
    if(!nmodes) {
        skip("Display format X1R5G5B5 not supported, skipping\n");
    } else {
        trace("Testing display format X1R5G5B5\n");
        for(i=0; x1r5g5b5_format_list[i].DisplayFormat != 0; i++)
        {
            hr = IDirect3D9_CheckDeviceType(d3d9, Adapter, DeviceType, x1r5g5b5_format_list[i].DisplayFormat, x1r5g5b5_format_list[i].BackBufferFormat, FALSE);

            if(x1r5g5b5_format_list[i].shouldPass)
                ok(hr == D3D_OK, "format %d %d didn't pass with hr=%#08x\n", x1r5g5b5_format_list[i].DisplayFormat, x1r5g5b5_format_list[i].BackBufferFormat, hr);
            else
                ok(hr != D3D_OK, "format %d %d didn't pass while it was expected to\n", x1r5g5b5_format_list[i].DisplayFormat, x1r5g5b5_format_list[i].BackBufferFormat);
        }
    }

    nmodes = IDirect3D9_GetAdapterModeCount(d3d9, D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
    if(!nmodes) {
        skip("Display format X8R8G8B8 not supported, skipping\n");
    } else {
        trace("Testing display format X8R8G8B8\n");
        for(i=0; x8r8g8b8_format_list[i].DisplayFormat != 0; i++)
        {
            hr = IDirect3D9_CheckDeviceType(d3d9, Adapter, DeviceType, x8r8g8b8_format_list[i].DisplayFormat, x8r8g8b8_format_list[i].BackBufferFormat, FALSE);

            if(x8r8g8b8_format_list[i].shouldPass)
                ok(hr == D3D_OK, "format %d %d didn't pass with hr=%#08x\n", x8r8g8b8_format_list[i].DisplayFormat, x8r8g8b8_format_list[i].BackBufferFormat, hr);
            else
                ok(hr != D3D_OK, "format %d %d didn't pass while it was expected to\n", x8r8g8b8_format_list[i].DisplayFormat, x8r8g8b8_format_list[i].BackBufferFormat);
        }
    }

    if(d3d9) IDirect3D9_Release(d3d9);
}

static void test_set_material(void)
{
    D3DPRESENT_PARAMETERS present_parameters;
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d9;
    HWND hwnd;
    HRESULT hr;

    d3d9 = pDirect3DCreate9( D3D_SDK_VERSION );
    ok(d3d9 != NULL, "Failed to create IDirect3D9 object\n");
    hwnd = CreateWindow( "static", "d3d9_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d9 || !hwnd) goto cleanup;

    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = hwnd;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;

    IDirect3D9_CreateDevice( d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, &present_parameters, &device );
    if(!device)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    hr = IDirect3DDevice9_SetMaterial(device, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, got %s\n",  DXGetErrorString9(hr));

 cleanup:
    if(device) IDirect3DDevice9_Release(device);
    if(d3d9) IDirect3D9_Release(d3d9);
}

START_TEST(device)
{
    HMODULE d3d9_handle = LoadLibraryA( "d3d9.dll" );
    if (!d3d9_handle)
    {
        skip("Could not load d3d9.dll\n");
        return;
    }

    pDirect3DCreate9 = (void *)GetProcAddress( d3d9_handle, "Direct3DCreate9" );
    ok(pDirect3DCreate9 != NULL, "Failed to get address of Direct3DCreate9\n");
    if (pDirect3DCreate9)
    {
        test_display_formats();
        test_display_modes();
        test_swapchain();
        test_refcount();
        test_mipmap_levels();
        test_cursor();
        test_reset();
        test_reset();
        test_scene();
        test_limits();
        test_depthstenciltest();
        test_draw_indexed();
        test_null_stream();
        test_vertex_buffer_alignment();
        test_lights();
        test_set_stream_source();
        test_set_material();
    }
}
