#ifndef GAME_SERVER_WEAPONS_ASYLUM_H
#define GAME_SERVER_WEAPONS_ASYLUM_H

#include <game/server/weapon.h>

enum EAsylumItem
{
	ASYLUM_PAN, ASYLUM_KNIFE, ASYLUM_BAT,
	ASYLUM_REVOLVER, ASYLUM_SMG, ASYLUM_SHOTGUN, ASYLUM_RAILGUN, ASYLUM_LAUNCHER,
	ASYLUM_MEDKIT, ASYLUM_DASH, ASYLUM_SHOCKWAVE, ASYLUM_CLUSTER,
	// Item Asylum inspired additions. Keep these after the original IDs so old
	// loadouts and demos remain stable.
	ASYLUM_SPOON, ASYLUM_DARKHEART, ASYLUM_ENERGYSWORD, ASYLUM_DYINGPAN,
	ASYLUM_CROSSBOW, ASYLUM_FREEZERAY, ASYLUM_BAZOOKA, ASYLUM_AMERICA,
	ASYLUM_MANTLE, ASYLUM_REROLL, ASYLUM_PARASOL, ASYLUM_CLEAVE,
	ASYLUM_LILYNETTE, ASYLUM_CHICAGO, ASYLUM_M1911, ASYLUM_BOW,
	ASYLUM_PIXELGUN, ASYLUM_VAMPIREKNIVES, ASYLUM_TASER, ASYLUM_HYPERLASER,
	NUM_ASYLUM_ITEMS
};

enum EAsylumCategory
{
	ASYLUM_CATEGORY_MELEE,
	ASYLUM_CATEGORY_RANGED,
	ASYLUM_CATEGORY_UTILITY,
};

struct SAsylumItem
{
	const char *m_pName;
	const char *m_pDescription;
	int m_Type;
	int m_Damage;
	int m_DelayMs;
	float m_Force;
	int m_Category;
};

const SAsylumItem &AsylumItem(int Item);
int AsylumRandomItem(int Category);
int AsylumWeaponID(int Item);
bool AsylumIsWeapon(int WeaponID);

class CAsylumWeapon : public CWeapon
{
	int m_Item;
	int m_MantleCharges;
	int m_Charge;
	int m_ENextTick;
	int m_RNextTick;
	int m_CounterUntil;
	int m_UltimateStartTick;
	int m_UltimateShots;
	void Fire(vec2 Direction) override;
	void FireUltimateBeam(vec2 Direction);
	static bool UltimateLaserHit(class CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool BulletHit(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool GrenadeHit(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool LaserHit(class CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife);

public:
	CAsylumWeapon(CCharacter *pOwner, int Item);
	int GetType() override { return AsylumItem(m_Item).m_Type; }
	int NumAmmoIcons() override;
	int Item() const { return m_Item; }
	int CooldownTicks() const { return m_ReloadTimer; }
	bool ConsumeMantle() { if(m_MantleCharges <= 0) return false; --m_MantleCharges; return true; }
	int MantleCharges() const { return m_MantleCharges; }
	void Tick() override;
	void TickPaused() override;
	void AddCharge(int ActualDamage);
	bool ActivateSkill(bool Ultimate, vec2 Direction);
	bool HandleIncomingDamage(vec2 &Force, int &Damage, int From);
	void SkillStatus(char *pBuf, int Size);
};

template<int ItemIndex>
class CAsylumItem : public CAsylumWeapon
{
public:
	CAsylumItem(CCharacter *pOwner) : CAsylumWeapon(pOwner, ItemIndex) {}
};

using CAsylumPan = CAsylumItem<ASYLUM_PAN>;
using CAsylumKnife = CAsylumItem<ASYLUM_KNIFE>;
using CAsylumBat = CAsylumItem<ASYLUM_BAT>;
using CAsylumRevolver = CAsylumItem<ASYLUM_REVOLVER>;
using CAsylumSMG = CAsylumItem<ASYLUM_SMG>;
using CAsylumShotgun = CAsylumItem<ASYLUM_SHOTGUN>;
using CAsylumRailgun = CAsylumItem<ASYLUM_RAILGUN>;
using CAsylumLauncher = CAsylumItem<ASYLUM_LAUNCHER>;
using CAsylumMedkit = CAsylumItem<ASYLUM_MEDKIT>;
using CAsylumDash = CAsylumItem<ASYLUM_DASH>;
using CAsylumShockwave = CAsylumItem<ASYLUM_SHOCKWAVE>;
using CAsylumCluster = CAsylumItem<ASYLUM_CLUSTER>;
using CAsylumSpoon = CAsylumItem<ASYLUM_SPOON>;
using CAsylumDarkheart = CAsylumItem<ASYLUM_DARKHEART>;
using CAsylumEnergySword = CAsylumItem<ASYLUM_ENERGYSWORD>;
using CAsylumDyingPan = CAsylumItem<ASYLUM_DYINGPAN>;
using CAsylumCrossbow = CAsylumItem<ASYLUM_CROSSBOW>;
using CAsylumFreezeRay = CAsylumItem<ASYLUM_FREEZERAY>;
using CAsylumBazooka = CAsylumItem<ASYLUM_BAZOOKA>;
using CAsylumAmerica = CAsylumItem<ASYLUM_AMERICA>;
using CAsylumMantle = CAsylumItem<ASYLUM_MANTLE>;
using CAsylumReroll = CAsylumItem<ASYLUM_REROLL>;
using CAsylumParasol = CAsylumItem<ASYLUM_PARASOL>;
using CAsylumCleave = CAsylumItem<ASYLUM_CLEAVE>;
using CAsylumLilynette = CAsylumItem<ASYLUM_LILYNETTE>;
using CAsylumChicago = CAsylumItem<ASYLUM_CHICAGO>;
using CAsylumM1911 = CAsylumItem<ASYLUM_M1911>;
using CAsylumBow = CAsylumItem<ASYLUM_BOW>;
using CAsylumPixelGun = CAsylumItem<ASYLUM_PIXELGUN>;
using CAsylumVampireKnives = CAsylumItem<ASYLUM_VAMPIREKNIVES>;
using CAsylumTaser = CAsylumItem<ASYLUM_TASER>;
using CAsylumHyperlaser = CAsylumItem<ASYLUM_HYPERLASER>;

#endif
