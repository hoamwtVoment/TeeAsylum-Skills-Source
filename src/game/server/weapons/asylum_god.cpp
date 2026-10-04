/* ★ God-tier Tee Asylum items: rare rolls inspired by Item Asylum, loud on purpose. */
#include "asylum.h"

#include <game/server/entities/asylum_fx.h>
#include <game/server/entities/laser.h>
#include <game/server/entities/projectile.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/huntern.h>

namespace
{
const char *const gs_apBanReasons[] = {"被锤了", "太菜了", "不尊重锤子", "在服务器里大声喧哗", "手速太快", "没有原因"};

CGameControllerHunterN *Hunter(CEntity *pEntity)
{
	return (CGameControllerHunterN *)pEntity->Controller();
}

// Affectable characters within Radius of Center that the owner can see.
int FindTargets(CCharacter *pOwner, int WeaponID, vec2 Center, float Radius, CCharacter **apTargets)
{
	CCharacter *apFound[MAX_CLIENTS];
	const int Num = pOwner->GameWorld()->FindEntities(Center, Radius, (CEntity **)apFound, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	int Count = 0;
	for(int i = 0; i < Num; ++i)
		if(AsylumCanAffect(pOwner, pOwner->GetPlayer()->GetCID(), apFound[i], WeaponID) &&
			!pOwner->GameServer()->Collision()->IntersectLine(pOwner->m_Pos, apFound[i]->m_Pos, nullptr, nullptr))
			apTargets[Count++] = apFound[i];
	return Count;
}

vec2 PushDir(vec2 From, vec2 To, vec2 Fallback)
{
	const vec2 Delta = To - From;
	return length(Delta) > 0.0f ? normalize(Delta) : Fallback;
}

void Ring(CGameWorld *pWorld, vec2 Center, float Radius, int Count)
{
	for(int i = 0; i < Count; ++i)
	{
		const float Angle = 2.0f * pi * i / Count;
		pWorld->CreateHammerHit(Center + vec2(cosf(Angle), sinf(Angle)) * Radius);
	}
}

void AnnounceBan(CCharacter *pOwner, int Victim, vec2 VictimPos)
{
	char aBuf[192];
	str_format(aBuf, sizeof(aBuf), "[封禁之锤] %s 已被 %s 封禁（理由：%s）", pOwner->Server()->ClientName(Victim),
		pOwner->Server()->ClientName(pOwner->GetPlayer()->GetCID()), gs_apBanReasons[secure_rand_below((int)(sizeof(gs_apBanReasons) / sizeof(gs_apBanReasons[0])))]);
	Hunter(pOwner)->SendChatTarget(-1, aBuf);
	AsylumPlayMeme(pOwner->GameWorld(), ASYLUM_MEME_ERROR, VictimPos, true);
	AsylumShowText(pOwner->GameWorld(), VictimPos - vec2(0.0f, 70.0f), "BANNED", 2.0f);
}
}

bool CAsylumWeapon::FireGodItem(vec2 Direction)
{
	const SAsylumItem &Item = AsylumItem(m_Item);
	const int CID = Character()->GetPlayer()->GetCID();
	CCharacter *apTargets[MAX_CLIENTS];
	switch(m_Item)
	{
	case ASYLUM_BANHAMMER:
	{
		const vec2 Center = Pos() + Direction * 36.0f;
		GameWorld()->CreateSound(Pos(), SOUND_HAMMER_FIRE);
		int Num = FindTargets(Character(), GetWeaponID(), Center, 44.0f, apTargets);
		if(!Num)
			return true;
		int64 Struck = 0;
		for(int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apTargets[i];
			const int Victim = pTarget->GetPlayer()->GetCID();
			Struck |= 1LL << Victim;
			GameWorld()->CreateHammerHit(pTarget->m_Pos);
			pTarget->TakeDamage(PushDir(Pos(), pTarget->m_Pos, Direction) * Item.m_Force + vec2(0.0f, -4.0f), Item.m_Damage, CID, WEAPON_HAMMER, GetWeaponID(), false);
			if(!pTarget->IsAlive())
				AnnounceBan(Character(), Victim, pTarget->m_Pos);
		}
		// The ground shakes: a weaker shockwave around the impact.
		AsylumPlayMeme(GameWorld(), ASYLUM_MEME_BONK, Center);
		Ring(GameWorld(), Center, 120.0f, 10);
		Num = FindTargets(Character(), GetWeaponID(), Center, 140.0f, apTargets);
		for(int i = 0; i < Num; ++i)
			if(!(Struck & (1LL << apTargets[i]->GetPlayer()->GetCID())))
				apTargets[i]->TakeDamage(vec2(0.0f, -9.0f), 15, CID, WEAPON_HAMMER, GetWeaponID(), false);
		return true;
	}
	case ASYLUM_BIRCHTREE:
		// The slam lands in TickGodItem after the tree has said its piece.
		m_SlamTick = Server()->Tick() + Server()->TickSpeed() * 4 / 5;
		AsylumPlayMeme(GameWorld(), ASYLUM_MEME_RUSTLE, Pos());
		GameWorld()->CreatePlayerSpawn(Pos());
		// Quote at most every 8 seconds so holding fire does not flood the chat.
		if(Server()->Tick() - m_LastQuoteTick >= Server()->TickSpeed() * 8)
		{
			m_LastQuoteTick = Server()->Tick();
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s：我爱树木。", Server()->ClientName(CID));
			Hunter(Character())->SendChatTarget(-1, aBuf);
			AsylumShowText(GameWorld(), Pos() - vec2(0.0f, 90.0f), "I LOVE TREES", 1.5f);
		}
		return true;
	case ASYLUM_ZENITH:
		AsylumSpawnBladeStorm(GameWorld(), CID, GetWeaponID(), Pos(), Pos() + Direction * 380.0f, Item.m_Damage);
		AsylumPlayMeme(GameWorld(), ASYLUM_MEME_SHING, Pos());
		return true;
	case ASYLUM_TSARBOMB:
	{
		new CProjectile(GameWorld(), WEAPON_GRENADE, GetWeaponID(), CID, Pos() + Direction * 21.0f, Direction, 6.0f, Server()->TickSpeed() * 2, TsarHit);
		GameWorld()->CreateSound(Pos(), SOUND_GRENADE_FIRE);
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "[沙皇炸弹] %s 扔出了沙皇炸弹，快跑！", Server()->ClientName(CID));
		Hunter(Character())->SendChatTarget(-1, aBuf);
		return true;
	}
	case ASYLUM_BLACKHOLE:
	{
		vec2 To = Pos() + Direction * 820.0f;
		GameServer()->Collision()->IntersectLine(Pos(), To, &To, nullptr);
		// Open the hole where the beam stops: on the first tee, else just off the wall.
		CCharacter *pFirst = GameWorld()->IntersectCharacter(Pos(), To, 0.0f, Character());
		const vec2 HolePos = pFirst ? pFirst->m_Pos : To - Direction * 24.0f;
		new CLaser(GameWorld(), WEAPON_LASER, GetWeaponID(), CID, Pos(), Direction, 820.0f, BlackholeLaserHit);
		AsylumSpawnBlackHole(GameWorld(), CID, GetWeaponID(), HolePos);
		GameWorld()->CreateSound(Pos(), SOUND_LASER_FIRE);
		return true;
	}
	case ASYLUM_JUDGE:
		new CProjectile(GameWorld(), WEAPON_GUN, GetWeaponID(), CID, Pos() + Direction * 21.0f, Direction, 6.0f, Server()->TickSpeed(), JudgeHit);
		GameWorld()->CreateSound(Pos(), SOUND_GUN_FIRE);
		return true;
	case ASYLUM_TRAIN:
	{
		// The train comes from behind and runs over everything in front.
		const int Side = Direction.x < 0.0f ? -1 : 1;
		AsylumSpawnTrain(GameWorld(), CID, GetWeaponID(), Pos() - vec2(Side * 520.0f, 0.0f), Side, Item.m_Damage, Item.m_Force);
		return true;
	}
	case ASYLUM_JUMPSCARE:
	{
		const int Num = FindTargets(Character(), GetWeaponID(), Pos(), 340.0f, apTargets);
		for(int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apTargets[i];
			const int Victim = pTarget->GetPlayer()->GetCID();
			const int Before = pTarget->GetHealth() + pTarget->GetArmor();
			pTarget->TakeDamage(vec2(0.0f, 0.0f), Item.m_Damage, CID, WEAPON_HAMMER, GetWeaponID(), false);
			// Like the freeze ray, a blocked hit (mantle, shield) does not freeze.
			if(pTarget->IsAlive() && !pTarget->IsProtected() && pTarget->GetHealth() + pTarget->GetArmor() < Before)
				pTarget->Freeze(1.0f, true);
			AsylumPlayMeme(GameWorld(), ASYLUM_MEME_SCREAM, pTarget->m_Pos, true, CmaskOneAndViewer(Victim));
			Hunter(Character())->ShowScreenText(Victim, "\n\n\n!!!!!!!!!!!!!!!!!!!!!!!!\nGET OUT OF MY HEAD\n!!!!!!!!!!!!!!!!!!!!!!!!", 1.2f);
			GameWorld()->CreateDamageIndCircle(pTarget->m_Pos, false, 0.0f, 8, 8, 1.2f);
		}
		AsylumPlayMeme(GameWorld(), ASYLUM_MEME_HEARTBEAT, Pos());
		GameWorld()->CreateDeath(Pos(), CID);
		return true;
	}
	case ASYLUM_MOYAI:
	{
		// The whole room hears it.
		AsylumPlayMeme(GameWorld(), ASYLUM_MEME_VINE_BOOM, Pos(), true);
		GameWorld()->CreateExplosionParticle(Pos());
		Ring(GameWorld(), Pos(), 240.0f, 16);
		const int Num = FindTargets(Character(), GetWeaponID(), Pos(), 240.0f, apTargets);
		for(int i = 0; i < Num; ++i)
			apTargets[i]->TakeDamage(PushDir(Pos(), apTargets[i]->m_Pos, Direction) * Item.m_Force + vec2(0.0f, -8.0f), Item.m_Damage, CID, WEAPON_GRENADE, GetWeaponID(), false);
		// Turned to stone: motionless and immune for a moment.
		Character()->Core()->m_Vel = vec2(0.0f, 0.0f);
		Character()->Protect(1.0f, false);
		return true;
	}
	}
	return false;
}

