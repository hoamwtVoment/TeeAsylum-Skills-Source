#include "asylum_visual_laser.h"

#include <game/generated/protocol.h>
#include "character.h"
#include <game/server/weapons/asylum.h>

CAsylumLaserProjectile::CAsylumLaserProjectile(CGameWorld *pGameWorld, int WeaponType,
	int WeaponID, int Owner, vec2 Pos, vec2 Direction, float HitRadius, int LifeSpan,
	FProjectileImpactCallback Callback, float VisualLength) :
	CProjectile(pGameWorld, WeaponType, WeaponID, Owner, Pos, Direction, HitRadius, LifeSpan, Callback),
	m_VisualDirection(length(Direction) > 0.0f ? normalize(Direction) : vec2(1, 0)),
	m_VisualLength(clamp(VisualLength, 4.0f, 128.0f)),
	m_VisualID(Server()->SnapNewID())
{
}

CAsylumLaserProjectile::~CAsylumLaserProjectile()
{
	Server()->SnapFreeID(m_VisualID);
}

void CAsylumLaserProjectile::Tick()
{
	const vec2 Previous = m_Pos;
	CProjectile::Tick();
	const vec2 Delta = m_Pos - Previous;
	if(length(Delta) > 0.0f)
		m_VisualDirection = normalize(Delta);
}

bool CAsylumLaserProjectile::NetworkClipped(int SnappingClient)
{
	return NetworkLineClipped(SnappingClient, m_Pos - m_VisualDirection * m_VisualLength, m_Pos);
}

void CAsylumLaserProjectile::Snap(int SnappingClient, int OtherMode)
{
	if(GetOwner() == -2 && OtherMode)
		return;
	vec2 From = m_Pos - m_VisualDirection * m_VisualLength;
	// Clip the decorative tail too, so a muzzle next to a wall does not leave
	// a visible line inside the wall behind it.
	vec2 BeforeWall;
	if(GameServer()->Collision()->IntersectLine(m_Pos, From, nullptr, &BeforeWall))
		From = BeforeWall;
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_VisualID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;
	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)From.x;
	pObj->m_FromY = (int)From.y;
	pObj->m_StartTick = OtherMode ? Server()->Tick() - 4 : Server()->Tick();
}

CAsylumVisualLaser::CAsylumVisualLaser(CGameWorld *pGameWorld, vec2 Start, vec2 Direction,
	float Length, float Speed, int LifeSpan) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_DDRACE, Start),
	m_Start(Start),
	m_Direction(length(Direction) > 0.0f ? normalize(Direction) : vec2(1.0f, 0.0f)),
	m_Length(maximum(1.0f, Length)),
	m_Speed(maximum(0.0f, Speed)),
	// Entities tick before the next snapshot. Add one tick so LifeSpan=1
	// produces one visible snapshot instead of being destroyed immediately.
	m_LifeSpan(maximum(2, LifeSpan + 1)),
	m_ID(Server()->SnapNewID())
{
	// CEntity::m_Pos is the visible segment endpoint. Keep the origin in
	// m_Start so the network object contains an actual line, not a point.
	m_Pos = m_Start + m_Direction * m_Length;
	GameWorld()->InsertEntity(this);
}

CAsylumVisualLaser::~CAsylumVisualLaser()
{
	Server()->SnapFreeID(m_ID);
}

void CAsylumVisualLaser::Reset()
{
	m_MarkedForDestroy = true;
}

void CAsylumVisualLaser::Tick()
{
	if(--m_LifeSpan <= 0)
	{
		m_MarkedForDestroy = true;
		return;
	}
	if(m_Speed > 0.0f)
	{
		const vec2 Delta = m_Direction * m_Speed;
		m_Start += Delta;
		m_Pos += Delta;
	}
}

void CAsylumVisualLaser::TickPaused()
{
	// A paused game must not advance or consume this effect's lifetime.
}

bool CAsylumVisualLaser::NetworkClipped(int SnappingClient)
{
	return NetworkLineClipped(SnappingClient, m_Start, m_Pos);
}

void CAsylumVisualLaser::Snap(int SnappingClient, int OtherMode)
{
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;
	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_Start.x;
	pObj->m_FromY = (int)m_Start.y;
	pObj->m_StartTick = OtherMode ? Server()->Tick() - 4 : Server()->Tick();
}

void CAsylumVisualLaser::Destroy()
{
	CEntity::Destroy();
}

CAsylumHeldLaserShape *CAsylumHeldLaserShape::ms_pFirstShape = nullptr;

