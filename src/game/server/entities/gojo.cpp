#include "gojo.h"

#include <engine/map.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

namespace
{
bool CanAffect(CEntity *pEntity, int Owner, CCharacter *pTarget, int WeaponID)
{
	CCharacter *pOwner = pEntity->GameServer()->GetPlayerChar(Owner);
	return pOwner && pOwner != pTarget && pOwner->IsAlive() && pTarget->IsAlive() &&
		pOwner->GameWorld() == pEntity->GameWorld() && pTarget->GameWorld() == pEntity->GameWorld() && !pTarget->IsDisabled() &&
		!pOwner->IsSolo() && !pTarget->IsSolo() && !pTarget->IsProtected() &&
		!pEntity->Controller()->IsAdminInvincible(pTarget->GetPlayer()->GetCID()) &&
		pEntity->Controller()->CanWeaponInteract(Owner, pTarget->GetPlayer()->GetCID(), WeaponID);
}

bool RoleAlive(CEntity *pEntity, int Owner)
{
	CCharacter *pOwner = pEntity->GameServer()->GetPlayerChar(Owner);
	CGojoState *pState = pEntity->Controller()->GetGojoState(Owner);
	return pOwner && pOwner->IsAlive() && !pOwner->IsDisabled() && pOwner->GameWorld() == pEntity->GameWorld() && pState && pState->m_Enabled;
}
}

void GojoSnapLine(IServer *pServer, int ID, vec2 From, vec2 To, int Skill, int Owner)
{
	(void)Owner;
	// Standard fallback and official laser@netobj.ddnet.tw extension share IDs.
	auto *pBasic = static_cast<CNetObj_Laser *>(pServer->SnapNewItem(NETOBJTYPE_LASER, ID, sizeof(CNetObj_Laser)));
	if(!pBasic) return;
	pBasic->m_FromX = round_to_int(From.x); pBasic->m_FromY = round_to_int(From.y);
	pBasic->m_X = round_to_int(To.x); pBasic->m_Y = round_to_int(To.y); pBasic->m_StartTick = pServer->Tick();
	auto *pExtra = static_cast<CNetObj_DDNetLaser *>(pServer->SnapNewItem(NETOBJTYPE_DDNETLASER, ID, sizeof(CNetObj_DDNetLaser)));
	if(!pExtra) return;
	pExtra->m_FromX = pBasic->m_FromX; pExtra->m_FromY = pBasic->m_FromY;
	pExtra->m_ToX = pBasic->m_X; pExtra->m_ToY = pBasic->m_Y; pExtra->m_StartTick = pBasic->m_StartTick;
	// Native rifle, door, shotgun and freeze palettes. Client color settings apply.
	pExtra->m_Type = Skill == GOJO_RED ? 2 : Skill == GOJO_PURPLE ? 1 : Skill == GOJO_DOMAIN ? 3 : 0;
	pExtra->m_Owner = -1; // Cosmetic shape: not a predicted damaging rifle beam.
	pExtra->m_SwitchNumber = -1; pExtra->m_Subtype = -1; pExtra->m_Flags = 0;
}

bool GojoHasBlue(CGameWorld *pWorld, int Owner)
{
	return GojoHasOrb(pWorld, Owner, GOJO_BLUE);
}

bool GojoHasOrb(CGameWorld *pWorld, int Owner, int Skill)
{
	for(CEntity *p = pWorld->FindFirst(CGameWorld::ENTTYPE_GOJO_ORB); p; p = p->TypeNext())
		if(static_cast<CGojoOrb *>(p)->Active() && static_cast<CGojoOrb *>(p)->Owner() == Owner && static_cast<CGojoOrb *>(p)->Skill() == Skill) return true;
	return false;
}

