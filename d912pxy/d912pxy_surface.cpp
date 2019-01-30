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

d912pxy_surface::d912pxy_surface(d912pxy_device* dev, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, INT surfType) : d912pxy_resource(dev, RTID_SURFACE, L"surface drt")
{
	isPooled = 0;
	Width = Width;// *(1 + surfType);
	Height = Height;// *(1 + surfType);

	backBuffer = 0;
	
	surf_dx9dsc.Format = Format;
	surf_dx9dsc.Width = Width;
	surf_dx9dsc.Height = Height;
	surf_dx9dsc.MultiSampleType = MultiSample;
	surf_dx9dsc.MultiSampleQuality = MultisampleQuality;
	surf_dx9dsc.Pool = D3DPOOL_DEFAULT;
	surf_dx9dsc.Type = D3DRTYPE_SURFACE;
	surf_dx9dsc.Usage = D3DUSAGE_DEPTHSTENCIL * surfType + D3DUSAGE_RENDERTARGET * (surfType == 0);

	lockDiscard = Lockable;

	m_fmt = d912pxy_helper::DXGIFormatFromDX9FMT(Format);


	srvHeapIdx = 0xFFFFFFFF;

	if (Format == 0x4C4C554E)//FOURCC NULL DX9 no rendertarget trick
	{
		subresFootprints = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)malloc(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT)*1);
		subresSizes = (size_t*)malloc(sizeof(size_t)*1);

	
		return;
	}


	if (surfType)
		d12res_zbuf(ConvertInnerDSVFormat(), 1.0f, Width, Height, GetDSVFormat());
	else {
		float white[4] = { 1.0f,1.0f,1.0f,1.0f };
		d12res_rtgt(m_fmt, white, Width, Height);
	}
	
	UpdateDescCache();

	if (!surfType)
	{
		dHeap = dev->GetDHeap(PXY_INNER_HEAP_RTV);
		dheapId = dHeap->CreateRTV(m_res, NULL);
	}
	else
	{
		dHeap = dev->GetDHeap(PXY_INNER_HEAP_DSV);

		D3D12_DEPTH_STENCIL_VIEW_DESC dsc2;
		dsc2.Format = GetDSVFormat();
		dsc2.Flags = D3D12_DSV_FLAG_NONE;
		dsc2.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsc2.Texture2D.MipSlice = 0;

		dheapId = dHeap->CreateDSV(m_res, &dsc2);
	}


}

d912pxy_surface::d912pxy_surface(d912pxy_device* dev, UINT Width, UINT Height, D3DFORMAT Format, DWORD Usage, UINT* levels, UINT arrSz) : d912pxy_resource(dev, RTID_SURFACE, L"surface texture")
{
	isPooled = 0;	
	surf_dx9dsc.Format = Format;
	surf_dx9dsc.Width = Width;
	surf_dx9dsc.Height = Height;
	surf_dx9dsc.MultiSampleType = D3DMULTISAMPLE_NONE;
	surf_dx9dsc.MultiSampleQuality = 0;
	surf_dx9dsc.Pool = D3DPOOL_DEFAULT;
	surf_dx9dsc.Type = D3DRTYPE_SURFACE;
	surf_dx9dsc.Usage = Usage;

	lockDiscard = 1;

	m_fmt = d912pxy_helper::DXGIFormatFromDX9FMT(Format);


	d12res_tex2d(Width, Height, m_fmt, (UINT16*)levels, arrSz);

	UpdateDescCache();

	initInternalBuf();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDsc;

	srvDsc.Format = m_fmt;

	srvDsc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (/*(m_fmt == DXGI_FORMAT_BC1_UNORM) || (m_fmt == DXGI_FORMAT_BC2_UNORM) || (m_fmt == DXGI_FORMAT_BC3_UNORM) ||*/ (m_fmt == DXGI_FORMAT_BC5_UNORM))
	{
		srvDsc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1, 0, 2, 3);
	}

	if ((m_fmt == DXGI_FORMAT_R8G8_UNORM))
	{
		srvDsc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 1);
	}

	if ((m_fmt == DXGI_FORMAT_R8_UNORM))
	{
		srvDsc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 3);
	}

	if (arrSz != 6)
	{

		srvDsc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

		srvDsc.Texture2DArray.MipLevels = descCache.MipLevels;
		srvDsc.Texture2DArray.MostDetailedMip = 0;
		srvDsc.Texture2DArray.PlaneSlice = 0;
		srvDsc.Texture2DArray.ResourceMinLODClamp = 0;
		srvDsc.Texture2DArray.ArraySize = arrSz;
		srvDsc.Texture2DArray.FirstArraySlice = 0;

	}
	else {
		srvDsc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

		srvDsc.TextureCube.MipLevels = descCache.MipLevels;
		srvDsc.TextureCube.MostDetailedMip = 0;		
		srvDsc.TextureCube.ResourceMinLODClamp = 0;				
	}

	for (int i = 0; i != arrSz; ++i)
	{
		for (int j = 0; j != (*levels); ++j)
		{
			UINT subresId = i * (*levels) + j;
			layers[subresId] = new d912pxy_surface_layer(
				this, 
				subresId, 
				subresFootprints[subresId].Footprint.RowPitch*subresFootprints[subresId].Footprint.Height, 				
				GetWPitchDX9(subresId),
				subresFootprints[subresId].Footprint.Width,
				mem_perPixel
			);
		}
	}
	
	dHeap = dev->GetDHeap(PXY_INNER_HEAP_SRV);
	dheapId = dHeap->CreateSRV(m_res, &srvDsc);

	backBuffer = 0;
	srvHeapIdx = 0xFFFFFFFF;


}

