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

UINT g_HeapIndex = 0;

d912pxy_dheap::d912pxy_dheap(d912pxy_device * dev, D3D12_DESCRIPTOR_HEAP_DESC * desc) : d912pxy_noncom(dev, L"dheap")
{
	LOG_ERR_THROW(d912pxy_s(DXDev)->CreateDescriptorHeap(desc, IID_PPV_ARGS(&heap)));

	handleSz = d912pxy_s(DXDev)->GetDescriptorHandleIncrementSize(desc->Type);

	cpuBase = heap->GetCPUDescriptorHandleForHeapStart();
	gpuBase = heap->GetGPUDescriptorHandleForHeapStart();

	slots = desc->NumDescriptors;

	

	size_t alcSize = sizeof(UINT8)*slots;

	slotFlags = (UINT8*)malloc(alcSize);
	ZeroMemory(slotFlags, alcSize);

	m_desc = desc;
	
	wchar_t buf[255];
	wsprintf(buf, L"slots / %u", g_HeapIndex);
	selfIID = g_HeapIndex;


	writeIdx = 0;
	cleanIdx = 0;
	slotsToCleanup = 0;

	heapStartCache = heap->GetGPUDescriptorHandleForHeapStart();

	++g_HeapIndex;
}

d912pxy_dheap::~d912pxy_dheap()
{
	free(slotFlags);
}

UINT d912pxy_dheap::OccupySlot()
{		
	if (!slots)
	{
		
		LOG_ERR_THROW2(-1, "dheapslots == 0");
	}

	while (slots)
	{
		UINT slotFlag = slotFlags[writeIdx];

		if (
			(slotFlag == D912PXY_DHEAP_SLOT_FREE) //slot free
		)
		{
			slotFlags[writeIdx] = D912PXY_DHEAP_SLOT_USED;
			--slots;
#ifdef FRAME_METRIC_DHEAP
#endif
			return writeIdx;
		}

		++writeIdx;
		if (writeIdx >= m_desc->NumDescriptors)
			writeIdx = 0;
	}	

	

	return -1;
}

void d912pxy_dheap::FreeSlot(UINT slot)
{
	slotFlags[slot] = D912PXY_DHEAP_SLOT_CLEANUP2;	
	++slotsToCleanup;	
}

void d912pxy_dheap::CleanupSlots(UINT count)
{	 
	//count = slotsToCleanup;
	UINT preClean = 0;
	while (slotsToCleanup && count && (preClean != slotsToCleanup))
	{
		if (slotFlags[cleanIdx] == D912PXY_DHEAP_SLOT_CLEANUP2)
		{
			--count;
			++preClean;
			slotFlags[cleanIdx] = D912PXY_DHEAP_SLOT_CLEANUP;
		} else if (slotFlags[cleanIdx] == D912PXY_DHEAP_SLOT_CLEANUP)
		{			
			++slots;
			--slotsToCleanup;
			--count;
			slotFlags[cleanIdx] = D912PXY_DHEAP_SLOT_FREE;
		}		

		++cleanIdx;		
		if (cleanIdx >= m_desc->NumDescriptors)
		{
			cleanIdx = 0;
		}
	}

#ifdef FRAME_METRIC_DHEAP
#endif
}

D3D12_CPU_DESCRIPTOR_HANDLE d912pxy_dheap::GetDHeapHandle(UINT slot)
{
	D3D12_CPU_DESCRIPTOR_HANDLE ret;
	ret.ptr = cpuBase.ptr + slot * handleSz;

	return ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE d912pxy_dheap::GetGPUDHeapHandle(UINT slot)
{
	D3D12_GPU_DESCRIPTOR_HANDLE ret;
	ret = heapStartCache;
	ret.ptr = ret.ptr + slot * handleSz;
	return ret;
}

UINT d912pxy_dheap::CreateSRV(ComPtr<ID3D12Resource> resource, D3D12_SHADER_RESOURCE_VIEW_DESC* dsc)
{
	UINT ret = OccupySlot();

	if (dsc)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC constDsc;
		constDsc = *dsc;
		d912pxy_s(DXDev)->CreateShaderResourceView(resource.Get(), &constDsc, GetDHeapHandle(ret));
	}
	else {
		d912pxy_s(DXDev)->CreateShaderResourceView(resource.Get(), NULL, GetDHeapHandle(ret));
	}



	return ret;
}

UINT d912pxy_dheap::CreateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC * dsc)
{
	UINT ret = OccupySlot();

	D3D12_CONSTANT_BUFFER_VIEW_DESC constDsc;
	constDsc = *dsc;
	d912pxy_s(DXDev)->CreateConstantBufferView(&constDsc, GetDHeapHandle(ret));
	
	return ret;
}

UINT d912pxy_dheap::CreateUAV(D3D12_UNORDERED_ACCESS_VIEW_DESC * dsc, ID3D12Resource* iRes)
{
	UINT ret = OccupySlot();

	D3D12_UNORDERED_ACCESS_VIEW_DESC constDsc;

	constDsc = *dsc;

	d912pxy_s(DXDev)->CreateUnorderedAccessView(
		iRes,
		0,
		&constDsc,
		GetDHeapHandle(ret)
	);

	return ret;
}

UINT d912pxy_dheap::CreateSampler(D3D12_SAMPLER_DESC * dsc)
{
	UINT ret = OccupySlot();

	D3D12_SAMPLER_DESC constDsc;
	constDsc = *dsc;
	d912pxy_s(DXDev)->CreateSampler(&constDsc, GetDHeapHandle(ret));

	return ret;
}

UINT d912pxy_dheap::CreateRTV(ComPtr<ID3D12Resource> resource, D3D12_RENDER_TARGET_VIEW_DESC* dsc)
{
	UINT ret = OccupySlot();

	if (dsc)
	{
		D3D12_RENDER_TARGET_VIEW_DESC constDsc;
		constDsc = *dsc;
		d912pxy_s(DXDev)->CreateRenderTargetView(resource.Get(), &constDsc, GetDHeapHandle(ret));
	}
	else {
		d912pxy_s(DXDev)->CreateRenderTargetView(resource.Get(), NULL, GetDHeapHandle(ret));
	}



	return ret;
}

UINT d912pxy_dheap::CreateDSV(ComPtr<ID3D12Resource> resource, D3D12_DEPTH_STENCIL_VIEW_DESC * dsc)
{
	UINT ret = OccupySlot();
	

	if (dsc)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC constDsc;
		constDsc = *dsc;
		d912pxy_s(DXDev)->CreateDepthStencilView(resource.Get(), &constDsc, GetDHeapHandle(ret));
	}
	else {
		d912pxy_s(DXDev)->CreateDepthStencilView(resource.Get(), NULL, GetDHeapHandle(ret));
	}



	return ret;
}

UINT d912pxy_dheap::CreateSRV_at(ComPtr<ID3D12Resource> resource, D3D12_SHADER_RESOURCE_VIEW_DESC * dsc, UINT32 slot)
{
	UINT ret = slot;

	if (dsc)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC constDsc;
		constDsc = *dsc;
		d912pxy_s(DXDev)->CreateShaderResourceView(resource.Get(), &constDsc, GetDHeapHandle(ret));
	}
	else {
		d912pxy_s(DXDev)->CreateShaderResourceView(resource.Get(), NULL, GetDHeapHandle(ret));
	}



	return ret;
}
