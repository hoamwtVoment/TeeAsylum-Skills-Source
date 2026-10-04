#include "asylum.h"

#include <game/server/entities/laser.h>
#include <game/server/entities/projectile.h>
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
	{"急救包 / Medkit", "回复40生命 / 冷却8秒", WEAPON_HAMMER, 0, 8000, 0.0f, 2},
	{"冲刺汽水 / Dash", "朝准星冲刺 / 冷却2.5秒", WEAPON_GUN, 0, 2500, 0.0f, 2},
	{"冲击波 / Shockwave", "周围20伤害+击退 / 冷却4秒", WEAPON_LASER, 20, 4000, 19.0f, 2},
	{"破片雷 / Cluster", "爆炸+10枚10伤碎片 / 冷却3秒", WEAPON_GRENADE, 40, 3000, 0.0f, 2},
	{"巨大汤勺 / Spoon", "30伤害 / 大范围强击退", WEAPON_HAMMER, 30, 1000, 24.0f, 0},
	{"黑心剑 / Darkheart", "30伤害 / 命中敌人回复10生命", WEAPON_HAMMER, 30, 650, 5.0f, 0},
	{"能量剑 / Energy sword", "40伤害 / 快速斩击", WEAPON_HAMMER, 40, 450, 7.0f, 0},
	{"垂死平底锅 / Dying pan", "40伤害 / 向下重击", WEAPON_HAMMER, 40, 1000, 16.0f, 0},
	{"弩 / Crossbow", "80伤害 / 远距离最高100伤害", WEAPON_GUN, 80, 2400, 3.0f, 1},
	{"冰冻射线 / Freeze ray", "10伤害 / 冻结1秒 / 冷却3秒", WEAPON_LASER, 10, 3000, 0.0f, 1},
	{"英式火箭筒 / British bazooka", "随机软弹20伤或茶杯爆炸50伤", WEAPON_GRENADE, 50, 1300, 8.0f, 1},
	{"美国 / America", "100伤害激光 / 巨大后坐力", WEAPON_LASER, 100, 3200, 2.0f, 1},
	{"神圣斗篷 / Holy mantle", "抵挡下次攻击 / 20秒充能", WEAPON_HAMMER, 0, 20000, 0.0f, 2},
	{"重抽骰子 / Re-roll dice", "原地重抽三件装备 / 不回血", WEAPON_GUN, 0, 10000, 0.0f, 2},
	{"滑翔伞 / Parasol", "持有时减缓下落 / 使用上升", WEAPON_GUN, 0, 4500, 0.0f, 2},
	{"连斩 / Cleave", "前方40伤害 / 10秒冷却", WEAPON_HAMMER, 40, 10000, 2.0f, 2},
	{"双枪 / Lilynette", "30伤穿透弹 / E反击 / 250实伤充能R", WEAPON_GUN, 30, 450, 0.0f, 1},
	{"芝加哥 / Chicago", "4伤精准连射 / 0.1秒", WEAPON_GUN, 4, 100, 0.0f, 1},
	{"M1911", "24至10伤单发 / 距离衰减", WEAPON_GUN, 24, 200, 0.0f, 1},
	{"铲子弓 / Bow", "44伤重力投射物 / 强击退", WEAPON_GRENADE, 44, 2500, 15.0f, 1},
	{"像素枪 / Pixel gun", "12至8伤精准射击 / 0.25秒", WEAPON_GUN, 12, 250, 0.0f, 1},
	{"吸血飞刀 / Vampire knives", "4至8枚飞刀各4伤 / 实伤50%回血", WEAPON_SHOTGUN, 4, 500, 0.0f, 1},
	{"电击枪 / Taser", "45至30伤 / 短暂冻结 / 3.5秒", WEAPON_LASER, 45, 3500, 0.0f, 1},
	{"超能激光 / Hyperlaser", "20伤小范围爆破 / 冻结目标35伤", WEAPON_GUN, 20, 660, 0.0f, 1},
	{"★封禁之锤 / Banhammer", "60伤重锤+震地波 / 击杀即“封禁”", WEAPON_HAMMER, 60, 1100, 14.0f, 0, true},
	{"★白桦树 / Birch tree", "“我爱树木。”0.8秒后90伤巨砸", WEAPON_HAMMER, 90, 3000, 26.0f, 0, true},
	{"★天顶剑 / Zenith", "飞剑风暴飞向准星再飞回 / 每剑18伤", WEAPON_HAMMER, 18, 1200, 6.0f, 0, true},
	{"★沙皇炸弹 / Tsar bobm", "落地倒数1.5秒 / 三圈核爆，可自伤", WEAPON_GRENADE, 80, 9000, 0.0f, 1, true},
	{"★黑洞射线枪 / Blackhole raygun", "25伤激光 / 落点生成3秒黑洞", WEAPON_LASER, 25, 4500, 0.0f, 1, true},
	{"★审判 / Judge", "命中掷0~9：随机神秘效果", WEAPON_GUN, 0, 700, 0.0f, 1, true},
	{"★失控列车 / Unstoppable train", "召唤穿墙列车 / 撞击40伤并撞飞", WEAPON_HAMMER, 40, 12000, 30.0f, 2, true},
	{"★惊吓 / Jumpscare", "附近敌人尖叫+冻结1秒 / 10伤", WEAPON_HAMMER, 10, 12000, 0.0f, 2, true},
	{"★摩艾 / Moyai", "VINE BOOM震飞周围敌人 / 自身石化1秒", WEAPON_HAMMER, 15, 8000, 24.0f, 2, true},
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

