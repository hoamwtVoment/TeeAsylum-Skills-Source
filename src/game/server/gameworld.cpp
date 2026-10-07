/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "gameworld.h"
#include "entities/character.h"
#include "entity.h"
#include "gamecontext.h"
#include "player.h"
#include "teams.h"
#include <algorithm>
#include <engine/shared/config.h>
#include <utility>

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld(int Team, CGameContext *pGameServer, IGameController *pController)
{
	m_ResponsibleTeam = Team;
	m_pGameServer = pGameServer;
	m_pController = pController;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();
	m_Events.SetGameServer(pGameServer, pController);

	m_Paused = false;
	m_ResetRequested = false;
	m_AdvanceTimeStoppedEntities = true;
	m_TimeStopAdvanceTick = -1;
	for(auto &pFirstEntityType : m_apFirstEntityTypes)
		pFirstEntityType = 0;
}

CGameWorld::~CGameWorld()
{
	// try destroy all entities
	RemoveEntities();

	// delete all entities that didn't destroy themselves
	for(auto &pFirstEntityType : m_apFirstEntityTypes)
		while(pFirstEntityType)
			delete pFirstEntityType;
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? 0 : m_apFirstEntityTypes[Type];
}

bool CGameWorld::IsTimeStopped() const
{
	return m_TimeStop.Influencing(m_pServer->Tick());
}

bool CGameWorld::IsTimeStopActive() const
{
	return m_TimeStop.Active(m_pServer->Tick());
}

bool CGameWorld::AdvanceTimeStoppedEntities()
{
	if(m_TimeStopAdvanceTick != Server()->Tick())
	{
		m_TimeStopAdvanceTick = Server()->Tick();
		m_AdvanceTimeStoppedEntities = !m_Paused && m_TimeStop.AdvanceOthers(Server()->Tick());
	}
	return m_AdvanceTimeStoppedEntities;
}

bool CGameWorld::IsClientTimeStopped(int CID) const
{
	return m_TimeStop.StopsClient(CID, m_pServer->Tick());
}

bool CGameWorld::IsClientFullyTimeStopped(int CID) const
{
	return IsClientTimeStopped(CID) && m_TimeStop.FullyStopped(m_pServer->Tick());
}

void CGameWorld::EndTimeStop()
{
	m_TimeStop.End(m_pServer->Tick());
}

int CGameWorld::TimeStopVisualMillis() const
{
	return m_TimeStop.VisualMillis(m_pServer->Tick(), m_pServer->TickSpeed());
}

bool CGameWorld::IsEntityTimeStopped(CEntity *pEntity) const
{
	if(!IsTimeStopped() || m_AdvanceTimeStoppedEntities)
		return false;
	// ALL non-character entities stop, including the caster's new bullets.
	return pEntity->m_ObjType != ENTTYPE_CHARACTER ||
		IsClientTimeStopped(static_cast<CCharacter *>(pEntity)->GetPlayer()->GetCID());
}

bool CGameWorld::StartTimeStop(int Owner, int DurationTicks)
{
	CCharacter *pOwner = m_pGameServer->GetPlayerChar(Owner);
	if(m_Paused || !pOwner || !pOwner->IsAlive() || pOwner->GameWorld() != this)
		return false;
	if(m_TimeStop.Active(m_pServer->Tick()))
	{
		// The explicit attack-interval test switch is allowed to refresh the
		// same owner's World while it is active. Normal gameplay remains guarded.
		if(!pOwner->GetPlayer()->m_AsylumNoAttackInterval || m_TimeStop.Owner() != Owner)
			return false;
		m_TimeStop.Reset();
	}
	return m_TimeStop.Start(Owner, m_pServer->Tick(), DurationTicks,
		ASYLUM_WORLD_WINDUP_MS * m_pServer->TickSpeed() / 1000,
		ASYLUM_WORLD_AUDIO_LEAD_MS * m_pServer->TickSpeed() / 1000);
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(distance(pEnt->m_Pos, Pos) < Radius + pEnt->m_ProximityRadius)
		{
			if(ppEnts)
				ppEnts[Num] = pEnt;
			Num++;
			if(Num == Max)
				break;
		}
	}

	return Num;
}

