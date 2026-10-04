/* Tee Asylum god-tier (★) item effects: short sound cues and summoned entities. */
#include "asylum_fx.h"

#include <engine/map.h>
#include <game/generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/entities/textentity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/huntern.h>
#include <game/server/player.h>

namespace
{
// No synthesized clips or layered sound sequences. Keep just one stock cue.
const int gs_aMemeSounds[] = {
    SOUND_HAMMER_HIT, SOUND_GRENADE_EXPLODE, SOUND_CTF_CAPTURE, SOUND_HAMMER_FIRE,
    SOUND_TEE_CRY, SOUND_PLAYER_AIRJUMP, SOUND_WEAPON_NOAMMO, SOUND_PLAYER_PAIN_LONG,
    SOUND_CTF_GRAB_EN, SOUND_PICKUP_ARMOR, SOUND_NINJA_FIRE, SOUND_PLAYER_SKID,
    SOUND_LASER_BOUNCE, SOUND_CHAT_SERVER, SOUND_BODY_LAND, SOUND_GRENADE_EXPLODE};
static_assert(sizeof(gs_aMemeSounds) / sizeof(gs_aMemeSounds[0]) == NUM_ASYLUM_MEMES, "Cue table mismatch");

const int TSAR_FUSE_TICKS = 75;

vec2 UnitVec(float Angle)
{
	return vec2(cosf(Angle), sinf(Angle));
}

// A laser whose start tick is renewed every snapshot is drawn at full width.
void SnapLine(IServer *pServer, int ID, vec2 From, vec2 To)
{
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(pServer->SnapNewItem(NETOBJTYPE_LASER, ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;
	pObj->m_X = round_to_int(To.x);
	pObj->m_Y = round_to_int(To.y);
	pObj->m_FromX = round_to_int(From.x);
	pObj->m_FromY = round_to_int(From.y);
	pObj->m_StartTick = pServer->Tick();
}

class CRhythmNote : public CEntity
{
	int m_Owner;
	int m_WeaponID;
	int m_Note;
	int m_Damage;
	float m_Force;
	vec2 m_Direction;
	int m_Age;
	int m_aIDs[7];

public:
	CRhythmNote(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, vec2 Direction, int Note, int Damage, float Force) :
		CEntity(pWorld, CGameWorld::ENTTYPE_CUSTOM, Pos), m_Owner(Owner), m_WeaponID(WeaponID), m_Note(Note), m_Damage(Damage), m_Force(Force), m_Direction(Direction), m_Age(0)
	{
		for(int &ID : m_aIDs)
			ID = Server()->SnapNewID();
		GameWorld()->InsertEntity(this);
	}
	~CRhythmNote() override
	{
		for(int ID : m_aIDs)
			Server()->SnapFreeID(ID);
	}
	void Reset() override { m_MarkedForDestroy = true; }
	void Tick() override
	{
		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		if(!pOwner || !pOwner->IsAlive() || pOwner->GameWorld() != GameWorld() || ++m_Age > Server()->TickSpeed())
		{
			m_MarkedForDestroy = true;
			return;
		}
		const vec2 Previous = m_Pos;
		vec2 To = m_Pos + m_Direction * 18.0f;
		const bool Wall = GameServer()->Collision()->IntersectLine(Previous, To, &To, nullptr) != 0;
		const std::list<CCharacter *> Targets = GameWorld()->IntersectedCharacters(Previous, To, 10.0f, pOwner);
		for(CCharacter *pTarget : Targets)
		{
			if(pOwner->IsSolo() || !AsylumCanAffect(this, m_Owner, pTarget, m_WeaponID) ||
				GameServer()->Collision()->IntersectLine(Previous, pTarget->m_Pos, nullptr, nullptr))
				continue;
			pTarget->TakeDamage(m_Direction * m_Force, m_Damage, m_Owner, WEAPON_GUN, m_WeaponID, false);
			GameWorld()->CreateHammerHit(pTarget->m_Pos);
			m_MarkedForDestroy = true;
			return;
		}
		m_Pos = To;
		if(Wall || GameLayerClipped(m_Pos))
		{
			GameWorld()->CreateHammerHit(m_Pos);
			m_MarkedForDestroy = true;
		}
	}
	bool NetworkClipped(int SnappingClient) override
	{
		return NetworkPointClipped(SnappingClient, m_Pos, vec2(32.0f, 32.0f));
	}
	void Snap(int SnappingClient, int OtherMode) override
	{
		if(OtherMode)
			return;
		// FNF note direction cycles left/down/up/right; travel follows the aim.
		const vec2 aDirections[] = {vec2(-1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(0.0f, -1.0f), vec2(1.0f, 0.0f)};
		const vec2 Forward = aDirections[m_Note % 4];
		const vec2 Side(-Forward.y, Forward.x);
		const vec2 aOutline[] = {vec2(20.0f, 0.0f), vec2(2.0f, -18.0f), vec2(2.0f, -7.0f),
			vec2(-17.0f, -7.0f), vec2(-17.0f, 7.0f), vec2(2.0f, 7.0f), vec2(2.0f, 18.0f)};
		for(int i = 0; i < 7; ++i)
		{
			const vec2 From = m_Pos + Forward * aOutline[i].x + Side * aOutline[i].y;
			const vec2 Next = aOutline[(i + 1) % 7];
			SnapLine(Server(), m_aIDs[i], From, m_Pos + Forward * Next.x + Side * Next.y);
		}
	}
};

class CBladeStorm : public CEntity
{
	enum
	{
		NUM_BLADES = 5,
		OUT_TICKS = 18,
		MAX_TICKS = 70,
	};
	int m_Owner;
	int m_WeaponID;
	int m_Damage;
	vec2 m_Start;
	vec2 m_Target;
	int m_Age;
	bool m_Returning;
	int m_aIDs[NUM_BLADES];
	int m_aNextHit[MAX_CLIENTS];

public:
	CBladeStorm(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, vec2 Target, int Damage) :
		CEntity(pWorld, CGameWorld::ENTTYPE_CUSTOM, Pos), m_Owner(Owner), m_WeaponID(WeaponID), m_Damage(Damage),
		m_Start(Pos), m_Target(Target), m_Age(0), m_Returning(false)
	{
		for(int &ID : m_aIDs)
			ID = Server()->SnapNewID();
		for(int &Tick : m_aNextHit)
			Tick = 0;
		GameWorld()->InsertEntity(this);
	}

	~CBladeStorm() override
	{
		for(int ID : m_aIDs)
			Server()->SnapFreeID(ID);
	}

	void Reset() override { m_MarkedForDestroy = true; }
	void TickPaused() override
	{
		for(int &Tick : m_aNextHit)
			if(Tick > 0)
				++Tick;
	}

	void Tick() override
	{
		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		if(!pOwner || !pOwner->IsAlive() || pOwner->GameWorld() != GameWorld())
		{
			m_MarkedForDestroy = true;
			return;
		}
		++m_Age;
		if(!m_Returning)
		{
			const float Progress = minimum(1.0f, m_Age / (float)OUT_TICKS);
			m_Pos = m_Start + (m_Target - m_Start) * (1.0f - (1.0f - Progress) * (1.0f - Progress));
			m_Returning = m_Age >= OUT_TICKS;
		}
		else
		{
			const vec2 Delta = pOwner->m_Pos - m_Pos;
			const float Dist = length(Delta);
			if(Dist < 32.0f || m_Age >= MAX_TICKS)
			{
				m_MarkedForDestroy = true;
				return;
			}
			m_Pos += Delta * minimum(1.0f, 34.0f / Dist);
		}
		if(m_Age % 6 == 0)
			GameWorld()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH);

		CCharacter *apTargets[MAX_CLIENTS];
		const int Num = GameWorld()->FindEntities(m_Pos, 56.0f, (CEntity **)apTargets, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apTargets[i];
			const int CID = pTarget->GetPlayer()->GetCID();
			if(m_aNextHit[CID] > Server()->Tick() || !AsylumCanAffect(this, m_Owner, pTarget, m_WeaponID))
				continue;
			m_aNextHit[CID] = Server()->Tick() + Server()->TickSpeed() / 4;
			const vec2 Delta = pTarget->m_Pos - m_Pos;
			const vec2 Push = length(Delta) > 0.0f ? normalize(Delta) : vec2(0.0f, -1.0f);
			GameWorld()->CreateHammerHit(pTarget->m_Pos);
			GameWorld()->CreateSound(pTarget->m_Pos, SOUND_NINJA_HIT);
			pTarget->TakeDamage(Push * 6.0f + vec2(0.0f, -2.0f), m_Damage, m_Owner, WEAPON_NINJA, m_WeaponID, false);
		}
	}

	void Snap(int SnappingClient, int OtherMode) override
	{
		if(OtherMode)
			return;
		const float Spin = m_Age * 0.55f;
		for(int i = 0; i < NUM_BLADES; ++i)
		{
			const vec2 Dir = UnitVec(Spin + 2.0f * pi * i / NUM_BLADES);
			SnapLine(Server(), m_aIDs[i], m_Pos + Dir * 12.0f, m_Pos + Dir * 50.0f);
		}
	}
};

class CTsarBomb : public CEntity
{
	int m_Owner;
	int m_WeaponID;
	int m_Age;
	int m_ID;

	void Ring(int Count, float Radius, int Damage, float AngleOffset)
	{
		for(int i = 0; i < Count; ++i)
			GameWorld()->CreateExplosion(m_Pos + UnitVec(AngleOffset + 2.0f * pi * i / Count) * Radius, m_Owner, WEAPON_GRENADE, m_WeaponID, Damage, false);
		GameWorld()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE);
	}

public:
	CTsarBomb(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos) :
		CEntity(pWorld, CGameWorld::ENTTYPE_CUSTOM, Pos), m_Owner(Owner), m_WeaponID(WeaponID), m_Age(0)
	{
		m_ID = Server()->SnapNewID();
		GameWorld()->InsertEntity(this);
		AsylumPlayMeme(GameWorld(), ASYLUM_MEME_ALARM, m_Pos);
	}

	~CTsarBomb() override { Server()->SnapFreeID(m_ID); }

	void Reset() override { m_MarkedForDestroy = true; }

	void Tick() override
	{
		++m_Age;
		if(m_Age < TSAR_FUSE_TICKS)
		{
			if(m_Age % 25 == 1)
			{
				const char aCount[2] = {(char)('3' - m_Age / 25), 0};
				AsylumShowText(GameWorld(), m_Pos - vec2(0.0f, 90.0f), aCount, 0.45f, 10);
			}
			// Mark the lethal area while the fuse burns.
			if(m_Age % 25 == 1)
				for(int i = 0; i < 16; ++i)
					GameWorld()->CreateHammerHit(m_Pos + UnitVec(2.0f * pi * i / 16) * 220.0f);
			if(m_Age % 8 == 0)
				GameWorld()->CreateDamageIndCircle(m_Pos, true, m_Age * 0.3f, 6, 6, 1.5f);
			return;
		}
		if(m_Age == TSAR_FUSE_TICKS)
		{
			GameWorld()->CreateExplosion(m_Pos, m_Owner, WEAPON_GRENADE, m_WeaponID, 80, false);
			AsylumPlayMeme(GameWorld(), ASYLUM_MEME_VINE_BOOM, m_Pos, true);
		}
		else if(m_Age == TSAR_FUSE_TICKS + 3)
			Ring(8, 110.0f, 36, 0.0f);
		else if(m_Age == TSAR_FUSE_TICKS + 7)
		{
			Ring(12, 220.0f, 26, pi / 12.0f);
			m_MarkedForDestroy = true;
		}
	}

	void Snap(int SnappingClient, int OtherMode) override
	{
		// A blinking grenade marks the bomb until it detonates.
		if(OtherMode || m_Age >= TSAR_FUSE_TICKS || (m_Age / 6) % 2)
			return;
		CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
		if(!pProj)
			return;
		pProj->m_X = round_to_int(m_Pos.x);
		pProj->m_Y = round_to_int(m_Pos.y);
		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
		pProj->m_StartTick = Server()->Tick();
		pProj->m_Type = WEAPON_GRENADE;
	}
};

class CBlackHole : public CEntity
{
	enum
	{
		NUM_ARMS = 6,
		LIFE_TICKS = 150,
	};
	int m_Owner;
	int m_WeaponID;
	int m_Age;
	int m_aIDs[NUM_ARMS];

public:
	CBlackHole(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos) :
		CEntity(pWorld, CGameWorld::ENTTYPE_CUSTOM, Pos), m_Owner(Owner), m_WeaponID(WeaponID), m_Age(0)
	{
		for(int &ID : m_aIDs)
			ID = Server()->SnapNewID();
		GameWorld()->InsertEntity(this);
		GameWorld()->CreatePlayerSpawn(m_Pos);
		AsylumPlayMeme(GameWorld(), ASYLUM_MEME_WUB, m_Pos);
	}

	~CBlackHole() override
	{
		for(int ID : m_aIDs)
			Server()->SnapFreeID(ID);
	}

	void Reset() override { m_MarkedForDestroy = true; }

	void Tick() override
	{
		++m_Age;
		const float Radius = 300.0f;
		CCharacter *apTargets[MAX_CLIENTS];
		const int Num = GameWorld()->FindEntities(m_Pos, Radius, (CEntity **)apTargets, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apTargets[i];
			if(!AsylumCanAffect(this, m_Owner, pTarget, m_WeaponID))
				continue;
			const vec2 Delta = m_Pos - pTarget->m_Pos;
			const float Dist = length(Delta);
			if(Dist < 1.0f)
				continue;
			vec2 Vel = pTarget->Core()->m_Vel + Delta / Dist * (0.6f + 1.6f * (1.0f - minimum(1.0f, Dist / Radius)));
			if(length(Vel) > 18.0f)
				Vel = normalize(Vel) * 18.0f;
			pTarget->Core()->m_Vel = ClampVel(pTarget->m_MoveRestrictions, Vel);
			if(Dist < 48.0f && m_Age % 12 == 0)
				pTarget->TakeDamage(vec2(0.0f, 0.0f), 8, m_Owner, WEAPON_LASER, m_WeaponID, false);
		}
		if(m_Age % 6 == 0)
			GameWorld()->CreateDamageIndCircle(m_Pos, true, m_Age * 0.2f, 3, 6, 1.3f);
		if(m_Age % 15 == 0)
			GameWorld()->CreateSound(m_Pos, (m_Age / 15) % 2 ? SOUND_LASER_BOUNCE : SOUND_HOOK_NOATTACH);
		if(m_Age >= LIFE_TICKS)
		{
			GameWorld()->CreateExplosion(m_Pos, m_Owner, WEAPON_LASER, m_WeaponID, 35, false);
			AsylumPlayMeme(GameWorld(), ASYLUM_MEME_VINE_BOOM, m_Pos);
			m_MarkedForDestroy = true;
		}
	}

	void Snap(int SnappingClient, int OtherMode) override
	{
		if(OtherMode)
			return;
		const float Spin = -m_Age * 0.22f;
		const float Reach = 54.0f + 10.0f * sinf(m_Age * 0.3f);
		for(int i = 0; i < NUM_ARMS; ++i)
		{
			const float Angle = Spin + 2.0f * pi * i / NUM_ARMS;
			SnapLine(Server(), m_aIDs[i], m_Pos + UnitVec(Angle) * 16.0f, m_Pos + UnitVec(Angle + 0.9f) * Reach);
		}
	}
};

class CTrain : public CEntity
{
	enum
	{
		NUM_LINES = 8,
		LIFE_TICKS = 72,
	};
	int m_Owner;
	int m_WeaponID;
	int m_Direction;
	int m_Damage;
	float m_Force;
	int m_Age;
	int64 m_HitMask;
	int m_aIDs[NUM_LINES];

	static constexpr float ms_HalfLength = 110.0f;
	static constexpr float ms_HalfHeight = 40.0f;

	vec2 Front(float Back, float Up) const { return m_Pos + vec2(m_Direction * (ms_HalfLength - Back), -Up); }

public:
	CTrain(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, int Direction, int Damage, float Force) :
		CEntity(pWorld, CGameWorld::ENTTYPE_CUSTOM, Pos), m_Owner(Owner), m_WeaponID(WeaponID),
		m_Direction(Direction < 0 ? -1 : 1), m_Damage(Damage), m_Force(Force), m_Age(0), m_HitMask(0)
	{
		for(int &ID : m_aIDs)
			ID = Server()->SnapNewID();
		GameWorld()->InsertEntity(this);
		AsylumPlayMeme(GameWorld(), ASYLUM_MEME_CHOO_CHOO, m_Pos, true);
	}

	~CTrain() override
	{
		for(int ID : m_aIDs)
			Server()->SnapFreeID(ID);
	}

	void Reset() override { m_MarkedForDestroy = true; }

	void Tick() override
	{
		++m_Age;
		m_Pos.x += m_Direction * 27.0f;
		if(m_Age >= LIFE_TICKS || GameLayerClipped(m_Pos))
		{
			m_MarkedForDestroy = true;
			return;
		}
		if(m_Age % 5 == 0)
			GameWorld()->CreateSound(m_Pos, SOUND_BODY_LAND);
		if(m_Age % 6 == 0)
			GameWorld()->CreatePlayerSpawn(Front(48.0f, ms_HalfHeight + 30.0f));

		CCharacter *apTargets[MAX_CLIENTS];
		const int Num = GameWorld()->FindEntities(m_Pos, ms_HalfLength + 40.0f, (CEntity **)apTargets, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apTargets[i];
			const int CID = pTarget->GetPlayer()->GetCID();
			const vec2 Delta = pTarget->m_Pos - m_Pos;
			if((m_HitMask & (1LL << CID)) || absolute(Delta.x) > ms_HalfLength + 20.0f || absolute(Delta.y) > ms_HalfHeight + 20.0f ||
				!AsylumCanAffect(this, m_Owner, pTarget, m_WeaponID))
				continue;
			m_HitMask |= 1LL << CID;
			GameWorld()->CreateHammerHit(pTarget->m_Pos);
			AsylumPlayMeme(GameWorld(), ASYLUM_MEME_BONK, pTarget->m_Pos);
			pTarget->TakeDamage(vec2(m_Direction * m_Force, -m_Force * 0.45f), m_Damage, m_Owner, WEAPON_GRENADE, m_WeaponID, false);
		}
	}

	bool NetworkClipped(int SnappingClient) override
	{
		const vec2 Extent(ms_HalfLength + 20.0f, ms_HalfHeight + 40.0f);
		return NetworkRectClipped(SnappingClient, m_Pos - Extent, m_Pos + Extent);
	}

	void Snap(int SnappingClient, int OtherMode) override
	{
		if(OtherMode)
			return;
		const float L = ms_HalfLength * 2.0f;
		const float H = ms_HalfHeight * 2.0f;
		// Offsets are (distance behind the nose, height above the floor of the car).
		const vec2 aaLines[NUM_LINES][2] = {
			{Front(L, H), Front(22.0f, H)}, // roof
			{Front(22.0f, H), Front(0.0f, 14.0f)}, // slanted nose
			{Front(0.0f, 14.0f), Front(0.0f, 0.0f)},
			{Front(0.0f, 0.0f), Front(L, 0.0f)}, // floor
			{Front(L, 0.0f), Front(L, H)}, // back
			{Front(48.0f, H), Front(48.0f, H + 26.0f)}, // chimney
			{Front(L - 70.0f, H), Front(L - 70.0f, 0.0f)}, // cabin
			{Front(L - 20.0f, -10.0f), Front(20.0f, -10.0f)}, // wheels
		};
		for(int i = 0; i < NUM_LINES; ++i)
			SnapLine(Server(), m_aIDs[i], aaLines[i][0] + vec2(0.0f, ms_HalfHeight), aaLines[i][1] + vec2(0.0f, ms_HalfHeight));
	}
};
}

void AsylumPlayMeme(CGameWorld *pWorld, int Meme, vec2 Pos, bool Global, int64 Mask)
{
    if(Meme < 0 || Meme >= NUM_ASYLUM_MEMES)
        return;
    if(Meme == ASYLUM_MEME_VINE_BOOM && AsylumPlayMapSound(pWorld, "vine_boom", Pos, Global, Mask))
        return;
    if(Global)
        pWorld->CreateSoundGlobal(gs_aMemeSounds[Meme], Mask);
    else
        pWorld->CreateSound(Pos, gs_aMemeSounds[Meme], Mask);
}

bool AsylumPlayMapSound(CGameWorld *pWorld, const char *pName, vec2 Pos, bool Global, int64 Mask)
{
	IMap *pMap = pWorld->GameServer()->Layers()->Map();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_SOUND, &Start, &Num);
	for(int i = 0; i < Num; ++i)
	{
		const CMapItemSound *pSound = (const CMapItemSound *)pMap->GetItem(Start + i, nullptr, nullptr);
		if(pSound->m_SoundName < 0)
			continue;
		const char *pSampleName = (const char *)pMap->GetData(pSound->m_SoundName);
		if(!pSampleName || str_comp(pSampleName, pName))
			continue;
		if(Global)
			pWorld->CreateMapSoundGlobal(i, Mask);
		else
			pWorld->CreateMapSound(Pos, i, Mask);
		return true;
	}
	return false;
}