int AsylumRandomItem(int Category, bool God)
{
	int aPool[NUM_ASYLUM_ITEMS], Count = 0;
	for(int i = 0; i < NUM_ASYLUM_ITEMS; ++i)
		if(gs_aItems[i].m_Category == Category && gs_aItems[i].m_God == God)
			aPool[Count++] = i;
	return Count ? aPool[secure_rand_below(Count)] : ASYLUM_PAN;
}

int AsylumWeaponID(int Item) { return WEAPON_ID_ASYLUM_PAN + clamp(Item, 0, NUM_ASYLUM_ITEMS - 1); }
bool AsylumIsWeapon(int ID) { return ID >= WEAPON_ID_ASYLUM_PAN && ID < WEAPON_ID_ASYLUM_PAN + NUM_ASYLUM_ITEMS; }

CAsylumWeapon::CAsylumWeapon(CCharacter *pOwner, int Item) : CWeapon(pOwner), m_Item(Item), m_MantleCharges(0),
	m_Charge(0), m_ENextTick(0), m_RNextTick(Item == ASYLUM_LILYNETTE ? pOwner->Server()->Tick() + pOwner->Server()->TickSpeed() * 17 : 0),
	m_CounterUntil(0), m_UltimateStartTick(-1), m_UltimateShots(0), m_SlamTick(-1), m_LastQuoteTick(-1000000)
{
	m_MaxAmmo = m_Ammo = -1;
	m_FireDelay = AsylumItem(Item).m_DelayMs;
	m_FullAuto = AsylumItem(Item).m_Category != ASYLUM_CATEGORY_UTILITY;
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
		m_RNextTick = Now + Server()->TickSpeed() * 34; // 6s wind-up + 11s firing + 17s recovery.
		m_CounterUntil = 0;
		GameServer()->SendChatTarget(CID, "[Lilynette] Cero... Metralleta！前摇6秒，扫射11秒；保持持有双枪。");
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
		GameServer()->SendChatTarget(CID, "[Lilynette] Sonido Counter：0.5秒内抵挡一次敌人攻击。");
	}
	// Abilities are attacks/actions, so they cannot retain a spawn shield.
	Character()->Protect(0.0f, false);
	GameWorld()->CreatePlayerSpawn(Pos());
	GameWorld()->CreateSound(Pos(), SOUND_NINJA_FIRE);
	return true;
}

bool CAsylumWeapon::HandleIncomingDamage(vec2 &Force, int &Damage, int From)
{
	if(m_Item != ASYLUM_LILYNETTE || Damage <= 0 || From < 0 || From == Character()->GetPlayer()->GetCID())
		return false;
	if(m_CounterUntil > Server()->Tick())
	{
		m_CounterUntil = 0;
		m_ENextTick = Server()->Tick() + Server()->TickSpeed() * 5;
		Character()->Protect(0.45f, false);
		Character()->Core()->m_Vel = ClampVel(Character()->m_MoveRestrictions, Character()->GetAimDirection() * 24.0f);
		GameWorld()->CreatePlayerSpawn(Pos());
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] 反击成功：免伤并向准星方向突进；E冷却5秒。");
		return true;
	}
	if(m_UltimateStartTick >= 0)
		Damage = maximum(1, Damage * 60 / 100);
	return false;
}

