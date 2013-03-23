/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>
#include "character.h"
#include "wall.h"

CWall::CWall(CGameWorld *pGameWorld, vec2 Pos1, vec2 Pos2, int Owner)
: CEntity(pGameWorld, NETOBJTYPE_LASER)
{
	m_From = Pos1;
	m_Pos = Pos2;
	m_Owner = Owner;
	m_IDEnd = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
	m_SpawnTick = Server()->Tick();

	if(g_Config.m_SvWallDelay == 0)
		m_Active = true;
}

void CWall::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
	if(GameServer()->GetPlayerChar(m_Owner))
		GameServer()->GetPlayerChar(m_Owner)->m_pWall = 0;
}

void CWall::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pHit = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		pHit = GameWorld()->IntersectCharacter(From, To, 2.5f, At, 0);
		if(pHit && pHit->GetPlayer()->m_Infected)
		{
			pHit->Die(m_Owner, WEAPON_WORLD);
		}
	}
}

void CWall::Tick()
{
	if(!m_Active)
	{
		if(Server()->Tick() > m_SpawnTick+Server()->TickSpeed()*g_Config.m_SvWallDelay)
		{
			m_SpawnTick = Server()->Tick();
			m_Active = true;
		}

		return;
	}

	// Check lifetime
	if(Server()->Tick() > m_SpawnTick+Server()->TickSpeed()*g_Config.m_SvWallLife)
	{
		Reset();
		return;
	}

	HitCharacter(m_From, m_Pos);
}

void CWall::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	if(m_Active)
	{
		pObj->m_X = (int) m_Pos.x;
		pObj->m_Y = (int) m_Pos.y;
		pObj->m_FromX = (int) m_From.x;
		pObj->m_FromY = (int) m_From.y;
		pObj->m_StartTick = Server()->Tick();
	}
	else //In case of inactivity mark the startpoint
	{
		pObj->m_FromY = (int) m_Pos.y;
		pObj->m_FromX = (int) m_Pos.x;
		pObj->m_Y = (int) m_Pos.y;
		pObj->m_X = (int) m_Pos.x;
		pObj->m_StartTick = Server()->Tick();
	}

	//Do an extra end point for better looking
	CNetObj_Laser *pEnd = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDEnd, sizeof(CNetObj_Laser)));
	if(!pEnd)
		return;

	pEnd->m_FromY = (int) m_From.y;
	pEnd->m_FromX = (int) m_From.x;
	pEnd->m_Y = (int) m_From.y;
	pEnd->m_X = (int) m_From.x;
	pEnd->m_StartTick = Server()->Tick();
}
