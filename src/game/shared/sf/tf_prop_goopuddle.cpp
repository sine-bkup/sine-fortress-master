#include "cbase.h"
#include "tf_prop_goopuddle.h"
#include "engine/IEngineSound.h"

#ifdef GAME_DLL
#include <tf_player.h>
#include "baseobject_shared.h"
#else
#include "beamdraw.h"
#include <view.h>
#endif

ConVar sf_goopuddle_think_tick_time("sf_goopuddle_think_tick_time", "0.1", FCVAR_CHEAT | FCVAR_REPLICATED);

ConVar sf_goo_team_requirements("sf_goo_team_requirements", "1", FCVAR_REPLICATED | FCVAR_ARCHIVE /*| FCVAR_DEVELOPMENTONLY*/, "Sets jump goo team requirements. 0 is any team, 1 is your own team, 2 is only enemy team");

IMPLEMENT_NETWORKCLASS_ALIASED(TFPropGooPuddle, DT_TFPropGooPuddle)

BEGIN_NETWORK_TABLE(CTFPropGooPuddle, DT_TFPropGooPuddle)
#ifdef CLIENT_DLL
	//RecvPropBool(RECVINFO())
	RecvPropArray3(RECVINFO_ARRAY(m_vecPuddlePos), RecvPropVector(RECVINFO(m_vecPuddlePos[0]))),
	RecvPropInt(RECVINFO(m_PuddleCount)),
	RecvPropFloat(RECVINFO(m_flRadius)),
	RecvPropFloat(RECVINFO(m_flLifetime)),
	RecvPropInt(RECVINFO(m_nGooType)),
	RecvPropBool(RECVINFO(m_bCritical)),
#else
	SendPropArray3(SENDINFO_ARRAY3(m_vecPuddlePos), SendPropVector(SENDINFO_ARRAY3(m_vecPuddlePos), 12, SPROP_COORD)),
	SendPropInt(SENDINFO(m_PuddleCount)),
	SendPropFloat(SENDINFO(m_flRadius)),
	SendPropFloat(SENDINFO(m_flLifetime)),
	SendPropInt(SENDINFO(m_nGooType)),
	SendPropBool(SENDINFO(m_bCritical)),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS(sf_prop_goopuddle, CTFPropGooPuddle);
PRECACHE_REGISTER(sf_prop_goopuddle);

#define SF_GOOPUDDLE_TESTMODEL "models/weapons/c_models/urinejar.mdl"

#ifdef CLIENT_DLL
extern IMaterialSystem* materials;
#endif

#ifdef GAME_DLL
CTFPropGooPuddle::CTFPropGooPuddle()
{
	m_FinishedSpreading = false;
	m_PuddleCount = 0;
	m_CurPuddleOffset = 0;
}

CTFPropGooPuddle::~CTFPropGooPuddle()
{
	for (int i = 0; i < m_PuddleCount; i++)
	{
		delete m_puddles[i];
		m_puddles[i] = NULL;
	}
}

void CTFPropGooPuddle::Spawn()
{
	SetState(PROPPUDDLESTATE_STARTING);

	SetThink(&CTFPropGooPuddle::PuddleThink);
	SetNextThink(gpGlobals->curtime);

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_SOLID | FSOLID_TRIGGER);
	// Since goo puddles don't use a model, we need to force it to transmit the entity
	AddEFlags(EFL_FORCE_CHECK_TRANSMIT);

	SetMoveType(MOVETYPE_CUSTOM);
}

void CTFPropGooPuddle::Precache()
{
	PrecacheModel(SF_GOOPUDDLE_TESTMODEL);

	BaseClass::Precache();
}

