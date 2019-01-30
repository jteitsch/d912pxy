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

d912pxy_shader_pair::d912pxy_shader_pair(UINT32 nodeId, d912pxy_device* dev) : d912pxy_comhandler(dev, L"shader pair")
{	
	maxPsoId = 512;

	UINT32 msz = sizeof(d912pxy_pso_cache_item*)*maxPsoId;

	psoItems = (d912pxy_pso_cache_item**)malloc(msz);
	ZeroMemory(psoItems, msz);
}

d912pxy_shader_pair::~d912pxy_shader_pair()
{
	for (int i = 0; i != maxPsoId; ++i)
	{
		if (psoItems[i] != 0)
		{
			delete psoItems[i];
		}
	}

	free(psoItems);
}

d912pxy_pso_cache_item* d912pxy_shader_pair::GetPSOCacheData(UINT32 idx, d912pxy_trimmed_dx12_pso* dsc)
{
	if (idx >= maxPsoId)
	{
		intptr_t oldEnd = maxPsoId * sizeof(d912pxy_pso_cache_item*);
		
	

		intptr_t extendSize = ((idx - maxPsoId) + 100) * sizeof(d912pxy_pso_cache_item*);

		maxPsoId = idx + 100;
		psoItems = (d912pxy_pso_cache_item**)realloc(psoItems, maxPsoId * sizeof(d912pxy_pso_cache_item*));

		ZeroMemory((void*)((intptr_t)psoItems + oldEnd), extendSize);
	}

	d912pxy_pso_cache_item* ret = psoItems[idx];

	if (!ret)
	{		
		ret = new d912pxy_pso_cache_item(m_dev, dsc);

		d912pxy_s(psoCache)->CompileItem(ret);

		psoItems[idx] = ret;
	}

	return ret;
}