bool GojoHasDomain(CGameWorld *pWorld, int Owner)
{
	for(CEntity *p = pWorld->FindFirst(CGameWorld::ENTTYPE_GOJO_DOMAIN); p; p = p->TypeNext())
		if(static_cast<CGojoDomain *>(p)->Active() && static_cast<CGojoDomain *>(p)->Owner() == Owner) return true;
	return false;
}

void GojoClearEntities(CGameWorld *pWorld, int Owner)
{
	for(CEntity *p = pWorld->FindFirst(CGameWorld::ENTTYPE_GOJO_ORB); p; p = p->TypeNext())
		if(static_cast<CGojoOrb *>(p)->Owner() == Owner) p->Reset();
	for(CEntity *p = pWorld->FindFirst(CGameWorld::ENTTYPE_GOJO_DOMAIN); p; p = p->TypeNext())
		if(static_cast<CGojoDomain *>(p)->Owner() == Owner) p->Reset();
}

bool GojoHasDomainMap(CGameWorld *pWorld)
{
	IMap *pMap = pWorld->GameServer()->Layers()->Map();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
	for(int i = 0; i < Num; ++i)
	{
		const auto *pEnv = static_cast<const CMapItemEnvelope *>(pMap->GetItem(Start + i, nullptr, nullptr));
		if(pEnv->m_Version < 2) continue;
		char aName[33]; IntsToStr(pEnv->m_aName, 8, aName);
		if(!str_comp(aName, "gojo_domain_black")) return true;
	}
	return false;
}

CGojoOrb::CGojoOrb(CGameWorld *pWorld, int Owner, int Skill, vec2 Pos, vec2 Direction, float Charge) :
	CEntity(pWorld, CGameWorld::ENTTYPE_GOJO_ORB, Pos), m_Owner(Owner), m_WeaponID(GojoWeaponID(Skill)), m_Skill(Skill),
	m_Charge(clamp(Charge, 0.0f, 1.0f)), m_Radius(Skill == GOJO_BLUE ? 36 + 188 * m_Charge : Skill == GOJO_RED ? 18 + 22 * m_Charge : 50 + 100 * m_Charge)
{
	const float Speed = Skill == GOJO_BLUE ? 20.0f - 3.0f * m_Charge : Skill == GOJO_RED ? 30.0f + 8 * m_Charge : 70.0f + 30 * m_Charge;
	m_Velocity = Direction * Speed;
	for(int &ID : m_aIDs) ID = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

CGojoOrb::~CGojoOrb()
{
	ClearCarry();
	for(int ID : m_aIDs) Server()->SnapFreeID(ID);
}

void CGojoOrb::ClearCarry()
{
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		if(!m_aCarried[CID]) continue;
		CGojoState *pState = Controller()->GetGojoState(CID);
		if(pState && pState->m_CarrySource == m_aIDs[0]) { pState->m_CarryUntil = 0; pState->m_CarrySource = -1; }
		m_aCarried[CID] = false;
	}
}

void CGojoOrb::Reset() { ClearCarry(); m_MarkedForDestroy = true; }