void CTFPropGooPuddle::RecalculateBounds()
{
	Vector m_minBounds = Vector(-64.9f, -64.9f, -64.9f);
	Vector m_maxBounds = Vector(64.9f, 64.9f, 64.9f);

	for (int i = 0; i < m_PuddleCount; ++i)
	{
		puddleinfo_t* info = m_puddles[i];

		if (!info)
			continue;

		Vector puddlePos = info->pos - GetAbsOrigin();

		if (puddlePos.x - PUDDLE_MAX_HALF_WIDTH < m_minBounds.x)
			m_minBounds.x = puddlePos.x - PUDDLE_MAX_HALF_WIDTH;

		if (puddlePos.x + PUDDLE_MAX_HALF_WIDTH > m_maxBounds.x)
			m_maxBounds.x = puddlePos.x + PUDDLE_MAX_HALF_WIDTH;

		if (puddlePos.y - PUDDLE_MAX_HALF_WIDTH < m_minBounds.y)
			m_minBounds.y = puddlePos.y - PUDDLE_MAX_HALF_WIDTH;

		if (puddlePos.y + PUDDLE_MAX_HALF_WIDTH > m_maxBounds.y)
			m_maxBounds.y = puddlePos.y + PUDDLE_MAX_HALF_WIDTH;

		if (puddlePos.z < m_minBounds.z)
			m_minBounds.z = puddlePos.z;

		if (puddlePos.z + PUDDLE_MAX_HEIGHT > m_maxBounds.z)
			m_maxBounds.z = puddlePos.z + PUDDLE_MAX_HEIGHT;
	}

	UTIL_SetSize(this, m_minBounds, m_maxBounds);
}

void CTFPropGooPuddle::StartTouch(CBaseEntity* pEntity)
{
	// Sometimes entities will notify entities they have stopped/started touching after RemoveThis(),
	// other times it won't.. so we are just gonna ignore it
	if (m_PropPuddleState == PROPPUDDLESTATE_DYING)
		return;
	
	// There is probably a better way to organize this code, but it's readable so -Vruk
	CTFPlayer* pPlayer;
	bool bIsPlayer;
	CBaseObject* pObject;
	bool bIsObject;

	if (pEntity->IsPlayer())
	{
		pPlayer = ToTFPlayer(pEntity);
		if (!pPlayer)
			return;
		bIsPlayer = true;
		bIsObject = false;
	}
	else if (pEntity->IsBaseObject())
	{
		// Assert( dynamic_cast<CTFPlayer*>( pEntity ) != 0 );
		// as if that assert would really help us anyways considering
		// how useless asserts are in the sdk for debugging
		pObject = static_cast<CBaseObject*>(pEntity);
		if (!pObject)
			return;
		bIsPlayer = false;
		bIsObject = true;
	}
	else
		return;
	
	switch (GetGooType())
	{
		case TF_GOO_JUMP:
		{
			//unsigned int m_nTouchingJumpGooCount;
			if (bIsPlayer)
			{
				if (pPlayer->InSameTeam(GetOwnerEntity()) && sf_goo_team_requirements.GetInt() != 2 || (!pPlayer->InSameTeam(GetOwnerEntity()) && sf_goo_team_requirements.GetInt() != 1))
				{
					pPlayer->m_Shared.IncrementJumpGooCounter();
				}
			}
			break;
		}

		case TF_GOO_TOXIC:
		{
			if (bIsPlayer)
			{
				//CUtlRBTree<float> m_treeTouchingToxicGooDamage;
				pPlayer->m_Shared.AcidBurn(ToTFPlayer(GetOwnerEntity()), m_flDamage, m_bCritical);
			}
			else if (bIsObject)
			{
				pObject->AcidBurn(ToTFPlayer(GetOwnerEntity()), m_flDamage, m_bCritical);
			}
			break;
		}

	default:
		break;
	}
}

void CTFPropGooPuddle::EndTouch(CBaseEntity* pEntity)
{
	// Sometimes entities will notify entities they have stopped/started touching after RemoveThis(),
	// other times it won't.. so we are just gonna ignore it
	if (m_PropPuddleState == PROPPUDDLESTATE_DYING)
		return;

	if (!pEntity->IsPlayer() && !pEntity->IsBaseObject())
		return;
	
	RemoveGooEffect(pEntity);
}

