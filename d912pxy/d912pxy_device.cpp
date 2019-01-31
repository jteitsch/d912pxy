/*
MIT License

Copyright(c) 2018-2019 megai2

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#include "stdafx.h"

using namespace Microsoft::WRL;

UINT64 d912pxy_device::apiOverhead = 0;

d912pxy_device::d912pxy_device(IDirect3DDevice9Proxy * dev) : IDirect3DDevice9Proxy(dev), d912pxy_comhandler(L"device")
{
	d912pxy_s(dev) = this;

	iframeExecTime = new Stopwatch();
	iframePrepTime = new Stopwatch();
	iframeReplTime = new Stopwatch();
	iframeSyncTime = new Stopwatch();

	m_dev = NULL;

	new d912pxy_vfs();

	d912pxy_s(vfs)->SetRoot("./d912pxy/pck");
	if (!d912pxy_s(vfs)->LoadVFS(PXY_VFS_BID_CSO, "shader_cso"))
	{
		LOG_ERR_THROW2(-1, "VFS error");
	}

	if (!d912pxy_s(vfs)->LoadVFS(PXY_VFS_BID_SHADER_PROFILE, "shader_profiles"))
	{
		LOG_ERR_THROW2(-1, "VFS error");
	}
	
	ZeroMemory(swapchains, sizeof(intptr_t)*PXY_INNER_MAX_SWAP_CHAINS);
		
	IDirect3DDevice9* tmpDev;
	LOG_ERR_THROW(dev->PostInit(&tmpDev));	
	
	LOG_ERR_THROW(tmpDev->GetDeviceCaps(&cached_dx9caps));

	//if (origD3D_create_call.pPresentationParameters->Windowed)
	LOG_ERR_THROW(tmpDev->GetDisplayMode(0, &cached_dx9displaymode));

	//tmpDev->Release();
	dev->Release();
	
	d912pxy_helper::d3d12_EnableDebugLayer();

	for (int i = 0; i != PXY_INNER_THREADID_MAX; ++i)
	{
		InitializeCriticalSection(&threadLockdEvents[i]);
	}
	InitializeCriticalSection(&threadLock);
	InitializeCriticalSection(&cleanupLock);

	ComPtr<IDXGIAdapter3> gpu = d912pxy_helper::GetAdapter();
	
	DXGI_ADAPTER_DESC2 pDesc;
	LOG_ERR_THROW(gpu->GetDesc2(&pDesc));

	gpu_totalVidmemMB = (DWORD)pDesc.DedicatedVideoMemory >> 20;

	m_d12evice = d912pxy_helper::CreateDevice(gpu);	

	m_d12evice_ptr = m_d12evice.Get();

	d912pxy_s(DXDev) = m_d12evice_ptr;

	D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT vaSizes;
	m_d12evice->CheckFeatureSupport(D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &vaSizes, sizeof(vaSizes));

	DXGI_QUERY_VIDEO_MEMORY_INFO vaMem;

	gpu->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vaMem);

	//heaps
	device_heap_config[PXY_INNER_HEAP_RTV] = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	device_heap_config[PXY_INNER_HEAP_DSV] = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	device_heap_config[PXY_INNER_HEAP_SRV] = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, PXY_INNER_MAX_IFRAME_BATCH_COUNT*10 + 1024, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
	//device_heap_config[PXY_INNER_HEAP_CBV] = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
	device_heap_config[PXY_INNER_HEAP_SPL] = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };

	for (int i = 0; i != PXY_INNER_MAX_DSC_HEAPS; ++i)
	{
		m_dheaps[i] = new d912pxy_dheap(this, &device_heap_config[i]);
	}

	mGPUque = new d912pxy_gpu_que(this, 2, PXY_INNER_MAX_CLEANUPS_PER_SYNC, PXY_INNER_MAX_IFRAME_CLEANUPS, 0);

	d912pxy_s(GPUque) = mGPUque;

	replayer = new d912pxy_replay(this);
	mShaderDB = new d912pxy_shader_db(this);

	iframe = new d912pxy_iframe(this, m_dheaps);

	d912pxy_s(iframe) = iframe;

	d912pxy_s(textureState)->SetStatePointer(&mTextureState);

	texLoader = new d912pxy_texture_loader(this);
	bufLoader = new d912pxy_buffer_loader(this);		
	
	swapchains[0] = NULL;

	//origD3D_create_call.pPresentationParameters->Windowed = !origD3D_create_call.pPresentationParameters->Windowed;

	if (!origD3D_create_call.pPresentationParameters->hDeviceWindow)
		origD3D_create_call.pPresentationParameters->hDeviceWindow = origD3D_create_call.hFocusWindow;

	if (!origD3D_create_call.pPresentationParameters->BackBufferHeight)
		origD3D_create_call.pPresentationParameters->BackBufferHeight = 1;

	if (!origD3D_create_call.pPresentationParameters->BackBufferWidth)
		origD3D_create_call.pPresentationParameters->BackBufferWidth = 1;

	swapchains[0] = new d912pxy_swapchain(
		this,
		0,
		origD3D_create_call.pPresentationParameters->hDeviceWindow,
		mGPUque->GetDXQue(),
		origD3D_create_call.pPresentationParameters->BackBufferWidth,
		origD3D_create_call.pPresentationParameters->BackBufferHeight,
		origD3D_create_call.pPresentationParameters->BackBufferCount,
		!origD3D_create_call.pPresentationParameters->Windowed,
		(origD3D_create_call.pPresentationParameters->PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE)
	);

	iframe->SetSwapper(swapchains[0]);

	new d912pxy_upload_pool(this);
	new d912pxy_vstream_pool(this);
	new d912pxy_surface_pool(this);

	d912pxy_s(thread_cleanup) = new d912pxy_cleanup_thread(this);

	iframe->Start();

	UINT uuLc = 1;
	mNullTexture = new d912pxy_surface(this, 1, 1, D3DFMT_A8B8G8R8, 0, &uuLc, 6);
	D3DLOCKED_RECT lr;

	for (int i = 0; i != 6; ++i)
	{
		mNullTexture->GetLayer(0, i)->LockRect(&lr, 0, i);
		*(UINT32*)lr.pBits = 0xFF000000;
		mNullTexture->GetLayer(0, i)->UnlockRect();
	}

	mNullTextureSRV = mNullTexture->GetSRVHeapId();

	for (int i = 0; i != 16; ++i)
		SetTexture(i, 0);

	UINT32 tmpUPbufSpace = 0xFFFF;

	mDrawUPVbuf = d912pxy_s(pool_vstream)->GetVStreamObject(tmpUPbufSpace, 0, 0)->AsDX9VB();
	mDrawUPIbuf = d912pxy_s(pool_vstream)->GetVStreamObject(tmpUPbufSpace *2, D3DFMT_INDEX16,1)->AsDX9IB();

	UINT16* ibufDt;
	mDrawUPIbuf->Lock(0, 0, (void**)&ibufDt, 0);

	for (int i = 0; i != tmpUPbufSpace; ++i)
	{
		ibufDt[i] = i;
	}

	mDrawUPIbuf->Unlock();
	mDrawUPStreamPtr = 0;

}

d912pxy_device::~d912pxy_device(void)
{	
	replayer->Finish();
	
	iframe->End();
	
	mDrawUPIbuf->Release();
	mDrawUPVbuf->Release();
	mNullTexture->Release();

	mGPUque->Flush(0);
	
	swapchains[0]->Release();
	
	//megai2: we have some tree like deletions of objects, so we must call this multiple times
	for (int i = 0; i != 100; ++i)
		mGPUque->Flush(0);

	
	delete bufLoader;
	
	delete iframe;
	
	delete mShaderDB;
	
	delete d912pxy_s(thread_cleanup);
	
	delete d912pxy_s(pool_vstream);
	
	delete d912pxy_s(pool_upload);
	
	delete d912pxy_s(pool_surface);
	
	delete mGPUque;
	
	delete replayer;
	
	delete texLoader;		
	
	delete iframeExecTime;
	delete iframePrepTime;
	delete iframeReplTime;
	delete iframeSyncTime;

		
	for (int i = 0; i != PXY_INNER_MAX_DSC_HEAPS; ++i)
		delete m_dheaps[i];
	
	delete d912pxy_s(vfs);
	
	
#ifdef _DEBUG
	d912pxy_helper::d3d12_ReportLeaks();
#endif
}

HRESULT WINAPI d912pxy_device::QueryInterface(REFIID riid, void** ppvObj)
{ 
	return d912pxy_comhandler::QueryInterface(riid, ppvObj);
}

ULONG WINAPI d912pxy_device::AddRef(void)
{ 
	return d912pxy_comhandler::AddRef();
}

ULONG WINAPI d912pxy_device::Release(void)
{ 	
	return d912pxy_comhandler::Release();
}

HRESULT WINAPI d912pxy_device::TestCooperativeLevel(void)
{
	return swapchains[0]->TestCoopLevel();
}

UINT WINAPI d912pxy_device::GetAvailableTextureMem(void)
{ 
	return gpu_totalVidmemMB; 
}

HRESULT WINAPI d912pxy_device::EvictManagedResources(void)
{ 
	//megai2: ignore this for now
	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::GetDeviceCaps(D3DCAPS9* pCaps)
{
	if (!pCaps)
		return D3DERR_INVALIDCALL;
	memcpy(pCaps, &cached_dx9caps, sizeof(D3DCAPS9));
	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode)
{ 
	if (!pMode)
		return D3DERR_INVALIDCALL;

	if (iSwapChain == 0)
	{
		if (origD3D_create_call.pPresentationParameters->Windowed)
			memcpy(pMode, &cached_dx9displaymode, sizeof(D3DDISPLAYMODE));
		else
		{
			pMode->Width = origD3D_create_call.pPresentationParameters->BackBufferWidth;
			pMode->Height = origD3D_create_call.pPresentationParameters->BackBufferHeight;
			pMode->RefreshRate = origD3D_create_call.pPresentationParameters->FullScreen_RefreshRateInHz;
			pMode->Format = origD3D_create_call.pPresentationParameters->BackBufferFormat;
		}
	} 
	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	if (!pParameters)
		return D3DERR_INVALIDCALL;
	pParameters->AdapterOrdinal = origD3D_create_call.Adapter;
	pParameters->DeviceType = origD3D_create_call.DeviceType;
	pParameters->hFocusWindow = origD3D_create_call.hFocusWindow;
	pParameters->BehaviorFlags = origD3D_create_call.BehaviorFlags;

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap)
{ 
	//megai2: not for full d3d9 porting here
	return D3DERR_INVALIDCALL;
}

void WINAPI d912pxy_device::SetCursorPosition(int X, int Y, DWORD Flags)
{ 
	SetCursorPos(X, Y);	 
}

BOOL WINAPI d912pxy_device::ShowCursor(BOOL bShow)
{ 
	//ShowCursor(bShow); <= insanity
	return true; 
}

HRESULT WINAPI d912pxy_device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain)
{ 
	//zero is always present
	for (int i = 1; i != PXY_INNER_MAX_SWAP_CHAINS; ++i)
	{
		if (swapchains[i])
			continue;
		
		swapchains[i] = new d912pxy_swapchain(this, i, pPresentationParameters->hDeviceWindow, mGPUque->GetDXQue(), pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, pPresentationParameters->BackBufferCount, !pPresentationParameters->Windowed, pPresentationParameters->PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE);

		*pSwapChain = swapchains[i];

		return D3D_OK;
	}
	return D3DERR_OUTOFVIDEOMEMORY;
}

HRESULT WINAPI d912pxy_device::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain)
{ 
	if (iSwapChain >= PXY_INNER_MAX_SWAP_CHAINS)
		return D3DERR_INVALIDCALL;

	*pSwapChain = swapchains[iSwapChain];

	return D3D_OK; 
}

UINT WINAPI d912pxy_device::GetNumberOfSwapChains(void)
{ 
	//This method returns the number of swap chains created by CreateDevice.
	return 1; 
}

HRESULT WINAPI d912pxy_device::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{ 
	API_OVERHEAD_TRACK_START(0)

	iframe->End();
	mGPUque->Flush(0);

	//pPresentationParameters->Windowed = !pPresentationParameters->Windowed;
	
	//swapchains[0]->SetFullscreen(!pPresentationParameters->Windowed);

	swapchains[0]->Resize(pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, !origD3D_create_call.pPresentationParameters->Windowed, (pPresentationParameters->PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE));
	
	iframe->Start();

	API_OVERHEAD_TRACK_END(0)
		
	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{ 
	API_OVERHEAD_TRACK_START(0)

#ifdef FRAME_METRIC_PRESENT
	iframeReplTime->Reset();
#endif

	iframe->End();

#ifdef FRAME_METRIC_PRESENT
	UINT64 replTime = iframeReplTime->Elapsed().count();
#endif

#ifdef PERFORMANCE_GRAPH_WRITE
	perfGraph->RecordPresent(iframe->GetBatchCount());
#endif

#ifdef FRAME_METRIC_PRESENT
	UINT64 prepCPUtime = iframePrepTime->Elapsed().count() - replTime;

#ifdef FRAME_METRIC_API_OVERHEAD

	API_OVERHEAD_TRACK_END(0)

	d912pxy_device::apiOverhead = 0;
#endif
	
	iframeExecTime->Reset();
#endif

	HRESULT ret = mGPUque->ExecuteCommands(1);

#ifdef FRAME_METRIC_PRESENT

	iframePrepTime->Reset();
#endif

	mDrawUPStreamPtr = 0;

	iframe->Start();
	
	return ret;
}

HRESULT WINAPI d912pxy_device::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer)
{ 	
	return swapchains[iSwapChain]->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}

HRESULT WINAPI d912pxy_device::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus)
{ 
	return swapchains[iSwapChain]->GetRasterStatus(pRasterStatus);
}

HRESULT WINAPI d912pxy_device::SetDialogBoxMode(BOOL bEnableDialogs)
{
	//ignore
	return D3D_OK;
}

void WINAPI d912pxy_device::SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp)
{ 
	API_OVERHEAD_TRACK_START(0)

	//megai2: should be handled after fullscreen changes, so skip it for now
	//if (origD3D_create_call.pPresentationParameters->Windowed)
		return;

	//get output info to convert ramp formats
	ComPtr<IDXGISwapChain4> d12sw = swapchains[iSwapChain]->GetD12swpc();
	ComPtr<IDXGIOutput> odata;
	LOG_ERR_THROW2(d12sw->GetContainingOutput(&odata), "set gamma ramp GetContainingOutput");

	DXGI_GAMMA_CONTROL_CAPABILITIES gammaCaps;
	LOG_ERR_THROW2(odata->GetGammaControlCapabilities(&gammaCaps), "set gamma ramp GetGammaControlCapabilities");

	//0 = 0
	//255 = 1.0f

	DXGI_GAMMA_CONTROL newRamp;

	newRamp.Scale.Red = 1.0f;
	newRamp.Scale.Green = 1.0f;
	newRamp.Scale.Blue = 1.0f;
	newRamp.Offset.Red = 0.0f;
	newRamp.Offset.Green = 0.0f;
	newRamp.Offset.Blue = 0.0f;
		
	float dlt = (gammaCaps.MaxConvertedValue - gammaCaps.MinConvertedValue) / gammaCaps.NumGammaControlPoints;
	float base = gammaCaps.MinConvertedValue;

	for (int i = 0; i != gammaCaps.NumGammaControlPoints; ++i)
	{
		float dxgiFI = base + dlt * i;
		for (int j = 0; j != 256; ++j)
		{
			float d9FI = j / 255.0f;
			float d9FIn = (j+1) / 255.0f;

			if ((dxgiFI >= d9FI) && (dxgiFI < d9FIn))
			{
				newRamp.GammaCurve[i].Red = pRamp->red[j] / 65535.0f;
				newRamp.GammaCurve[i].Green = pRamp->green[j] / 65535.0f;
				newRamp.GammaCurve[i].Blue = pRamp->blue[j] / 65535.0f;
				break;
			}
		}

		if (dxgiFI < 0)
		{
			newRamp.GammaCurve[i].Red = pRamp->red[0] / 65535.0f;
			newRamp.GammaCurve[i].Green = pRamp->green[0] / 65535.0f;
			newRamp.GammaCurve[i].Blue = pRamp->blue[0] / 65535.0f;
		}
		else if (dxgiFI > 1.0f) {
			newRamp.GammaCurve[i].Red = pRamp->red[255] / 65535.0f;
			newRamp.GammaCurve[i].Green = pRamp->green[255] / 65535.0f;
			newRamp.GammaCurve[i].Blue = pRamp->blue[255] / 65535.0f;
		}


	}

	LOG_ERR_THROW2(odata->SetGammaControl(&newRamp), "set gamma ramp SetGammaControl ");	

	API_OVERHEAD_TRACK_END(0)
}

void WINAPI d912pxy_device::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp)
{ 
	API_OVERHEAD_TRACK_START(0)

	//if (origD3D_create_call.pPresentationParameters->Windowed)
		return;

	//get output info to convert ramp formats
	ComPtr<IDXGISwapChain4> d12sw = swapchains[iSwapChain]->GetD12swpc();
	ComPtr<IDXGIOutput> odata;
	LOG_ERR_THROW2(d12sw->GetContainingOutput(&odata), "get gamma ramp GetContainingOutput");

	DXGI_GAMMA_CONTROL_CAPABILITIES gammaCaps;
	LOG_ERR_THROW2(odata->GetGammaControlCapabilities(&gammaCaps), "get gamma ramp GetGammaControlCapabilities");

	DXGI_GAMMA_CONTROL curRamp;
	LOG_ERR_THROW(odata->GetGammaControl(&curRamp));

	float dlt = (gammaCaps.MaxConvertedValue - gammaCaps.MinConvertedValue) / gammaCaps.NumGammaControlPoints;
	float base = gammaCaps.MinConvertedValue;

	for (int j = 0; j != 256; ++j)
	{
		float d9FI = j / 255.0f;

		for (int i = 0; i != gammaCaps.NumGammaControlPoints; ++i)
		{
			float dxgiFI = base + dlt * i;
			
			if ((dxgiFI >= d9FI) && ((dxgiFI + dlt) < d9FI))
			{
				pRamp->red[j] = (WORD)(curRamp.GammaCurve[i].Red * 65535);
				pRamp->green[j] = (WORD)(curRamp.GammaCurve[i].Green * 65535);
				pRamp->blue[j] = (WORD)(curRamp.GammaCurve[i].Blue * 65535);
				break;
			}

		}
	}

	API_OVERHEAD_TRACK_END(0)
}

HRESULT WINAPI d912pxy_device::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle)
{ 
	API_OVERHEAD_TRACK_START(0)

	void* extendedPlace = (void*)((intptr_t)malloc(sizeof(d912pxy_texture) + 8) + 8);

	*ppTexture = new (extendedPlace) d912pxy_texture(this, Width, Height, Levels, Usage, Format);

	API_OVERHEAD_TRACK_END(0)
	
	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle)
{ 
	API_OVERHEAD_TRACK_START(0)

	*ppVolumeTexture = new d912pxy_vtexture(this, Width, Height, Depth, Levels, Usage, Format);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle)
{ 
	API_OVERHEAD_TRACK_START(0)

	void* extendedPlace = (void*)((intptr_t)malloc(sizeof(d912pxy_ctexture) + 8) + 8);

	*ppCubeTexture = new (extendedPlace) d912pxy_ctexture(this, EdgeLength, Levels, Usage, Format);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	*ppVertexBuffer = d912pxy_s(pool_vstream)->GetVStreamObject(Length, FVF, 0)->AsDX9VB();

	//*ppVertexBuffer = new d912pxy_vbuf(this, Length, Usage, FVF);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	*ppIndexBuffer = d912pxy_s(pool_vstream)->GetVStreamObject(Length, Format, 1)->AsDX9IB();

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{ 
	
	
	API_OVERHEAD_TRACK_START(0)

	*ppSurface = new d912pxy_surface(this, Width, Height, Format, MultiSample, MultisampleQuality, Lockable, 0);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	*ppSurface = new d912pxy_surface(this, Width, Height, Format, MultiSample, MultisampleQuality, Discard, 1);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget)
{ 
	
	API_OVERHEAD_TRACK_START(0)
	
	if (RenderTargetIndex >= PXY_INNER_MAX_RENDER_TARGETS)
		return D3DERR_INVALIDCALL;

	d912pxy_surface* rtSurf = (d912pxy_surface*)pRenderTarget;

	iframe->BindSurface(1 + RenderTargetIndex, rtSurf);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	*ppRenderTarget = iframe->GetBindedSurface(RenderTargetIndex + 1);
	(*ppRenderTarget)->AddRef();

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{ 
		
	API_OVERHEAD_TRACK_START(0)

	iframe->BindSurface(0, (d912pxy_surface*)pNewZStencil);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	*ppZStencilSurface = iframe->GetBindedSurface(0);
	(*ppZStencilSurface)->AddRef();	

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

//scissors

HRESULT WINAPI d912pxy_device::SetScissorRect(CONST RECT* pRect) 
{ 
		
	API_OVERHEAD_TRACK_START(0)

	iframe->SetScissors((D3D12_RECT*)pRect);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::GetScissorRect(RECT* pRect) { 
		
	return D3DERR_INVALIDCALL; 
}

HRESULT WINAPI d912pxy_device::SetViewport(CONST D3DVIEWPORT9* pViewport)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	D3D12_VIEWPORT main_viewport;
	main_viewport.Height = pViewport->Height * 1.0f;
	main_viewport.Width = pViewport->Width * 1.0f;
	main_viewport.TopLeftX = pViewport->X * 1.0f;
	main_viewport.TopLeftY = pViewport->Y * 1.0f;
	main_viewport.MaxDepth = pViewport->MaxZ;
	main_viewport.MinDepth = pViewport->MinZ;

	iframe->SetViewport(&main_viewport);

	API_OVERHEAD_TRACK_END(0)
	
	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::GetViewport(D3DVIEWPORT9* pViewport)
{ 
	
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI d912pxy_device::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	/*if (State > D3DRS_BLENDOPALPHA)
		return D3DERR_INVALIDCALL;*/

	switch (State)
	{
		case D3DRS_ENABLE_D912PXY_API_HACKS:
			return 343434;
		break;
		case D3DRS_SCISSORTESTENABLE: 
			if (Value)
				d912pxy_s(iframe)->RestoreScissor();
			else 
				d912pxy_s(iframe)->IgnoreScissor();
			//		break;

		case D3DRS_STENCILREF:
			replayer->OMStencilRef(Value);			
		break; //57,   /* Reference value used in stencil test */

		case D3DRS_BLENDFACTOR:
		{
			DWORD Color = Value;

			float fvClra[4];

			for (int i = 0; i != 4; ++i)
			{
				fvClra[i] = ((Color >> (i << 3)) & 0xFF) / 255.0f;
			}

			replayer->OMBlendFac(fvClra);
		}
		break; //193,   /* D3DCOLOR used for a constant blend factor during alpha blending for devices that support D3DPBLENDCAPS_BLENDFACTOR */
		
		default:
			d912pxy_s(psoCache)->State(State,Value);
	}

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue)
{ 
	switch (State)
	{
	case D3DRS_D912PXY_ENQUEUE_PSO_COMPILE:
		d912pxy_s(psoCache)->UseWithFeedbackPtr((void**)pValue);
		break;
	case D3DRS_D912PXY_SETUP_PSO:
		d912pxy_s(psoCache)->UseCompiled((d912pxy_pso_cache_item*)pValue);
		break;
	default:
		return D3DERR_INVALIDCALL;
	}

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::ValidateDevice(DWORD* pNumPasses)
{ 
		//megai2: pretend we can do anything! YES!
	*pNumPasses = 1;
	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::SetSoftwareVertexProcessing(BOOL bSoftware)
{ 
	return D3DERR_INVALIDCALL; 
}

BOOL WINAPI d912pxy_device::GetSoftwareVertexProcessing(void) 
{ 
	return D3DERR_INVALIDCALL; 
}

//state blocks

HRESULT WINAPI d912pxy_device::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) 
{
	API_OVERHEAD_TRACK_START(0)

	d912pxy_sblock* ret = new d912pxy_sblock(this, Type);
	*ppSB = ret;

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::BeginStateBlock(void) 
{ 
	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::EndStateBlock(IDirect3DStateBlock9** ppSB) 
{ 
	API_OVERHEAD_TRACK_START(0)

	d912pxy_sblock* ret = new d912pxy_sblock(this, D3DSBT_ALL);

	*ppSB = ret;

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

//scene terminators

HRESULT WINAPI d912pxy_device::BeginScene(void)
{ 
	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::EndScene(void)
{ 
	d912pxy_s(iframe)->EndSceneReset();

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::Clear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	API_OVERHEAD_TRACK_START(0)

	if (Flags & D3DCLEAR_TARGET)
	{
		float fvColor[4] =
		{
			((Color >> 24) & 0xFF) / 255.0f,
			((Color >> 0) & 0xFF) / 255.0f,
			((Color >> 8) & 0xFF) / 255.0f,
			((Color >> 16) & 0xFF) / 255.0f
		};

		d912pxy_surface* surf = iframe->GetBindedSurface(1);

		if (surf)
			replayer->RTClear(surf, fvColor);
			//iframe->GetBindedSurface(1)->d912_rtv_clear(fvColor, Count, (D3D12_RECT*)pRects);//megai2: rect is 4 uint structure, may comply
	}

	if (Flags & (D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER))
	{
		DWORD cvtCf = ((D3D12_CLEAR_FLAG_DEPTH * ((Flags & D3DCLEAR_ZBUFFER) != 0)) | (D3D12_CLEAR_FLAG_STENCIL * ((Flags & D3DCLEAR_STENCIL) != 0)));

		d912pxy_surface* surf = iframe->GetBindedSurface(0);

		if (surf)
			replayer->DSClear(surf, Z, Stencil & 0xFF, (D3D12_CLEAR_FLAGS)cvtCf);

		//	surf->d912_dsv_clear(Z, Stencil & 0xFF, Count, (D3D12_RECT*)pRects, (D3D12_CLEAR_FLAGS)cvtCf);
	}

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

//textures

HRESULT WINAPI d912pxy_device::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture)
{ 
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI d912pxy_device::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture)
{
	API_OVERHEAD_TRACK_START(0)

	Stage = (Stage & 0xF) + 16 * ((Stage >> 4) != 0);

	UINT64 srvId = 0;//megai2: make this to avoid memory reading. but we must be assured that mNullTextureSRV is equal to this constant!

	if (pTexture)
	{
		srvId = *(UINT64*)((intptr_t)pTexture - 0x8);
		if (srvId & 0x100000000)
		{
			srvId = pTexture->GetPriority();
		}
	}
		
	mTextureState.dirty |= (1 << (Stage >> 2));
	mTextureState.texHeapID[Stage] = (UINT32)srvId;

#ifdef TRACK_SHADER_BUGS_PROFILE
	if (pTexture)
	{
		d912pxy_basetexture* btex = dynamic_cast<d912pxy_basetexture*>(pTexture);

		stageFormatsTrack[Stage] = btex->GetBaseSurface()->GetDX9DescAtLevel(0).Format;
	}
	else
		stageFormatsTrack[Stage] = D3DFMT_UNKNOWN;
#endif

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

//megai2: texture stage states are fixed pipeline and won't work if we use shaders, is that correct?

HRESULT WINAPI d912pxy_device::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue)
{ 
	
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI d912pxy_device::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{ 
		
	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue)
{ 
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI d912pxy_device::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{ 
	API_OVERHEAD_TRACK_START(0)

	
	d912pxy_s(samplerState)->ModSampler(Sampler, Type, Value);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

//drawers

HRESULT WINAPI d912pxy_device::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{ 
	
	if (1)
	{
		return D3D_OK;
	}
}

HRESULT WINAPI d912pxy_device::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{ 
	
	API_OVERHEAD_TRACK_START(0)

#ifdef _DEBUG
	if (PrimitiveType == D3DPT_TRIANGLEFAN)
	{
				return D3D_OK;
	}
#endif

	d912pxy_s(iframe)->CommitBatch(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

	/*if (mPSO->GetPShader()->GetID() == 0x7E0715D1F372444A)
	{
		float tmpFv4[4] = { -1, 0, 0, 0 };

		for (int i = 0; i != 256; ++i)
		{
			mBatch->SetShaderConstF(1, 254, 1, tmpFv4);
			iframe->CommitBatch(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
			tmpFv4[0] += (2.0f / 256.0f)*1;
		}
	}*/

#ifdef PER_BATCH_FLUSH_DEBUG
	replayer->Finish();

	iframe->End();
	mGPUque->Flush(0);

	iframe->Start();
#endif

#ifdef TRACK_SHADER_BUGS_PROFILE
	for (int i = 0; i!=32;++i)
		if (stageFormatsTrack[i] == D3DFMT_D24X8)
		{
			TrackShaderCodeBugs(PXY_INNER_SHDR_BUG_PCF_SAMPLER, i+1, d912pxy_s(psoCache)->GetPShader()->GetID());
		}

	UINT srgbState = d912pxy_s(textureState)->GetTexStage(30);
	if (srgbState)
		for (int i = 0; i != 32; ++i)
		{
			if (srgbState & 1)
			{
				if (d912pxy_s(textureState)->GetTexStage(i) != mNullTextureSRV)
				{
					TrackShaderCodeBugs(PXY_INNER_SHDR_BUG_SRGB_READ, 1, d912pxy_s(psoCache)->GetPShader()->GetID());
					break;
				}
			}
			srgbState = srgbState >> 1;
		}

	if (d912pxy_s(psoCache)->GetDX9RsValue(D3DRS_SRGBWRITEENABLE))
		TrackShaderCodeBugs(PXY_INNER_SHDR_BUG_SRGB_WRITE, 1, d912pxy_s(psoCache)->GetPShader()->GetID());

	if (d912pxy_s(psoCache)->GetDX9RsValue(D3DRS_ALPHATESTENABLE))
		TrackShaderCodeBugs(PXY_INNER_SHDR_BUG_ALPHA_TEST, 1, d912pxy_s(psoCache)->GetPShader()->GetID());

	if (d912pxy_s(psoCache)->GetDX9RsValue(D3DRS_CLIPPLANEENABLE))
	{
		UINT32 cp = d912pxy_s(psoCache)->GetDX9RsValue(D3DRS_CLIPPLANEENABLE);
		if (cp & 1)
		{
			TrackShaderCodeBugs(PXY_INNER_SHDR_BUG_CLIPPLANE0, 1, d912pxy_s(psoCache)->GetVShader()->GetID());
		}
	}
#endif

	API_OVERHEAD_TRACK_END(0)
		
	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags)
{ 
	
	return D3D_OK; 
}

//vdecl 

HRESULT WINAPI d912pxy_device::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	*ppDecl = (IDirect3DVertexDeclaration9*)(new d912pxy_vdecl(this, pVertexElements));

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	if (pDecl)
	{
		d912pxy_s(psoCache)->IAFormat((d912pxy_vdecl*)pDecl);			
	}

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl)
{ 
	
	return D3DERR_INVALIDCALL; 
}

HRESULT WINAPI d912pxy_device::CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader)
{ 
	
	API_OVERHEAD_TRACK_START(0)

	*ppShader = (IDirect3DVertexShader9*)(new d912pxy_vshader(this, pFunction, mShaderDB));

	API_OVERHEAD_TRACK_END(0)
	
	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::SetVertexShader(IDirect3DVertexShader9* pShader)
{
	
	API_OVERHEAD_TRACK_START(0)

//	if (!pShader)
	//	return D3D_OK;

	d912pxy_vshader* shd = (d912pxy_vshader*)pShader;

	d912pxy_s(psoCache)->VShader(shd);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::GetVertexShader(IDirect3DVertexShader9** ppShader)
{
	
	return D3DERR_INVALIDCALL;
}

HRESULT WINAPI d912pxy_device::CreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader)
{
	
	API_OVERHEAD_TRACK_START(0)

	*ppShader = (IDirect3DPixelShader9*)(new d912pxy_pshader(this, pFunction, mShaderDB));

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::SetPixelShader(IDirect3DPixelShader9* pShader) 
{
	
	API_OVERHEAD_TRACK_START(0)

//	if (!pShader)
	//	return D3D_OK;

	d912pxy_pshader* shd = (d912pxy_pshader*)pShader;

	d912pxy_s(psoCache)->PShader(shd);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::GetPixelShader(IDirect3DPixelShader9** ppShader) 
{
	
	return D3DERR_INVALIDCALL;
}


HRESULT WINAPI d912pxy_device::SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount)
{ 

	API_OVERHEAD_TRACK_START(0)

#ifdef _DEBUG
	if (last_vs_fvconsts < ((StartRegister + Vector4fCount) << 2))
		last_vs_fvconsts = (StartRegister + Vector4fCount) << 2;

	if (PXY_INNER_MAX_SHADER_CONSTS <= ((StartRegister + Vector4fCount) * 4))
	{
			Vector4fCount = PXY_INNER_MAX_SHADER_CONSTS/4 - StartRegister;
	}
#endif

	d912pxy_s(batch)->SetShaderConstF(0, StartRegister, Vector4fCount, (float*)pConstantData);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount){ return 0; }
HRESULT WINAPI d912pxy_device::SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount){ return 0; }

HRESULT WINAPI d912pxy_device::SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) 
{ 

	API_OVERHEAD_TRACK_START(0)

#ifdef _DEBUG
	if (last_ps_fvconsts < ((StartRegister + Vector4fCount) << 2))
		last_ps_fvconsts = (StartRegister + Vector4fCount) << 2;

	if (PXY_INNER_MAX_SHADER_CONSTS <= ((StartRegister + Vector4fCount) * 4))
	{
			Vector4fCount = PXY_INNER_MAX_SHADER_CONSTS/4 - StartRegister;
	}
#endif

	d912pxy_s(batch)->SetShaderConstF(1, StartRegister, Vector4fCount, (float*)pConstantData);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK;
}

HRESULT WINAPI d912pxy_device::SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {  return 0; }
HRESULT WINAPI d912pxy_device::SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount) { return 0; }

HRESULT WINAPI d912pxy_device::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) { return 0; }
HRESULT WINAPI d912pxy_device::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) { return 0; }
HRESULT WINAPI d912pxy_device::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount){ return 0; }

HRESULT WINAPI d912pxy_device::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) { return 0; }
HRESULT WINAPI d912pxy_device::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) { return 0; }
HRESULT WINAPI d912pxy_device::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) { return 0; }

