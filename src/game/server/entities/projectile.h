/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PROJECTILE_H
#define GAME_SERVER_ENTITIES_PROJECTILE_H

#include <game/server/entity.h>

typedef bool (*FProjectileImpactCallback)(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);

class CProjectile : public CEntity
{
public:
	CProjectile(
		CGameWorld *pGameWorld,
		int WeaponType,
		int WeaponID,
		int Owner,
		vec2 Pos,
		vec2 Direction,
		float HitRadius,
		int LifeSpan,
		FProjectileImpactCallback Callback = nullptr,
		SEntityCustomData CustomData = {nullptr, nullptr},
		int Layer = 0,
		int Number = 0);
	~CProjectile();

	void GetProjectileProperties(float *pCurvature, float *pSpeed);
	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void TickPaused() override;
	virtual bool NetworkClipped(int SnappingClient) override;
	virtual void Snap(int SnappingClient, int OtherMode) override;
	virtual void Destroy() override;

private:
	vec2 m_Direction;
	int m_Owner;
	int m_StartTick;
	int m_WeaponID;
	FProjectileImpactCallback m_Callback;
	int m_ID;
	vec2 m_StartPos; // Hunter
	float m_CustomSpeed;
	bool m_IgnoreWalls;
	bool m_FloorSlide;
	vec2 m_SlideVelocity;
	bool m_NpcHit = false;
	int m_CombatDamageOverride = -1;

	// DDRace
	int m_TuneZone; //TODO: make curvature and property

	// Hitdata
	int64 m_HitMask;
	int m_OwnerIsSafe;
	int m_NumHits;

	SEntityCustomData m_CustomData;

public:
	int m_Type;
	float m_Radius;
	int m_LifeSpan;

	// DDRace
	int m_Hit;
	bool m_IsSolo;
	int m_Bouncing;
	int GetOwner() { return m_Owner; }
	int GetWeaponID() { return m_WeaponID; }
	vec2 GetStartPos() const { return m_StartPos; }
	void SetCombatDamageOverride(int Damage) { m_CombatDamageOverride = Damage; }
	int CombatDamageOverride() const { return m_CombatDamageOverride; }
	// Optional server-only linear trajectory. Defaults preserve all existing weapons.
	void SetCustomTrajectory(float Speed, bool IgnoreWalls) { m_CustomSpeed = Speed; m_IgnoreWalls = IgnoreWalls; }
	// Opt-in physical projectile: floor contact preserves horizontal momentum.
	void SetFloorSliding(vec2 Velocity) { m_FloorSlide = true; m_SlideVelocity = Velocity; }
	/* Hunter Start */
	void SetOwner(int Owner) { m_Owner = Owner; }
	void SetStartTick(int Tick) { m_StartTick = Tick; }
	void SetStartPos(vec2 Pos) { m_StartPos = Pos; }
	void SetDir(vec2 Direction) { m_Direction = Direction; }
	/* Hunter End */
	void SetBouncing(int Value);
	bool FillExtraInfo(CNetObj_DDNetProjectile *pProj);

	void *GetCustomData() { return m_CustomData.m_pData; }
};

#endif