void CGojoOrb::Tick()
{
	if(m_MarkedForDestroy || !RoleAlive(this, m_Owner)) { Reset(); return; }
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	CGojoState *pState = Controller()->GetGojoState(m_Owner);
	pState->m_LastActionTick = Server()->Tick();
	if(++m_Age > Server()->TickSpeed() * (m_Skill == GOJO_BLUE ? 8 : 3)) { Reset(); return; }
	if(m_Skill == GOJO_BLUE && pOwner->CurrentWeapon() && pOwner->CurrentWeapon()->GetWeaponID() == GojoWeaponID(GOJO_BLUE) &&
		pOwner->IsFireHeld() && !pOwner->IsFrozen() && !pOwner->IsGojoImmobilized())
	{
		vec2 Target = pOwner->GetAimTarget();
		vec2 To = Target - pOwner->m_Pos;
		if(length(To) > 1000) Target = pOwner->m_Pos + normalize(To) * 1000;
		To = Target - m_Pos;
		if(length(To) > 1)
		{
			const float Speed = 20.0f - 3.0f * m_Charge;
			// Larger output has greater inertia: changing the cursor never teleports it.
			m_Velocity = mix(m_Velocity, normalize(To) * minimum(Speed, length(To) * 0.3f), 0.30f - 0.08f * m_Charge);
		}
	}
	const vec2 Previous = m_Pos;
	const vec2 Next = m_Pos + m_Velocity;
	if(GameLayerClipped(Next)) { Reset(); return; }
	if(m_Skill == GOJO_PURPLE)
	{
		// Sample the swept path, not just endpoints: fast Purple crosses thin
		// walls too. Every 32 solid units removes only 0.16 units of radius.
		const float Travel = distance(Previous, Next);
		const int Steps = maximum(1, (int)ceilf(Travel / 4.0f));
		float SolidTravel = 0;
		for(int i = 0; i < Steps; ++i)
			if(GameServer()->Collision()->CheckPoint(mix(Previous, Next, (i + 0.5f) / Steps))) SolidTravel += Travel / Steps;
		m_Radius = maximum(24.0f, m_Radius - SolidTravel * 0.005f);
		if(SolidTravel > 0 && m_Age % 3 == 0) GameWorld()->CreateExplosionParticle(m_Pos);
	}
	else if(GameServer()->Collision()->IntersectLine(Previous, Next, nullptr, nullptr))
	{
		GameWorld()->CreateExplosionParticle(m_Pos);
		Reset(); return;
	}
	m_Pos = Next;
	if(m_Skill == GOJO_BLUE || m_Skill == GOJO_RED)
	{
		for(CEntity *p = GameWorld()->FindFirst(CGameWorld::ENTTYPE_GOJO_ORB); p; p = p->TypeNext())
		{
			auto *pOther = static_cast<CGojoOrb *>(p);
			if(pOther == this || pOther->m_MarkedForDestroy || (pOther->Skill() != GOJO_BLUE && pOther->Skill() != GOJO_RED) || pOther->Skill() == m_Skill) continue;
			if(distance(m_Pos, pOther->m_Pos) > m_Radius + pOther->Radius()) continue;
			CGojoOrb *pRed = m_Skill == GOJO_RED ? this : pOther;
			const int Owner = pRed->Owner();
			const vec2 Aim = length(pRed->Velocity()) > 0 ? normalize(pRed->Velocity()) : vec2(1, 0);
			const vec2 Center = (m_Pos + pOther->m_Pos) * 0.5f;
			const float Power = clamp((m_Charge + pOther->Charge()) * 0.5f + 0.2f, 0.0f, 1.0f);
			CGojoState *pPurpleState = Controller()->GetGojoState(Owner);
			CPlayer *pPurplePlayer = Controller()->GetPlayerIfInRoom(Owner);
			if(!pPurpleState || !pPurplePlayer || GojoHasOrb(GameWorld(), Owner, GOJO_PURPLE) ||
				(!pPurplePlayer->m_AsylumNoCooldown && pPurpleState->m_NextCast[GOJO_PURPLE] > Server()->Tick())) continue;
			pOther->Reset(); Reset();
			new CGojoOrb(GameWorld(), Owner, GOJO_PURPLE, Center, Aim, Power);
			pPurpleState->m_NextCast[GOJO_PURPLE] = pPurplePlayer->m_AsylumNoCooldown ? 0 : Server()->Tick() +
				GojoSkillCooldownTicks(GOJO_PURPLE, Server()->TickSpeed(), pPurpleState->BrainDamaged(Server()->Tick()));
			pPurpleState->m_LastCastSkill = GOJO_PURPLE;
			pPurpleState->m_LastCastTick = pPurpleState->m_LastActionTick = Server()->Tick();
			GameWorld()->CreateExplosionParticle(Center);
			GameServer()->SendChatTarget(Owner, "苍与赫相撞——虚式·茈。");
			return;
		}
	}
	if(m_Skill == GOJO_BLUE) Pull(); else Hit(Previous);
}