//buffer binders

HRESULT WINAPI d912pxy_device::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride)
{ 

	API_OVERHEAD_TRACK_START(0)


	if (StreamNumber >= PXY_INNER_MAX_VBUF_STREAMS)
		return D3DERR_INVALIDCALL;

	iframe->SetVBuf((d912pxy_vbuf*)pStreamData, StreamNumber, OffsetInBytes, Stride);

	API_OVERHEAD_TRACK_END(0)
	
	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::SetStreamSourceFreq(UINT StreamNumber, UINT Divider)
{ 
	API_OVERHEAD_TRACK_START(0)


	if (StreamNumber >= PXY_INNER_MAX_VBUF_STREAMS)
		return D3DERR_INVALIDCALL;

	iframe->SetStreamFreq(StreamNumber, Divider);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::SetIndices(IDirect3DIndexBuffer9* pIndexData)
{ 

	API_OVERHEAD_TRACK_START(0)

	if (pIndexData)
		iframe->SetIBuf((d912pxy_ibuf*)pIndexData);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride) { return 0; }
HRESULT WINAPI d912pxy_device::GetStreamSourceFreq(UINT StreamNumber, UINT* Divider) { return 0; }
HRESULT WINAPI d912pxy_device::GetIndices(IDirect3DIndexBuffer9** ppIndexData){ return 0; }

//query!

HRESULT WINAPI d912pxy_device::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery)
{ 

	API_OVERHEAD_TRACK_START(0)

	*ppQuery = (IDirect3DQuery9*)new d912pxy_query(this, Type);

	API_OVERHEAD_TRACK_END(0)

	return 0; 
}

//UNIMPLEMENTED !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

HRESULT WINAPI d912pxy_device::UpdateSurface(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint) { return D3DERR_INVALIDCALL; }
HRESULT WINAPI d912pxy_device::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) { return D3DERR_INVALIDCALL; }
HRESULT WINAPI d912pxy_device::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) { return D3DERR_INVALIDCALL; }
HRESULT WINAPI d912pxy_device::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) { return D3DERR_INVALIDCALL; }
HRESULT WINAPI d912pxy_device::StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter)
{ 	
	API_OVERHEAD_TRACK_START(0)
	replayer->StretchRect((d912pxy_surface*)pSourceSurface, (d912pxy_surface*)pDestSurface);
	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}