void CTFPropGooPuddle::PerformCustomPhysics(Vector* pNewPosition, Vector* pNewVelocity, QAngle* pNewAngles, QAngle* pNewAngVelocity)
{
	Vector prevOrigin;
	VectorCopy(GetAbsOrigin(), prevOrigin);

	PhysicsTouchTriggers(&prevOrigin);
}

void CTFPropGooPuddle::RemoveGooEffect(CBaseEntity* pEntity)
{
	if (!pEntity) return;

	// There is probably a better way to organize this code, but it's readable so -Vruk
	CTFPlayer* pPlayer;
	bool bIsPlayer;
	CBaseObject* pObject;
	bool bIsObject;

	if (pEntity->IsPlayer())
	{
		pPlayer = ToTFPlayer(pEntity);
		if (!pPlayer)
			return;
		bIsPlayer = true;
		bIsObject = false;
	}
	else if (pEntity->IsBaseObject())
	{
		// Assert( dynamic_cast<CTFPlayer*>( pEntity ) != 0 );
		// as if that assert would really help us anyways considering
		// how useless asserts are in the sdk for debugging
		pObject = static_cast<CBaseObject*>(pEntity);
		if (!pObject)
			return;
		bIsPlayer = false;
		bIsObject = true;
	}
	else
		return;

	switch (GetGooType())
	{
		case TF_GOO_JUMP:
		{
			if (bIsPlayer)
			{
				if (pPlayer->InSameTeam(GetOwnerEntity()) && sf_goo_team_requirements.GetInt() != 2 || (!pPlayer->InSameTeam(GetOwnerEntity()) && sf_goo_team_requirements.GetInt() != 1))
				{
					pPlayer->m_Shared.DecrementJumpGooCounter();
				}
			}
			break;
		}

		case TF_GOO_TOXIC:
		{
			if (bIsPlayer)
			{
				pPlayer->m_Shared.RemoveAcidBurn(ToTFPlayer(GetOwnerEntity()), m_flDamage, m_bCritical);
			}
			else if (bIsObject)
			{
				pObject->RemoveAcidBurn(ToTFPlayer(GetOwnerEntity()), m_flDamage, m_bCritical);
			}
			break;
		}
		default:
			break;
	}

}

void CTFPropGooPuddle::RemoveAllEffects()
{
	touchlink_t* root = (touchlink_t*)GetDataObject(TOUCHLINK);
	if (!root) return;

	for (touchlink_t* link = root->nextLink; link != root; link = link->nextLink)
	{
		CBaseEntity* pTouch = link->entityTouched;
		if (!pTouch) continue;
		if (!pTouch->IsPlayer() && !pTouch->IsBaseObject()) continue;

		RemoveGooEffect(pTouch);
	}
}

void CTFPropGooPuddle::PuddleThink()
{
	switch (m_PropPuddleState)
	{
		case PROPPUDDLESTATE_STARTING:
		{
			for (int i = 0; i < 6; i++)
			{
				float angle = (6.2831 / 6) * (i + 1);
				Vector angleDir = Vector(cos(angle), sin(angle), 0);
				Vector nextPos = GroundPuddle((angleDir * PUDDLE_MAX_HALF_WIDTH) + GetAbsOrigin());
				if (nextPos.IsZero())
					continue;
				puddleinfo_t* info = new puddleinfo_t;
				info->pos = nextPos;
				info->finishedSpreading = false;
				info->lifetime.Start(m_flLifetime);

				m_puddles[m_PuddleCount] = info;

				m_vecPuddlePos.Set(i, nextPos);
				m_PuddleCount++;
			}
			RecalculateBounds();
			SetState(PROPPUDDLESTATE_SPREADING);
			break;
		}
		case PROPPUDDLESTATE_SPREADING:
		{
			int PuddlesMade = Spread();
			if (PuddlesMade == 0)
			{
				SetState(PROPPUDDLESTATE_ACTIVE);
				RecalculateBounds();
			}
			break;
		}
		case PROPPUDDLESTATE_ACTIVE:
		{
			// Let this flow into the PROPPUDDLESTATE_DYING by not putting a break
			if (gpGlobals->curtime - m_flStateTimestamp > m_flLifetime)
			{
				SetState(PROPPUDDLESTATE_DYING);
			}
			else
			{
				break;
			}
		}
		case PROPPUDDLESTATE_DYING:
		{
			RemoveAllEffects();
			RemoveThis();
			break;
		}
	}

	SetNextThink(gpGlobals->curtime + sf_goopuddle_think_tick_time.GetFloat());

}

