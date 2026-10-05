#include "asylum.h"

#include <game/server/entities/laser.h>
#include <game/server/entities/projectile.h>
#include <game/server/entities/asylum_visual_laser.h>
#include <game/server/gamemodes/huntern.h>
#include <game/server/weapons.h>

namespace
{
const SAsylumItem gs_aItems[] = {
	{"平底锅 / Pan", "40伤害 / 中速挥击", WEAPON_HAMMER, 40, 600, 11.0f, 0},
	{"小刀 / Knife", "20伤害 / 快速连击", WEAPON_HAMMER, 20, 230, 3.0f, 0},
	{"球棒 / Bat", "30伤害 / 强力击退", WEAPON_HAMMER, 30, 850, 21.0f, 0},
	{"左轮 / Revolver", "30伤害 / 精准单发", WEAPON_GUN, 30, 420, 1.0f, 1},
	{"冲锋枪 / SMG", "10伤害 / 高速连射", WEAPON_GUN, 10, 100, 0.3f, 1},
	{"霰弹枪 / Shotgun", "5发散射 / 每发10伤害", WEAPON_SHOTGUN, 10, 700, 1.5f, 1},
	{"电磁炮 / Railgun", "70伤害 / 慢速激光", WEAPON_LASER, 70, 1500, 2.0f, 1},
	{"榴弹炮 / Launcher", "爆炸最高60伤害 / 可自伤", WEAPON_GRENADE, 60, 1000, 0.0f, 1},
	{"急救包 / Medkit", "一次性回复100%最大生命", WEAPON_HAMMER, 0, 8000, 0.0f, 2},
	{"冲刺汽水 / Dash", "朝准星冲刺 / 冷却2.5秒", WEAPON_GUN, 0, 2500, 0.0f, 2},
	{"冲击波 / Shockwave", "周围20伤害+击退 / 冷却4秒", WEAPON_LASER, 20, 4000, 19.0f, 2},
	{"破片雷 / Cluster", "爆炸+10枚10伤碎片 / 冷却3秒", WEAPON_GRENADE, 40, 3000, 0.0f, 2},
	{"巨大汤勺 / Spoon", "25伤害 / 4格长判定 / 击退+冻结倒地", WEAPON_HAMMER, 25, 1000, 24.0f, 0},
	{"黑心剑 / Darkheart", "28伤害 / 吸血50% / E冲刺45 / R旋风", WEAPON_HAMMER, 28, 650, 5.0f, 0},
	{"能量剑 / Energy sword", "36伤害 / 0.75秒斩击", WEAPON_HAMMER, 36, 750, 7.0f, 0},
	{"垂死平底锅 / Dying pan", "26伤害 / 向下重击+冻结倒地", WEAPON_HAMMER, 26, 1000, 16.0f, 0},
	{"弩 / Crossbow", "82伤害 / 距离最高150 / 1发装填3秒", WEAPON_GUN, 82, 1000, 3.0f, 1},
	{"冰冻射线 / Freeze ray", "10伤害 / 冻结1.5秒 / 冷却3秒", WEAPON_LASER, 10, 3000, 0.0f, 1},
	{"英式火箭筒 / British bazooka", "2/3软弹15伤或1/3茶杯45伤 / 直接命中", WEAPON_GRENADE, 45, 1000, 8.0f, 1},
	{"美国 / America", "1000伤害一次性激光 / 巨大后坐力", WEAPON_LASER, 1000, 3200, 9.0f, 1},
	{"神圣斗篷 / Holy mantle", "一次性激活护盾 / 抵挡下一次攻击", WEAPON_HAMMER, 0, 20000, 0.0f, 2},
	{"重抽骰子 / Re-roll dice", "原地重抽三件装备 / 不回血", WEAPON_GUN, 0, 10000, 0.0f, 2},
	{"滑翔伞 / Parasol", "持有时减缓下落 / 使用上升", WEAPON_GUN, 0, 4500, 0.0f, 2},
	{"连斩 / Cleave", "前方40伤害 / 10秒冷却", WEAPON_HAMMER, 40, 10000, 2.0f, 2},
	{"双枪 / Lilynette", "30伤短激光穿透弹 / E反击 / 250实伤充能R", WEAPON_GUN, 30, 450, 0.0f, 1},
	{"芝加哥 / Chicago", "4伤精准连射 / 0.1秒", WEAPON_GUN, 4, 100, 0.0f, 1},
	{"M1911", "24至10伤单发 / 距离衰减", WEAPON_GUN, 24, 200, 0.0f, 1},
	{"铲子弓 / Bow", "44伤重力投射物 / 强击退", WEAPON_GRENADE, 44, 2500, 15.0f, 1},
	{"像素枪 / Pixel gun", "12至8伤精准射击 / 0.25秒", WEAPON_GUN, 12, 250, 0.0f, 1},
	{"吸血飞刀 / Vampire knives", "4至8枚飞刀各4伤 / 实伤50%回血", WEAPON_SHOTGUN, 4, 500, 0.0f, 1},
	{"电击枪 / Taser", "45至30伤 / 短暂冻结 / 3.5秒", WEAPON_LASER, 45, 3500, 0.0f, 1},
	{"超能激光 / Hyperlaser", "20伤小范围爆破 / 电击目标35伤", WEAPON_GUN, 20, 660, 0.0f, 1},
	{"mad noob's wisdom", "M1911击杀5名玩家后获得 / 使用后升级并消耗", WEAPON_GUN, 0, 250, 0.0f, 3},
	{"pixelated weapon permit", "Pixel gun击杀5名玩家后获得 / 使用后升级并消耗", WEAPON_GUN, 0, 250, 0.0f, 3},
	{"mad noob's shotgun", "4发逐颗装弹 / 8发散射每发14伤", WEAPON_SHOTGUN, 14, 250, 0.0f, 4},
	{"pixel rifle", "35发弹匣 / 14伤 / 450RPM全自动", WEAPON_GUN, 14, 133, 0.0f, 4},
	{"9mm", "13至8伤 / 17发弹匣 / 1秒装弹 / 4杀升级", WEAPON_GUN, 13, 400, 0.0f, 1},
	{"消音手枪 / Suppressed pistol", "19至7伤 / 12发 / 2秒装弹 / 6杀升级", WEAPON_GUN, 19, 250, 0.0f, 1},
	{"SSG-08", "45至30伤 / 7发 / 3秒装弹 / 10杀升级", WEAPON_GUN, 45, 1500, 0.0f, 1},
	{"脉冲枪 / Blaster", "三连发每发15伤 / R充能2.5秒后下一发30伤+定身", WEAPON_GUN, 15, 600, 0.0f, 1},
	{"ammu-nation coupon", "9mm击杀4人 / 使用后升级micro smg并消耗", WEAPON_GUN, 0, 250, 0.0f, 3},
	{"pro hitman training", "消音手枪击杀6人 / 使用后升级MAC-10并消耗", WEAPON_GUN, 0, 250, 0.0f, 3},
	{"s1mple private training lessons", "SSG-08击杀10人 / 使用后升级AWP并消耗", WEAPON_GUN, 0, 250, 0.0f, 3},
	{"Micro SMG", "13至9伤 / 50发 / 300RPM / 1秒装弹", WEAPON_GUN, 13, 200, 0.0f, 4},
	{"Suppressed MAC-10", "9至4伤 / 32发 / 1200RPM / 装弹近身10伤+冻结", WEAPON_GUN, 9, 50, 0.0f, 4},
	{"AWP", "躯干150伤 / 四肢75伤 / 10发 / 3秒装弹", WEAPON_GUN, 150, 2000, 0.0f, 4},
};
static_assert(sizeof(gs_aItems) / sizeof(gs_aItems[0]) == NUM_ASYLUM_ITEMS, "Item table mismatch");

int ItemFromWeapon(int ID) { return clamp(ID - WEAPON_ID_ASYLUM_PAN, 0, NUM_ASYLUM_ITEMS - 1); }

bool CanAffect(CCharacter *pOwner, CCharacter *pTarget, int WeaponID)
{
	if(!pTarget || !pTarget->IsAlive() || pOwner == pTarget || pTarget->IsSolo() || pOwner->IsSolo())
		return false;
	return ((CGameControllerHunterN *)pOwner->Controller())->CanWeaponInteract(pOwner->GetPlayer()->GetCID(), pTarget->GetPlayer()->GetCID(), WeaponID);
}
}

const SAsylumItem &AsylumItem(int Item) { return gs_aItems[clamp(Item, 0, NUM_ASYLUM_ITEMS - 1)]; }

int AsylumRandomItem(int Category)
{
	int aPool[NUM_ASYLUM_ITEMS], Count = 0;
	for(int i = 0; i < NUM_ASYLUM_ITEMS; ++i)
		if(gs_aItems[i].m_Category == Category)
			aPool[Count++] = i;
	return Count ? aPool[secure_rand_below(Count)] : ASYLUM_PAN;
}

int AsylumWeaponID(int Item) { return WEAPON_ID_ASYLUM_PAN + clamp(Item, 0, NUM_ASYLUM_ITEMS - 1); }
bool AsylumIsWeapon(int ID) { return ID >= WEAPON_ID_ASYLUM_PAN && ID < WEAPON_ID_ASYLUM_PAN + NUM_ASYLUM_ITEMS; }