void CGojoOrb::Pull()
{
	const float Reach = m_Radius * 3;
	CEntity *apEntities[MAX_CLIENTS];
	CCharacter *apInside[MAX_CLIENTS];
	int Inside = 0;
	const int Count = GameWorld()->FindEntities(m_Pos, Reach, apEntities, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Count; ++i)
	{
		auto *pTarget = static_cast<CCharacter *>(apEntities[i]);
		if(!CanAffect(this, m_Owner, pTarget, m_WeaponID) || GameServer()->Collision()->IntersectLine(m_Pos, pTarget->m_Pos, nullptr, nullptr)) continue;
		const vec2 Delta = m_Pos - pTarget->m_Pos;
		const float Dist = length(Delta);
		if(Dist > 1)
		{
			const float Near = 1 - clamp(Dist / Reach, 0.0f, 1.0f);
			const float Speed = minimum(36.0f, Dist * 0.35f);
			// Capture toward the MOVING center, not toward where it used to be.
			const vec2 Desired = m_Velocity + normalize(Delta) * Speed;
			vec2 Vel = mix(pTarget->Core()->m_Vel, Desired, 0.35f + 0.35f * Near);
			if(length(Vel) > 44) Vel = normalize(Vel) * 44;
			pTarget->Core()->m_Vel = ClampVel(pTarget->m_MoveRestrictions, Vel);
			CGojoState *pState = Controller()->GetGojoState(pTarget->GetPlayer()->GetCID());
			if(pState)
			{
				pState->m_BluePullUntil = Server()->Tick() + 2;
				pState->m_BluePullVelocity = pTarget->Core()->m_Vel;
			}
		}
		if(Dist < m_Radius + 20) apInside[Inside++] = pTarget;
	}
	if(Inside >= 2 && m_Age % maximum(1, Server()->TickSpeed() / 4) == 0)
		for(int i = 0; i < Inside; ++i)
			apInside[i]->TakeDamage(vec2(0, 0), 2 + round_to_int(2 * m_Charge), m_Owner, WEAPON_LASER, m_WeaponID, false);
}

