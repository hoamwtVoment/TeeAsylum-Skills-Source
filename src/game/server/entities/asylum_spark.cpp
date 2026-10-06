#include "asylum_spark.h"

#include <game/server/entities/asylum_fx.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/weapon.h>

namespace {
const float SPARK_RANGE = 1400.0f;
const float SPARK_MOUTH_RADIUS = 32.0f;
const float SPARK_END_RADIUS = 104.0f;
} // namespace

CMasterSpark::CMasterSpark(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, vec2 Direction, int Damage) :
	CEntity(pWorld, CGameWorld::ENTTYPE_ASYLUM_SPARK, Pos), m_Owner(Owner), m_WeaponID(WeaponID), m_Damage(Damage), m_Age(0), m_Direction(Direction)
{
	for(int &ID : m_aIDs)
		ID = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

CMasterSpark::~CMasterSpark()
{
	for(int ID : m_aIDs)
		Server()->SnapFreeID(ID);
}

void CMasterSpark::Reset()
{
	m_MarkedForDestroy = true;
}

int CMasterSpark::ChargeTicks()
{
	return maximum(1, GameWorld()->Server()->TickSpeed() * 3 / 5);
}

float CMasterSpark::BeamRadius(float Along) const
{
	return mix(SPARK_MOUTH_RADIUS, SPARK_END_RADIUS, clamp(Along / SPARK_RANGE, 0.0f, 1.0f));
}

void CMasterSpark::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->IsAlive() || pOwner->GameWorld() != GameWorld() ||
		pOwner->IsFrozen() || pOwner->IsDisabled() || !pOwner->CurrentWeapon() ||
		pOwner->CurrentWeapon()->GetWeaponID() != m_WeaponID ||
		(!Controller()->IsGameRunning() && !Controller()->IsWarmup()))
	{
		Reset();
		return;
	}

	++m_Age;
	const int Charge = ChargeTicks();
	if(m_Age >= Charge + Server()->TickSpeed() * 2)
	{
		Reset();
		return;
	}
	const vec2 Aim = pOwner->GetAimDirection();
	if(m_Age < Charge)
		m_Direction = Aim;
	else
	{
		// Charging aims freely; the active cannon can only sweep slowly.
		const float Current = angle(m_Direction);
		const float Difference = angle(Aim) - Current;
		const float Wrapped = atan2f(sinf(Difference), cosf(Difference));
		const float Limit = 1.4f / Server()->TickSpeed();
		const float Turned = Current + clamp(Wrapped, -Limit, Limit);
		m_Direction = vec2(cosf(Turned), sinf(Turned));
	}
	m_Pos = pOwner->m_Pos + m_Direction * 30.0f;
	// Recoil slows movement but never grants invulnerability or ignores walls.
	pOwner->Core()->m_Vel.x = clamp(pOwner->Core()->m_Vel.x, -2.5f, 2.5f);

	if(m_Age < Charge)
		return;
	const int Elapsed = m_Age - Charge;
	const int DamageInterval = maximum(1, Server()->TickSpeed() * 12 / 100);
	if(Elapsed % DamageInterval == 0)
		DamageBeam(pOwner);
	if(Elapsed % maximum(1, Server()->TickSpeed() * 2 / 5) == 0)
		GameWorld()->CreateSound(m_Pos, SOUND_LASER_FIRE);
}

