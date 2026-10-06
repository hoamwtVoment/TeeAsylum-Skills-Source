#include "gojo.h"

#include <game/server/entities/gojo.h>
#include <game/server/gamecontroller.h>
#include <game/server/weapons.h>

namespace
{
const char *gs_apNames[] = {"苍拳", "苍", "赫", "虚式·茈", "无量空处"};
const int gs_aCooldownMs[] = {650, 5000, 5000, 12000, 45000};
const int gs_aChargeMs[] = {0, 1800, 1300, 2600, 800};
const int gs_aMinimumChargeMs[] = {0, 500, 500, 1000, 800};
const int gs_aEnergyCost[] = {3, 12, 14, 28, 70};
const int gs_aChargeCost[] = {0, 20, 12, 22, 0};
}

int GojoWeaponID(int Skill) { return WEAPON_ID_GOJO_FIST + clamp(Skill, 0, NUM_GOJO_SKILLS - 1); }
bool GojoIsWeapon(int ID) { return ID >= WEAPON_ID_GOJO_FIST && ID <= WEAPON_ID_GOJO_DOMAIN; }
const char *GojoSkillName(int Skill) { return gs_apNames[clamp(Skill, 0, NUM_GOJO_SKILLS - 1)]; }
int GojoSkillCooldownTicks(int Skill, int TickSpeed, bool BrainDamaged)
{
	return maximum(1, gs_aCooldownMs[clamp(Skill, 0, NUM_GOJO_SKILLS - 1)] * (BrainDamaged ? 2 : 1) * TickSpeed / 1000);
}

CGojoWeapon::CGojoWeapon(CCharacter *pOwner, int Skill) : CWeapon(pOwner), m_Skill(Skill)
{
	m_Ammo = m_MaxAmmo = -1;
	m_FullAuto = true;
	m_FireDelay = 20;
	for(int &ID : m_aIDs) ID = Server()->SnapNewID();
}

CGojoWeapon::~CGojoWeapon()
{
	for(int ID : m_aIDs) Server()->SnapFreeID(ID);
}

bool CGojoWeapon::IgnoreCooldown() { return Character()->GetPlayer()->m_AsylumNoCooldown; }
int CGojoWeapon::GetType() { return m_Skill == GOJO_FIST ? WEAPON_HAMMER : m_Skill == GOJO_DOMAIN ? WEAPON_LASER : WEAPON_GUN; }
int CGojoWeapon::MaxChargeTicks() { return maximum(1, gs_aChargeMs[m_Skill] * Server()->TickSpeed() / 1000); }
int CGojoWeapon::MinimumChargeTicks() { return maximum(1, (gs_aMinimumChargeMs[m_Skill] * Server()->TickSpeed() + 999) / 1000); }
float CGojoWeapon::Charge() { return clamp(m_ChargeTicks / (float)MaxChargeTicks(), 0.0f, 1.0f); }

void CGojoWeapon::ResetCooldown()
{
	m_ReloadTimer = 0;
	CGojoState *pState = Character()->Controller()->GetGojoState(Character()->GetPlayer()->GetCID());
	if(pState) pState->m_NextCast[m_Skill] = 0;
}