void CAsylumWeapon::TickGodItem()
{
	if(m_Item != ASYLUM_BIRCHTREE || m_SlamTick < 0 || Server()->Tick() < m_SlamTick)
		return;
	// A slam that is late (weapon was not ticking) or interrupted is dropped.
	const bool Stale = Server()->Tick() - m_SlamTick > 2;
	m_SlamTick = -1;
	if(Stale || Character()->CurrentWeapon() != this || Character()->IsFrozen())
		return;
	const SAsylumItem &Item = AsylumItem(m_Item);
	const int CID = Character()->GetPlayer()->GetCID();
	const vec2 Direction = Character()->GetAimDirection();
	const vec2 Center = Pos() + Direction * 48.0f;
	GameWorld()->CreateExplosionParticle(Center);
	Ring(GameWorld(), Center, 100.0f, 12);
	AsylumPlayMeme(GameWorld(), ASYLUM_MEME_VINE_BOOM, Center);
	CCharacter *apTargets[MAX_CLIENTS];
	const int Num = FindTargets(Character(), GetWeaponID(), Center, 100.0f, apTargets);
	for(int i = 0; i < Num; ++i)
		apTargets[i]->TakeDamage(PushDir(Center, apTargets[i]->m_Pos, Direction) * 8.0f + vec2(0.0f, -Item.m_Force), Item.m_Damage, CID, WEAPON_HAMMER, GetWeaponID(), false);
}