int AsylumUpgradeKills(int Item)
{
	if(Item == ASYLUM_M1911 || Item == ASYLUM_PIXELGUN) return 5;
	if(Item == ASYLUM_9MM) return 4;
	if(Item == ASYLUM_SUPPRESSED_PISTOL) return 6;
	return Item == ASYLUM_SSG08 ? 10 : 0;
}

int AsylumUpgradeBase(int Module)
{
	if(Module == ASYLUM_UPGRADE_M1911) return ASYLUM_M1911;
	if(Module == ASYLUM_UPGRADE_PIXELGUN) return ASYLUM_PIXELGUN;
	if(Module == ASYLUM_UPGRADE_9MM) return ASYLUM_9MM;
	if(Module == ASYLUM_UPGRADE_SUPPRESSED) return ASYLUM_SUPPRESSED_PISTOL;
	return Module == ASYLUM_UPGRADE_SSG08 ? ASYLUM_SSG08 : -1;
}

int AsylumUpgradeModule(int Base)
{
	for(int Item = 0; Item < NUM_ASYLUM_ITEMS; ++Item)
		if(AsylumItem(Item).m_Category == ASYLUM_CATEGORY_UPGRADE && AsylumUpgradeBase(Item) == Base) return Item;
	return -1;
}

int AsylumUpgradeResult(int Base)
{
	if(Base == ASYLUM_M1911) return ASYLUM_M1911_UPG;
	if(Base == ASYLUM_PIXELGUN) return ASYLUM_PIXELGUN_UPG;
	if(Base == ASYLUM_9MM) return ASYLUM_MICROSMG;
	if(Base == ASYLUM_SUPPRESSED_PISTOL) return ASYLUM_SUPPRESSED_MAC10;
	return Base == ASYLUM_SSG08 ? ASYLUM_AWP : -1;
}

int AsylumRangedDamage(int Item, float Range, bool Head, bool Limb)
{
	auto Falloff = [&](float Base, float End, float StartStuds, float EndStuds) {
		return Base + (End - Base) * clamp((Range - CAsylumWeapon::IAStudToDDNet(StartStuds)) /
			CAsylumWeapon::IAStudToDDNet(EndStuds - StartStuds), 0.0f, 1.0f);
	};
	if(Item == ASYLUM_9MM || Item == ASYLUM_MICROSMG)
		return round_to_int(Falloff(12.5f, Item == ASYLUM_9MM ? 8.3f : 8.5f, 60, 100));
	if(Item == ASYLUM_SUPPRESSED_PISTOL) return round_to_int(Falloff(19, 6.5f, 20, 80) * (Head ? 1.3f : 1));
	if(Item == ASYLUM_SSG08) return round_to_int(Falloff(45, 30, 100, 300) * (Head ? 1.6f : 1));
	if(Item == ASYLUM_SUPPRESSED_MAC10) return round_to_int(Falloff(9, 4, 30, 60) * (Head ? 1.2f : 1));
	if(Item == ASYLUM_AWP) return Limb ? 75 : 150;
	return AsylumItem(Item).m_Damage;
}

CAsylumWeapon::CAsylumWeapon(CCharacter *pOwner, int Item) : CWeapon(pOwner), m_Item(Item), m_MantleCharges(0),
	m_Charge(0), m_ENextTick(0), m_RNextTick(Item == ASYLUM_LILYNETTE ? pOwner->Server()->Tick() + pOwner->Server()->TickSpeed() * 17 : 0),
	m_CounterUntil(0), m_UltimateStartTick(-1), m_UltimateShots(0), m_MagazineSize(0),
	m_MagazineReloadMs(0), m_MagazineReloadEnd(0), m_ShellReload(false), m_DarkheartSpinEnd(0), m_DarkheartNextHit(0),
	m_DashEnd(0), m_DashDirection(0, 0), m_DashLastPos(0, 0), m_aDashHit{}, m_SpeedBoostEnd(0), m_SpeedBoostType(0)
{
	m_MaxAmmo = m_Ammo = -1;
	m_FireDelay = AsylumItem(Item).m_DelayMs;
	m_FullAuto = AsylumItem(Item).m_Category != ASYLUM_CATEGORY_UTILITY;
}

CAsylumWeapon::~CAsylumWeapon()
{
	EndDash();
	EndCounterPhase();
	// Invalidate before an allocator can reuse this address for a different
	// item. Pointer equality alone is not a weapon lifetime identity.
	CAsylumHeldLaserShape::RemoveForWeapon(this);
}

void CAsylumWeapon::OnGiven(bool IsAmmoFillUp)
{
	const int Item = m_Item;
	if(Item == ASYLUM_M1911)
	{
		m_MagazineSize = 7;
		m_MagazineReloadMs = 1500;
	}
	else if(Item == ASYLUM_PIXELGUN)
	{
		m_MagazineSize = 12;
		m_MagazineReloadMs = 1000;
	}
	else if(Item == ASYLUM_M1911_UPG)
	{
		m_MagazineSize = 4;
		m_MagazineReloadMs = 500;
		m_ShellReload = true;
	}
	else if(Item == ASYLUM_PIXELGUN_UPG)
	{
		m_MagazineSize = 35;
		m_MagazineReloadMs = 2000;
	}
	else if(Item == ASYLUM_HYPERLASER)
	{
		m_MagazineSize = 10;
		m_MagazineReloadMs = 1750;
	}
	else if(Item == ASYLUM_CROSSBOW)
	{
		m_MagazineSize = 1;
		m_MagazineReloadMs = 3000;
	}
	else if(Item == ASYLUM_9MM) { m_MagazineSize = 17; m_MagazineReloadMs = 1000; }
	else if(Item == ASYLUM_SUPPRESSED_PISTOL) { m_MagazineSize = 12; m_MagazineReloadMs = 2000; }
	else if(Item == ASYLUM_SSG08) { m_MagazineSize = 7; m_MagazineReloadMs = 3000; }
	else if(Item == ASYLUM_MICROSMG) { m_MagazineSize = 50; m_MagazineReloadMs = 1000; }
	else if(Item == ASYLUM_SUPPRESSED_MAC10) { m_MagazineSize = 32; m_MagazineReloadMs = 2000; }
	else if(Item == ASYLUM_AWP) { m_MagazineSize = 10; m_MagazineReloadMs = 3000; }
	else if(Item == ASYLUM_TASER)
	{
		m_MagazineSize = 1;
		m_MagazineReloadMs = 2000;
		m_FireDelay = 1500;
	}
	if(m_MagazineSize > 0)
	{
		m_MaxAmmo = m_MagazineSize;
		if(!IsAmmoFillUp || m_Ammo < 0)
			m_Ammo = m_MagazineSize;
	}
	if(Item == ASYLUM_M1911 || Item == ASYLUM_PIXELGUN || Item == ASYLUM_M1911_UPG ||
		Item == ASYLUM_9MM || Item == ASYLUM_SUPPRESSED_PISTOL || Item == ASYLUM_SSG08 || Item == ASYLUM_AWP)
		m_FullAuto = false;
	if(!IsAmmoFillUp)
	{
		CAsylumHeldLaserShape::SSegment aLines[CAsylumHeldLaserShape::MAX_SEGMENTS];
		int Count = AsylumBuildHeldShape(m_Item, aLines);
		if(Count > 0)
			new CAsylumHeldLaserShape(GameWorld(), Character()->GetPlayer()->GetCID(), GetWeaponID(), aLines, Count, this);
	}
}