void CGojoWeapon::Fire(vec2 Direction)
{
	const int CID = Character()->GetPlayer()->GetCID();
	CGojoState *pState = Character()->Controller()->GetGojoState(CID);
	if(!pState || !pState->m_Enabled || Character()->IsGojoImmobilized()) return;
	if(!IgnoreCooldown() && Server()->Tick() < pState->m_NextCast[m_Skill]) return;
	if(m_Skill == GOJO_BLUE && GojoHasBlue(GameWorld(), CID)) return;
	if((m_Skill == GOJO_RED || m_Skill == GOJO_PURPLE) && GojoHasOrb(GameWorld(), CID, m_Skill)) return;
	if(m_Skill == GOJO_DOMAIN && GojoHasDomain(GameWorld(), CID)) return;
	if(m_Charging) return;
	if(!Character()->GetPlayer()->m_AsylumInfCursedEnergy && pState->m_CursedEnergy < gs_aEnergyCost[m_Skill]) { EnergyNotice(pState); return; }
	Character()->Protect(0.0f, false);
	pState->m_LastActionTick = Server()->Tick();
	if(m_Skill != GOJO_FIST)
	{
		if(!m_Charging)
		{
			m_Charging = true;
			m_ChargeTicks = 0;
			if(m_Skill == GOJO_PURPLE)
				GameServer()->SendChatTarget(CID, "九纲，偏光，表里之间——虚式·茈。按住念咒蓄力，松开发出。");
		}
		return;
	}
	if(!pState->SpendEnergy(gs_aEnergyCost[GOJO_FIST], Server()->Tick(), Character()->GetPlayer()->m_AsylumInfCursedEnergy)) return;
	pState->m_LastCastSkill = GOJO_FIST;
	pState->m_LastCastTick = Server()->Tick();
	pState->m_NextCast[GOJO_FIST] = IgnoreCooldown() ? 0 : Server()->Tick() + GojoSkillCooldownTicks(GOJO_FIST, Server()->TickSpeed(), pState->BrainDamaged(Server()->Tick()));
	const vec2 Fist = Pos() + Direction * 48.0f;
	CEntity *apEntities[MAX_CLIENTS];
	int Count = GameWorld()->FindEntities(Pos(), 152, apEntities, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Count; ++i)
	{
		auto *pTarget = static_cast<CCharacter *>(apEntities[i]);
		const int Victim = pTarget->GetPlayer()->GetCID();
		if(pTarget == Character() || pTarget->IsSolo() || Character()->IsSolo() ||
			!Character()->Controller()->CanWeaponInteract(CID, Victim, GetWeaponID()) ||
			GameServer()->Collision()->IntersectLine(Pos(), pTarget->m_Pos, nullptr, nullptr)) continue;
		const vec2 Delta = pTarget->m_Pos - Pos();
		if(dot(Delta, Direction) < 0 || fabs(dot(Delta, vec2(-Direction.y, Direction.x))) > 64) continue;
		const vec2 Pull = Fist - pTarget->m_Pos;
		const vec2 Force = length(Pull) > 1 ? normalize(Pull) * 10.0f + vec2(0, -2) : vec2(0, -2);
		// Blue's attraction contributes six extra points over the base punch.
		pTarget->TakeDamage(Force, 40, CID, WEAPON_HAMMER, GetWeaponID(), false);
		GameWorld()->CreateHammerHit(pTarget->m_Pos);
	}
	if(Character()->Controller()->IntersectCombatNpc(Pos(), Pos() + Direction * 152, 64, nullptr))
		Character()->Controller()->DamageCombatNpcBox(CID, GetWeaponID(), 40, Pos(), Direction, 152, 64);
	GameWorld()->CreateHammerHit(Fist);
	GameWorld()->CreateSound(Pos(), SOUND_HAMMER_FIRE);
}

void CGojoWeapon::Release()
{
	const int CID = Character()->GetPlayer()->GetCID();
	CGojoState *pState = Character()->Controller()->GetGojoState(CID);
	const float Charged = Charge();
	m_Charging = false;
	if(!pState || !pState->m_Enabled || !ReadyToRelease()) return;
	if(m_Skill == GOJO_DOMAIN && Charged < 1.0f) return;
	const float Cost = gs_aEnergyCost[m_Skill] + gs_aChargeCost[m_Skill] * Charged;
	if(!pState->SpendEnergy(Cost, Server()->Tick(), Character()->GetPlayer()->m_AsylumInfCursedEnergy)) { EnergyNotice(pState); return; }
	pState->m_LastCastSkill = m_Skill;
	pState->m_LastCastTick = Server()->Tick();
	pState->m_LastActionTick = Server()->Tick();
	pState->m_NextCast[m_Skill] = IgnoreCooldown() ? 0 : Server()->Tick() + GojoSkillCooldownTicks(m_Skill, Server()->TickSpeed(), pState->BrainDamaged(Server()->Tick()));
	Character()->Protect(0.0f, false);
	if(m_Skill == GOJO_DOMAIN)
	{
		if(!GojoHasDomain(GameWorld(), CID)) new CGojoDomain(GameWorld(), CID, Pos());
		GameServer()->SendChatTarget(CID, "领域展开——无量空处。");
	}
	else
	{
		const vec2 Aim = Character()->GetAimDirection();
		new CGojoOrb(GameWorld(), CID, m_Skill, Pos() + Aim * 48.0f, Aim, Charged);
		if(m_Skill == GOJO_BLUE)
			GameServer()->SendChatTarget(CID, Charged >= 0.99f ? "最大输出·苍。体积翻倍、飞行提速；按住左键快速引导。" : "术式顺转·苍。再次按住左键引导，松开后高速飞行。");
	}
	GameWorld()->CreateSound(Pos(), m_Skill == GOJO_RED ? SOUND_GRENADE_FIRE : SOUND_LASER_FIRE);
}