HRESULT WINAPI d912pxy_device::ColorFill(IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color) { return D3DERR_INVALIDCALL; }
HRESULT WINAPI d912pxy_device::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) { return D3DERR_INVALIDCALL; }

HRESULT WINAPI d912pxy_device::SetClipPlane(DWORD Index, CONST float* pPlane) 
{ 
	API_OVERHEAD_TRACK_START(0)
	d912pxy_s(batch)->SetShaderConstF(1, PXY_INNER_MAX_SHADER_CONSTS_IDX - 2 - Index, 1, (float*)pPlane);  return D3D_OK;
	API_OVERHEAD_TRACK_END(0)
}

//clipping
//^ done in shaders

HRESULT WINAPI d912pxy_device::GetClipPlane(DWORD Index, float* pPlane) { return D3DERR_INVALIDCALL; }
HRESULT WINAPI d912pxy_device::SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus) { return 0; }
HRESULT WINAPI d912pxy_device::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) { return D3DERR_INVALIDCALL; }

//fixed pipe states

HRESULT WINAPI d912pxy_device::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) { return 0; }
HRESULT WINAPI d912pxy_device::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) { return 0; }
HRESULT WINAPI d912pxy_device::MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) { return 0; }

HRESULT WINAPI d912pxy_device::SetMaterial(CONST D3DMATERIAL9* pMaterial) { return 0; }
HRESULT WINAPI d912pxy_device::GetMaterial(D3DMATERIAL9* pMaterial) { return 0; }
HRESULT WINAPI d912pxy_device::SetLight(DWORD Index, CONST D3DLIGHT9* pLight) { return 0; }
HRESULT WINAPI d912pxy_device::GetLight(DWORD Index, D3DLIGHT9* pLight) { return 0; }
HRESULT WINAPI d912pxy_device::LightEnable(DWORD Index, BOOL Enable) { return 0; }
HRESULT WINAPI d912pxy_device::GetLightEnable(DWORD Index, BOOL* pEnable) { return 0; }