void CMasterSpark::DamageBeam(CCharacter *pOwner)
{
	if(pOwner->IsSolo())
		return;
	CEntity *apEntities[MAX_CLIENTS];
	const int Num = GameWorld()->FindEntities(m_Pos + m_Direction * (SPARK_RANGE / 2.0f),
		SPARK_RANGE / 2.0f + SPARK_END_RADIUS, apEntities, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	const vec2 Side(-m_Direction.y, m_Direction.x);
	// The Boss is a controller-owned combat NPC, not an ENTTYPE_CHARACTER.
	// Trace the same expanding beam lanes; terrain still blocks each lane.
	for(int Lane = -3; Lane <= 3; ++Lane)
	{
		const float Offset = Lane / 3.0f;
		const vec2 From = m_Pos + Side * (Offset * SPARK_MOUTH_RADIUS);
		const vec2 To = m_Pos + m_Direction * SPARK_RANGE + Side * (Offset * SPARK_END_RADIUS);
		if(!GameServer()->Collision()->CheckPoint(From) && Controller()->IntersectCombatNpc(From, To, 0, nullptr))
		{
			Controller()->DamageCombatNpc(m_Owner, m_WeaponID, m_Damage);
			break;
		}
	}
	for(int i = 0; i < Num; ++i)
	{
		CCharacter *pTarget = static_cast<CCharacter *>(apEntities[i]);
		if(!AsylumCanAffect(this, m_Owner, pTarget, m_WeaponID))
			continue;
		const vec2 Delta = pTarget->m_Pos - m_Pos;
		const float Along = dot(Delta, m_Direction);
		const float Across = dot(Delta, Side);
		const float Radius = BeamRadius(Along);
		if(Along < 0.0f || Along > SPARK_RANGE || fabsf(Across) > Radius + pTarget->GetProximityRadius())
			continue;
		// Each lane is blocked by terrain, but tees never block the next target.
		const float Lane = clamp(Across / Radius, -1.0f, 1.0f);
		const vec2 From = m_Pos + Side * (Lane * SPARK_MOUTH_RADIUS);
		if(GameServer()->Collision()->CheckPoint(From) ||
			GameServer()->Collision()->IntersectLine(From, pTarget->m_Pos, nullptr, nullptr))
			continue;
		pTarget->TakeDamage(m_Direction * 1.5f, m_Damage, m_Owner, WEAPON_LASER, m_WeaponID, false);
	}
}

bool CMasterSpark::NetworkClipped(int SnappingClient)
{
	if(m_Age < ChargeTicks())
		return NetworkPointClipped(SnappingClient, m_Pos, vec2(90.0f, 90.0f));
	return NetworkLineClipped(SnappingClient, m_Pos, m_Pos + m_Direction * SPARK_RANGE,
		vec2(SPARK_END_RADIUS * 2.0f, SPARK_END_RADIUS * 2.0f));
}

void CMasterSpark::SnapLine(int ID, vec2 From, vec2 To)
{
	CNetObj_Laser *pObj = (CNetObj_Laser *)Server()->SnapNewItem(NETOBJTYPE_LASER, ID, sizeof(CNetObj_Laser));
	if(!pObj)
		return;
	pObj->m_FromX = round_to_int(From.x);
	pObj->m_FromY = round_to_int(From.y);
	pObj->m_X = round_to_int(To.x);
	pObj->m_Y = round_to_int(To.y);
	pObj->m_StartTick = Server()->Tick();
}

void CMasterSpark::Snap(int SnappingClient, int OtherMode)
{
	if(OtherMode)
		return;
	const int Charge = ChargeTicks();
	const bool Charging = m_Age < Charge;
	const float Radius = Charging ? 12.0f + 36.0f * m_Age / Charge : 42.0f + 4.0f * sinf(m_Age * 0.5f);
	const vec2 Side(-m_Direction.y, m_Direction.x);
	// A spinning twelve-sided magic circle at the muzzle, also while charging.
	for(int i = 0; i < NUM_RING_LINES; ++i)
	{
		const float A = 2.0f * pi * i / NUM_RING_LINES + m_Age * 0.12f;
		const float B = 2.0f * pi * (i + 1) / NUM_RING_LINES + m_Age * 0.12f;
		SnapLine(m_aIDs[NUM_BEAM_LINES + i], m_Pos + vec2(cosf(A), sinf(A)) * Radius,
			m_Pos + vec2(cosf(B), sinf(B)) * Radius);
	}
	if(Charging)
		return;
	for(int i = 0; i < NUM_BEAM_LINES; ++i)
	{
		const float Lane = 2.0f * i / (NUM_BEAM_LINES - 1) - 1.0f;
		const vec2 From = m_Pos + Side * (Lane * SPARK_MOUTH_RADIUS);
		vec2 To = m_Pos + m_Direction * SPARK_RANGE + Side * (Lane * SPARK_END_RADIUS);
		GameServer()->Collision()->IntersectLine(From, To, &To, nullptr);
		SnapLine(m_aIDs[i], From, To);
	}
}