CAsylumHeldLaserShape::CAsylumHeldLaserShape(CGameWorld *pGameWorld, int Owner, int WeaponID,
	const SSegment *pSegments, int NumSegments, CWeapon *pWeaponInstance) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_CUSTOM), m_pPreviousShape(nullptr), m_pNextShape(ms_pFirstShape),
	m_pWeaponInstance(pWeaponInstance), m_Owner(Owner), m_WeaponID(WeaponID),
	m_NumSegments(pSegments ? clamp(NumSegments, 0, MAX_SEGMENTS) : 0), m_Held(false)
{
	CCharacter *pChr = GameServer()->GetPlayerChar(Owner);
	if(!m_pWeaponInstance && pChr && pChr->GameWorld() == GameWorld())
		for(int Slot = 0; Slot < NUM_WEAPON_SLOTS; ++Slot)
		{
			CWeapon *pWeapon = pChr->GetWeapon(Slot);
			if(pWeapon && pWeapon->GetWeaponID() == WeaponID)
			{
				m_pWeaponInstance = pWeapon;
				break;
			}
		}
	// A newly constructed weapon can reuse the removed weapon's allocation
	// address. Supersede its prior visual before comparing pointer identity.
	for(CAsylumHeldLaserShape *pShape = ms_pFirstShape; pShape; pShape = pShape->m_pNextShape)
		if(pShape->GameWorld() == GameWorld() && pShape->m_Owner == Owner && pShape->m_WeaponID == WeaponID)
			pShape->Reset();
	if(ms_pFirstShape)
		ms_pFirstShape->m_pPreviousShape = this;
	ms_pFirstShape = this;
	for(int i = 0; i < m_NumSegments; ++i)
	{
		m_aLocal[i] = pSegments[i];
		m_aWorld[i] = pSegments[i];
		m_aIDs[i] = Server()->SnapNewID();
	}
	GameWorld()->InsertEntity(this);
	TickDefered();
}

CAsylumHeldLaserShape::~CAsylumHeldLaserShape()
{
	if(m_pPreviousShape)
		m_pPreviousShape->m_pNextShape = m_pNextShape;
	else
		ms_pFirstShape = m_pNextShape;
	if(m_pNextShape)
		m_pNextShape->m_pPreviousShape = m_pPreviousShape;
	for(int i = 0; i < m_NumSegments; ++i)
		Server()->SnapFreeID(m_aIDs[i]);
}

void CAsylumHeldLaserShape::RemoveForWeapon(CWeapon *pWeapon)
{
	for(CAsylumHeldLaserShape *pShape = ms_pFirstShape; pShape; pShape = pShape->m_pNextShape)
		if(pShape->m_pWeaponInstance == pWeapon)
			pShape->Reset();
}

void CAsylumHeldLaserShape::Reset()
{
	m_MarkedForDestroy = true;
}

void CAsylumHeldLaserShape::Tick()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
	if(!pChr || !pChr->IsAlive() || pChr->GameWorld() != GameWorld())
	{
		m_MarkedForDestroy = true;
		return;
	}
	bool HasWeapon = false;
	for(int Slot = 0; Slot < NUM_WEAPON_SLOTS; ++Slot)
		if(pChr->GetWeapon(Slot) == m_pWeaponInstance && m_pWeaponInstance)
			HasWeapon = true;
	if(!HasWeapon)
		m_MarkedForDestroy = true;
}

void CAsylumHeldLaserShape::TickDefered()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
	m_Held = !m_MarkedForDestroy && m_pWeaponInstance && pChr && pChr->IsAlive() &&
		pChr->GameWorld() == GameWorld() && pChr->CurrentWeapon() == m_pWeaponInstance;
	if(!m_Held)
		return;
	m_Pos = pChr->m_Pos;
	const vec2 Aim = pChr->GetAimDirection();
	// Mirror firearm grips vertically when facing left. Do not invert melee
	// silhouettes; both gun orientations must keep their handles underneath.
	const int Category = AsylumItem(static_cast<CAsylumWeapon *>(m_pWeaponInstance)->Item()).m_Category;
	const float Mirror = (Category == ASYLUM_CATEGORY_RANGED || Category == 4) && Aim.x < 0 ? -1.0f : 1.0f;
	const vec2 Side = vec2(-Aim.y, Aim.x) * Mirror;
	for(int i = 0; i < m_NumSegments; ++i)
	{
		m_aWorld[i].m_From = m_Pos + Aim * m_aLocal[i].m_From.x + Side * m_aLocal[i].m_From.y;
		m_aWorld[i].m_To = m_Pos + Aim * m_aLocal[i].m_To.x + Side * m_aLocal[i].m_To.y;
	}
}

bool CAsylumHeldLaserShape::NetworkClipped(int SnappingClient)
{
	if(!m_Held)
		return true;
	for(int i = 0; i < m_NumSegments; ++i)
		if(!NetworkLineClipped(SnappingClient, m_aWorld[i].m_From, m_aWorld[i].m_To))
			return false;
	return true;
}