int AsylumBuildHeldShape(int Item, CAsylumHeldLaserShape::SSegment *aLines)
{
	int Count = 0;
	auto Line = [&](vec2 From, vec2 To) { aLines[Count++] = {From, To}; };
	if(Item == ASYLUM_SPOON)
	{
	// IA hitbox is 7.5 studs long x 5 studs wide. At 1.875 studs per
	// DDNet tile this is 128 x 85 world units; the laser silhouette uses
	// the same envelope as the authoritative melee rectangle.
	const float Length = CAsylumWeapon::IAStudToDDNet(7.5f);
	const float HalfWidth = CAsylumWeapon::IAStudToDDNet(5.0f) * 0.5f;
	// Restore the double-line handle and closed bowl. A circular bowl
	// uses the full hitbox width without changing the 128-unit reach.
	const float CenterX = Length - HalfWidth;
	const float HandleEnd = CenterX - HalfWidth;
	Line(vec2(0, 3), vec2(HandleEnd, 3));
	Line(vec2(0, -3), vec2(HandleEnd, -3));
	Line(vec2(0, -3), vec2(0, 3));
	for(int i = 0; i < 12; ++i)
	{
		float A = 2 * pi * i / 12.0f;
		float B = 2 * pi * (i + 1) / 12.0f;
		Line(vec2(CenterX + cosf(A) * HalfWidth, sinf(A) * HalfWidth),
			vec2(CenterX + cosf(B) * HalfWidth, sinf(B) * HalfWidth));
	}
	}
	else if(Item == ASYLUM_DARKHEART || Item == ASYLUM_ENERGYSWORD)
	{
		const float Length = CAsylumWeapon::IAStudToDDNet(3.75f); // Explicit 2D fallback, Wiki gives no numeric box.
		Line(vec2(0, 0), vec2(18, 0));
		Line(vec2(18, -12), vec2(18, 12));
		if(Item == ASYLUM_DARKHEART)
		{
			Line(vec2(18, -5), vec2(Length - 10, -5));
			Line(vec2(18, 5), vec2(Length - 10, 5));
			Line(vec2(Length - 10, -5), vec2(Length, 0));
			Line(vec2(Length, 0), vec2(Length - 10, 5));
		}
		else
		{
			for(int Sign : {-1, 1})
			{
				Line(vec2(18, Sign * 6), vec2(45, Sign * 15));
				Line(vec2(45, Sign * 15), vec2(Length, Sign * 8));
				Line(vec2(Length, Sign * 8), vec2(25, Sign * 3));
			}
		}
	}
	else if(Item == ASYLUM_DYINGPAN)
	{
		Line(vec2(0, 0), vec2(32, 0));
		for(int i = 0; i < 8; ++i)
		{
			const float A = 2 * pi * i / 8, B = 2 * pi * (i + 1) / 8;
			Line(vec2(48 + cosf(A) * 16, sinf(A) * 28), vec2(48 + cosf(B) * 16, sinf(B) * 28));
		}
	}
	else if(Item == ASYLUM_CROSSBOW)
	{
		Line(vec2(6, 0), vec2(58, 0));
		Line(vec2(42, -27), vec2(52, 0));
		Line(vec2(52, 0), vec2(42, 27));
		Line(vec2(42, -27), vec2(12, 0));
		Line(vec2(12, 0), vec2(42, 27));
	}
	else if(Item == ASYLUM_M1911 || Item == ASYLUM_PIXELGUN || Item == ASYLUM_CHICAGO ||
		Item == ASYLUM_M1911_UPG || Item == ASYLUM_PIXELGUN_UPG || Item == ASYLUM_LILYNETTE ||
		Item == ASYLUM_FREEZERAY || Item == ASYLUM_HYPERLASER || Item == ASYLUM_TASER ||
		Item == ASYLUM_9MM || Item == ASYLUM_SUPPRESSED_PISTOL || Item == ASYLUM_SSG08 || Item == ASYLUM_BLASTER ||
		Item == ASYLUM_MICROSMG || Item == ASYLUM_SUPPRESSED_MAC10 || Item == ASYLUM_AWP)
	{
		const bool LongGun = Item == ASYLUM_CHICAGO || Item == ASYLUM_M1911_UPG || Item == ASYLUM_PIXELGUN_UPG || Item == ASYLUM_SSG08 || Item == ASYLUM_AWP;
		const int Guns = Item == ASYLUM_LILYNETTE ? 2 : 1;
		for(int Gun = 0; Gun < Guns; ++Gun)
		{
			const float Y = Guns == 2 ? (Gun == 0 ? -16.0f : 16.0f) : 0.0f;
			const float End = LongGun ? 70.0f : (Item == ASYLUM_FREEZERAY || Item == ASYLUM_HYPERLASER ? 52.0f : 38.0f);
			Line(vec2(8, Y - 6), vec2(End, Y - 6));
			Line(vec2(End, Y - 6), vec2(End, Y + 2));
			Line(vec2(End, Y + 2), vec2(20, Y + 2));
			Line(vec2(20, Y + 2), vec2(16, Y + 16));
			Line(vec2(16, Y + 16), vec2(8, Y + 16));
			Line(vec2(8, Y + 16), vec2(8, Y - 6));
			if(LongGun) Line(vec2(32, Y + 2), vec2(32, Y + 18));
			if(Item == ASYLUM_SUPPRESSED_PISTOL || Item == ASYLUM_SUPPRESSED_MAC10) {
				Line(vec2(End, Y - 4), vec2(End + 20, Y - 4));
				Line(vec2(End + 20, Y - 4), vec2(End + 20, Y));
				Line(vec2(End + 20, Y), vec2(End, Y));
			}
			if(Item == ASYLUM_SSG08 || Item == ASYLUM_AWP) {
				Line(vec2(26, Y - 6), vec2(26, Y - 14));
				Line(vec2(20, Y - 14), vec2(42, Y - 14));
			}
		}
	}
	return Count;
}

void CAsylumWeapon::BeginMagazineReload()
{
	if(m_MagazineSize <= 0 || m_Ammo > 0 || m_MagazineReloadEnd > Server()->Tick())
		return;
	const int WaitTicks = (m_Item == ASYLUM_CROSSBOW ? 1000 : (m_Item == ASYLUM_TASER ? 1500 : 0)) * Server()->TickSpeed() / 1000;
	m_MagazineReloadEnd = Server()->Tick() + WaitTicks + maximum(1, m_MagazineReloadMs * Server()->TickSpeed() / 1000);
	m_ReloadTimer = maximum(1, m_MagazineReloadEnd - Server()->Tick());
	if(m_Item == ASYLUM_SUPPRESSED_MAC10 && Character()->CurrentWeapon() == this && !Character()->IsFrozen())
	{
		const vec2 Aim = Character()->GetAimDirection();
		const float Reach = IAStudToDDNet(3.75f); // Tee-scale melee fallback; Wiki supplies no numeric box.
		vec2 Hit;
		if(Character()->Controller()->IntersectCombatNpc(Pos(), Pos() + Aim * Reach, Reach / 2, &Hit))
			Character()->Controller()->DamageCombatNpc(Character()->GetPlayer()->GetCID(), GetWeaponID(), 10);
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			CCharacter *pTarget = GameServer()->GetPlayerChar(CID);
			if(!CanAffect(Character(), pTarget, GetWeaponID())) continue;
			const vec2 Delta = pTarget->m_Pos - Pos();
			if(dot(Delta, Aim) < 0 || dot(Delta, Aim) > Reach + 14 ||
				fabs(dot(Delta, vec2(-Aim.y, Aim.x))) > Reach / 2 + 14 ||
				GameServer()->Collision()->IntersectLine(Pos(), pTarget->m_Pos, nullptr, nullptr)) continue;
			const int Before = maximum(0, pTarget->GetHealth()) + pTarget->GetArmor();
			pTarget->TakeDamage(Aim * 8 + vec2(0, -3), 10, Character()->GetPlayer()->GetCID(), WEAPON_HAMMER, GetWeaponID(), false);
			if(maximum(0, pTarget->GetHealth()) + pTarget->GetArmor() < Before) ApplyRagdoll(pTarget, 1.5f);
			GameWorld()->CreateHammerHit(pTarget->m_Pos);
		}
	}
}

void CAsylumWeapon::AmmoStatus(char *pBuf, int Size)
{
	if(m_MagazineSize > 0)
		str_format(pBuf, Size, "弹匣 %d/%d%s", maximum(0, m_Ammo), m_MagazineSize,
			m_MagazineReloadEnd > Server()->Tick() ? "（装弹中）" : "");
	else
		pBuf[0] = 0;
}

void CAsylumWeapon::AddCharge(int ActualDamage)
{
	if(m_Item != ASYLUM_LILYNETTE || ActualDamage <= 0 || Server()->Tick() < m_RNextTick || m_UltimateStartTick >= 0)
		return;
	const int Previous = m_Charge;
	m_Charge = minimum(250, m_Charge + ActualDamage);
	if(Previous < 250 && m_Charge == 250)
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] 充能完成：按R释放 Cero Metralleta。");
}