//palette

HRESULT WINAPI d912pxy_device::SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY* pEntries) { return 0; }
HRESULT WINAPI d912pxy_device::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) { return 0; }
HRESULT WINAPI d912pxy_device::SetCurrentTexturePalette(UINT PaletteNumber) { return 0; }
HRESULT WINAPI d912pxy_device::GetCurrentTexturePalette(UINT *PaletteNumber) { return 0; }

//npatch

HRESULT WINAPI d912pxy_device::SetNPatchMode(float nSegments) { return 0; }
float WINAPI d912pxy_device::GetNPatchMode(void) { return 0; }

HRESULT WINAPI d912pxy_device::DrawRectPatch(UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo) { return 0; }
HRESULT WINAPI d912pxy_device::DrawTriPatch(UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo) { return 0; }
HRESULT WINAPI d912pxy_device::DeletePatch(UINT Handle) { return 0; }

//megai2: you should know, that there is no apps, that can't storage their data in vertex buffers 
HRESULT WINAPI d912pxy_device::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) 
{
	API_OVERHEAD_TRACK_START(0)



	void* dstPtr;
	mDrawUPVbuf->Lock(mDrawUPStreamPtr, 0, &dstPtr, 0);
	memcpy(dstPtr, pVertexStreamZeroData, VertexStreamZeroStride * 3 * PrimitiveCount);
	mDrawUPVbuf->Unlock();
	
	d912pxy_ibuf* oi = iframe->GetIBuf();
	d912pxy_device_streamsrc oss = iframe->GetStreamSource(0);
	d912pxy_device_streamsrc ossi = iframe->GetStreamSource(1);
	
	iframe->SetIBuf((d912pxy_ibuf*)mDrawUPIbuf);
	iframe->SetVBuf((d912pxy_vbuf*)mDrawUPVbuf, 0, mDrawUPStreamPtr, VertexStreamZeroStride);	
	iframe->SetStreamFreq(0, 1);
	iframe->SetStreamFreq(1, 0);

	mDrawUPStreamPtr += PrimitiveCount * 3 * VertexStreamZeroStride;

	DrawIndexedPrimitive(PrimitiveType, 0, 0, 0, 0, PrimitiveCount);
	
	iframe->SetIBuf(oi);
	iframe->SetVBuf(oss.buffer, 0, oss.offset, oss.stride);
	iframe->SetStreamFreq(0, oss.divider);
	iframe->SetStreamFreq(1, ossi.divider);

	API_OVERHEAD_TRACK_END(0)

	return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) { 
 return D3D_OK; 
}