void CGojoOrb::Hit(vec2 Previous)
{
	CEntity *apEntities[MAX_CLIENTS];
	const int Count = GameWorld()->FindEntities((Previous + m_Pos) * 0.5f, m_Radius + length(m_Velocity), apEntities, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	const vec2 Push = normalize(m_Velocity);
	const vec2 Side(-Push.y, Push.x);
	const int RedInterval = maximum(1, Server()->TickSpeed() * 4 / 100);
	for(int i = 0; i < Count; ++i)
	{
		auto *pTarget = static_cast<CCharacter *>(apEntities[i]);
		const int CID = pTarget->GetPlayer()->GetCID();
		if(!CanAffect(this, m_Owner, pTarget, m_WeaponID)) continue;
		if(m_Skill == GOJO_PURPLE && m_aHit[CID]) continue;
		if(m_Skill == GOJO_RED && m_aCarried[CID]) continue;
		vec2 Closest = Previous;
		closest_point_on_line(Previous, m_Pos, pTarget->m_Pos, Closest);
		const float Dist = distance(Closest, pTarget->m_Pos);
		if(Dist > m_Radius + 28) continue;
		if(m_Skill == GOJO_PURPLE)
		{
			m_aHit[CID] = true;
			// Once per projectile, at most 70 on a normal 100-HP character.
			pTarget->TakeDamage(Push * 8 + vec2(0, -3), 40 + round_to_int(30 * m_Charge), m_Owner, WEAPON_LASER, m_WeaponID, false);
		}
		else if([&]() {
			const vec2 Incoming = pTarget->m_Pos - (Previous - Push * (m_Radius + 28));
			return length(Incoming) < 1 || dot(normalize(Incoming), Push) >= 0.80f;
		}())
		{
			// A cone aligned with actual travel, not a world-X test. Upward and
			// downward head-on hits are handled identically to horizontal ones.
			if(m_Age < m_aNextRedHit[CID]) continue;
			m_aNextRedHit[CID] = m_Age + RedInterval;
			const int Before = pTarget->GetHealth() + pTarget->GetArmor();
			pTarget->TakeDamage(vec2(0, 0), 2, m_Owner, WEAPON_GRENADE, m_WeaponID, false);
			if(pTarget->IsAlive() && (pTarget->GetHealth() + pTarget->GetArmor() < Before || pTarget->GetPlayer()->m_AsylumTestGod))
			{
				m_aCarried[CID] = true;
				m_aCarryOffset[CID] = Push * (m_Radius + 22) + Side * clamp(dot(pTarget->m_Pos - m_Pos, Side), -m_Radius * 0.5f, m_Radius * 0.5f);
			}
		}
		else
		{
			if(m_aHit[CID]) continue; // One harmless graze, but a later front hit may still capture it.
			m_aHit[CID] = true;
			pTarget->TakeDamage(Push * 18 + vec2(0, -6), 0, m_Owner, WEAPON_GRENADE, m_WeaponID, false);
		}
		GameWorld()->CreateHammerHit(pTarget->m_Pos);
	}
	if(m_Skill == GOJO_RED)
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			if(!m_aCarried[CID]) continue;
			CCharacter *pTarget = GameServer()->GetPlayerChar(CID);
			CGojoState *pState = Controller()->GetGojoState(CID);
			if(!pTarget || !pState || !CanAffect(this, m_Owner, pTarget, m_WeaponID)) { m_aCarried[CID] = false; continue; }
			pState->m_CarryUntil = Server()->Tick() + 2;
			pState->m_CarrySource = m_aIDs[0];
			pState->m_CarryVelocity = m_Velocity;
			pState->m_CarryTarget = m_Pos + m_aCarryOffset[CID];
			if(distance(pTarget->Core()->m_Pos, pState->m_CarryTarget) > 180 + m_Radius)
			{
				// A wall/domain can stop the tee; don't remotely carry or damage it forever.
				if(pState->m_CarrySource == m_aIDs[0]) { pState->m_CarryUntil = 0; pState->m_CarrySource = -1; }
				m_aCarried[CID] = false;
				continue;
			}
			if(m_Age >= m_aNextRedHit[CID])
			{
				m_aNextRedHit[CID] = m_Age + RedInterval;
				pTarget->TakeDamage(vec2(0, 0), 2, m_Owner, WEAPON_GRENADE, m_WeaponID, false);
			}
		}
	if(m_Skill == GOJO_PURPLE && !m_NpcHit)
	{
		const int Applied = Controller()->DamageCombatNpcBox(m_Owner, m_WeaponID, 40 + round_to_int(30 * m_Charge),
			Previous - Push * m_Radius, Push, distance(Previous, m_Pos) + 2 * m_Radius, m_Radius);
		if(Applied > 0) m_NpcHit = true;
	}
	else if(m_Skill == GOJO_RED && m_Age - m_LastNpcHitAge >= RedInterval &&
		Controller()->IntersectCombatNpc(Previous, m_Pos, m_Radius, nullptr))
	{
		m_LastNpcHitAge = m_Age;
		Controller()->DamageCombatNpc(m_Owner, m_WeaponID, 2);
	}
}

bool CGojoOrb::NetworkClipped(int CID) { return NetworkPointClipped(CID, m_Pos, vec2(m_Radius * 4, m_Radius * 4)); }