void CTFPropGooPuddle::SetState(PropPuddleState state)
{
	m_PropPuddleState = state;
	m_flStateTimestamp = gpGlobals->curtime;
}

bool CTFPropGooPuddle::CreatePuddle(Vector parentPosition, int *traceCount)
{
	float distance = RandomFloat(25.0f, 60.0f);
	float radAngle = RandomFloat(0.0f, 3.14159);

	//Find the direction we are from the center
	Vector newDirection = (parentPosition - GetAbsOrigin()).Normalized();
	QAngle newAngle;
	VectorAngles(newDirection, newAngle);

	//The initial rotation of angles faces positive y
	Vector startingDirection = Vector(0, 1, 0);
	QAngle startingAngle;
	VectorAngles(startingDirection, startingAngle);

	//This finds the rotation between the starting rotation and the direction we *want* to be going
	float diff = AngleDiff(newAngle.y, startingAngle.y);
	//float flAngle = QuaternionAngleDiff(newRotation, startingRotation);

	//Rotate the angle so it faces the direction we want it to
	Vector nextPos(cos(radAngle), sin(radAngle), 0.0f);
	VectorYawRotate(nextPos, diff, nextPos);

	nextPos *= distance;
	nextPos += parentPosition;

	if (nextPos.DistTo(GetAbsOrigin()) > m_flRadius)
		return false;

	nextPos = GroundPuddle(nextPos, traceCount);
	if (nextPos.IsZero())
		return false;

	puddleinfo_t* info = new puddleinfo_t;

	info->pos = nextPos;
	info->lifetime.Start(m_flLifetime);
	info->finishedSpreading = false;
	m_puddles[m_PuddleCount] = info;

	m_vecPuddlePos.Set(m_PuddleCount, nextPos);

	return true;
}

int CTFPropGooPuddle::Spread()
{
	int newPuddles = 0;
	int puddleCount = m_PuddleCount;

	int traceCount = 16;

	for (int i = m_CurPuddleOffset; i < puddleCount && traceCount > 0; i++)
	{
		if (m_PuddleCount >= MAX_PUDDLES - 1)
		{
			m_FinishedSpreading = true;
			break;
		}

		puddleinfo_t* parentInfo = m_puddles[i];

		if (!parentInfo)
			continue;

		if (parentInfo->finishedSpreading)
			continue;

		if (!CreatePuddle(parentInfo->pos, &traceCount))
		{
			parentInfo->finishedSpreading = true;
			continue;
		}

		m_PuddleCount++;
		newPuddles++;

		m_CurPuddleOffset = puddleCount - 1;

		//UTIL_DecalTrace(&tr, "Scorch");

		//CBaseEntity* dispenser = CreateEntityByName("obj_dispenser");

		//DispatchSpawn(dispenser);
		//dispenser->Teleport(&nextPos, NULL, NULL);

	}
	return newPuddles;
}
Vector CTFPropGooPuddle::GroundPuddle(Vector pos, int* tracecount)
{
	trace_t tr;
	UTIL_TraceLine(pos + Vector(0.0f, 0.0f, 50.0f), pos + Vector(0.0f, 0.0f, -200.0f), MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr);
	if (tracecount)
	{
		(*tracecount)--;
	}

	if (tr.fraction == 1.0)
		return Vector(0, 0, 0);

	return tr.endpos + Vector(0.0f, 0.0f, 1.0f);
}