HRESULT WINAPI d912pxy_device::SetFVF(DWORD FVF) { return 0; }
HRESULT WINAPI d912pxy_device::GetFVF(DWORD* pFVF) { return 0; }

////////////////////////////////////////////

HRESULT d912pxy_device::PostInit(IDirect3DDevice9** realDev)
{
	//skip call to Id3d9
	return D3D_OK;
}

D3D12_HEAP_PROPERTIES d912pxy_device::GetResourceHeap(D3D12_HEAP_TYPE Type)
{
	D3D12_HEAP_PROPERTIES ret;

	ret.Type = Type;
	ret.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	ret.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	ret.CreationNodeMask = 1;
	ret.VisibleNodeMask = 1;

	return ret;
}

d912pxy_dheap * d912pxy_device::GetDHeap(UINT slot)
{
	return m_dheaps[slot];
}

void d912pxy_device::IFrameCleanupEnqeue(d912pxy_comhandler * obj)
{
	EnterCriticalSection(&cleanupLock);
	mGPUque->EnqueueCleanup(obj);
	LeaveCriticalSection(&cleanupLock);
}

void d912pxy_device::LockThread(UINT thread)
{

	LeaveCriticalSection(&threadLockdEvents[thread]);

	EnterCriticalSection(&threadLock);
	LeaveCriticalSection(&threadLock);

	EnterCriticalSection(&threadLockdEvents[thread]);
}