CEntity *CGameWorld::ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis)
{
	// Find other players
	float ClosestRange = Radius * 2;
	CEntity *pClosest = 0;

	CEntity *p = FindFirst(Type);
	for(; p; p = p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		float Len = distance(Pos, p->m_Pos);
		if(Len < p->m_ProximityRadius + Radius)
		{
			if(Len < ClosestRange)
			{
				ClosestRange = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

void CGameWorld::InsertEntity(CEntity *pEnt)
{
#ifdef CONF_DEBUG
	for(CEntity *pCur = m_apFirstEntityTypes[pEnt->m_ObjType]; pCur; pCur = pCur->m_pNextTypeEntity)
		dbg_assert(pCur != pEnt, "err");
#endif

	// insert it
	if(m_apFirstEntityTypes[pEnt->m_ObjType])
		m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
	pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
	pEnt->m_pPrevTypeEntity = 0x0;
	m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void CGameWorld::RemoveEntity(CEntity *pEnt)
{
	// not in the list
	if(!pEnt->m_pNextTypeEntity && !pEnt->m_pPrevTypeEntity && m_apFirstEntityTypes[pEnt->m_ObjType] != pEnt)
		return;

	// remove
	if(pEnt->m_pPrevTypeEntity)
		pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
	else
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
	if(pEnt->m_pNextTypeEntity)
		pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

	// keep list traversing valid
	if(m_pNextTraverseEntity == pEnt)
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;

	pEnt->m_pNextTypeEntity = 0;
	pEnt->m_pPrevTypeEntity = 0;
}

//
void CGameWorld::Snap(int SnappingClient, int OtherMode)
{
	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->InternalSnap(SnappingClient, OtherMode);
			pEnt = m_pNextTraverseEntity;
		}

	if(OtherMode != 1) // Enable all for 0 and enable distracting for 2
		m_Events.Snap(SnappingClient);
}

void CGameWorld::OnPostSnap()
{
	m_Events.Clear();
}

void CGameWorld::Reset()
{
	m_TimeStop.Reset();
	// reset all entities
	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Reset();
			pEnt = m_pNextTraverseEntity;
		}
	RemoveEntities();

	Controller()->OnReset();
	RemoveEntities();

	m_ResetRequested = false;
}

void CGameWorld::RemoveEntities()
{
	// destroy objects marked for destruction
	for(auto *pEnt : m_apFirstEntityTypes)
		for(; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(pEnt->m_MarkedForDestroy)
			{
				RemoveEntity(pEnt);
				pEnt->Destroy();
			}
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset();
	if(!IsTimeStopActive() && m_TimeStop.Owner() >= 0)
		EndTimeStop();
	if(IsTimeStopActive())
	{
		CCharacter *pOwner = m_pGameServer->GetPlayerChar(m_TimeStop.Owner());
		if(!pOwner || !pOwner->IsAlive() || pOwner->IsDisabled() || pOwner->GetPlayer()->IsPaused() ||
			pOwner->GameWorld() != this ||
			(!Controller()->IsGameRunning() && !Controller()->IsWarmup() && !Controller()->IsGamePaused()))
			EndTimeStop();
		else if(m_Paused)
			m_TimeStop.Pause();
	}

	// One fractional virtual clock for all stopped entities: broad smoothstep
	// slowdown, then zero Tick/physics advances. Never scale stored velocities.
	AdvanceTimeStoppedEntities();
	if(!m_Paused)
	{
		// update all objects
		for(auto *pEnt : m_apFirstEntityTypes)
			for(; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				if(IsEntityTimeStopped(pEnt))
					pEnt->TickPaused();
				else
					pEnt->Tick();
				pEnt = m_pNextTraverseEntity;
			}

		for(auto *pEnt : m_apFirstEntityTypes)
			for(; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				if(!IsEntityTimeStopped(pEnt))
					pEnt->TickDefered();
				pEnt = m_pNextTraverseEntity;
			}
	}
	else
	{
		// update all objects
		for(auto *pEnt : m_apFirstEntityTypes)
			for(; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickPaused();
				pEnt = m_pNextTraverseEntity;
			}
	}

	RemoveEntities();
}

// TODO: should be more general
//CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CEntity *pNotThis)
CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, CCharacter *pNotThis, bool IgnoreSolo)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CCharacter *pClosest = 0;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		if(IgnoreSolo && p->IsSolo())
			continue;

		vec2 IntersectPos;
		if(closest_point_on_line(Pos0, Pos1, p->m_Pos, IntersectPos))
		{
			float Len = distance(p->m_Pos, IntersectPos);
			if(Len < p->m_ProximityRadius + Radius)
			{
				Len = distance(Pos0, IntersectPos);
				if(Len < ClosestLen)
				{
					p->m_HitData.m_Intersection = IntersectPos;
					p->m_HitData.m_HitDistance = Len;
					ClosestLen = Len;
					pClosest = p;
				}
			}
		}
	}

	return pClosest;
}

bool CGameWorld::IntersectThisCharacter(vec2 Pos0, vec2 Pos1, float Radius, CCharacter *pChar)
{
	vec2 IntersectPos;
	if(closest_point_on_line(Pos0, Pos1, pChar->m_Pos, IntersectPos))
	{
		float Len = distance(pChar->m_Pos, IntersectPos);
		if(Len < pChar->m_ProximityRadius + Radius)
		{
			Len = distance(Pos0, IntersectPos);
			pChar->m_HitData.m_Intersection = IntersectPos;
			pChar->m_HitData.m_HitDistance = Len;
			return true;
		}
	}

	return false;
}

bool CompareIntersectDistance(const CCharacter *pFirst, const CCharacter *pSecond)
{
	return pFirst->m_HitData.m_HitDistance < pSecond->m_HitData.m_HitDistance;
}

std::list<class CCharacter *> CGameWorld::IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, class CEntity *pNotThis, bool IgnoreSolo)
{
	std::list<CCharacter *> listOfChars;

	CCharacter *pChr = (CCharacter *)FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
	{
		if(pChr == pNotThis)
			continue;

		if(IgnoreSolo && pChr->IsSolo())
			continue;

		vec2 IntersectPos;
		if(closest_point_on_line(Pos0, Pos1, pChr->m_Pos, IntersectPos))
		{
			float Len = distance(pChr->m_Pos, IntersectPos);
			if(Len < pChr->m_ProximityRadius + Radius)
			{
				pChr->m_HitData.m_Intersection = IntersectPos;
				pChr->m_HitData.m_HitDistance = distance(Pos0, IntersectPos);
				listOfChars.push_back(pChr);
			}
		}
	}

	listOfChars.sort(CompareIntersectDistance);
	return listOfChars;
}