void CGojoOrb::Snap(int SnappingClient, int OtherMode)
{
	if(OtherMode || m_MarkedForDestroy) return;
	const float Spin = m_Age * 0.17f;
	const vec2 Aim = normalize(m_Velocity), Side(-Aim.y, Aim.x);
	int Index = 0;
	auto Line = [&](vec2 A, vec2 B, int Kind) {
		if(Index < 48) GojoSnapLine(Server(), m_aIDs[Index++], A, B, Kind, m_Owner);
	};
	if(m_Skill == GOJO_BLUE)
	{
		// Six inward winding arms and moving fragments convey attraction.
		for(int Arm = 0; Arm < 6; ++Arm)
			for(int i = 0; i < 4; ++i)
			{
				const float A = Spin + Arm * pi / 3 + i * 0.6f;
				Line(m_Pos + direction(A) * (m_Radius * (i + 0.2f) / 4), m_Pos + direction(A + 0.6f) * (m_Radius * (i + 1.2f) / 4), GOJO_BLUE);
			}
		for(int i = 0; i < 8; ++i)
		{
			const float A = -Spin * 0.6f + i * pi / 4;
			Line(m_Pos + direction(A) * m_Radius * 1.15f, m_Pos + direction(A + 0.25f) * m_Radius, GOJO_BLUE);
			Line(m_Pos + direction(A) * m_Radius * 0.12f, m_Pos + direction(A + pi / 2) * m_Radius * 0.22f, GOJO_BLUE);
		}
	}
	else if(m_Skill == GOJO_RED)
	{
		for(int i = 0; i < 8; ++i)
		{
			const float A = Spin + i * pi / 4;
			const vec2 Tip = m_Pos + direction(A) * m_Radius * 1.15f;
			Line(m_Pos + direction(A - 0.25f) * m_Radius * 0.3f, Tip, GOJO_RED);
			Line(Tip, m_Pos + direction(A + 0.25f) * m_Radius * 0.3f, GOJO_RED);
		}
		for(int Arc = 0; Arc < 3; ++Arc)
			for(int i = 0; i < 8; ++i)
			{
				const vec2 Center = m_Pos - Aim * (18 + Arc * 22);
				const float A = angle(Aim) + pi * 0.6f + i * pi * 0.8f / 8;
				const float R = m_Radius * (1 + Arc * 0.18f);
				Line(Center + direction(A) * R, Center + direction(A + pi * 0.8f / 8) * R, GOJO_RED);
			}
	}
	else
	{
		// Hollow Purple remains a ROUND sphere: circular silhouette, inner
		// energy orbit and short surface cracks. No pointed nose or bullet tail.
		for(int i = 0; i < 24; ++i)
		{
			const float A = i * 2 * pi / 24;
			Line(m_Pos + direction(A) * m_Radius, m_Pos + direction(A + 2 * pi / 24) * m_Radius, GOJO_PURPLE);
		}
		for(int i = 0; i < 12; ++i)
		{
			const float A = Spin * 0.6f + i * pi / 6;
			const float R = m_Radius * 0.65f;
			Line(m_Pos + direction(A) * R, m_Pos + direction(A + pi / 6) * R, GOJO_PURPLE);
		}
		for(int i = 0; i < 6; ++i)
		{
			const float A = -Spin + i * pi / 3;
			Line(m_Pos + direction(A) * m_Radius * 0.45f, m_Pos + direction(A + 0.2f) * m_Radius * 0.85f, i & 1 ? GOJO_PURPLE : GOJO_BLUE);
			Line(m_Pos + direction(A + 0.2f) * m_Radius * 0.85f, m_Pos + direction(A + 0.1f) * m_Radius, GOJO_PURPLE);
		}
	}
}

CGojoDomain::CGojoDomain(CGameWorld *pWorld, int Owner, vec2 Pos) : CEntity(pWorld, CGameWorld::ENTTYPE_GOJO_DOMAIN, Pos), m_Owner(Owner)
{
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CCharacter *pChar = GameServer()->GetPlayerChar(CID);
		m_aAdmitted[CID] = pChar && pChar->GameWorld() == pWorld && distance(pChar->m_Pos, Pos) <= GOJO_DOMAIN_RADIUS;
	}
	for(int &ID : m_aIDs) ID = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
}