void d912pxy_device::InitLockThread(UINT thread)
{
	EnterCriticalSection(&threadLockdEvents[thread]);
}

void d912pxy_device::LockAsyncThreads()
{
#ifdef FRAME_METRIC_SYNC
	iframeSyncTime->Reset();	
#endif

	EnterCriticalSection(&threadLock);

	InterlockedIncrement(&threadInterruptState);

	texLoader->SignalWork();
	bufLoader->SignalWork();
	replayer->Finish();
	//iframe->PSO()->SignalWork();

	for (int i = 0; i != PXY_INNER_THREADID_MAX; ++i)
	{
		EnterCriticalSection(&threadLockdEvents[i]);			
	}
	
#ifdef FRAME_METRIC_SYNC
#endif
}

void d912pxy_device::UnLockAsyncThreads()
{
	for (int i = 0; i != PXY_INNER_THREADID_MAX; ++i)
	{
		LeaveCriticalSection(&threadLockdEvents[i]);
	}

 	InterlockedDecrement(&threadInterruptState);
	LeaveCriticalSection(&threadLock);
}

#ifdef TRACK_SHADER_BUGS_PROFILE

void d912pxy_device::TrackShaderCodeBugs(UINT type, UINT val, d912pxy_shader_uid faultyId)
{
	char buf[1024];
	sprintf(buf, "%s/%016llX.bin", d912pxy_shader_db_bugs_dir, faultyId);

	UINT32 size;
	UINT32* data = (UINT32*)d912pxy_s(vfs)->LoadFile(buf, &size, PXY_VFS_BID_SHADER_PROFILE);

	if (data == NULL)
	{
		data = (UINT32*)malloc(PXY_INNER_SHDR_BUG_FILE_SIZE);
		ZeroMemory(data, PXY_INNER_SHDR_BUG_FILE_SIZE);
		data[type] = val;

		d912pxy_s(vfs)->WriteFile(buf, data, PXY_INNER_SHDR_BUG_FILE_SIZE, PXY_VFS_BID_SHADER_PROFILE);
	}
	else {

		if (size != PXY_INNER_SHDR_BUG_FILE_SIZE)
		{
			LOG_ERR_THROW2(-1, "wrong shader profile file size");
		}

		if (data[type] != val)
		{
			data[type] = val;

			d912pxy_s(vfs)->ReWriteFile(buf, data, PXY_INNER_SHDR_BUG_FILE_SIZE, PXY_VFS_BID_SHADER_PROFILE);
		}	
	}

	free(data);

	/*
	FILE* bf = fopen(buf, "rb");

	//have a bug file, check for contents
	if (bf)
	{
		fseek(bf, 0, SEEK_END);
		int sz = ftell(bf);
		fseek(bf, 0, SEEK_SET);
		sz = sz >> 3;

		for (int i = 0; i != sz; ++i)
		{
			UINT bty;
			fread(&bty, 1, 4, bf);

			if (type == bty)
			{
				UINT bva;
				fread(&bva, 1, 4, bf);

				if (bva == val)
				{
					fclose(bf);
					return;
				}
			}
			else
				fseek(bf, 4, SEEK_CUR);			
		}

		fclose(bf);
	}

	bf = fopen(buf, "ab");

	fwrite(&type, 1, 4, bf);
	fwrite(&val, 1, 4, bf);

	fflush(bf);
	fclose(bf);*/
}

#endif