bool AsylumHasJumpscareMap(CGameWorld *pWorld)
{
	IMap *pMap = pWorld->GameServer()->Layers()->Map();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
	for(int i = 0; i < Num; ++i)
	{
		const CMapItemEnvelope *pEnv = (const CMapItemEnvelope *)pMap->GetItem(Start + i, nullptr, nullptr);
		char aName[33];
		IntsToStr(pEnv->m_aName, 8, aName);
		if(!str_comp(aName, "asylum_jumpscare"))
			return true;
	}
	return false;
}

bool AsylumHasTimeStopMap(CGameWorld *pWorld)
{
	IMap *pMap = pWorld->GameServer()->Layers()->Map();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
	for(int i = 0; i < Num; ++i)
	{
		const CMapItemEnvelope *pEnv = (const CMapItemEnvelope *)pMap->GetItem(Start + i, nullptr, nullptr);
		char aName[33];
		IntsToStr(pEnv->m_aName, 8, aName);
		if(!str_comp(aName, "asylum_time_stop_gray"))
			return true;
	}
	return false;
}

void AsylumShowText(CGameWorld *pWorld, vec2 Pos, const char *pText, float Seconds, int Gap)
{
	char aText[64];
	str_copy(aText, pText, sizeof(aText));
	new CTextEntity(pWorld, Pos, CTextEntity::TYPE_LASER, Gap, CTextEntity::ALIGN_MIDDLE, aText, Seconds);
}

bool AsylumCanAffect(CEntity *pSource, int Owner, CCharacter *pTarget, int WeaponID)
{
	if(!pTarget || !pTarget->IsAlive() || pTarget->IsSolo() || pTarget->GetPlayer()->GetCID() == Owner)
		return false;
	return ((CGameControllerHunterN *)pSource->Controller())->CanWeaponInteract(Owner, pTarget->GetPlayer()->GetCID(), WeaponID);
}

void AsylumSpawnBladeStorm(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, vec2 Target, int Damage)
{
	new CBladeStorm(pWorld, Owner, WeaponID, Pos, Target, Damage);
}

void AsylumSpawnTsarBomb(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos)
{
	new CTsarBomb(pWorld, Owner, WeaponID, Pos);
}

void AsylumSpawnBlackHole(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos)
{
	new CBlackHole(pWorld, Owner, WeaponID, Pos);
}

void AsylumSpawnTrain(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, int Direction, int Damage, float Force)
{
	new CTrain(pWorld, Owner, WeaponID, Pos, Direction, Damage, Force);
}

void AsylumSpawnNote(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, vec2 Direction, int Note, int Damage, float Force)
{
	new CRhythmNote(pWorld, Owner, WeaponID, Pos, Direction, Note, Damage, Force);
}