void CGameWorld::ReleaseHooked(int ClientID)
{
	CCharacter *pChr = (CCharacter *)CGameWorld::FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
	{
		CCharacterCore *Core = pChr->Core();
		if(Core->m_HookedPlayer == ClientID && !pChr->m_Super)
		{
			Core->m_HookedPlayer = -1;
			Core->m_HookState = HOOK_RETRACTED;
			Core->m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
			Core->m_HookState = HOOK_RETRACTED;
		}
	}
}

void CGameWorld::CreateDamageIndCircle(vec2 Pos, bool Clockwise, float Angle, int Amount, int Total, float RadiusScale, int64 Mask)
{
	float s = 3 * pi / 2 + Angle;
	float e = s + 2 * pi;
	for(int i = 0; i < Amount; i++)
	{
		float f = Clockwise ? mix(s, e, float(i) / float(Total)) : mix(e, s, float(i) / float(Total));
		CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd), Mask);
		vec2 Dir = vec2(cosf(f), sinf(f)) * (1.0f - RadiusScale) * 75.0f;

		if(pEvent)
		{
			pEvent->m_X = round_to_int(Pos.x + Dir.x);
			pEvent->m_Y = round_to_int(Pos.y + Dir.y);
			pEvent->m_Angle = f * 256.0f;
		}
	}
}

void CGameWorld::CreateDamageInd(vec2 Pos, float Angle, int Amount, int64 Mask)
{
	float a = 3 * pi / 2 + Angle;
	int s = round_to_int((a - pi / 3) * 256.0f);
	int e = round_to_int((a + pi / 3) * 256.0f);
	for(int i = 0; i < Amount; i++)
	{
		int f = mix(s, e, float(i + 1) / float(Amount + 1));
		CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd), Mask);
		if(pEvent)
		{
			pEvent->m_X = round_to_int(Pos.x);
			pEvent->m_Y = round_to_int(Pos.y);
			pEvent->m_Angle = f;
		}
	}
}