void CGojoWeapon::Tick()
{
	CWeapon::Tick();
	CGojoState *pState = Character()->Controller()->GetGojoState(Character()->GetPlayer()->GetCID());
	if(!pState || !pState->m_Enabled || Character()->CurrentWeapon() != this || Character()->IsFrozen() || Character()->IsDisabled() || Character()->IsGojoImmobilized())
	{
		m_Charging = false;
		m_ChargeTicks = 0;
		return;
	}
	if(!m_Charging) return;
	pState->m_LastActionTick = Server()->Tick();
	if(!pState->BrainDamaged(Server()->Tick()) || Server()->Tick() % 2 == 0)
		m_ChargeTicks = minimum(MaxChargeTicks(), m_ChargeTicks + 1);
	if(!Character()->IsFireHeld() || (m_Skill == GOJO_DOMAIN && m_ChargeTicks >= MaxChargeTicks())) Release();
}

void CGojoWeapon::TickPaused() { CWeapon::TickPaused(); }

void CGojoWeapon::EnergyNotice(CGojoState *pState)
{
	if(Server()->Tick() - pState->m_LastEnergyNoticeTick < Server()->TickSpeed()) return;
	pState->m_LastEnergyNoticeTick = Server()->Tick();
	GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "咒力不足，未释放术式，不进入冷却；停止消耗2秒后蓝条恢复。");
}

int CGojoWeapon::NumAmmoIcons()
{
	CGojoState *pState = Character()->Controller()->GetGojoState(Character()->GetPlayer()->GetCID());
	if(!pState) return 0;
	return clamp(round_to_int(pState->m_CursedEnergy * 10 / GOJO_CURSED_ENERGY_MAX), 0, 10);
}