bool CAsylumWeapon::TsarHit(CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(pHit && pHit->GetPlayer()->GetCID() == pProj->GetOwner())
		return false;
	AsylumSpawnTsarBomb(pProj->GameWorld(), pProj->GetOwner(), pProj->GetWeaponID(), pHit ? pHit->m_Pos : Pos);
	return true;
}

bool CAsylumWeapon::BlackholeLaserHit(CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(!pHit)
		return true; // The hole opens at the first wall; never bounce.
	return LaserHit(pLaser, Pos, pHit, EndOfLife);
}

bool CAsylumWeapon::LightningHit(CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	return !pHit; // Purely visual: pass through tees, stop at the ground.
}

bool CAsylumWeapon::JudgeHit(CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife)
{
	if(!pHit)
		return true;
	const int Owner = pProj->GetOwner();
	if(pHit->GetPlayer()->GetCID() == Owner)
		return false;
	CGameWorld *pWorld = pProj->GameWorld();
	CCharacter *pOwner = pProj->GameServer()->GetPlayerChar(Owner);
	if(pOwner && pOwner->GameWorld() != pWorld)
		pOwner = nullptr;
	const int WeaponID = pProj->GetWeaponID();
	const vec2 HitPos = pHit->m_Pos;
	// One roll in twenty is a zero: the judge turns on the shooter.
	const int Roll = secure_rand_below(20) == 0 ? 0 : 1 + secure_rand_below(9);
	if(Roll > 0)
		pWorld->CreateDamageInd(HitPos, -pi / 2.0f, Roll);
	switch(Roll)
	{
	case 0:
		if(pOwner)
		{
			pOwner->TakeDamage(vec2(0.0f, 0.0f), 30, Owner, WEAPON_GUN, WeaponID, false);
			AsylumPlayMeme(pWorld, ASYLUM_MEME_SAD, pOwner->m_Pos);
		}
		pProj->GameServer()->SendChatTarget(Owner, "[审判] 掷出 0：审判反噬了你自己。");
		break;
	case 1: // Oops, the judge heals the target.
		pHit->IncreaseHealth(25);
		pWorld->CreateSound(HitPos, SOUND_PICKUP_HEALTH);
		AsylumPlayMeme(pWorld, ASYLUM_MEME_SAD, HitPos);
		break;
	case 2:
		pHit->TakeDamage(vec2(0.0f, -30.0f), 5, Owner, WEAPON_GUN, WeaponID, false);
		AsylumPlayMeme(pWorld, ASYLUM_MEME_BOING, HitPos);
		break;
	case 3:
	{
		const int Before = pHit->GetHealth() + pHit->GetArmor();
		pHit->TakeDamage(vec2(0.0f, 0.0f), 10, Owner, WEAPON_GUN, WeaponID, false);
		if(pHit->IsAlive() && !pHit->IsProtected() && pHit->GetHealth() + pHit->GetArmor() < Before)
			pHit->Freeze(1.5f, true);
		AsylumPlayMeme(pWorld, ASYLUM_MEME_ERROR, HitPos);
		break;
	}
	case 4:
		pWorld->CreateExplosion(HitPos, Owner, WEAPON_GRENADE, WeaponID, 45, false);
		AsylumPlayMeme(pWorld, ASYLUM_MEME_VINE_BOOM, HitPos);
		break;
	case 5:
	{
		// Lightning from the sky, or from the ceiling when there is one.
		vec2 Top = HitPos - vec2(0.0f, 600.0f);
		pProj->GameServer()->Collision()->IntersectLine(HitPos, Top, nullptr, &Top);
		new CLaser(pWorld, WEAPON_LASER, WeaponID, Owner, Top, vec2(0.0f, 1.0f), distance(Top, HitPos) + 48.0f, LightningHit);
		pHit->TakeDamage(vec2(0.0f, 4.0f), 45, Owner, WEAPON_LASER, WeaponID, false);
		AsylumPlayMeme(pWorld, ASYLUM_MEME_THUNDER, HitPos);
		break;
	}
	case 6: // Swap places with the target.
		if(pOwner && AsylumCanAffect(pOwner, Owner, pHit, WeaponID))
		{
			const vec2 OwnerPos = pOwner->m_Pos;
			pOwner->Core()->m_Pos = pOwner->m_Pos = HitPos;
			pHit->Core()->m_Pos = pHit->m_Pos = OwnerPos;
			pOwner->ResetHook();
			pHit->ResetHook();
			pWorld->CreatePlayerSpawn(OwnerPos);
			pWorld->CreatePlayerSpawn(HitPos);
			AsylumPlayMeme(pWorld, ASYLUM_MEME_SHING, OwnerPos);
			AsylumPlayMeme(pWorld, ASYLUM_MEME_SHING, HitPos);
		}
		break;
	case 7:
	{
		const int Before = maximum(0, pHit->GetHealth()) + pHit->GetArmor();
		pHit->TakeDamage(vec2(0.0f, 0.0f), 30, Owner, WEAPON_GUN, WeaponID, false);
		const int Lost = Before - maximum(0, pHit->GetHealth()) - pHit->GetArmor();
		if(pOwner && Lost > 0)
			pOwner->IncreaseHealth(Lost);
		AsylumPlayMeme(pWorld, ASYLUM_MEME_KACHING, HitPos);
		break;
	}
	case 8:
		pHit->TakeDamage(vec2(0.0f, -6.0f), 66, Owner, WEAPON_GUN, WeaponID, false);
		AsylumPlayMeme(pWorld, ASYLUM_MEME_FANFARE, HitPos);
		break;
	default:
	{
		pHit->TakeDamage(vec2(0.0f, -10.0f), 99, Owner, WEAPON_GUN, WeaponID, false);
		AsylumPlayMeme(pWorld, ASYLUM_MEME_DRUMROLL, HitPos, true);
		AsylumShowText(pWorld, HitPos - vec2(0.0f, 80.0f), "JUDGEMENT", 2.0f);
		char aBuf[160];
		str_format(aBuf, sizeof(aBuf), "[审判] %s 掷出 9：最终审判！", pProj->Server()->ClientName(Owner));
		Hunter(pProj)->SendChatTarget(-1, aBuf);
		break;
	}
	}
	return true;
}