bool CAsylumWeapon::ActivateSkill(bool Ultimate, vec2 Direction)
{
	const int CID = Character()->GetPlayer()->GetCID();
	if(m_Item == ASYLUM_BLASTER && Ultimate)
	{
		if(Server()->Tick() < m_RNextTick || m_BlasterChargeEnd || m_BlasterReady) return false;
		m_BlasterChargeEnd = Server()->Tick() + Server()->TickSpeed() * 5 / 2;
		m_ReloadTimer = Server()->TickSpeed() * 5 / 2;
		m_BurstRemaining = 0;
		return true;
	}
	if(m_MagazineSize > 0 && Ultimate)
	{
		if(m_Ammo == m_MagazineSize || m_MagazineReloadEnd > Server()->Tick())
			return false;
		if(m_ShellReload)
		{
			m_MagazineReloadEnd = Server()->Tick() + maximum(1, Server()->TickSpeed() / 2);
			m_ReloadTimer = maximum(1, Server()->TickSpeed() / 2);
		}
		else
		{
			m_Ammo = 0;
			BeginMagazineReload();
		}
		GameServer()->SendChatTarget(CID, "开始装弹；完成前不能射击。");
		return true;
	}
	if(m_Item == ASYLUM_DARKHEART)
	{
		const int Now = Server()->Tick();
		if(Ultimate)
		{
			if(Now < m_RNextTick)
			{
				GameServer()->SendChatTarget(CID, "[Darkheart] R旋风仍在冷却。");
				return false;
			}
			m_DarkheartSpinEnd = Now + round_to_int(Server()->TickSpeed() * DARKHEART_SPIN_SECONDS);
			m_SpeedBoostEnd = m_DarkheartSpinEnd;
			m_SpeedBoostType = 2;
			m_DarkheartNextHit = Now + Server()->TickSpeed() / 2;
			m_UltimateShots = 0;
			m_RNextTick = m_DarkheartSpinEnd + Server()->TickSpeed() * 15;
			m_ReloadTimer = Server()->TickSpeed() * 3;
			GameServer()->SendChatTarget(CID, "[Darkheart] R Tornado：短前摇后旋风11次，每次8伤并吸血；结束15秒冷却。");
		}
		else
		{
			if(Now < m_ENextTick)
			{
				GameServer()->SendChatTarget(CID, "[Darkheart] E剑气仍在冷却。");
				return false;
			}
			m_ENextTick = Now + Server()->TickSpeed() * 5;
			m_DashEnd = Now + maximum(1, round_to_int(Server()->TickSpeed() * DARKHEART_DASH_SECONDS));
			m_DashDirection = Direction;
			m_DashLastPos = Pos();
			mem_zero(m_aDashHit, sizeof(m_aDashHit));
			m_DashNpcHit = false;
			Character()->Core()->m_AsylumPhase = true;
			Character()->ResetHook();
			Character()->Core()->ResetDragVelocity();
			Character()->Core()->m_Vel = ClampVel(Character()->m_MoveRestrictions, Direction * 28.0f);
			Character()->Protect(0.3f, false);
			GameServer()->SendTuningParams(CID, Character()->m_TuneZone);
			GameServer()->SendChatTarget(CID, "[Darkheart] E Dash：45伤冲刺，实际伤害全额吸血；5秒冷却。");
		}
		if(Ultimate)
		Character()->Protect(0.0f, false);
		GameWorld()->CreateSound(Pos(), SOUND_HAMMER_FIRE);
		return true;
	}
	if(m_Item != ASYLUM_LILYNETTE)
	{
		GameServer()->SendChatTarget(CID, "当前武器没有E/R附加技能；特殊道具仍用左键。");
		return false;
	}
	const int Now = Server()->Tick();
	if(m_UltimateStartTick >= 0)
	{
		GameServer()->SendChatTarget(CID, "[Lilynette] 正在释放 Cero Metralleta。");
		return false;
	}
	if(Ultimate)
	{
		if(Now < m_RNextTick || m_Charge < 250)
		{
			GameServer()->SendChatTarget(CID, "[Lilynette] R未就绪：需要250实际伤害充能，且冷却结束。");
			return false;
		}
		m_Charge = 0;
		m_UltimateStartTick = Now;
		m_UltimateShots = 0;
		m_ReloadTimer = maximum(1, Server()->TickSpeed() / 5);
		m_CeroFacing = Direction.x < 0 ? -1 : 1;
		m_CeroAimLocked = false;
		// Control-speed tuning alone does not remove existing air momentum.
		const float Limit = 3.0f * 32.0f / Server()->TickSpeed();
		Character()->Core()->m_Vel.x = clamp(Character()->Core()->m_Vel.x, -Limit, Limit);
		EndCounterPhase();
		Character()->ResetHook();
		Character()->Core()->ResetDragVelocity();
		GameServer()->SendTuningParams(CID, Character()->m_TuneZone);
		new CAsylumCeroVisual(GameWorld(), CID, this, Now);
		// Six seconds of wind-up, eleven seconds of fire, then seventeen
		// seconds of cooldown. No charge may be earned during any of them.
		m_RNextTick = Now + Server()->TickSpeed() * 34;
		m_CounterUntil = 0;
		GameServer()->SendChatTarget(CID, "[Lilynette] 前摇6秒可自由转向；发射时锁定左右方向，光束11秒，移速3、禁钩、穿墙。");
	}
	else
	{
		if(Now < m_ENextTick || m_CounterUntil > Now)
		{
			GameServer()->SendChatTarget(CID, "[Lilynette] E反击仍在冷却。");
			return false;
		}
		m_CounterUntil = Now + maximum(1, Server()->TickSpeed() / 2);
		m_ENextTick = Now + Server()->TickSpeed() * 7;
		GameWorld()->CreateHammerHit(Pos()); // Native white torso flash.
		GameServer()->SendChatTarget(CID, "[Lilynette] Sonido Counter：0.5秒内抵挡一次敌人攻击。");
	}
	// Abilities are attacks/actions, so they cannot retain a spawn shield.
	Character()->Protect(0.0f, false);
	GameWorld()->CreatePlayerSpawn(Pos());
	GameWorld()->CreateSound(Pos(), SOUND_NINJA_FIRE);
	return true;
}

void CAsylumWeapon::DarkheartAttack(vec2 Direction, int Damage, float Radius, float Forward)
{
	vec2 NpcHit;
	if(Character()->Controller()->IntersectCombatNpc(Pos(), Pos() + Direction * (Radius + Forward), Radius * 0.5f, &NpcHit))
	{
		const int Applied = Character()->Controller()->DamageCombatNpc(Character()->GetPlayer()->GetCID(), GetWeaponID(), Damage);
		// The common NPC path already gives Darkheart's ordinary 50% heal;
		// E/R restore 100% of actual damage, just like their player hit path.
		Character()->IncreaseHealth(Applied - Applied / 2);
	}
	CCharacter *apTargets[MAX_CLIENTS];
	vec2 Center = Pos() + Direction * Forward;
	int Num = GameWorld()->FindEntities(Center, Radius, (CEntity **)apTargets, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i)
	{
		CCharacter *pTarget = apTargets[i];
		if(!CanAffect(Character(), pTarget, GetWeaponID()) || GameServer()->Collision()->IntersectLine(Pos(), pTarget->m_Pos, nullptr, nullptr))
			continue;
		int Before = maximum(0, pTarget->GetHealth()) + pTarget->GetArmor();
		pTarget->TakeDamage(Direction * 5.0f, Damage, Character()->GetPlayer()->GetCID(), WEAPON_HAMMER, GetWeaponID(), false);
		int Lost = Before - maximum(0, pTarget->GetHealth()) - pTarget->GetArmor();
		if(Lost > 0)
			Character()->IncreaseHealth(Lost);
		GameWorld()->CreateHammerHit(pTarget->m_Pos);
	}
}

bool CAsylumWeapon::HandleIncomingDamage(vec2 &Force, int &Damage, int From)
{
	if(m_Item != ASYLUM_LILYNETTE || Damage <= 0)
		return false;
	if(m_CounterUntil > Server()->Tick() && (From >= 0 || From == -2) && From != Character()->GetPlayer()->GetCID())
	{
		m_CounterUntil = 0;
		m_ENextTick = Server()->Tick() + Server()->TickSpeed() * 5;
		Character()->Protect(0.45f, false);
		Character()->Core()->m_Vel = ClampVel(Character()->m_MoveRestrictions, Character()->GetAimDirection() * 24.0f);
		m_SpeedBoostEnd = Server()->Tick() + Server()->TickSpeed() / 2;
		m_SpeedBoostType = 1;
		m_CounterPhaseEnd = m_SpeedBoostEnd;
		m_CounterDirection = Character()->GetAimDirection();
		Character()->Core()->m_AsylumPhase = true;
		Character()->ResetHook();
		Character()->Core()->ResetDragVelocity();
		GameServer()->SendTuningParams(Character()->GetPlayer()->GetCID(), Character()->m_TuneZone);
		GameWorld()->CreateHammerHit(Pos());
		GameWorld()->CreatePlayerSpawn(Pos());
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] 反击成功：免伤并向准星方向突进；E冷却5秒。");
		return true;
	}
	return false;
}

void CAsylumWeapon::ApplyRagdoll(CCharacter *pTarget, float Seconds)
{
	if(!pTarget || !pTarget->IsAlive() ||
		((CGameControllerHunterN *)pTarget->Controller())->IsRagdollImmune(pTarget->GetPlayer()->GetCID()))
		return;
	CWeapon *pHeld = pTarget->CurrentWeapon();
	if(pHeld && AsylumIsWeapon(pHeld->GetWeaponID()) && ((CAsylumWeapon *)pHeld)->RagdollImmune())
		return;
	pTarget->Freeze(Seconds, true);
}

void CAsylumWeapon::EndDash()
{
	if(m_DashEnd <= 0)
		return;
	m_DashEnd = 0;
	Character()->Core()->m_AsylumPhase = false;
	GameServer()->SendTuningParams(Character()->GetPlayer()->GetCID(), Character()->m_TuneZone);
}

void CAsylumWeapon::EndCounterPhase()
{
	if(m_CounterPhaseEnd <= 0) return;
	m_CounterPhaseEnd = 0;
	Character()->Core()->m_AsylumPhase = false;
	GameServer()->SendTuningParams(Character()->GetPlayer()->GetCID(), Character()->m_TuneZone);
}

void CAsylumWeapon::OnUnequip()
{
	EndDash(); EndCounterPhase(); m_CounterUntil = 0;
	m_BurstRemaining = 0;
	if(m_UltimateStartTick >= 0)
	{
		m_UltimateStartTick = -1;
		GameServer()->SendTuningParams(Character()->GetPlayer()->GetCID(), Character()->m_TuneZone);
	}
}