void CGojoWeapon::Snap(int SnappingClient, int OtherMode)
{
	if(OtherMode || Character()->CurrentWeapon() != this || Character()->NetworkClipped(SnappingClient)) return;
	const int CID = Character()->GetPlayer()->GetCID();
	const CGojoState *pState = Character()->Controller()->GetGojoState(CID);
	if(!pState || !pState->m_Enabled) return;
	const vec2 Aim = Character()->GetAimDirection(), Side(-Aim.y, Aim.x);
	const vec2 Center = Pos() + Aim * 35;
	const float Power = m_Charging ? Charge() : 0;
	const float Radius = 10 + Power * 28;
	const float Spin = Server()->Tick() * 0.13f;
	int Index = 0;
	auto Line = [&](vec2 A, vec2 B, int Kind) {
		if(Index < 32) GojoSnapLine(Server(), m_aIDs[Index++], A, B, Kind, CID);
	};
	if(m_Skill == GOJO_PURPLE)
	{
		// Blue/red poles converge during the chant; Purple forms between them.
		for(int Pole = -1; Pole <= 1; Pole += 2)
		{
			const vec2 Focus = Center + Side * (Pole * (30 - 22 * Power));
			for(int i = 0; i < 4; ++i)
				Line(Focus + direction(Spin + i * pi / 2) * (8 + 6 * Power), Focus + direction(Spin + (i + 1) * pi / 2) * (8 + 6 * Power), Pole < 0 ? GOJO_BLUE : GOJO_RED);
			Line(Focus, Center + Aim * (8 + 10 * Power), Pole < 0 ? GOJO_BLUE : GOJO_RED);
		}
		for(int i = 0; i < 6; ++i)
			Line(Center + direction(-Spin + i * pi / 3) * Radius, Center + direction(-Spin + (i + 2) * pi / 3) * Radius, GOJO_PURPLE);
	}
	else if(m_Skill == GOJO_BLUE)
	{
		for(int Arm = 0; Arm < 4; ++Arm)
			for(int i = 0; i < 3; ++i)
			{
				const float A = Spin + Arm * pi / 2 + i * 0.65f;
				Line(Center + direction(A) * (Radius * i / 3), Center + direction(A + 0.65f) * (Radius * (i + 1) / 3), GOJO_BLUE);
			}
	}
	else if(m_Skill == GOJO_RED)
	{
		for(int i = 0; i < 8; ++i)
		{
			const float A = -Spin + i * pi / 4;
			const vec2 Tip = Center + direction(A) * Radius;
			Line(Center + direction(A - 0.2f) * Radius * 0.25f, Tip, GOJO_RED);
			Line(Tip, Center + direction(A + 0.2f) * Radius * 0.25f, GOJO_RED);
		}
	}
	else if(m_Skill == GOJO_DOMAIN)
	{
		for(int Ring = 0; Ring < 2; ++Ring)
			for(int i = 0; i < 4; ++i)
				Line(Center + direction(Spin * (Ring ? -1 : 1) + i * pi / 2) * (Radius + Ring * 7), Center + direction(Spin * (Ring ? -1 : 1) + (i + 1) * pi / 2) * (Radius + Ring * 7), GOJO_DOMAIN);
		for(int i = 0; i < 6; ++i) Line(Center + direction(i * pi / 3) * 4, Center + direction(i * pi / 3) * Radius, GOJO_DOMAIN);
	}
	else
	{
		// Angular knuckles plus a Blue crescent, not another held orb.
		for(int i = 0; i < 4; ++i) Line(Center + Side * (i * 5 - 8) - Aim * 6, Center + Side * (i * 5 - 8) + Aim * 7, GOJO_BLUE);
		for(int i = 0; i < 6; ++i)
		{
			const float A = angle(Aim) + pi * 0.6f + i * pi * 0.8f / 6;
			Line(Center + direction(A) * 17, Center + direction(A + pi * 0.8f / 6) * 17, GOJO_BLUE);
		}
		const int Age = Server()->Tick() - pState->m_LastCastTick;
		if(pState->m_LastCastSkill == GOJO_FIST && Age >= 0 && Age < Server()->TickSpeed() / 4)
			for(int i = -2; i <= 2; ++i)
				Line(Center + Side * (i * 8), Center + Aim * (50 + Age * 3) + Side * (i * 14), GOJO_BLUE);
	}
	// Real blue gauge, local to its owner; ammo icons also show the same resource.
	if(SnappingClient == CID || SnappingClient == -1)
	{
		const vec2 Left = Pos() + vec2(-40, -64);
		const float Fill = clamp(pState->m_CursedEnergy / GOJO_CURSED_ENERGY_MAX, 0.0f, 1.0f);
		for(int i = 0; i < 8; ++i)
		{
			const float FilledWidth = clamp(Fill * 80 - i * 10, 0.0f, 9.0f);
			if(FilledWidth > 0) GojoSnapLine(Server(), m_aIDs[32 + i], Left + vec2(i * 10, 0), Left + vec2(i * 10 + FilledWidth, 0), GOJO_BLUE, CID);
		}
		GojoSnapLine(Server(), m_aIDs[40], Left + vec2(0, -4), Left + vec2(0, 4), GOJO_BLUE, CID);
		GojoSnapLine(Server(), m_aIDs[41], Left + vec2(80, -4), Left + vec2(80, 4), GOJO_BLUE, CID);
	}
	const int JumpAge = Server()->Tick() - pState->m_LastInfinityJumpTick;
	if(JumpAge >= 0 && JumpAge < Server()->TickSpeed() / 3)
	{
		const vec2 Foot = Pos() + vec2(0, 26 + JumpAge);
		for(int i = 0; i < 6; ++i)
			GojoSnapLine(Server(), m_aIDs[42 + i], Foot + vec2((i - 3) * 11, 4), Foot + vec2((i - 3) * 13 + 8, -4), GOJO_BLUE, CID);
	}
}
