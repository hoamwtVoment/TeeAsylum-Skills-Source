/* Tee Asylum god-tier (★) item effects: meme sound sequences and summoned entities. */
#include "asylum_fx.h"

#include <game/generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/entities/textentity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/huntern.h>
#include <game/server/player.h>

namespace
{
struct SMemeStep
{
	int m_Delay; // ticks after the meme starts
	int m_Sound;
};

const SMemeStep gs_aBonk[] = {{0, SOUND_HAMMER_HIT}, {0, SOUND_BODY_LAND}, {4, SOUND_HAMMER_HIT}};
const SMemeStep gs_aVineBoom[] = {{0, SOUND_GRENADE_EXPLODE}, {0, SOUND_NINJA_HIT}, {1, SOUND_GRENADE_EXPLODE}, {3, SOUND_BODY_LAND}};
const SMemeStep gs_aFanfare[] = {{0, SOUND_CTF_CAPTURE}, {30, SOUND_PICKUP_NINJA}};
const SMemeStep gs_aDrumroll[] = {
	{0, SOUND_HAMMER_FIRE}, {3, SOUND_HAMMER_FIRE}, {6, SOUND_HAMMER_FIRE}, {9, SOUND_HAMMER_FIRE},
	{12, SOUND_HAMMER_FIRE}, {15, SOUND_HAMMER_FIRE}, {18, SOUND_HAMMER_FIRE}, {21, SOUND_HAMMER_FIRE},
	{24, SOUND_HAMMER_FIRE}, {26, SOUND_HAMMER_FIRE}, {28, SOUND_HAMMER_FIRE}, {30, SOUND_HAMMER_FIRE},
	{32, SOUND_HAMMER_FIRE}, {34, SOUND_HAMMER_FIRE}, {36, SOUND_HAMMER_FIRE},
	{42, SOUND_GRENADE_EXPLODE}, {42, SOUND_CTF_CAPTURE}};
const SMemeStep gs_aSad[] = {{0, SOUND_TEE_CRY}, {14, SOUND_PLAYER_PAIN_LONG}, {34, SOUND_TEE_CRY}};
const SMemeStep gs_aBoing[] = {{0, SOUND_PLAYER_JUMP}, {4, SOUND_PLAYER_AIRJUMP}, {8, SOUND_PLAYER_AIRJUMP}, {12, SOUND_PLAYER_AIRJUMP}};
const SMemeStep gs_aError[] = {{0, SOUND_WEAPON_NOAMMO}, {6, SOUND_WEAPON_NOAMMO}, {12, SOUND_WEAPON_NOAMMO}, {20, SOUND_CHAT_HIGHLIGHT}};
const SMemeStep gs_aScream[] = {
	{0, SOUND_PLAYER_PAIN_LONG}, {0, SOUND_TEE_CRY}, {3, SOUND_PLAYER_DIE}, {6, SOUND_PLAYER_PAIN_LONG},
	{10, SOUND_TEE_CRY}, {14, SOUND_PLAYER_PAIN_LONG}, {18, SOUND_PLAYER_DIE}};
const SMemeStep gs_aChooChoo[] = {{0, SOUND_CTF_GRAB_EN}, {16, SOUND_CTF_GRAB_EN}, {34, SOUND_CTF_RETURN}};
const SMemeStep gs_aKaching[] = {{0, SOUND_PICKUP_ARMOR}, {3, SOUND_PICKUP_HEALTH}, {7, SOUND_CHAT_HIGHLIGHT}};
const SMemeStep gs_aShing[] = {{0, SOUND_NINJA_FIRE}, {2, SOUND_LASER_FIRE}, {5, SOUND_NINJA_HIT}};
const SMemeStep gs_aRustle[] = {{0, SOUND_PLAYER_SKID}, {6, SOUND_PLAYER_SKID}, {12, SOUND_PLAYER_SKID}, {18, SOUND_PLAYER_SKID}, {26, SOUND_BODY_LAND}};
const SMemeStep gs_aWub[] = {{0, SOUND_LASER_BOUNCE}, {5, SOUND_HOOK_NOATTACH}, {10, SOUND_LASER_BOUNCE}, {15, SOUND_HOOK_NOATTACH}, {20, SOUND_LASER_BOUNCE}};
// The beeps speed up towards the Tsar bobm detonation (TSAR_FUSE_TICKS).
const SMemeStep gs_aAlarm[] = {
	{0, SOUND_CHAT_SERVER}, {20, SOUND_CHAT_SERVER}, {38, SOUND_CHAT_SERVER}, {52, SOUND_CHAT_SERVER},
	{62, SOUND_CHAT_SERVER}, {68, SOUND_CHAT_SERVER}, {72, SOUND_CHAT_SERVER}};
const SMemeStep gs_aHeartbeat[] = {{0, SOUND_BODY_LAND}, {8, SOUND_BODY_LAND}, {38, SOUND_BODY_LAND}, {46, SOUND_BODY_LAND}};
const SMemeStep gs_aThunder[] = {{0, SOUND_LASER_FIRE}, {2, SOUND_GRENADE_EXPLODE}, {7, SOUND_LASER_BOUNCE}, {12, SOUND_GRENADE_EXPLODE}};

struct SMeme
{
	const SMemeStep *m_pSteps;
	int m_NumSteps;
};

template<int N>
constexpr SMeme Meme(const SMemeStep (&aSteps)[N])
{
	return SMeme{aSteps, N};
}

const SMeme gs_aMemes[] = {
	Meme(gs_aBonk), Meme(gs_aVineBoom), Meme(gs_aFanfare), Meme(gs_aDrumroll),
	Meme(gs_aSad), Meme(gs_aBoing), Meme(gs_aError), Meme(gs_aScream),
	Meme(gs_aChooChoo), Meme(gs_aKaching), Meme(gs_aShing), Meme(gs_aRustle),
	Meme(gs_aWub), Meme(gs_aAlarm), Meme(gs_aHeartbeat), Meme(gs_aThunder)};
static_assert(sizeof(gs_aMemes) / sizeof(gs_aMemes[0]) == NUM_ASYLUM_MEMES, "Meme table mismatch");

const int TSAR_FUSE_TICKS = 75;

void PlayStep(CGameWorld *pWorld, const SMemeStep &Step, vec2 Pos, bool Global, int64 Mask)
{
	if(Global)
		pWorld->CreateSoundGlobal(Step.m_Sound, Mask);
	else
		pWorld->CreateSound(Pos, Step.m_Sound, Mask);
}

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

class CSoundSequence : public CEntity
{
	const SMeme *m_pMeme;
	int m_Next;
	int m_StartTick;
	bool m_Global;
	int64 m_Mask;

public:
	CSoundSequence(CGameWorld *pWorld, const SMeme *pMeme, int Next, vec2 Pos, bool Global, int64 Mask) :
		CEntity(pWorld, CGameWorld::ENTTYPE_CUSTOM, Pos), m_pMeme(pMeme), m_Next(Next), m_Global(Global), m_Mask(Mask)
	{
		m_StartTick = Server()->Tick();
		GameWorld()->InsertEntity(this);
	}

	void Reset() override { m_MarkedForDestroy = true; }
	void TickPaused() override { ++m_StartTick; }
	void Tick() override
	{
		const int Elapsed = Server()->Tick() - m_StartTick;
		while(m_Next < m_pMeme->m_NumSteps && m_pMeme->m_pSteps[m_Next].m_Delay <= Elapsed)
			PlayStep(GameWorld(), m_pMeme->m_pSteps[m_Next++], m_Pos, m_Global, m_Mask);
		if(m_Next >= m_pMeme->m_NumSteps)
			m_MarkedForDestroy = true;
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
	const SMeme &Info = gs_aMemes[Meme];
	int Next = 0;
	while(Next < Info.m_NumSteps && Info.m_pSteps[Next].m_Delay == 0)
		PlayStep(pWorld, Info.m_pSteps[Next++], Pos, Global, Mask);
	if(Next < Info.m_NumSteps)
		new CSoundSequence(pWorld, &Info, Next, Pos, Global, Mask);
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