CGojoDomain::~CGojoDomain()
{
	for(int CID = 0; CID < MAX_CLIENTS; ++CID) Release(CID);
	for(int ID : m_aIDs) Server()->SnapFreeID(ID);
}

void CGojoDomain::Release(int CID)
{
	if(!m_aAffected[CID]) return;
	m_aAffected[CID] = false;
	CGojoState *pState = Controller()->GetGojoState(CID);
	if(!pState || pState->m_DomainSource != m_aIDs[0]) return;
	pState->m_DomainUntil = 0;
	pState->m_DomainSource = -1;
	pState->m_DomainFadeStart = Server()->Tick();
	pState->m_BrainUntil = maximum(pState->m_BrainUntil, Server()->Tick() + GOJO_BRAIN_SECONDS * Server()->TickSpeed());
}

void CGojoDomain::Reset()
{
	for(int CID = 0; CID < MAX_CLIENTS; ++CID) Release(CID);
	m_MarkedForDestroy = true;
}

void CGojoDomain::TickPaused()
{
	for(int &Tick : m_aLastBarrierHit) if(Tick > 0) ++Tick;
}

void GojoConstrainMovement(CCharacter *pCharacter, vec2 Previous)
{
	for(CEntity *p = pCharacter->GameWorld()->FindFirst(CGameWorld::ENTTYPE_GOJO_DOMAIN); p; p = p->TypeNext())
		static_cast<CGojoDomain *>(p)->ConstrainMovement(pCharacter, Previous);
}

void CGojoDomain::ConstrainMovement(CCharacter *pCharacter, vec2 Previous)
{
	if(m_MarkedForDestroy || pCharacter->GetPlayer()->GetCID() == m_Owner) return;
	const int CID = pCharacter->GetPlayer()->GetCID();
	vec2 Current = pCharacter->Core()->m_Pos;
	const float Boundary = GOJO_DOMAIN_RADIUS + CCharacter::ms_PhysSize;
	if(m_aAdmitted[CID])
	{
		if(distance(Current, m_Pos) > Boundary) m_aAdmitted[CID] = false;
		return; // Initially-inside players may leave, but not re-enter.
	}
	vec2 Closest = Previous;
	closest_point_on_line(Previous, Current, m_Pos, Closest);
	if(distance(Closest, m_Pos) >= Boundary && distance(Current, m_Pos) >= Boundary) return;
	const vec2 Step = Current - Previous;
	const vec2 Delta = Previous - m_Pos;
	const float A = dot(Step, Step), B = 2 * dot(Delta, Step), C = dot(Delta, Delta) - Boundary * Boundary;
	vec2 Normal = length(Delta) > 1 ? normalize(Delta) : vec2(1, 0);
	vec2 Position = m_Pos + Normal * (Boundary + 2);
	const float Discriminant = B * B - 4 * A * C;
	if(A > 0.001f && C >= 0 && Discriminant >= 0)
	{
		const float T = clamp((-B - sqrtf(Discriminant)) / (2 * A), 0.0f, 1.0f);
		const vec2 Entry = Previous + Step * T;
		if(distance(Entry, m_Pos) > 1) Normal = normalize(Entry - m_Pos);
		Position = Entry + Normal * 2;
	}
	if(GameServer()->Collision()->TestBox(Position, vec2(28, 28))) Position = Previous;
	pCharacter->Core()->m_Pos = Position;
	const float Inward = dot(pCharacter->Core()->m_Vel, Normal);
	if(Inward < 0) pCharacter->Core()->m_Vel -= Normal * Inward;
	if(Server()->Tick() - m_aLastBarrierHit[CID] >= maximum(1, Server()->TickSpeed() / 5))
	{
		m_aLastBarrierHit[CID] = Server()->Tick();
		GameWorld()->CreateHammerHit(m_Pos + Normal * GOJO_DOMAIN_RADIUS);
	}
}