void CAsylumWeapon::TickDash()
{
	if(m_DashEnd <= 0)
		return;
	if(Character()->CurrentWeapon() != this || Character()->IsFrozen() || Character()->IsDisabled())
	{
		EndDash();
		return;
	}
	// Sweep the actual traveled segment, never the projected path behind a
	// wall. Each eligible player takes one hit even across multiple ticks.
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CCharacter *pTarget = GameServer()->GetPlayerChar(CID);
		if(m_aDashHit[CID] || !pTarget || !CanAffect(Character(), pTarget, GetWeaponID()))
			continue;
		vec2 Closest = Pos();
		closest_point_on_line(m_DashLastPos, Pos(), pTarget->m_Pos, Closest);
		if(distance(Closest, pTarget->m_Pos) > 28.0f || GameServer()->Collision()->IntersectLine(Closest, pTarget->m_Pos, nullptr, nullptr))
			continue;
		m_aDashHit[CID] = true;
		const int Before = maximum(0, pTarget->GetHealth()) + pTarget->GetArmor();
		pTarget->TakeDamage(m_DashDirection * 5.0f, DARKHEART_DASH_DAMAGE, Character()->GetPlayer()->GetCID(), WEAPON_HAMMER, GetWeaponID(), false);
		const int Lost = Before - maximum(0, pTarget->GetHealth()) - pTarget->GetArmor();
		if(Lost > 0)
			Character()->IncreaseHealth(Lost);
		GameWorld()->CreateHammerHit(pTarget->m_Pos);
	}
	vec2 NpcHit;
	if(!m_DashNpcHit && Character()->Controller()->IntersectCombatNpc(m_DashLastPos, Pos(), 28, &NpcHit))
	{
		m_DashNpcHit = true;
		const int Applied = Character()->Controller()->DamageCombatNpc(Character()->GetPlayer()->GetCID(), GetWeaponID(), DARKHEART_DASH_DAMAGE);
		Character()->IncreaseHealth(Applied - Applied / 2);
	}
	m_DashLastPos = Pos();
	if(Server()->Tick() >= m_DashEnd)
		EndDash();
	else
		Character()->Core()->m_Vel = ClampVel(Character()->m_MoveRestrictions, m_DashDirection * 28.0f);
}

float CAsylumWeapon::WalkspeedBonusTiles()
{
	if(Character()->IsFrozen() || Character()->IsDisabled())
		return 0.0f;
	if(m_Item == ASYLUM_DARKHEART && m_DarkheartSpinEnd > Server()->Tick() && Character()->CurrentWeapon() == this)
		return 4.0f;
	if(m_Item == ASYLUM_LILYNETTE)
	{
		if(m_SpeedBoostType == 1 && m_SpeedBoostEnd > Server()->Tick())
			return 85.0f;
	}
	return 0.0f;
}

void CAsylumWeapon::SkillStatus(char *pBuf, int Size)
{
	pBuf[0] = 0;
	if(m_Item == ASYLUM_BLASTER)
	{
		str_format(pBuf, Size, "Blaster | R %.1fs | %s", maximum(0, m_RNextTick - Server()->Tick()) / (float)Server()->TickSpeed(),
			m_BlasterChargeEnd ? "Pew Blast充能中" : (m_BlasterReady ? "下一发30伤+0.5秒定身" : "三连发每发15伤"));
		return;
	}
	if(m_Item == ASYLUM_DARKHEART)
	{
		const int Now = Server()->Tick();
		str_format(pBuf, Size, "Darkheart | E %.1fs | R %.1fs | %s", maximum(0, m_ENextTick - Now) / (float)Server()->TickSpeed(), maximum(0, m_RNextTick - Now) / (float)Server()->TickSpeed(), m_DarkheartSpinEnd > Now ? "移速20 / 旋风吸血100%" : "普攻吸血50%");
		return;
	}
	if(m_Item != ASYLUM_LILYNETTE)
	{
		if(m_MagazineSize > 0) str_copy(pBuf, "R：手动装弹 | 空弹自动装填", Size);
		return;
	}
	const int Now = Server()->Tick();
	const char *pState = m_UltimateStartTick >= 0 ? (Now - m_UltimateStartTick < Server()->TickSpeed() * 6 ? "前摇" : "持续光束") :
		(m_SpeedBoostEnd > Now ? "移速+85" : (m_CounterUntil > Now ? "反击窗口" : (m_Charge >= 250 && Now >= m_RNextTick ? "R就绪" : "充能")));
	str_format(pBuf, Size, "Lilynette | 充能 %d/250 | E %.1fs | R %.1fs | %s", m_Charge,
		maximum(0, m_ENextTick - Now) / (float)Server()->TickSpeed(), maximum(0, m_RNextTick - Now) / (float)Server()->TickSpeed(), pState);
	if(m_UltimateStartTick >= 0)
		str_append(pBuf, " / 免伤40% / 移速固定3 / 免倒地", Size);
}

void CAsylumWeapon::AdminStatus(char *pBuf, int Size) const
{
	str_format(pBuf, Size, "item=%d charge=%d cero_active=%d pulses=%d facing=%d counter_phase=%d ammo=%d magazine=%d reload_ticks=%d reload_end=%d blaster_ready=%d", m_Item, m_Charge, m_UltimateStartTick >= 0, m_UltimateShots, m_CeroFacing, m_CounterPhaseEnd > 0, m_Ammo, m_MagazineSize, m_ReloadTimer, m_MagazineReloadEnd, m_BlasterReady);
}

void CAsylumWeapon::FireUltimateArea()
{
	// Wiki gallery: 85 x 25 studs. Every pulse affects the entire rectangle,
	// including behind walls; visuals neither collide nor deal extra damage.
	const float Reach = IAStudToDDNet(85), HalfHeight = IAStudToDDNet(25) / 2;
	const vec2 Direction(m_CeroFacing, 0);
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CCharacter *pTarget = GameServer()->GetPlayerChar(CID);
		if(!pTarget || !CanAffect(Character(), pTarget, GetWeaponID())) continue;
		const vec2 Delta = pTarget->m_Pos - Pos();
		const float Along = Delta.x * m_CeroFacing;
		// Character radius 14, matching the 28-world-unit Tee body.
		if(Along < -14 || Along > Reach + 14 || fabs(Delta.y) > HalfHeight + 14) continue;
		pTarget->TakeDamage(vec2(0, 0), 2, Character()->GetPlayer()->GetCID(), WEAPON_LASER, GetWeaponID(), false);
	}
	Character()->Controller()->DamageCombatNpcBox(Character()->GetPlayer()->GetCID(), GetWeaponID(), 2, Pos(), Direction, Reach, HalfHeight);
	if(m_UltimateShots % 10 == 0)
		GameWorld()->CreateSound(Pos(), SOUND_LASER_FIRE);
}

void CAsylumWeapon::Tick()
{
	CWeapon::Tick();
	if(m_Item == ASYLUM_BLASTER)
	{
		if(m_BlasterChargeEnd && Server()->Tick() >= m_BlasterChargeEnd)
		{
			m_BlasterChargeEnd = 0; m_BlasterReady = true; m_ReloadTimer = 0;
		}
		if(m_BurstRemaining > 0 && Server()->Tick() >= m_BurstNextTick)
		{
			if(Character()->CurrentWeapon() != this || Character()->IsFrozen() || Character()->IsDisabled())
				m_BurstRemaining = 0;
			else
			{
				FireBlaster(m_BurstDirection, false);
				--m_BurstRemaining;
				m_BurstNextTick = Server()->Tick() + maximum(1, Server()->TickSpeed() / 10);
			}
		}
	}
	TickDash();
	if(m_CounterPhaseEnd > 0 && (Server()->Tick() >= m_CounterPhaseEnd || Character()->CurrentWeapon() != this || Character()->IsDisabled()))
		EndCounterPhase();
	if(m_CounterPhaseEnd > 0)
		Character()->Core()->m_Vel = ClampVel(Character()->m_MoveRestrictions, m_CounterDirection * 24.0f);
	if(m_MagazineSize > 0)
	{
		if(m_MagazineReloadEnd > 0 && Server()->Tick() >= m_MagazineReloadEnd)
		{
			m_Ammo = m_ShellReload ? minimum(m_MagazineSize, m_Ammo + 1) : m_MagazineSize;
			if(m_ShellReload && m_Ammo < m_MagazineSize)
			{
				m_MagazineReloadEnd = Server()->Tick() + maximum(1, Server()->TickSpeed() / 2);
				m_ReloadTimer = maximum(1, Server()->TickSpeed() / 2);
			}
			else
			{
				m_MagazineReloadEnd = 0;
				m_ReloadTimer = 0;
			}
		}
		else
			BeginMagazineReload();
	}
	if(m_Item == ASYLUM_DARKHEART && m_DarkheartSpinEnd > 0)
	{
		const bool Held = Character()->CurrentWeapon() == this;
		if(!Held || Character()->IsFrozen() || Character()->IsDisabled())
			m_DarkheartSpinEnd = 0;
		else
		{
			m_ReloadTimer = maximum(1, m_DarkheartSpinEnd - Server()->Tick());
			if(Server()->Tick() >= m_DarkheartNextHit && m_UltimateShots < DARKHEART_SPIN_HITS)
			{
				++m_UltimateShots;
				m_DarkheartNextHit = Server()->Tick() + maximum(1, Server()->TickSpeed() * 2 / 10);
				DarkheartAttack(Character()->GetAimDirection(), DARKHEART_SPIN_DAMAGE, DARKHEART_SPIN_RADIUS, 0.0f);
				GameWorld()->CreateExplosionParticle(Pos());
			}
			if(Server()->Tick() >= m_DarkheartSpinEnd)
			{
				m_DarkheartSpinEnd = 0;
				Character()->Freeze(0.35f, true);
			}
		}
	}
	if(m_SpeedBoostEnd > 0)
	{
		if(Server()->Tick() >= m_SpeedBoostEnd)
		{
			m_SpeedBoostEnd = 0;
			m_SpeedBoostType = 0;
		}
	}
	if(m_Item != ASYLUM_LILYNETTE)
		return;
	const bool Held = Character()->CurrentWeapon() == this;
	if(!Held)
		m_CounterUntil = 0;
	if(m_CounterUntil && Server()->Tick() >= m_CounterUntil)
	{
		m_CounterUntil = 0;
		GameWorld()->CreateHammerHit(Pos());
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] 反击未命中：E冷却7秒。");
	}
	if(m_UltimateStartTick < 0)
		return;
	if(!Held || Character()->IsDisabled() ||
		(!Character()->Controller()->IsGameRunning() && !Character()->Controller()->IsWarmup()))
	{
		m_UltimateStartTick = -1;
		GameServer()->SendTuningParams(Character()->GetPlayer()->GetCID(), Character()->m_TuneZone);
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] 光束中断；充能已消耗，冷却保留。");
		return;
	}
	const int Elapsed = Server()->Tick() - m_UltimateStartTick;
	m_ReloadTimer = maximum(1, Server()->TickSpeed() / 5); // No ordinary shots during the ability.
	Character()->ResetHook();
	Character()->Core()->ResetDragVelocity();
	if(Elapsed < Server()->TickSpeed() * 6)
	{
		m_CeroFacing = Character()->GetAimDirection().x < 0 ? -1 : 1;
		return;
	}
	if(!m_CeroAimLocked)
	{
		m_CeroFacing = Character()->GetAimDirection().x < 0 ? -1 : 1;
		m_CeroAimLocked = true;
	}
	const int ShotsDue = minimum(250, (Elapsed - Server()->TickSpeed() * 6 + 1) * 250 / (Server()->TickSpeed() * 11));
	while(m_UltimateShots < ShotsDue)
	{
		++m_UltimateShots;
		FireUltimateArea();
	}
	if(Elapsed >= Server()->TickSpeed() * 17)
	{
		m_UltimateStartTick = -1;
		GameServer()->SendTuningParams(Character()->GetPlayer()->GetCID(), Character()->m_TuneZone);
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] Cero Metralleta结束；17秒后可再次充能。");
	}
}