void CTFPropGooPuddle::RemoveThis(void)
{
#ifdef CLIENT_DLL
	ParticleProp()->StopEmission();
#endif
	SetThink(&BaseClass::SUB_Remove);
	SetTouch(NULL);
#ifdef GAME_DLL

	if (PhysIsInCallback())
		PhysCallbackRemove(this->GetNetworkable());
	else
		UTIL_Remove(this);
#endif
}

#else

CTFPropGooPuddle::C_TFPropGooPuddle()
{
	m_iInitializedPuddles = 0;
	m_iActivePuddles = 0;
	m_bStopPlayingLoopSound = false;
	m_nLoopSoundGuid = -1;
}

CTFPropGooPuddle::~C_TFPropGooPuddle()
{
	enginesound->StopSoundByGuid(m_nLoopSoundGuid);
}

void CTFPropGooPuddle::Spawn()
{
	m_flSpawnTimestamp = gpGlobals->curtime;

	BaseClass::Spawn();
	//m_pMaterial.Init("sprites/googun.vmt", TEXTURE_GROUP_OTHER);


	SetNextClientThink(CLIENT_THINK_ALWAYS);
}

void CTFPropGooPuddle::Precache()
{
	PrecacheMaterial(PUDDLE_TOXIC_MATERIAL);
	PrecacheMaterial(PUDDLE_JUMP_MATERIAL);
}

void CTFPropGooPuddle::OnDataChanged(DataUpdateType_t updateType)
{
	switch (updateType)
	{
		case DATA_UPDATE_CREATED:
			if (m_nGooType == TF_GOO_TOXIC)
			{
				m_pMaterial.Init(PUDDLE_TOXIC_MATERIAL, TEXTURE_GROUP_CLIENT_EFFECTS);
				EmitSound("Weapon_GooGun.AcidPoolStart");
			}
			else
				m_pMaterial.Init(PUDDLE_JUMP_MATERIAL, TEXTURE_GROUP_CLIENT_EFFECTS);
			
			break;
		case DATA_UPDATE_DATATABLE_CHANGED:
			InitializeNewPuddleData();

			//Warning("New Data for Goo Puddle\n");
			break;
	}

}

float CTFPropGooPuddle::GetTimeSinceSpawned()
{
	return gpGlobals->curtime - m_flSpawnTimestamp;
}

void CTFPropGooPuddle::InitializeNewPuddleData()
{
	for (int i = 0; i < m_PuddleCount; i++)
	{
		c_puddleinfo_t* info = &m_puddleArray[i];

		if (info->state == PUDDLESTATE_UNINITIALIZED)
		{
			InitializePuddleInfo(info, m_vecPuddlePos[i]);
		}
	}
}

void CTFPropGooPuddle::InitializePuddleInfo(c_puddleinfo_t* info, Vector pos)
{
	trace_t tr;
	UTIL_TraceLine(pos, pos + Vector(0.0f, 0.0f, -10.0f), MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr);

	if (tr.fraction == 1.0f)
		return;

	info->pos = pos;
	info->normal = tr.plane.normal;
	info->size = 0;
	info->maxSize = RandomFloat(PUDDLE_MIN_SIZE, PUDDLE_MAX_SIZE);
	info->SetState(PUDDLESTATE_STARTING);
	info->lifetime.Start(m_flLifetime - GetTimeSinceSpawned());

	m_iInitializedPuddles += 1;
}