void CAsylumWeapon::SkillStatus(char *pBuf, int Size)
{
	if(m_Item != ASYLUM_LILYNETTE)
	{
		pBuf[0] = 0;
		return;
	}
	const int Now = Server()->Tick();
	const char *pState = m_UltimateStartTick >= 0 ? (Now - m_UltimateStartTick < Server()->TickSpeed() * 6 ? "前摇" : "扫射") :
		(m_CounterUntil > Now ? "反击窗口" : (m_Charge >= 250 && Now >= m_RNextTick ? "R就绪" : "充能"));
	str_format(pBuf, Size, "Lilynette | 充能 %d/250 | E %.1fs | R %.1fs | %s", m_Charge,
		maximum(0, m_ENextTick - Now) / (float)Server()->TickSpeed(), maximum(0, m_RNextTick - Now) / (float)Server()->TickSpeed(), pState);
}

bool CAsylumWeapon::UltimateLaserHit(CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(!pHit)
		return true; // Stop on a wall; never ricochet the volley.
	if(pHit->GetPlayer()->GetCID() != pLaser->GetOwner() && pHit->m_HitData.m_FirstImpact)
		pHit->TakeDamage(vec2(0, 0), 2, pLaser->GetOwner(), WEAPON_LASER, pLaser->GetWeaponID(), false);
	return false; // Beams can pierce multiple enemies.
}

void CAsylumWeapon::FireUltimateBeam(vec2 Direction)
{
	new CLaser(GameWorld(), WEAPON_LASER, GetWeaponID(), Character()->GetPlayer()->GetCID(), Pos(), Direction, 1000.0f, UltimateLaserHit);
	if(m_UltimateShots % 10 == 0)
		GameWorld()->CreateSound(Pos(), SOUND_LASER_FIRE);
}

void CAsylumWeapon::Tick()
{
	CWeapon::Tick();
	TickGodItem();
	if(m_Item != ASYLUM_LILYNETTE)
		return;
	const bool Held = Character()->CurrentWeapon() == this;
	if(!Held)
		m_CounterUntil = 0;
	if(m_CounterUntil && Server()->Tick() >= m_CounterUntil)
	{
		m_CounterUntil = 0;
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] 反击未命中：E冷却7秒。");
	}
	if(m_UltimateStartTick < 0)
		return;
	if(!Held || Character()->IsFrozen() || Character()->IsDisabled() ||
		(!Character()->Controller()->IsGameRunning() && !Character()->Controller()->IsWarmup()))
	{
		m_UltimateStartTick = -1;
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] 扫射中断；充能已消耗，冷却保留。");
		return;
	}
	const int Elapsed = Server()->Tick() - m_UltimateStartTick;
	m_ReloadTimer = maximum(1, Server()->TickSpeed() / 5); // No ordinary shots during the ability.
	Character()->Core()->m_Vel.x = clamp(Character()->Core()->m_Vel.x, -6.0f, 6.0f);
	if(Elapsed < Server()->TickSpeed() * 6)
		return;
	const int ShotsDue = minimum(250, (Elapsed - Server()->TickSpeed() * 6 + 1) * 250 / (Server()->TickSpeed() * 11));
	while(m_UltimateShots < ShotsDue)
	{
		++m_UltimateShots;
		FireUltimateBeam(Character()->GetAimDirection());
	}
	if(Elapsed >= Server()->TickSpeed() * 17)
	{
		m_UltimateStartTick = -1;
		GameServer()->SendChatTarget(Character()->GetPlayer()->GetCID(), "[Lilynette] Cero Metralleta结束；17秒后可再次充能。");
	}
}

void CAsylumWeapon::TickPaused()
{
	CWeapon::TickPaused();
	if(m_ENextTick > 0) ++m_ENextTick;
	if(m_RNextTick > 0) ++m_RNextTick;
	if(m_CounterUntil > 0) ++m_CounterUntil;
	if(m_UltimateStartTick >= 0) ++m_UltimateStartTick;
	if(m_SlamTick >= 0) ++m_SlamTick;
}

