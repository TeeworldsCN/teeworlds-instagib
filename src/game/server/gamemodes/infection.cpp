/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include "infection.h"


CGameControllerINF::CGameControllerINF(CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pGameType = "Infection";
	m_pConsole = GameServer()->Console();
}

bool CGameControllerINF::GameStarted()
{
	return m_GameStarted;
}

void CGameControllerINF::Tick()
{
	IGameController::Tick();
	//DoWincheck is already called in IGam..::Tick()

	int PlayerIngame = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			PlayerIngame++;
	}

	if(PlayerIngame < 3)
		if(Server()->Tick() % Server()->TickSpeed() == 0) // Send notification every second
		{
			GameServer()->SendBroadcast("You need at least 3 players to start a game", -1);
			return;
		}
	//TODO: Restart the game when map changes or similar things

	//choose random start zombie
	if(!m_GameStarted)
	{
		if(Server()->Tick() > m_RoundStartTick+Server()->TickSpeed()*g_Config.m_SvStartDelay)
		{
			bool Done = false;
			do
			{
				int Rand = rand_num() % (MAX_CLIENTS-1);
				CPlayer *pP = GameServer()->m_apPlayers[Rand];
				if(pP && pP->GetTeam() != TEAM_SPECTATORS)
				{
					pP->m_StartZombie = true;
					m_GameStarted = true;
					GameServer()->SendBroadcast("You have been chosen as start zombie!", Rand);
					if(pP->GetCharacter())
						pP->GetCharacter()->Infect(pP->GetCID(), vec2(0, 0), false);

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "%d: %s chosen as start zombie", Rand, Server()->ClientName(Rand));
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Infect", aBuf);
					Done = true;
				}
			} while(!Done);
		}
	}
}

void CGameControllerINF::OnCharacterSpawn(CCharacter *pChr)
{
	pChr->IncreaseHealth(10);

	if(pChr->GetPlayer()->m_Infected)
	{
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
	}
	else
	{
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
		pChr->GiveWeapon(WEAPON_GUN, 10);
	}

	OnPlayerInfoChange(pChr->GetPlayer());
}

int CGameControllerINF::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	if(!pKiller || !m_GameStarted)
		return 0;

	// Being evil
	if(Weapon == WEAPON_SELF)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*3.0f;

	if(pVictim->GetPlayer()->m_Infected) // Zombie killed
	{
		// Make explosion and infect other players
		if((g_Config.m_SvZombiesExplode == 1 && pVictim->GetPlayer()->m_StartZombie) || g_Config.m_SvZombiesExplode == 2)
		{
			for(int i = 0; i < 5; i++)
				GameServer()->CreateExplosion(pVictim->GetCore()->m_Pos, pVictim->GetPlayer()->GetCID(), WEAPON_HAMMER, true);

			//Infect humans around zombies
			CCharacter *apEnts[MAX_CLIENTS];
			float Radius = 135.0f * 1.2f;
			float InnerRadius = 48.0f * 1.2f;
			int Num = GameServer()->m_World.FindEntities(pVictim->GetCore()->m_Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Num; i++)
			{
				vec2 Diff = apEnts[i]->GetCore()->m_Pos - pVictim->GetCore()->m_Pos;
				vec2 ForceDir = normalize(Diff);
				float l = length(Diff);
				l = 1-clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);
				float Dmg = 6 * l;
				if((int)Dmg)
				{
					apEnts[i]->Infect(pVictim->GetPlayer()->GetCID(), ForceDir*Dmg*10);
				}
			}
		}

		if(pVictim->GetPlayer() != pKiller)
		{
			pKiller->m_Score++;
			pKiller->m_Kills++;

			if(pKiller->m_Kills > 0 && pKiller->m_Kills % g_Config.m_SvAirstrikeKills == 0)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), g_Config.m_SvAirstrikeText, Server()->ClientName(pKiller->GetCID()));
				GameServer()->SendChatTarget(-1, aBuf);
				GameServer()->SendBroadcast("You earned an airstrike. Use hammer to use it", pKiller->GetCID());
			}
		}
	}
	else // human killed
	{
		pVictim->GetPlayer()->m_Kills = 0; // reset the humans kills
		pVictim->GetPlayer()->m_Infected = true;

		if(pVictim->GetPlayer() != pKiller)
		{
			pKiller->m_Score++;
			pKiller->m_Kills++;

			if(pKiller->m_Kills == g_Config.m_SvSuperjumpKills)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), g_Config.m_SvSuperjumpText, Server()->ClientName(pKiller->GetCID()));
				GameServer()->SendChatTarget(-1, aBuf);
				GameServer()->SendBroadcast("You earned superjump, hold jump to use", pKiller->GetCID());
				if(pKiller->GetCharacter())
					 pKiller->GetCharacter()->GetCore()->m_HasSuperjump = true;
			}
		}
	}

	OnPlayerInfoChange(pVictim->GetPlayer());
	return 0;
}