void CTFPropGooPuddle::ClientThink()
{
	for (int i = 0; i < m_PuddleCount; i++)
	{
		c_puddleinfo_t* info = &m_puddleArray[i];

		switch (info->state)
		{
			case PUDDLESTATE_STARTING:
			{
				float growRate = info->maxSize / 2.0f;
				info->size = growRate * (GetTimeSinceSpawned());

				if (info->size > info->maxSize)
				{
					info->size = info->maxSize;
					info->SetState(PUDDLESTATE_ACTIVE);
					m_iActivePuddles += 1;
				}
				break;
			}
			case PUDDLESTATE_ACTIVE:
			{
				if (info->lifetime.GetElapsedTime() >= m_flLifetime - PUDDLE_FADE_TIME)
				{
					info->SetState(PUDDLESTATE_FINISHING);
				}
				break;
			}
			case PUDDLESTATE_FINISHING:
			{
				//float shrinkRate = 3.0f / info->maxSize;
				//float finishingTime = (gpGlobals->curtime - info->state_timestamp);
				// No division by zero
				//float adjustedTime = (finishingTime == 0) ? (0) : (1 / finishingTime);

				//info->size = shrinkRate * adjustedTime;

				float distanceToCentreRemapped = ((GetAbsOrigin().DistTo(info->pos)) / m_flRadius) * 2.0f;
				float remappedRemainingTime = RemapValClamped(info->lifetime.GetRemainingTime() + 0.2f, 0.0f, PUDDLE_FADE_TIME, 0.0f, 1.0f);

				info->size = (info->maxSize * remappedRemainingTime * 1.0f / distanceToCentreRemapped);
				if (info->size <= 1.0f)
				{
					info->size = 0.0f;
					KillPuddle(info);
					m_iActivePuddles -= 1;
				}
				break;
			}
			case PUDDLESTATE_UNINITIALIZED:
			default:
				continue;
		}
	}
	if (m_nGooType == TF_GOO_TOXIC)
	{
		//If the active sound isn't already playing, and we have puddles, play the active looping sound
		if (m_nLoopSoundGuid == -1 && m_iInitializedPuddles != 0 && !m_bStopPlayingLoopSound)
		{
			if (m_bCritical)
				EmitSound("Weapon_GooGun.AcidPoolActiveLoopCrit");
			else
				EmitSound("Weapon_GooGun.AcidPoolActiveLoop");

			m_nLoopSoundGuid = enginesound->GetGuidForLastSoundEmitted();
		}

		if (enginesound->IsSoundStillPlaying(m_nLoopSoundGuid))
		{
			//Ramp up volume over 4 seconds after spawning
			float flPercentStarted = Clamp((gpGlobals->curtime - m_flSpawnTimestamp) / 4.0f, 0.0f, 1.0f);

			//Ramp down volume over 4 seconds before despawning
			float flRemainingTime = (m_flSpawnTimestamp + m_flLifetime) - gpGlobals->curtime;
			//Clamp this to prevent a division by zero and to prevent anything funky with the lifetime
			float flTimeToDecayVolume = Clamp(m_flLifetime - 4.0f, 1.0f, m_flLifetime);
			float flPercentageOfDecayLeft = Clamp(flRemainingTime / (flTimeToDecayVolume), 0.0f, 1.0f);

			float volume = Clamp(flPercentStarted * flPercentageOfDecayLeft, 0.0f, 1.0f);

			//Kill the sound if we've reached the end of the lifetime
			if (flPercentageOfDecayLeft == 0.0f)
			{
				enginesound->StopSoundByGuid(m_nLoopSoundGuid);
				m_bStopPlayingLoopSound = true;
				if (m_bCritical)
					EmitSound("Weapon_GooGun.AcidPoolEndCrit");
				else
					EmitSound("Weapon_GooGun.AcidPoolEnd");
			}
			else
				enginesound->SetVolumeByGuid(m_nLoopSoundGuid, volume);

		}
	}

	SetNextClientThink(0.1f);
}

