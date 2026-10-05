#ifndef GAME_SERVER_ENTITIES_ASYLUM_VISUAL_LASER_H
#define GAME_SERVER_ENTITIES_ASYLUM_VISUAL_LASER_H

#include <game/server/entity.h>
#include "projectile.h"

// A real CProjectile with only its network appearance changed. The existing
// callback, trajectory, ownership, collision, first-impact mask and lifetime
// remain authoritative. No second cosmetic projectile needs to be tracked.
class CAsylumLaserProjectile : public CProjectile
{
public:
	CAsylumLaserProjectile(CGameWorld *pGameWorld, int WeaponType, int WeaponID, int Owner,
		vec2 Pos, vec2 Direction, float HitRadius, int LifeSpan,
		FProjectileImpactCallback Callback = nullptr, float VisualLength = 48.0f);
	~CAsylumLaserProjectile();
	void Tick() override;
	bool NetworkClipped(int SnappingClient) override;
	void Snap(int SnappingClient, int OtherMode) override;

private:
	vec2 m_VisualDirection;
	float m_VisualLength;
	int m_VisualID;
};

// A lightweight, server-authoritative visual laser segment. It only emits a
// standard CNetObj_Laser; gameplay damage must be handled by the caller's
// CProjectile/CLaser. Keeping the two paths separate avoids double hits while
// allowing custom weapons to draw short moving bolts or fan-shaped beams.
class CAsylumVisualLaser : public CEntity
{
public:
	CAsylumVisualLaser(CGameWorld *pGameWorld, vec2 Start, vec2 Direction, float Length,
		float Speed = 0.0f, int LifeSpan = 1);
	~CAsylumVisualLaser();

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	bool NetworkClipped(int SnappingClient) override;
	void Snap(int SnappingClient, int OtherMode) override;
	void Destroy() override;

private:
	vec2 m_Start;
	vec2 m_Direction;
	float m_Length;
	float m_Speed;
	int m_LifeSpan;
	int m_ID;
};

// Local-space decorative lines that follow an equipped weapon. X points
// along the owner's aim; Y is perpendicular. Allocate once when the weapon
// is first given, not on every frame or every equip operation.
class CAsylumHeldLaserShape : public CEntity
{
public:
	struct SSegment { vec2 m_From; vec2 m_To; };
	static const int MAX_SEGMENTS = 16;
	CAsylumHeldLaserShape(CGameWorld *pGameWorld, int Owner, int WeaponID,
		const SSegment *pSegments, int NumSegments, class CWeapon *pWeaponInstance = nullptr);
	~CAsylumHeldLaserShape();
	static void RemoveForWeapon(class CWeapon *pWeapon);
	void Reset() override;
	void Tick() override;
	void TickDefered() override;
	bool NetworkClipped(int SnappingClient) override;
	void Snap(int SnappingClient, int OtherMode) override;

private:
	static CAsylumHeldLaserShape *ms_pFirstShape;
	CAsylumHeldLaserShape *m_pPreviousShape;
	CAsylumHeldLaserShape *m_pNextShape;
	// Identity only: never dereference this after the constructor, because
	// its weapon may already have been removed before the visual gets a tick.
	class CWeapon *m_pWeaponInstance;
	int m_Owner;
	int m_WeaponID;
	int m_NumSegments;
	bool m_Held;
	SSegment m_aLocal[MAX_SEGMENTS];
	SSegment m_aWorld[MAX_SEGMENTS];
	int m_aIDs[MAX_SEGMENTS];
};

// One source of held geometry for both real players and the combat NPC.
int AsylumBuildHeldShape(int Item, CAsylumHeldLaserShape::SSegment *pSegments);

// One bounded visual per activation, not 250 damaging laser entities.
class CAsylumCeroVisual : public CEntity
{
public:
	CAsylumCeroVisual(CGameWorld *pWorld, int Owner, class CAsylumWeapon *pWeapon, int StartTick);
	~CAsylumCeroVisual();
	void Reset() override { m_MarkedForDestroy = true; }
	void Tick() override;
	void TickPaused() override { ++m_StartTick; }
	void TickDefered() override;
	bool NetworkClipped(int SnappingClient) override;
	void Snap(int SnappingClient, int OtherMode) override;
private:
	bool Update();
	int m_Owner, m_StartTick, m_Facing = 1;
	class CAsylumWeapon *m_pWeaponIdentity;
	static constexpr int NUM_LINES = 32;
	int m_aIDs[NUM_LINES];
	CAsylumHeldLaserShape::SSegment m_aLines[NUM_LINES];
};

#endif