void CGojoDomain::Tick()
{
	if(m_MarkedForDestroy || !RoleAlive(this, m_Owner) || ++m_Age > GOJO_DOMAIN_SECONDS * Server()->TickSpeed()) { Reset(); return; }
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	Controller()->GetGojoState(m_Owner)->m_LastActionTick = Server()->Tick();
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CCharacter *pTarget = GameServer()->GetPlayerChar(CID);
		CGojoState *pState = Controller()->GetGojoState(CID);
		if(!pTarget || !pState || !CanAffect(this, m_Owner, pTarget, GojoWeaponID(GOJO_DOMAIN)) ||
			distance(m_Pos, pTarget->m_Pos) > GOJO_DOMAIN_RADIUS || distance(pOwner->m_Pos, pTarget->m_Pos) <= GOJO_DOMAIN_SAFE_RADIUS)
		{
			Release(CID); continue;
		}
		if(!m_aAffected[CID] || !pState->Immobilized(Server()->Tick()))
		{
			m_aAffected[CID] = true;
			if(!pState->Immobilized(Server()->Tick())) pState->m_DomainStart = Server()->Tick();
			GameServer()->SendChatTarget(CID, "无量空处：信息过载，无法行动。离开后仍有15秒脑损伤。");
		}
		pState->m_DomainUntil = Server()->Tick() + 2;
		pState->m_DomainSource = m_aIDs[0];
		pState->m_DomainFadeStart = -1;
		pTarget->Core()->m_Vel = vec2(0, 0);
		pTarget->Core()->ResetDragVelocity();
		pTarget->ResetHook();
		if(m_Age % maximum(1, Server()->TickSpeed() / 2) == 0)
			pTarget->TakeDamage(vec2(0, 0), 1, m_Owner, WEAPON_LASER, GojoWeaponID(GOJO_DOMAIN), false);
	}
}

bool CGojoDomain::NetworkClipped(int CID) { return NetworkPointClipped(CID, m_Pos, vec2(GOJO_DOMAIN_RADIUS * 2, GOJO_DOMAIN_RADIUS * 2)); }

void CGojoDomain::Snap(int SnappingClient, int OtherMode)
{
	if(OtherMode || m_MarkedForDestroy) return;
	for(int i = 0; i < 32; ++i)
	{
		const float A = 2 * pi * i / 32;
		GojoSnapLine(Server(), m_aIDs[i], m_Pos + direction(A) * GOJO_DOMAIN_RADIUS, m_Pos + direction(A + 2 * pi / 32) * GOJO_DOMAIN_RADIUS, GOJO_DOMAIN, m_Owner);
	}
	for(int i = 0; i < 8; ++i)
	{
		const float A = i * pi / 4 + m_Age * 0.002f;
		const vec2 Axis = direction(A), Tangent(-Axis.y, Axis.x);
		const vec2 Center = m_Pos + Axis * (GOJO_DOMAIN_RADIUS * 0.91f);
		GojoSnapLine(Server(), m_aIDs[32 + i * 3], Center - Tangent * 16 - Axis * 14, Center + Tangent * 16 + Axis * 14, GOJO_DOMAIN, m_Owner);
		GojoSnapLine(Server(), m_aIDs[33 + i * 3], Center - Tangent * 16 + Axis * 14, Center + Tangent * 16 - Axis * 14, GOJO_DOMAIN, m_Owner);
		GojoSnapLine(Server(), m_aIDs[34 + i * 3], Center - Axis * 24, Center + Axis * 24, GOJO_PURPLE, m_Owner);
		const vec2 Noise = m_Pos + direction(-A + m_Age * 0.012f) * (120 + 40 * (i % 4));
		GojoSnapLine(Server(), m_aIDs[56 + i], Noise - Tangent * 10, Noise + Tangent * 10 + Axis * 8, GOJO_DOMAIN, m_Owner);
	}
}