d912pxy_surface::d912pxy_surface(d912pxy_device* dev, ComPtr<ID3D12Resource> fromResource, D3D12_RESOURCE_STATES inState, d912pxy_swapchain* isBackBuffer): d912pxy_resource(dev, RTID_SURFACE, L"surface bb")
{	


	backBuffer = isBackBuffer;

	m_res = fromResource;

	m_res->SetName(L"swapchain bb");

	D3D12_RESOURCE_DESC origDsc = m_res->GetDesc();

	surf_dx9dsc.Format = D3DFMT_UNKNOWN;
	surf_dx9dsc.Width = (origDsc.Width & 0xFFFFFFFF) * 1;// +((origDsc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0));
	surf_dx9dsc.Height = origDsc.Height * 1;// +((origDsc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0));
	surf_dx9dsc.MultiSampleType = D3DMULTISAMPLE_NONE;
	surf_dx9dsc.MultiSampleQuality = 0;
	surf_dx9dsc.Pool = D3DPOOL_DEFAULT;
	surf_dx9dsc.Type = D3DRTYPE_SURFACE;

	surf_dx9dsc.Usage = D3DUSAGE_DEPTHSTENCIL * ((origDsc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0) +
		D3DUSAGE_RENDERTARGET * ((origDsc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0);

	lockDiscard = 0;

	m_fmt = origDsc.Format;

	stateCache = inState;

	if (origDsc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		dHeap = dev->GetDHeap(PXY_INNER_HEAP_RTV);
		dheapId = dHeap->CreateRTV(m_res, NULL);			
	}
	else if (origDsc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		dHeap = dev->GetDHeap(PXY_INNER_HEAP_DSV);
		dheapId = dHeap->CreateDSV(m_res, NULL);
	}

	UpdateDescCache();

	srvHeapIdx = 0xFFFFFFFF;


}

d912pxy_surface::~d912pxy_surface()
{


	if (dHeap)
		dHeap->FreeSlot(dheapId);

	if (srvHeapIdx == 0xFFFFFFFF)
	{
		if (m_res != nullptr)
		{
			for (int i = 0; i != descCache.DepthOrArraySize; ++i)
			{
				for (int j = 0; j != descCache.MipLevels; ++j)
				{
					UINT subresId = i * descCache.MipLevels + j;
					delete layers[subresId];
				}
			}
		}
	}

	if (srvHeapIdx != 0xFFFFFFFF)
	{
	
		m_dev->GetDHeap(PXY_INNER_HEAP_SRV)->FreeSlot(srvHeapIdx);
	}
}

#define D912PXY_METHOD_IMPL_CN d912pxy_surface

D912PXY_IUNK_IMPL

/*** IDirect3DResource9 methods ***/
D912PXY_METHOD_IMPL(GetDevice)(THIS_ IDirect3DDevice9** ppDevice) { return d912pxy_resource::GetDevice(ppDevice); }
D912PXY_METHOD_IMPL(SetPrivateData)(THIS_ REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags){ return d912pxy_resource::SetPrivateData(refguid, pData, SizeOfData, Flags); }
D912PXY_METHOD_IMPL(GetPrivateData)(THIS_ REFGUID refguid, void* pData, DWORD* pSizeOfData){ return d912pxy_resource::GetPrivateData(refguid, pData, pSizeOfData); }
D912PXY_METHOD_IMPL(FreePrivateData)(THIS_ REFGUID refguid){ return d912pxy_resource::FreePrivateData(refguid); }
D912PXY_METHOD_IMPL_(DWORD, SetPriority)(THIS_ DWORD PriorityNew){ return d912pxy_resource::SetPriority(PriorityNew); }
D912PXY_METHOD_IMPL_(DWORD, GetPriority)(THIS){ return d912pxy_resource::GetPriority(); }
D912PXY_METHOD_IMPL_(void, PreLoad)(THIS){ d912pxy_resource::PreLoad(); }
D912PXY_METHOD_IMPL_(D3DRESOURCETYPE, GetType)(THIS) { return d912pxy_resource::GetType(); }

//surface methods
D912PXY_METHOD_IMPL(GetContainer)(THIS_ REFIID riid, void** ppContainer)
{ 

	return D3DERR_INVALIDCALL; 
}

D912PXY_METHOD_IMPL(GetDesc)(THIS_ D3DSURFACE_DESC *pDesc)
{ 


	*pDesc = surf_dx9dsc;
	return D3D_OK; 
}

D912PXY_METHOD_IMPL(LockRect)(THIS_ D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags)
{ 
	return D3D_OK;
}

D912PXY_METHOD_IMPL(UnlockRect)(THIS)
{ 


	return D3D_OK; 
}

D912PXY_METHOD_IMPL(GetDC)(THIS_ HDC *phdc)
{ 

	return D3DERR_INVALIDCALL; 
}

D912PXY_METHOD_IMPL(ReleaseDC)(THIS_ HDC hdc)
{ 

	return D3D_OK; 
}

#undef D912PXY_METHOD_IMPL_CN

void d912pxy_surface::d912_rtv_clear(FLOAT * color4f, UINT NumRects, D3D12_RECT * pRects)
{
	ComPtr<ID3D12GraphicsCommandList> cq = d912pxy_s(GPUcl)->GID(CLG_SEQ);

	const float* cc4f;
	const D3D12_RECT* cpRects;
	cc4f = color4f;
	cpRects = pRects;

	cq->ClearRenderTargetView(dHeap->GetDHeapHandle(dheapId), cc4f, NumRects, cpRects);
}

void d912pxy_surface::d912_dsv_clear(FLOAT Depth, UINT8 Stencil, UINT NumRects, D3D12_RECT * pRects, D3D12_CLEAR_FLAGS flag)
{
	ComPtr<ID3D12GraphicsCommandList> cq = d912pxy_s(GPUcl)->GID(CLG_SEQ);

	const D3D12_RECT* cpRects;
	cpRects = pRects;

	cq->ClearDepthStencilView(dHeap->GetDHeapHandle(dheapId), flag, Depth, Stencil, NumRects, cpRects);
}

void d912pxy_surface::d912_rtv_clear2(FLOAT * color4f, ID3D12GraphicsCommandList * cl)
{
	const float* cc4f;
	cc4f = color4f;

	cl->ClearRenderTargetView(dHeap->GetDHeapHandle(dheapId), cc4f, 0, 0);
}

void d912pxy_surface::d912_dsv_clear2(FLOAT Depth, UINT8 Stencil, D3D12_CLEAR_FLAGS flag, ID3D12GraphicsCommandList * cl)
{
	cl->ClearDepthStencilView(dHeap->GetDHeapHandle(dheapId), flag, Depth, Stencil, 0, 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE d912pxy_surface::GetDHeapHandle()
{
	return dHeap->GetDHeapHandle(dheapId);
}

void d912pxy_surface::initInternalBuf()
{	
	//UpdateDescCache();
	
	mem_perPixel = d912pxy_helper::BitsPerPixel(m_fmt)/8;

	//surfMemRef = malloc(surfMemSz);

//
}

size_t d912pxy_surface::GetFootprintMemSz()
{


	size_t retSum = 0;

	for (int i = 0; i != subresCountCache; ++i)
		retSum += subresSizes[i];
	
	return retSum;
}

DXGI_FORMAT d912pxy_surface::GetDSVFormat()
{
	DXGI_FORMAT ret = m_fmt;
	switch (surf_dx9dsc.Format)
	{
		//case D3DFMT_INTZ:
		case D3DFMT_D32:			
			return DXGI_FORMAT_D32_FLOAT;
		case D3DFMT_INTZ:
		case D3DFMT_D24X8:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
		default:
			return ret;
	}
}

DXGI_FORMAT d912pxy_surface::GetSRVFormat()
{
	DXGI_FORMAT ret = m_fmt;
	switch (surf_dx9dsc.Format)
	{
		//case D3DFMT_INTZ:
		case D3DFMT_D32:
			return DXGI_FORMAT_R32_UINT;
		case D3DFMT_INTZ:
		case D3DFMT_D24X8:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		default:
			return ret;
	}
}

DXGI_FORMAT d912pxy_surface::ConvertInnerDSVFormat()
{
	DXGI_FORMAT ret = m_fmt;
	switch (surf_dx9dsc.Format)
	{
		//case D3DFMT_INTZ:
		case D3DFMT_D32:
			return DXGI_FORMAT_R32_TYPELESS;
		case D3DFMT_INTZ:
		case D3DFMT_D24X8:
			return DXGI_FORMAT_R24G8_TYPELESS;
		default:
			return ret;
	}
}

d912pxy_surface * d912pxy_surface::CheckRTV()
{
	if (backBuffer)
	{
		return backBuffer->GetRTBackBuffer();
	}

	return this;
}

void d912pxy_surface::DelayedLoad(void* mem, UINT lv)
{
	d912pxy_upload_item* ul = d912pxy_s(pool_upload)->GetUploadObject(subresFootprints[lv].Footprint.RowPitch*subresFootprints[lv].Footprint.Height);

	UINT wPitch = GetWPitchLV(lv);
	UINT blockHeight = subresFootprints[lv].Footprint.Height;

	switch (surf_dx9dsc.Format)
	{
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
		case D3DFMT_ATI2:
			blockHeight = blockHeight >> 2;
		default:
			;;
	}

	ul->Reconstruct(
		mem,
		subresFootprints[lv].Footprint.RowPitch,
		blockHeight,
		wPitch,
		0
	);	

	if (isPooled)
	{
		MakeGPUResident();
	}

	UploadSurfaceData(ul, lv);
	
	ThreadRef(-1);
}

void d912pxy_surface::CreateUploadBuffer(UINT id, UINT size)
{
	uploadRes[id] = d912pxy_s(pool_upload)->GetUploadObject(size);

#ifdef FORCE_DX9_PITCH_REPLAY
	mappedMem[id] = uploadRes[id]->MapRPtr();
#else
	mappedMem[id] = uploadRes[id]->MapDPtr();
#endif
}

UINT d912pxy_surface::FinalReleaseCB()
{
	if (isPooled)
	{
		if (d912pxy_s(pool_surface))
		{
			EvictFromGPU();

			d912pxy_surface* tv = this;
			d912pxy_s(pool_surface)->PoolRW(isPooled, &tv, 1);
			return 0;
		}
		else {
			return 1;
		}
	}
	else
		return 1;	
}

UINT32 d912pxy_surface::PooledAction(UINT32 use)
{
	d912pxy_s(pool_surface)->PooledActionLock();

	if (!d912pxy_comhandler::PooledAction(use))
	{
		d912pxy_s(pool_surface)->PooledActionUnLock();
		return 0;
	}

	if (use)
	{
		d12res_tex2d(surf_dx9dsc.Width, surf_dx9dsc.Height, m_fmt, &descCache.MipLevels, descCache.DepthOrArraySize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDsc;
		srvDsc.Format = m_fmt;
		srvDsc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDsc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (/*(m_fmt == DXGI_FORMAT_BC1_UNORM) || (m_fmt == DXGI_FORMAT_BC2_UNORM) || (m_fmt == DXGI_FORMAT_BC3_UNORM) ||*/ (m_fmt == DXGI_FORMAT_BC5_UNORM))
		{
			srvDsc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1, 0, 2, 3);
		}
				
		if (descCache.DepthOrArraySize != 6)
		{

			srvDsc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

			srvDsc.Texture2DArray.MipLevels = descCache.MipLevels;
			srvDsc.Texture2DArray.MostDetailedMip = 0;
			srvDsc.Texture2DArray.PlaneSlice = 0;
			srvDsc.Texture2DArray.ResourceMinLODClamp = 0;
			srvDsc.Texture2DArray.ArraySize = descCache.DepthOrArraySize;
			srvDsc.Texture2DArray.FirstArraySlice = 0;

		}
		else {
			srvDsc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

			srvDsc.TextureCube.MipLevels = descCache.MipLevels;
			srvDsc.TextureCube.MostDetailedMip = 0;
			srvDsc.TextureCube.ResourceMinLODClamp = 0;
		}
		
		dHeap->CreateSRV_at(m_res, &srvDsc, dheapId);

		for (int i = 0; i != descCache.DepthOrArraySize; ++i)
		{
			for (int j = 0; j != descCache.MipLevels; ++j)
			{
				UINT subresId = i * descCache.MipLevels + j;
				layers[subresId] = new d912pxy_surface_layer(
					this,
					subresId,
					subresFootprints[subresId].Footprint.RowPitch*subresFootprints[subresId].Footprint.Height,
					GetWPitchDX9(subresId),
					subresFootprints[subresId].Footprint.Width,
					mem_perPixel
				);
			}
		}

		backBuffer = 0;
		srvHeapIdx = 0xFFFFFFFF;//FIXME: possible crash
		lockDiscard = 1;
	}
	else {		
		m_res = nullptr;

		for (int i = 0; i != descCache.DepthOrArraySize; ++i)
		{
			for (int j = 0; j != descCache.MipLevels; ++j)
			{
				UINT subresId = i * descCache.MipLevels + j;
				delete layers[subresId];
			}
		}

		if (uploadRes[0] != 0)
		{
			uploadRes[0]->Release();
			uploadRes[0] = 0;
		}

		if (uploadRes[1] != 0)
		{
			uploadRes[1]->Release();
			uploadRes[1] = 0;
		}
	}

	d912pxy_s(pool_surface)->PooledActionUnLock();

	return 0;
}

d912pxy_surface_layer * d912pxy_surface::GetLayer(UINT32 mip, UINT32 ar)
{
	return layers[descCache.MipLevels * ar + mip];
}

UINT d912pxy_surface::GetSRVHeapId()
{
	if (descCache.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
	{
		if (surf_dx9dsc.Format == D3DFMT_NULL)
			return 0;

		if (srvHeapIdx == 0xFFFFFFFF)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC newSrv;

			newSrv.Format = GetSRVFormat();
			newSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			newSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			newSrv.Texture2D.PlaneSlice = 0;
			newSrv.Texture2D.MipLevels = 1;
			newSrv.Texture2D.MostDetailedMip = 0;
			newSrv.Texture2D.ResourceMinLODClamp = 0;

			srvHeapIdx = m_dev->GetDHeap(PXY_INNER_HEAP_SRV)->CreateSRV(m_res, &newSrv);

			if (descCache.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
			{				
				if (d912pxy_s(CMDReplay)->ViewTransit(this, D3D12_RESOURCE_STATE_DEPTH_READ))
				{
					d912pxy_s(iframe)->NoteBindedSurfaceTransit(this, 0);
				}			
			}
			else {
				//megai2: doin no transit here allows us to use surface as RTV and SRV in one time, but some drivers handle this bad
				if (d912pxy_s(CMDReplay)->ViewTransit(this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
				{
					d912pxy_s(iframe)->NoteBindedSurfaceTransit(this, 1);
				}			
			}
		}
		else {
			if (descCache.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
			{
				if (d912pxy_s(CMDReplay)->ViewTransit(this, D3D12_RESOURCE_STATE_DEPTH_READ))
				{
					d912pxy_s(iframe)->NoteBindedSurfaceTransit(this, 0);
				}		
			}
			else {
				//megai2: doin no transit here allows us to use surface as RTV and SRV in one time, but some drivers handle this bad
				if (d912pxy_s(CMDReplay)->ViewTransit(this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
				{
					d912pxy_s(iframe)->NoteBindedSurfaceTransit(this, 1);
				}
			}
		}

		return srvHeapIdx;
	}
	
	return dheapId;
}

UINT d912pxy_surface::GetWPitchDX9(UINT lv)
{
	UINT w = subresFootprints[lv].Footprint.Width;

	switch (surf_dx9dsc.Format)
	{
	case D3DFMT_ATI2:
		return max(1, ((w + 3) / 4)) * 4;
	case D3DFMT_DXT1:
		return max(1, ((w + 3) / 4)) * 8;
	case D3DFMT_DXT2:		
	case D3DFMT_DXT3:		
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return max(1, ((w + 3) / 4)) * 16;
	case D3DFMT_V8U8:
	case D3DFMT_L8:
	{

		//megai2: do not ask my why, but retrace ask for same pitch as original d3d call returned. and it somehowtricky aligned
		UINT ret = w * mem_perPixel;

		while (ret & 3)
			++ret;

		return ret;
	}
	default:
		return w * mem_perPixel;
	}
}

UINT d912pxy_surface::GetWPitchLV(UINT lv)
{
	UINT w = subresFootprints[lv].Footprint.Width;

	switch (surf_dx9dsc.Format)
	{			
	case D3DFMT_DXT1:
		return max(1, ((w + 3) / 4)) * 8;
	case D3DFMT_ATI2:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return max(1, ((w + 3) / 4)) * 16;
	case D3DFMT_V8U8:
	case D3DFMT_L8:
	{

		//megai2: do not ask my why, but retrace ask for same pitch as original d3d call returned. and it somehowtricky aligned
		UINT ret = w * mem_perPixel;

		while (ret & 3)
			++ret;

		return ret;
	}
	default:
		return w * mem_perPixel;
	}
}

void d912pxy_surface::PerformViewTypeTransit(D3D12_RESOURCE_STATES type)
{
	IFrameBarrierTrans(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, type, CLG_SEQ);
}

void d912pxy_surface::UploadSurfaceData(d912pxy_upload_item* ul, UINT lv)
{
	ComPtr<ID3D12GraphicsCommandList> cq = d912pxy_s(GPUcl)->GID(CLG_TEX);

	D3D12_RANGE offsetToSubres;

	offsetToSubres.Begin = 0;
	offsetToSubres.End = subresSizes[lv];

	D3D12_TEXTURE_COPY_LOCATION srcR, dstR;

	srcR.pResource = ul->GetResourcePtr();
	srcR.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;	
	UINT64 activeSize;
	d912pxy_s(DXDev)->GetCopyableFootprints(&m_res->GetDesc(), lv, 1, offsetToSubres.Begin, &srcR.PlacedFootprint, 0, 0, &activeSize);




	dstR.SubresourceIndex = lv;
	dstR.pResource = m_res.Get();
	dstR.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	
	D3D12_RESOURCE_BARRIER bar;

	bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	bar.Transition.pResource = m_res.Get();
	bar.Transition.StateBefore = stateCache;
	bar.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	bar.Transition.Subresource = lv;
	bar.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	cq->ResourceBarrier(1, &bar);	

	cq->CopyTextureRegion(&dstR, 0, 0, 0, &srcR, NULL);

	bar.Transition.StateAfter = stateCache;
	bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;

	cq->ResourceBarrier(1, &bar);

	ul->Release();

}

D3DSURFACE_DESC d912pxy_surface::GetDX9DescAtLevel(UINT level)
{
	D3DSURFACE_DESC ret;

	ret = surf_dx9dsc;

	ret.Height = subresFootprints[level].Footprint.Height;
	ret.Width = subresFootprints[level].Footprint.Width;

	return ret;
}
