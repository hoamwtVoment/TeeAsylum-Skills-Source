#ifndef GAME_SERVER_WEAPONS_GOJO_H
#define GAME_SERVER_WEAPONS_GOJO_H

#include <game/server/gojo_state.h>
#include <game/server/weapon.h>

int GojoWeaponID(int Skill);
bool GojoIsWeapon(int WeaponID);
const char *GojoSkillName(int Skill);
int GojoSkillCooldownTicks(int Skill, int TickSpeed, bool BrainDamaged);

class CGojoWeapon : public CWeapon
{
	int m_Skill;
	int m_ChargeTicks = 0;
	bool m_Charging = false;
	int m_aIDs[48];
	void EnergyNotice(CGojoState *pState);
	void Fire(vec2 Direction) override;
	void Release();
	int MaxChargeTicks();
	int MinimumChargeTicks();

public:
	CGojoWeapon(CCharacter *pOwner, int Skill);
	~CGojoWeapon() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient, int OtherMode) override;
	bool IgnoreCooldown() override;
	int NumAmmoIcons() override;
	int GetType() override;
	float FixedWalkspeedTiles() override { return m_Charging ? 12.8f : -1.0f; }
	int Skill() const { return m_Skill; }
	bool Charging() const { return m_Charging; }
	bool ReadyToRelease() { return m_ChargeTicks >= MinimumChargeTicks(); }
	float Charge();
	void ResetCooldown();
};

template<int SkillIndex>
class CGojoSkill : public CGojoWeapon
{
public:
	CGojoSkill(CCharacter *pOwner) : CGojoWeapon(pOwner, SkillIndex) {}
};

using CGojoFist = CGojoSkill<GOJO_FIST>;
using CGojoBlue = CGojoSkill<GOJO_BLUE>;
using CGojoRed = CGojoSkill<GOJO_RED>;
using CGojoPurple = CGojoSkill<GOJO_PURPLE>;
using CGojoDomainSkill = CGojoSkill<GOJO_DOMAIN>;

#endif