int CAsylumWeapon::NumAmmoIcons()
{
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
		if(Index == ASYLUM_CROSSBOW)
			Damage += clamp((int)(distance(pProj->GetStartPos(), pHit->m_Pos) / 350.0f), 0, 2) * 10;
		if(Index == ASYLUM_HYPERLASER)
		{
			CCharacter *apTargets[MAX_CLIENTS];
			int Num = pProj->GameWorld()->FindEntities(pHit->m_Pos, 55.0f, (CEntity **)apTargets, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			pProj->GameWorld()->CreateExplosionParticle(pHit->m_Pos);
			for(int i = 0; i < Num; ++i)
				if(apTargets[i]->GetPlayer()->GetCID() != pProj->GetOwner() &&
					!pProj->GameServer()->Collision()->IntersectLine(pHit->m_Pos, apTargets[i]->m_Pos, nullptr, nullptr))
					apTargets[i]->TakeDamage(vec2(0, 0), apTargets[i]->IsFrozen() ? 35 : 20, pProj->GetOwner(), WEAPON_GUN, pProj->GetWeaponID(), false);
			return true;
		}
		const int Before = maximum(0, pHit->GetHealth()) + pHit->GetArmor();
		pHit->TakeDamage(vec2(0, -Item.m_Force), Damage, pProj->GetOwner(), pProj->m_Type, pProj->GetWeaponID(), false);
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

bool CAsylumWeapon::LaserHit(CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(!pHit || pHit->GetPlayer()->GetCID() == pLaser->GetOwner()) return false;
	const int Index = ItemFromWeapon(pLaser->GetWeaponID());
	const SAsylumItem &Item = AsylumItem(Index);
	CCharacter *pOwner = pLaser->GameServer()->GetPlayerChar(pLaser->GetOwner());
	bool Freeze = (Index == ASYLUM_FREEZERAY || Index == ASYLUM_TASER) && pOwner && CanAffect(pOwner, pHit, pLaser->GetWeaponID()) && !pHit->IsProtected();
	if(Freeze)
	{
		CWeapon *pUtility = pHit->GetWeapon(2);
		if(pUtility && pUtility->GetWeaponID() == AsylumWeaponID(ASYLUM_MANTLE) && ((CAsylumWeapon *)pUtility)->MantleCharges())
			Freeze = false;
	}
	int Damage = Item.m_Damage;
	const float Range = pOwner ? distance(pOwner->m_Pos, pHit->m_Pos) : 1000.0f;
	if(Index == ASYLUM_M1911) Damage = 24 - (int)(clamp((Range - 300.0f) / 400.0f, 0.0f, 1.0f) * 14.0f);
	if(Index == ASYLUM_PIXELGUN) Damage = 12 - (int)(clamp((Range - 600.0f) / 400.0f, 0.0f, 1.0f) * 4.0f);
	if(Index == ASYLUM_TASER) Damage = 45 - (int)(clamp((Range - 200.0f) / 300.0f, 0.0f, 1.0f) * 15.0f);
	const int Before = pHit->GetHealth() + pHit->GetArmor();
	pHit->TakeDamage(vec2(0, -Item.m_Force), Damage, pLaser->GetOwner(), WEAPON_LASER, pLaser->GetWeaponID(), false);
	if(Freeze && pHit->IsAlive() && !pHit->IsProtected() && pHit->GetHealth() + pHit->GetArmor() < Before)
		pHit->Freeze(Index == ASYLUM_TASER ? 0.75f : 1.0f, true);
	return true;
}

bool CAsylumWeapon::GrenadeHit(CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(pHit && pHit->GetPlayer()->GetCID() == pProj->GetOwner()) return false;
	const int Index = ItemFromWeapon(pProj->GetWeaponID());
	int Damage = AsylumItem(Index).m_Damage;
	if(Index == ASYLUM_BAZOOKA && pProj->m_Type == WEAPON_GUN) Damage = 20;
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
	if(Item.m_God && FireGodItem(Direction))
		return;
	if(m_Item == ASYLUM_MEDKIT)
	{
		Character()->IncreaseHealth(40);
		GameWorld()->CreateSound(Pos(), SOUND_PICKUP_HEALTH);
		GameWorld()->CreatePlayerSpawn(Pos());
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
		vec2 Center = Wave ? Pos() : Pos() + Direction * (m_Item == ASYLUM_CLEAVE ? 55.0f : 30.0f);
		float Radius = Wave ? 130.0f : (m_Item == ASYLUM_SPOON ? 65.0f : (m_Item == ASYLUM_CLEAVE ? 60.0f : 32.0f));
		CCharacter *apTargets[MAX_CLIENTS];
		int Num = GameWorld()->FindEntities(Center, Radius, (CEntity **)apTargets, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		GameWorld()->CreateSound(Pos(), Wave ? SOUND_LASER_FIRE : SOUND_HAMMER_FIRE);
		if(Wave) GameWorld()->CreateExplosionParticle(Pos());
		for(int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apTargets[i];
			if(!CanAffect(Character(), pTarget, GetWeaponID()) || GameServer()->Collision()->IntersectLine(Pos(), pTarget->m_Pos, nullptr, nullptr)) continue;
			vec2 Delta = pTarget->m_Pos - Pos();
			vec2 Push = length(Delta) > 0.0f ? normalize(Delta) : Direction;
			GameWorld()->CreateHammerHit(pTarget->m_Pos);
			vec2 Force = m_Item == ASYLUM_DYINGPAN ? vec2(Push.x * 4.0f, 16.0f) : Push * Item.m_Force + vec2(0, -3);
			const int Before = pTarget->GetHealth() + pTarget->GetArmor();
			pTarget->TakeDamage(Force, Item.m_Damage, CID, Item.m_Type, GetWeaponID(), false);
			if(m_Item == ASYLUM_DARKHEART && maximum(0, pTarget->GetHealth()) + pTarget->GetArmor() < Before)
				Character()->IncreaseHealth(10);
		}
		return;
	}
	if(m_Item == ASYLUM_RAILGUN || m_Item == ASYLUM_FREEZERAY || m_Item == ASYLUM_AMERICA ||
		m_Item == ASYLUM_CHICAGO || m_Item == ASYLUM_M1911 || m_Item == ASYLUM_PIXELGUN || m_Item == ASYLUM_TASER)
	{
		new CLaser(GameWorld(), WEAPON_LASER, GetWeaponID(), CID, Pos(), Direction, m_Item == ASYLUM_AMERICA ? 1200.0f : 900.0f, LaserHit);
		if(m_Item == ASYLUM_AMERICA)
			Character()->Core()->m_Vel = ClampVel(Character()->m_MoveRestrictions, Character()->Core()->m_Vel - Direction * 22.0f);
		GameWorld()->CreateSound(Pos(), SOUND_LASER_FIRE);
		return;
	}
	bool Grenade = m_Item == ASYLUM_LAUNCHER || m_Item == ASYLUM_CLUSTER || m_Item == ASYLUM_BAZOOKA;
	int Count = m_Item == ASYLUM_SHOTGUN ? 5 : (m_Item == ASYLUM_VAMPIREKNIVES ? 4 + secure_rand_below(5) : 1);
	for(int i = 0; i < Count; ++i)
	{
		float Spread = Count > 1 ? (i - (Count - 1) * 0.5f) * (m_Item == ASYLUM_VAMPIREKNIVES ? 0.10f : 0.085f) : 0.0f;
		if(m_Item == ASYLUM_SMG) Spread = (secure_rand_below(1001) / 1000.0f - 0.5f) * 0.12f;
		float Angle = angle(Direction) + Spread;
		vec2 Dir(cosf(Angle), sinf(Angle));
		int Life = Grenade || m_Item == ASYLUM_BOW ? Server()->TickSpeed() * 2 : (Count > 1 ? Server()->TickSpeed() / 4 : Server()->TickSpeed());
		int Type = Item.m_Type;
		if(m_Item == ASYLUM_BAZOOKA && secure_rand_below(2) == 0) Type = WEAPON_GUN;
		new CProjectile(GameWorld(), Type, GetWeaponID(), CID, Pos() + Dir * 21.0f,
			Dir, 6.0f, Life, Grenade ? GrenadeHit : BulletHit);
	}
	GameWorld()->CreateSound(Pos(), Grenade ? SOUND_GRENADE_FIRE : (Count == 5 ? SOUND_SHOTGUN_FIRE : SOUND_GUN_FIRE));
}