void CAsylumHeldLaserShape::Snap(int SnappingClient, int OtherMode)
{
	if(!m_Held || m_MarkedForDestroy)
		return;
	for(int i = 0; i < m_NumSegments; ++i)
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aIDs[i], sizeof(CNetObj_Laser)));
		if(!pObj)
			return;
		pObj->m_X = (int)m_aWorld[i].m_To.x;
		pObj->m_Y = (int)m_aWorld[i].m_To.y;
		pObj->m_FromX = (int)m_aWorld[i].m_From.x;
		pObj->m_FromY = (int)m_aWorld[i].m_From.y;
		pObj->m_StartTick = OtherMode ? Server()->Tick() - 4 : Server()->Tick();
	}
}

CAsylumCeroVisual::CAsylumCeroVisual(CGameWorld *pWorld, int Owner, CAsylumWeapon *pWeapon, int StartTick) :
	CEntity(pWorld, CGameWorld::ENTTYPE_CUSTOM), m_Owner(Owner), m_StartTick(StartTick), m_pWeaponIdentity(pWeapon)
{
	for(int &ID : m_aIDs) ID = Server()->SnapNewID();
	GameWorld()->InsertEntity(this);
	TickDefered();
}

CAsylumCeroVisual::~CAsylumCeroVisual()
{
	for(int ID : m_aIDs) Server()->SnapFreeID(ID);
}

bool CAsylumCeroVisual::Update()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
	// Never dereference an obsolete weapon identity. Also distinguish
	// repeated activations and recycled weapon allocations by start tick.
	if(m_MarkedForDestroy || !pChr || !pChr->IsAlive() || pChr->GameWorld() != GameWorld() || pChr->CurrentWeapon() != m_pWeaponIdentity)
		return false;
	int Start;
	if(!m_pWeaponIdentity->CeroState(Start, m_Facing) || Start != m_StartTick) return false;
	m_Pos = pChr->m_Pos;
	return true;
}

void CAsylumCeroVisual::Tick()
{
	if(!Update()) Reset();
}

void CAsylumCeroVisual::TickDefered()
{
	if(!Update()) { Reset(); return; }
	const int Elapsed = Server()->Tick() - m_StartTick;
	const bool Firing = Elapsed >= Server()->TickSpeed() * 6;
	const float Reach = CAsylumWeapon::IAStudToDDNet(85);
	const float HalfHeight = CAsylumWeapon::IAStudToDDNet(25) / 2;
	const vec2 Axis = Firing ? vec2(m_Facing, 0) : GameServer()->GetPlayerChar(m_Owner)->GetAimDirection();
	const vec2 Side(-Axis.y, Axis.x);
	// During wind-up these lines collapse into a pulsing blue energy core;
	// during firing their outer edges exactly match the damaging rectangle.
	const float Pulse = 0.75f + 0.25f * sinf(Elapsed * 0.18f);
	for(int i = 0; i < 16; ++i)
	{
		const float Y = -HalfHeight + 2 * HalfHeight * i / 15;
		m_aLines[i].m_From = m_Pos + Side * (Firing ? Y : Y * 0.12f * Pulse);
		m_aLines[i].m_To = m_Pos + Axis * (Firing ? Reach : 40 * Pulse) + Side * (Firing ? Y : Y * 0.12f * Pulse);
	}
	// Two eight-segment energy rings travel forward within the beam volume.
	for(int Ring = 0; Ring < 2; ++Ring)
	{
		const float Progress = fmodf(Elapsed * 0.025f + Ring * 0.5f, 1.0f);
		const vec2 Center = m_Pos + Axis * (Firing ? 40 + Progress * (Reach - 65) : 32);
		const float Height = Firing ? HalfHeight : (32 + Ring * 12) * Pulse;
		for(int i = 0; i < 8; ++i)
		{
			const float A = 2 * pi * i / 8, B = 2 * pi * (i + 1) / 8;
			m_aLines[16 + Ring * 8 + i] = {Center + Axis * (cosf(A) * 24) + Side * (sinf(A) * Height), Center + Axis * (cosf(B) * 24) + Side * (sinf(B) * Height)};
		}
	}
}

bool CAsylumCeroVisual::NetworkClipped(int SnappingClient)
{
	if(!Update()) return true;
	for(const auto &Line : m_aLines)
		if(!NetworkLineClipped(SnappingClient, Line.m_From, Line.m_To)) return false;
	return true;
}

void CAsylumCeroVisual::Snap(int SnappingClient, int OtherMode)
{
	if(!Update()) return;
	for(int i = 0; i < NUM_LINES; ++i)
	{
		auto *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aIDs[i], sizeof(CNetObj_Laser)));
		if(!pObj) return;
		pObj->m_X = round_to_int(m_aLines[i].m_To.x); pObj->m_Y = round_to_int(m_aLines[i].m_To.y);
		pObj->m_FromX = round_to_int(m_aLines[i].m_From.x); pObj->m_FromY = round_to_int(m_aLines[i].m_From.y);
		pObj->m_StartTick = Server()->Tick() - (OtherMode ? 4 : 0);
	}
}