void CTFPropGooPuddle::KillPuddle(c_puddleinfo_t* info)
{
	info->SetState(PUDDLESTATE_DEAD);
}

/*
void CTFPropGooPuddle::UpdatePuddleCount()
{
	int i = 0;
	for (; m_vecPuddlePos[i] == NULL;)
	{
		i++;
	}
	m_PuddleCount = i;
}*/

/*
void CTFPropGooPuddle::GetRenderBounds(Vector& mins, Vector& maxs)
{
	if (!m_PuddleCount)
	{
		mins = Vector(0, 0, 0);
		maxs = Vector(0, 0, 0);
		return;
	}
	mins = CollisionProp()->OBBMins();
	maxs = CollisionProp()->OBBMaxs();

}*/

void CTFPropGooPuddle::GetRenderBoundsWorldspace(Vector& mins, Vector& maxs)
{
	if (!m_PuddleCount)
	{
		mins = Vector(0, 0, 0);
		maxs = Vector(0, 0, 0);
		return;
	}
	mins = GetAbsOrigin() + CollisionProp()->OBBMins();
	maxs = GetAbsOrigin() + CollisionProp()->OBBMaxs();
}

bool CTFPropGooPuddle::ShouldDraw()
{
	return true;
}

int CTFPropGooPuddle::DrawModel(int flags)
{
	CMeshBuilder meshBuilder;

	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->Bind(m_pMaterial, GetClientRenderable());

	color32 color = { 255, 255, 255, 255 };

	for (int i = 0; i < m_PuddleCount; i++)
	{
		c_puddleinfo_t* info = &m_puddleArray[i];
		if (info->state != PUDDLESTATE_DEAD && info->state != PUDDLESTATE_UNINITIALIZED)
		{
			//DrawSprite(info->pos, 25.0f, 25.0f, color);
			DrawPuddle(info, color);
		}
	}

	return BaseClass::DrawModel(flags);
}

void CTFPropGooPuddle::DrawPuddle(c_puddleinfo_t* info, color32 color)
{
	unsigned char pColor[4] = { color.r, color.g, color.b, color.a };

	// Generate half-widths
	float flHalfSize = info->size / 2.0f;

	Vector puddleToPosDir = (CurrentViewOrigin() - info->pos).Normalized();

	// Compute direction vectors for the sprite
	Vector fwd = info->normal;

	//if we are facing the plane from behind, turn it around
	if (DotProduct(fwd.Normalized(), puddleToPosDir) < 0)
		fwd = -fwd;


	Vector up = Vector(0, 1, 0);
	Vector right;

	CrossProduct(up, fwd, right);

	CrossProduct(fwd, right, up);

	CMeshBuilder meshBuilder;
	Vector point;
	CMatRenderContextPtr pRenderContext(materials);
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin(pMesh, MATERIAL_QUADS, 1);

	meshBuilder.Color4ubv(pColor);
	meshBuilder.TexCoord2f(0, 0, 1);
	VectorMA(info->pos, -flHalfSize, up, point);
	VectorMA(point, -flHalfSize, right, point);
	meshBuilder.Position3fv(point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv(pColor);
	meshBuilder.TexCoord2f(0, 0, 0);
	VectorMA(info->pos, flHalfSize, up, point);
	VectorMA(point, -flHalfSize, right, point);
	meshBuilder.Position3fv(point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv(pColor);
	meshBuilder.TexCoord2f(0, 1, 0);
	VectorMA(info->pos, flHalfSize, up, point);
	VectorMA(point, flHalfSize, right, point);
	meshBuilder.Position3fv(point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv(pColor);
	meshBuilder.TexCoord2f(0, 1, 1);
	VectorMA(info->pos, -flHalfSize, up, point);
	VectorMA(point, flHalfSize, right, point);
	meshBuilder.Position3fv(point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}
#endif