void CAsylumWeapon::TickPaused()
{
	CWeapon::TickPaused();
	if(m_ENextTick > 0) ++m_ENextTick;
	if(m_RNextTick > 0) ++m_RNextTick;
	if(m_CounterUntil > 0) ++m_CounterUntil;
	if(m_CounterPhaseEnd > 0) ++m_CounterPhaseEnd;
	if(m_UltimateStartTick >= 0) ++m_UltimateStartTick;
	if(m_MagazineReloadEnd > 0) ++m_MagazineReloadEnd;
	if(m_DarkheartSpinEnd > 0) ++m_DarkheartSpinEnd;
	if(m_DarkheartNextHit > 0) ++m_DarkheartNextHit;
	if(m_DashEnd > 0) ++m_DashEnd;
	if(m_SpeedBoostEnd > 0) ++m_SpeedBoostEnd;
	if(m_BlasterChargeEnd > 0) ++m_BlasterChargeEnd;
	if(m_BurstNextTick > 0) ++m_BurstNextTick;
}

int CAsylumWeapon::NumAmmoIcons()
{
	if(m_MagazineSize > 0)
		return clamp(m_Ammo * 10 / m_MagazineSize, 0, 10);
	const int FullTicks = maximum(1, m_FireDelay * Server()->TickSpeed() / 1000);
	return clamp(10 - m_ReloadTimer * 10 / FullTicks, 0, 10);
}