void CGameControllerINF::OnPlayerInfoChange(CPlayer *pP)
{
	pP->m_TeeInfos.m_UseCustomColor = 1;
	if(pP->m_Infected)
	{
		/* 391726, 1769216, 65310, 9240320 */
		pP->m_TeeInfos.m_ColorBody = 0x2EFA05;
		pP->m_TeeInfos.m_ColorFeet = 0x2EFA05;
	}
	else
	{
		pP->m_TeeInfos.m_ColorBody = 0xFF;
		pP->m_TeeInfos.m_ColorFeet = 0xFF;
	}
}

void CGameControllerINF::StartRound()
{
	IGameController::StartRound();

	m_GameStarted = false;
	m_ZombiesWon = false;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pP = GameServer()->m_apPlayers[i];
		if(!pP)
			continue;

		pP->m_Infected = false;
		pP->m_StartZombie = false;
		pP->m_Kills = 0;
		pP->m_HasSuperjump = false;
	}
}

void CGameControllerINF::EndRound()
{
	IGameController::EndRound();
	if(m_ZombiesWon)
		GameServer()->SendBroadcast("Zombies won!", -1);
	else
		GameServer()->SendBroadcast("Humans won!", -1);
}

void CGameControllerINF::DoWincheck()
{
	if(m_GameOverTick == -1)
	{
		int Zombies, PlayerIngame;
		for(int i = Zombies = PlayerIngame = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameServer()->m_apPlayers[i])
				continue;

			if(GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			{
				PlayerIngame++;
				if(GameServer()->m_apPlayers[i]->m_Infected == true)
					Zombies++;
			}
		}

		// All ingame players are Zombies
		if(PlayerIngame == Zombies)
		{
			m_ZombiesWon = true;
			EndRound();
		}
		else
			IGameController::DoWincheck();
	}
}

bool CGameControllerINF::CanSpawn(int Team, vec2 *pPos, int ClientID)
{
	CSpawnEval Eval;

	// spectators can't spawn
	if(Team == TEAM_SPECTATORS)
		return false;

	Team = GameServer()->m_apPlayers[ClientID]->m_Infected ? TEAM_RED : TEAM_BLUE;

	Eval.m_FriendlyTeam = Team;

	// first try own team spawn, then normal spawn and then enemy
	EvaluateSpawnType(&Eval, 1+(Team&1));
	if(!Eval.m_Got)
	{
		EvaluateSpawnType(&Eval, 0);
		if(!Eval.m_Got)
			EvaluateSpawnType(&Eval, 1+((Team+1)&1));
	}

	*pPos = Eval.m_Pos;
	return Eval.m_Got;
}

bool CGameControllerINF::IsFriendlyFire(int ClientID1, int ClientID2)
{
	if(ClientID1 == ClientID2)
		return false;

	if(!GameServer()->m_apPlayers[ClientID1] || !GameServer()->m_apPlayers[ClientID2])
		return false;

	return GameServer()->m_apPlayers[ClientID1]->m_Infected == GameServer()->m_apPlayers[ClientID2]->m_Infected;
}
