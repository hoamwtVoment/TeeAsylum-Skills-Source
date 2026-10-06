#ifndef GAME_SERVER_GOJO_STATE_H
#define GAME_SERVER_GOJO_STATE_H

#include <game/gamecore.h>

enum EGojoSkill
{
	GOJO_FIST,
	GOJO_BLUE,
	GOJO_RED,
	GOJO_PURPLE,
	GOJO_DOMAIN,
	NUM_GOJO_SKILLS
};

enum
{
	GOJO_DOMAIN_BASE_MS = 2147400000,
	GOJO_DOMAIN_RADIUS = 640, // 40 tiles across, smaller than 50.
	GOJO_DOMAIN_SAFE_RADIUS = 96,
	GOJO_DOMAIN_SECONDS = 6,
	GOJO_BRAIN_SECONDS = 15,
	GOJO_CURSED_ENERGY_MAX = 200,
	GOJO_EXTRA_JUMP_COST = 12
};

// Room-owned: explicit RCON transformation only. Never part of Asylum's item
// table, random pools, upgrade modules or ordinary give/loadout commands.
struct CGojoState
{
	bool m_Enabled = false;
	int m_NextCast[NUM_GOJO_SKILLS] = {};
	int m_LastActionTick = -1000000;
	int m_LastHealTick = 0;
	float m_Infinity = 100.0f;
	float m_CursedEnergy = GOJO_CURSED_ENERGY_MAX;
	int m_LastEnergySpendTick = -1000000;
	int m_LastEnergyNoticeTick = -1000000;
	int m_LastCastSkill = -1;
	int m_LastCastTick = -1000000;
	int m_LastInfinityJumpTick = -1000000;
	int m_InfinityBlockUntil = 0;
	int m_DomainUntil = 0;
	int m_DomainStart = 0;
	int m_DomainSource = -1;
	int m_DomainFadeStart = -1;
	int m_BrainUntil = 0;
	int m_CarryUntil = 0;
	int m_CarrySource = -1;
	vec2 m_CarryVelocity = vec2(0, 0);
	vec2 m_CarryTarget = vec2(0, 0);
	int m_BluePullUntil = 0;
	vec2 m_BluePullVelocity = vec2(0, 0);

	bool Immobilized(int Tick) const { return m_DomainUntil > Tick; }
	bool BrainDamaged(int Tick) const { return m_BrainUntil > Tick; }
	bool Carried(int Tick) const { return m_CarryUntil > Tick; }
	bool BluePulled(int Tick) const { return m_BluePullUntil > Tick; }
	bool SpendEnergy(float Cost, int Tick, bool Infinite = false)
	{
		if(Infinite) { m_CursedEnergy = GOJO_CURSED_ENERGY_MAX; return true; }
		if(m_CursedEnergy < Cost) return false;
		m_CursedEnergy = maximum(0.0f, m_CursedEnergy - Cost);
		m_LastEnergySpendTick = Tick;
		return true;
	}
	void ClearEffects()
	{
		m_DomainUntil = m_DomainStart = m_BrainUntil = m_CarryUntil = 0;
		m_DomainSource = m_DomainFadeStart = m_CarrySource = -1;
		m_CarryVelocity = vec2(0, 0);
		m_CarryTarget = m_BluePullVelocity = vec2(0, 0);
		m_BluePullUntil = 0;
	}
	void Pause()
	{
		for(int &Tick : m_NextCast) if(Tick > 0) ++Tick;
		if(m_LastActionTick >= 0) ++m_LastActionTick;
		if(m_LastEnergySpendTick >= 0) ++m_LastEnergySpendTick;
		if(m_LastEnergyNoticeTick >= 0) ++m_LastEnergyNoticeTick;
		if(m_LastCastTick >= 0) ++m_LastCastTick;
		if(m_LastInfinityJumpTick >= 0) ++m_LastInfinityJumpTick;
		++m_LastHealTick;
		if(m_InfinityBlockUntil > 0) ++m_InfinityBlockUntil;
		if(m_DomainUntil > 0) ++m_DomainUntil;
		if(m_DomainStart > 0) ++m_DomainStart;
		if(m_DomainFadeStart >= 0) ++m_DomainFadeStart;
		if(m_BrainUntil > 0) ++m_BrainUntil;
		if(m_CarryUntil > 0) ++m_CarryUntil;
		if(m_BluePullUntil > 0) ++m_BluePullUntil;
	}
};

#endif
