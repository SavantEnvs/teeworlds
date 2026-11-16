// Map loading as done by the client and server: datafile + CMap::Load, CLayers::Init and collision setup.
#include "mem_storage.h"

#include <base/vmath.h>
#include <engine/map.h>
#include <game/collision.h>
#include <game/layers.h>

#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *pData, size_t Size)
{
	CMemStorage Storage(pData, Size);
	IEngineMap *pMap = CreateEngineMap();
	if(!pMap->Load("fuzz.map", &Storage))
	{
		delete pMap;
		return -1;
	}

	CLayers Layers;
	Layers.Init(0, pMap);

	// Like CGameClient/CGameContext, collision is set up unconditionally after CLayers::Init.
	CCollision Collision;
	Collision.Init(&Layers);
	const float Width = Collision.GetWidth() * 32.0f;
	const float Height = Collision.GetHeight() * 32.0f;
	volatile unsigned Sink = 0;
	for(int i = 0; i < 8; i++)
	{
		const vec2 A(Width * i / 7.0f, Height * (7 - i) / 7.0f);
		const vec2 B(Width * (7 - i) / 7.0f, Height * i / 7.0f);
		vec2 Hit, Before;
		Sink = Sink + (unsigned)Collision.IntersectLine(A, B, &Hit, &Before);
		Sink = Sink + (unsigned)Collision.CheckPoint(A) + (unsigned)Collision.GetCollisionAt(B.x, B.y);
		vec2 Pos = A, Vel(3.0f, -2.0f);
		int Bounces = 0;
		Collision.MovePoint(&Pos, &Vel, 0.5f, &Bounces);
		bool Death = false;
		Pos = B;
		Collision.MoveBox(&Pos, &Vel, vec2(28.0f, 28.0f), 0.5f, &Death);
		Sink = Sink + (unsigned)Collision.TestBox(A, vec2(28.0f, 28.0f));
	}

	for(int i = 0; i < pMap->NumItems(); i++)
	{
		int Type, ID;
		pMap->GetItem(i, &Type, &ID);
	}
	delete pMap;
	return 0;
}
