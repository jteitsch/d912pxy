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

d912pxy_vstream_pool::d912pxy_vstream_pool(d912pxy_device * dev) : d912pxy_pool_memcat<d912pxy_vstream*>(dev, PXY_INNDER_VSTREAM_POOL_BITIGNORE, PXY_INNDER_VSTREAM_POOL_BITLIMIT)
{
	d912pxy_s(pool_vstream) = this;
}

d912pxy_vstream_pool::~d912pxy_vstream_pool()
{
	d912pxy_s(pool_vstream) = NULL;

	for (int i = 0; i != PXY_INNDER_VSTREAM_POOL_BITCNT; ++i)
	{
		UINT tsz = 0;

		while (memTable[i]->HaveElements())
		{
			memTable[i]->GetElement()->Release();
			memTable[i]->Next();
			++tsz;
		}

		

		delete memTable[i];
	}
}

d912pxy_vstream * d912pxy_vstream_pool::GetVStreamObject(UINT size, UINT fmt, UINT isIB)
{
	d912pxy_vstream* ret = NULL;
	UINT mc = MemCatFromSize(size);



	PoolRW(mc, &ret, 0);

	if (!ret)
	{
		PoolRW(mc, &ret, 1);
	}
	else {
		ret->PooledAction(1);
	}
	
	ret->NoteFormatChange(fmt, isIB);

	return ret;
}

d912pxy_vstream * d912pxy_vstream_pool::AllocProc(UINT32 cat)
{
	return new d912pxy_vstream(m_dev, MemCatToSize(cat) , 0, 0, 0);
}