void CGameWorld::CreateHammerHit(vec2 Pos, int64 Mask)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit), Mask);
	if(pEvent)
	{
		pEvent->m_X = round_to_int(Pos.x);
		pEvent->m_Y = round_to_int(Pos.y);
	}
}

void CGameWorld::CreateExplosionParticle(vec2 Pos, int64 Mask)
{
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion), Mask);
	if(pEvent)
	{
		pEvent->m_X = round_to_int(Pos.x);
		pEvent->m_Y = round_to_int(Pos.y);
	}
}

void CGameWorld::CreateExplosion(vec2 Pos, int Owner, int Weapon, int WeaponID, int MaxDamage, bool NoKnockback, int64 Mask)
{
	Controller()->ExplosionCombatNpc(Pos, Owner, WeaponID, MaxDamage);
	// create the event
	CreateExplosionParticle(Pos, Mask);

	CEntity *apEnts[MAX_CLIENTS] = {};
	float Radius = 135.0f;
	float InnerRadius = 48.0f;
	int Num = FindEntities(Pos, Radius, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for(int i = 0; i < Num; i++)
	{
		CCharacter *pCharacter = static_cast<CCharacter *>(apEnts[i]);
		vec2 Diff = pCharacter->m_Pos - Pos;
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if(l)
			ForceDir = normalize(Diff);
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Strength;
		if(Owner < 0 || !GameServer()->m_apPlayers[Owner] || !GameServer()->m_apPlayers[Owner]->m_TuneZone)
			Strength = GameServer()->Tuning()->m_ExplosionStrength;
		else
			Strength = GameServer()->TuningList()[GameServer()->m_apPlayers[Owner]->m_TuneZone].m_ExplosionStrength;

		float Knockback = Strength * l;
		float Dmg = MaxDamage * l;

		if(int(Knockback) == 0 && int(Dmg) == 0)
			continue;

		if(NoKnockback)
			pCharacter->TakeDamage({0.0f, 0.0f}, (int)Dmg, Owner, Weapon, WeaponID, true);
		else
			pCharacter->TakeDamage(ForceDir * Knockback * 2, (int)Dmg, Owner, Weapon, WeaponID, true);
	}
}

void CGameWorld::CreatePlayerSpawn(vec2 Pos, int64 Mask)
{
	// create the event
	CNetEvent_Spawn *ev = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn), Mask);
	if(ev)
	{
		ev->m_X = round_to_int(Pos.x);
		ev->m_Y = round_to_int(Pos.y);
	}
}

void CGameWorld::CreateDeath(vec2 Pos, int ClientID, int64 Mask)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death), Mask);
	if(pEvent)
	{
		pEvent->m_X = round_to_int(Pos.x);
		pEvent->m_Y = round_to_int(Pos.y);
		pEvent->m_ClientID = ClientID;
	}
}

void CGameWorld::CreateSound(vec2 Pos, int Sound, int64 Mask)
{
	if(Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = round_to_int(Pos.x);
		pEvent->m_Y = round_to_int(Pos.y);
		pEvent->m_SoundID = Sound;
	}
}

void CGameWorld::CreateSoundGlobal(int Sound, int64 Mask)
{
	if(Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundGlobal *pEvent = (CNetEvent_SoundGlobal *)m_Events.Create(NETEVENTTYPE_SOUNDGLOBAL, sizeof(CNetEvent_SoundGlobal), Mask);
	if(pEvent)
		pEvent->m_SoundID = Sound;
}

void CGameWorld::CreateMapSound(vec2 Pos, int Sound, int64 Mask)
{
	if(Sound < 0)
		return;
	CNetEvent_MapSoundWorld *pEvent = (CNetEvent_MapSoundWorld *)m_Events.Create(NETEVENTTYPE_MAPSOUNDWORLD, sizeof(CNetEvent_MapSoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = round_to_int(Pos.x);
		pEvent->m_Y = round_to_int(Pos.y);
		pEvent->m_SoundID = Sound;
	}
}

void CGameWorld::CreateMapSoundGlobal(int Sound, int64 Mask)
{
	if(Sound < 0)
		return;
	CNetMsg_Sv_MapSoundGlobal Msg;
	Msg.m_SoundID = Sound;
	// Direct messages must be room-scoped, just like this world's snapshots.
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		if(CmaskIsSet(Mask, CID) && Controller()->GetPlayerIfInRoom(CID))
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, CID);
}