bool CAsylumWeapon::BulletHit(CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(pHit)
	{
		if(pHit->GetPlayer()->GetCID() == pProj->GetOwner()) return false;
		const int Index = ItemFromWeapon(pProj->GetWeaponID());
		const SAsylumItem &Item = AsylumItem(Index);
		if(Index == ASYLUM_LILYNETTE && !pHit->m_HitData.m_FirstImpact)
			return false;
		int Damage = Index == ASYLUM_CLUSTER ? 10 : Item.m_Damage;
		const float Range = distance(pProj->GetStartPos(), pHit->m_Pos);
		if(Index == ASYLUM_9MM || Index == ASYLUM_SUPPRESSED_PISTOL || Index == ASYLUM_SSG08 ||
			Index == ASYLUM_MICROSMG || Index == ASYLUM_SUPPRESSED_MAC10 || Index == ASYLUM_AWP)
			Damage = AsylumRangedDamage(Index, Range, Pos.y < pHit->m_Pos.y - 7, Pos.y > pHit->m_Pos.y + 8);
		if(Index == ASYLUM_M1911) Damage = round_to_int(24 - clamp((Range - IAStudToDDNet(30)) / IAStudToDDNet(40), 0.0f, 1.0f) * 14);
		if(Index == ASYLUM_PIXELGUN) Damage = round_to_int(12 - clamp((Range - IAStudToDDNet(60)) / IAStudToDDNet(40), 0.0f, 1.0f) * 4);
		if(Index == ASYLUM_M1911_UPG) Damage = round_to_int(14 - clamp((Range - IAStudToDDNet(10)) / IAStudToDDNet(20), 0.0f, 1.0f) * 4.7f);
		if(Index == ASYLUM_PIXELGUN_UPG) Damage = round_to_int(14 - clamp((Range - IAStudToDDNet(80)) / IAStudToDDNet(40), 0.0f, 1.0f) * 4.5f);
		if(Index == ASYLUM_CROSSBOW)
			Damage = round_to_int(82.3f + clamp((distance(pProj->GetStartPos(), pHit->m_Pos) - IAStudToDDNet(120)) / IAStudToDDNet(40), 0.0f, 1.0f) * 67.7f);
		if(Index == ASYLUM_HYPERLASER)
		{
			CCharacter *apTargets[MAX_CLIENTS];
			int Num = pProj->GameWorld()->FindEntities(pHit->m_Pos, IAStudToDDNet(2.5f), (CEntity **)apTargets, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			pProj->GameWorld()->CreateExplosionParticle(pHit->m_Pos);
			for(int i = 0; i < Num; ++i)
				if(apTargets[i]->GetPlayer()->GetCID() != pProj->GetOwner() &&
					!pProj->GameServer()->Collision()->IntersectLine(pHit->m_Pos, apTargets[i]->m_Pos, nullptr, nullptr))
					apTargets[i]->TakeDamage(vec2(0, 0), apTargets[i]->IsShocked() ? 35 : 20, pProj->GetOwner(), WEAPON_GUN, pProj->GetWeaponID(), false);
			return true;
		}
		const int Before = maximum(0, pHit->GetHealth()) + pHit->GetArmor();
		pHit->TakeDamage(vec2(0, -Item.m_Force), Damage, pProj->GetOwner(), pProj->m_Type, pProj->GetWeaponID(), false);
		if(Index == ASYLUM_FREEZERAY && pHit->IsAlive() && !pHit->IsProtected() &&
			maximum(0, pHit->GetHealth()) + pHit->GetArmor() < Before)
			pHit->Freeze(1.5f, true);
		if(Index == ASYLUM_BOW && maximum(0, pHit->GetHealth()) + pHit->GetArmor() < Before)
			ApplyRagdoll(pHit, 1.5f);
		if(Index == ASYLUM_BOW)
			return maximum(0, pHit->GetHealth()) + pHit->GetArmor() < Before;
		if(Index == ASYLUM_VAMPIREKNIVES)
		{
			CCharacter *pOwner = pProj->GameServer()->GetPlayerChar(pProj->GetOwner());
			const int Lost = Before - maximum(0, pHit->GetHealth()) - pHit->GetArmor();
			if(pOwner && pOwner->GameWorld() == pProj->GameWorld() && Lost > 0)
				pOwner->IncreaseHealth(Lost / 2);
		}
		if(Index == ASYLUM_LILYNETTE)
			return false;
	}
	return true;
}

bool CAsylumWeapon::BlasterHit(CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(!pHit) return true;
	if(pHit->GetPlayer()->GetCID() == pProj->GetOwner()) return false;
	pHit->TakeDamage(vec2(0, 0), 15, pProj->GetOwner(), WEAPON_GUN, pProj->GetWeaponID(), false);
	return true;
}

bool CAsylumWeapon::BlasterChargedHit(CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(!pHit) return true;
	if(pHit->GetPlayer()->GetCID() == pProj->GetOwner()) return false;
	const int Before = maximum(0, pHit->GetHealth()) + pHit->GetArmor();
	pHit->TakeDamage(vec2(0, 0), 30, pProj->GetOwner(), WEAPON_GUN, pProj->GetWeaponID(), false);
	if(pHit->IsAlive() && maximum(0, pHit->GetHealth()) + pHit->GetArmor() < Before)
		pHit->Freeze(0.5f, true); // Hitstun adaptation, independent of ragdoll immunity.
	return true;
}

void CAsylumWeapon::FireBlaster(vec2 Direction, bool Charged)
{
	auto *pBolt = new CAsylumLaserProjectile(GameWorld(), WEAPON_GUN, GetWeaponID(), Character()->GetPlayer()->GetCID(),
		Pos() + Direction * 21, Direction, Charged ? 14 : 4, Server()->TickSpeed() * 8,
		Charged ? BlasterChargedHit : BlasterHit, Charged ? 96 : 24);
	pBolt->SetCustomTrajectory(IAStudToDDNet(150), false);
	pBolt->SetCombatDamageOverride(Charged ? 30 : 15);
	GameWorld()->CreateSound(Pos(), SOUND_LASER_FIRE);
}

bool CAsylumWeapon::LaserHit(CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(!pHit) return true; // Energy beams stop at the first wall, never ricochet.
	if(pHit->GetPlayer()->GetCID() == pLaser->GetOwner()) return false;
	const int Index = ItemFromWeapon(pLaser->GetWeaponID());
	const SAsylumItem &Item = AsylumItem(Index);
	CCharacter *pOwner = pLaser->GameServer()->GetPlayerChar(pLaser->GetOwner());
	bool Freeze = (Index == ASYLUM_FREEZERAY || Index == ASYLUM_TASER) && pOwner && CanAffect(pOwner, pHit, pLaser->GetWeaponID()) && !pHit->IsProtected();
	if(Freeze)
	{
		if(((CGameControllerHunterN *)pHit->Controller())->HasMantleShield(pHit->GetPlayer()->GetCID()))
			Freeze = false;
		CWeapon *pUtility = pHit->GetWeapon(2);
		if(pUtility && pUtility->GetWeaponID() == AsylumWeaponID(ASYLUM_MANTLE) && ((CAsylumWeapon *)pUtility)->MantleCharges())
			Freeze = false;
	}
	int Damage = Item.m_Damage;
	const float Range = pOwner ? distance(pOwner->m_Pos, pHit->m_Pos) : 1000.0f;
	if(Index == ASYLUM_M1911) Damage = round_to_int(24 - clamp((Range - IAStudToDDNet(30)) / IAStudToDDNet(40), 0.0f, 1.0f) * 14);
	if(Index == ASYLUM_PIXELGUN) Damage = round_to_int(12 - clamp((Range - IAStudToDDNet(60)) / IAStudToDDNet(40), 0.0f, 1.0f) * 4);
	if(Index == ASYLUM_M1911_UPG) Damage = round_to_int(14 - clamp((Range - IAStudToDDNet(10)) / IAStudToDDNet(20), 0.0f, 1.0f) * 4.7f);
	if(Index == ASYLUM_PIXELGUN_UPG) Damage = round_to_int(14 - clamp((Range - IAStudToDDNet(80)) / IAStudToDDNet(40), 0.0f, 1.0f) * 4.5f);
	if(Index == ASYLUM_TASER) Damage = round_to_int(45 - clamp((Range - IAStudToDDNet(20)) / IAStudToDDNet(30), 0.0f, 1.0f) * 15);
	const int Before = pHit->GetHealth() + pHit->GetArmor();
	pHit->TakeDamage(vec2(0, -Item.m_Force), Damage, pLaser->GetOwner(), WEAPON_LASER, pLaser->GetWeaponID(), false);
	if(Freeze && pHit->IsAlive() && !pHit->IsProtected() && pHit->GetHealth() + pHit->GetArmor() < Before)
	{
		if(Index == ASYLUM_TASER)
		{
			pHit->Shock(2.0f);
			// Shock is distinct from ragdoll: even a ragdoll-immune player
			// is shocked, but only other players get the extra 1s frozen tail.
			ApplyRagdoll(pHit, 3.0f);
		}
		else pHit->Freeze(1.5f, true);
	}
	return true;
}

bool CAsylumWeapon::GrenadeHit(CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(pHit && pHit->GetPlayer()->GetCID() == pProj->GetOwner()) return false;
	const int Index = ItemFromWeapon(pProj->GetWeaponID());
	int Damage = AsylumItem(Index).m_Damage;
	if(Index == ASYLUM_BAZOOKA)
	{
		// Both original projectiles are direct-hit weapons, not splash damage.
		// DDNet freeze represents the soft crumpet's brief ragdoll effect.
		pProj->GameWorld()->CreateExplosionParticle(Pos);
		if(pHit)
		{
			const bool Soft = pProj->m_Type == WEAPON_GUN;
			const int Before = maximum(0, pHit->GetHealth()) + pHit->GetArmor();
			pHit->TakeDamage(vec2(0, -AsylumItem(Index).m_Force), Soft ? 15 : 45,
				pProj->GetOwner(), pProj->m_Type, pProj->GetWeaponID(), false);
			if(Soft && pHit->IsAlive() && !pHit->IsProtected() &&
				maximum(0, pHit->GetHealth()) + pHit->GetArmor() < Before)
				ApplyRagdoll(pHit, 1.5f);
		}
		return true;
	}
	pProj->GameWorld()->CreateSound(Pos, SOUND_GRENADE_EXPLODE);
	pProj->GameWorld()->CreateExplosion(Pos, pProj->GetOwner(), WEAPON_GRENADE, pProj->GetWeaponID(), Damage, false);
	if(Index == ASYLUM_CLUSTER)
	{
		for(int i = 0; i < 10; ++i)
		{
			float Angle = 2.0f * pi * i / 10.0f;
			new CProjectile(pProj->GameWorld(), WEAPON_SHOTGUN, pProj->GetWeaponID(), pProj->GetOwner(),
				Pos, vec2(cosf(Angle), sinf(Angle)) * 0.65f, 4.0f, pProj->Server()->TickSpeed() / 4, BulletHit);
		}
	}
	return true;
}

void CAsylumWeapon::Fire(vec2 Direction)
{
	const SAsylumItem &Item = AsylumItem(m_Item);
	const int CID = Character()->GetPlayer()->GetCID();
	if(m_Item == ASYLUM_BLASTER)
	{
		if(m_BlasterChargeEnd) return;
		FireBlaster(Direction, m_BlasterReady);
		if(m_BlasterReady) { m_BlasterReady = false; m_RNextTick = Server()->Tick() + Server()->TickSpeed() * 8; }
		else { m_BurstRemaining = 2; m_BurstDirection = Direction; m_BurstNextTick = Server()->Tick() + maximum(1, Server()->TickSpeed() / 10); }
		return;
	}
	if(IsUpgradeModule())
	{
		((CGameControllerHunterN *)Character()->Controller())->ConsumeUpgradeModule(Character(), 3);
		return;
	}
	if(m_Item == ASYLUM_MEDKIT)
	{
		if(Character()->GetHealth() >= Character()->m_MaxHealth)
		{
			m_ReloadTimer = maximum(1, Server()->TickSpeed() / 5);
			return;
		}
		Character()->IncreaseHealth(Character()->m_MaxHealth);
		GameWorld()->CreateSound(Pos(), SOUND_PICKUP_HEALTH);
		GameWorld()->CreatePlayerSpawn(Pos());
		((CGameControllerHunterN *)Character()->Controller())->ConsumeSingleUse(Character(), Character()->GetActiveWeapon());
		return;
	}
	if(m_Item == ASYLUM_DASH || m_Item == ASYLUM_PARASOL)
	{
		Character()->Core()->m_Vel = ClampVel(Character()->m_MoveRestrictions, m_Item == ASYLUM_PARASOL ? vec2(Direction.x * 5.0f, -12.0f) : Direction * 24.0f);
		GameWorld()->CreateSound(Pos(), SOUND_NINJA_FIRE);
		GameWorld()->CreatePlayerSpawn(Pos());
		return;
	}
	if(m_Item == ASYLUM_MANTLE)
	{
		m_MantleCharges = 1;
		GameWorld()->CreateSound(Pos(), SOUND_PICKUP_ARMOR);
		GameServer()->SendChatTarget(CID, "[Holy mantle] 护盾就绪：抵挡下一次伤害。");
		((CGameControllerHunterN *)Character()->Controller())->ConsumeSingleUse(Character(), Character()->GetActiveWeapon());
		return;
	}
	if(m_Item == ASYLUM_REROLL)
	{
		// Queue replacement: deleting this weapon inside HandleFire is unsafe.
		((CGameControllerHunterN *)Character()->Controller())->RerollLoadout(Character());
		GameWorld()->CreatePlayerSpawn(Pos());
		return;
	}
	if(Item.m_Category == ASYLUM_CATEGORY_MELEE || m_Item == ASYLUM_SHOCKWAVE || m_Item == ASYLUM_CLEAVE)
	{
		bool Wave = m_Item == ASYLUM_SHOCKWAVE;
		const float MeleeLength = IAStudToDDNet(m_Item == ASYLUM_SPOON ? 7.5f : (m_Item == ASYLUM_CLEAVE ? 35.0f : 3.75f));
		const float MeleeWidth = IAStudToDDNet(m_Item == ASYLUM_SPOON ? 5.0f : (m_Item == ASYLUM_CLEAVE ? 35.0f : 3.75f));
		vec2 Center = Wave ? Pos() : Pos() + Direction * (MeleeLength * 0.5f);
		vec2 NpcHit;
		if(Character()->Controller()->IntersectCombatNpc(Pos(), Pos() + Direction * (Wave ? IAStudToDDNet(13) : MeleeLength), Wave ? IAStudToDDNet(13) : MeleeWidth * 0.5f, &NpcHit))
			Character()->Controller()->DamageCombatNpc(CID, GetWeaponID(), Item.m_Damage);
		float Radius = Wave ? IAStudToDDNet(13) : sqrtf(MeleeLength * MeleeLength + MeleeWidth * MeleeWidth) * 0.5f;
		CCharacter *apTargets[MAX_CLIENTS];
		int Num = GameWorld()->FindEntities(Center, Radius, (CEntity **)apTargets, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		GameWorld()->CreateSound(Pos(), Wave ? SOUND_LASER_FIRE : SOUND_HAMMER_FIRE);
		if(Wave) GameWorld()->CreateExplosionParticle(Pos());
		for(int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apTargets[i];
			if(!CanAffect(Character(), pTarget, GetWeaponID()) || GameServer()->Collision()->IntersectLine(Pos(), pTarget->m_Pos, nullptr, nullptr)) continue;
			if(!Wave)
			{
				const vec2 Delta = pTarget->m_Pos - Pos();
				const float Along = dot(Delta, Direction);
				const float Across = fabs(dot(Delta, vec2(-Direction.y, Direction.x)));
				// Explicit local-space rectangle, not a forward circle that hits
				// opponents behind the user. Dimensions are weapon hitbox sizes;
				// the target's existing 28-unit tee body may overlap its boundary.
				const float Body = pTarget->GetProximityRadius();
				const float ClosestAlong = clamp(Along, 0.0f, MeleeLength);
				const float ClosestAcross = minimum(Across, MeleeWidth * 0.5f);
				if(Along < 0.0f || (Along - ClosestAlong) * (Along - ClosestAlong) +
					(Across - ClosestAcross) * (Across - ClosestAcross) > Body * Body)
					continue;
			}
			vec2 Delta = pTarget->m_Pos - Pos();
			vec2 Push = length(Delta) > 0.0f ? normalize(Delta) : Direction;
			GameWorld()->CreateHammerHit(pTarget->m_Pos);
			vec2 Force = m_Item == ASYLUM_DYINGPAN ? vec2(Push.x * 4.0f, 16.0f) : Push * Item.m_Force + vec2(0, -3);
			const int Before = pTarget->GetHealth() + pTarget->GetArmor();
			pTarget->TakeDamage(Force, Item.m_Damage, CID, Item.m_Type, GetWeaponID(), false);
			if((m_Item == ASYLUM_SPOON || m_Item == ASYLUM_DYINGPAN) &&
				maximum(0, pTarget->GetHealth()) + pTarget->GetArmor() < Before)
				ApplyRagdoll(pTarget, 1.5f);
			if(m_Item == ASYLUM_DARKHEART && maximum(0, pTarget->GetHealth()) + pTarget->GetArmor() < Before)
				Character()->IncreaseHealth((Before - maximum(0, pTarget->GetHealth()) - pTarget->GetArmor()) / 2);
		}
		return;
	}
	if(m_Item == ASYLUM_RAILGUN || m_Item == ASYLUM_TASER)
	{
		new CLaser(GameWorld(), WEAPON_LASER, GetWeaponID(), CID, Pos(), Direction, 1500.0f, LaserHit);
		GameWorld()->CreateSound(Pos(), SOUND_LASER_FIRE);
		return;
	}
	bool Grenade = m_Item == ASYLUM_LAUNCHER || m_Item == ASYLUM_CLUSTER || m_Item == ASYLUM_BAZOOKA;
	int Count = m_Item == ASYLUM_M1911_UPG ? 8 : (m_Item == ASYLUM_SHOTGUN ? 5 : (m_Item == ASYLUM_VAMPIREKNIVES ? 4 + secure_rand_below(5) : 1));
	for(int i = 0; i < Count; ++i)
	{
		float Spread = Count > 1 ? (i - (Count - 1) * 0.5f) * (m_Item == ASYLUM_VAMPIREKNIVES ? 0.16f : 0.12f) : 0.0f;
		if(m_Item == ASYLUM_SMG) Spread = (secure_rand_below(1001) / 1000.0f - 0.5f) * 0.20f;
		if(m_Item == ASYLUM_M1911 || m_Item == ASYLUM_PIXELGUN || m_Item == ASYLUM_PIXELGUN_UPG)
			Spread = (secure_rand_below(10001) / 10000.0f - 0.5f) * (m_Item == ASYLUM_PIXELGUN ? 2.0f : 2.5f) * pi / 180;
		if(m_Item == ASYLUM_9MM || m_Item == ASYLUM_SUPPRESSED_PISTOL || m_Item == ASYLUM_MICROSMG || m_Item == ASYLUM_SUPPRESSED_MAC10)
			Spread = (secure_rand_below(10001) / 10000.0f - 0.5f) *
				(m_Item == ASYLUM_MICROSMG ? 3.0f : (m_Item == ASYLUM_SUPPRESSED_MAC10 ? 4.0f : 2.0f)) * pi / 180;
		float Angle = angle(Direction) + Spread;
		vec2 Dir(cosf(Angle), sinf(Angle));
		int Life = Grenade || m_Item == ASYLUM_BOW ? Server()->TickSpeed() * 2 : (Count > 1 ? Server()->TickSpeed() / 4 : Server()->TickSpeed());
		// Only genuine energy bolts use laser snapshots; ordinary firearms use
		// native pistol/shotgun sprites and first-wall projectile collisions.
		int Type = Grenade ? Item.m_Type : (m_Item == ASYLUM_SHOTGUN || m_Item == ASYLUM_M1911_UPG ? WEAPON_SHOTGUN : WEAPON_GUN);
		if(m_Item == ASYLUM_BAZOOKA && secure_rand_below(3) < 2) Type = WEAPON_GUN;
		if(m_Item == ASYLUM_LILYNETTE || m_Item == ASYLUM_FREEZERAY || m_Item == ASYLUM_HYPERLASER)
		{
			const bool Freeze = m_Item == ASYLUM_FREEZERAY;
			auto *pProjectile = new CAsylumLaserProjectile(GameWorld(), WEAPON_GUN, GetWeaponID(), CID, Pos() + Dir * 21.0f,
				Dir, IAStudToDDNet(m_Item == ASYLUM_LILYNETTE ? 1.5f : 0.35f),
				Freeze ? Server()->TickSpeed() * 2 : (m_Item == ASYLUM_LILYNETTE ? Server()->TickSpeed() * 3 : maximum(1, Server()->TickSpeed() * 2 / 3)), BulletHit,
				IAStudToDDNet(m_Item == ASYLUM_LILYNETTE ? 3.0f : 2.0f));
			pProjectile->SetCustomTrajectory(IAStudToDDNet(Freeze ? 500 : (m_Item == ASYLUM_HYPERLASER ? 300 : 335)), Freeze);
		}
		else if(m_Item == ASYLUM_BOW)
		{
			auto *pShovel = new CAsylumLaserProjectile(GameWorld(), Type, GetWeaponID(), CID,
				Pos() + Dir * 21.0f, Dir, 16.0f, Server()->TickSpeed() * 30, BulletHit, 40.0f);
			pShovel->SetFloorSliding(Dir * (IAStudToDDNet(125) / Server()->TickSpeed()));
		}
		else
		{
			auto *pBullet = new CProjectile(GameWorld(), Type, GetWeaponID(), CID, Pos() + Dir * 21.0f,
				Dir, 6.0f, m_Item == ASYLUM_AMERICA ? Server()->TickSpeed() * 2 : Life, Grenade ? GrenadeHit : BulletHit);
			if(m_Item == ASYLUM_M1911 || m_Item == ASYLUM_M1911_UPG || m_Item == ASYLUM_PIXELGUN || m_Item == ASYLUM_PIXELGUN_UPG || m_Item == ASYLUM_CHICAGO || m_Item == ASYLUM_AMERICA ||
				m_Item == ASYLUM_9MM || m_Item == ASYLUM_SUPPRESSED_PISTOL || m_Item == ASYLUM_SSG08 || m_Item == ASYLUM_MICROSMG || m_Item == ASYLUM_SUPPRESSED_MAC10 || m_Item == ASYLUM_AWP)
				pBullet->SetCustomTrajectory(IAStudToDDNet(500), false);
		}
	}
	if(m_Item == ASYLUM_AMERICA)
	{
		Character()->ApplyKnockback(-Direction * 80.0f, true);
		ApplyRagdoll(Character(), 1.0f);
		((CGameControllerHunterN *)Character()->Controller())->ConsumeSingleUse(Character(), Character()->GetActiveWeapon());
	}
	GameWorld()->CreateSound(Pos(), Grenade ? SOUND_GRENADE_FIRE : (Count > 1 && m_Item != ASYLUM_VAMPIREKNIVES ? SOUND_SHOTGUN_FIRE : SOUND_GUN_FIRE));
}
