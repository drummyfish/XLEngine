#include "Logic_NPC.h"
#include "../math/Math.h"
#include "../math/Vector3.h"

enum NPCStates_e
{
	NPC_STATE_IDLE=0,
	NPC_STATE_WALK,
	NPC_STATE_DISABLED
};

NPC::NPC(const XLEngine_Plugin_API *pAPI)
{
	//Create the NPC object.
	m_Data.uObjID = pAPI->Object_Create("NPC", 0xffff);
	pAPI->Object_SetRenderComponent(m_Data.uObjID, "SPRITE_ZAXIS");

	pAPI->Object_AddLogic(m_Data.uObjID, "LOGIC_NPC");
	pAPI->Object_SetGameData(m_Data.uObjID, &m_Data);

	Enable(pAPI, false);
}

void NPC::Reset(const XLEngine_Plugin_API *pAPI, s32 NPC_file, float x, float y, float z, s32 worldX, s32 worldY, float dirx, float diry)
{
	char szTexName[64];
	sprintf(szTexName, "TEXTURE.%03d", NPC_file);
	m_Data.ahTex[0] = pAPI->Texture_LoadTexList(2, 7, 0xff, "", szTexName, 0, XL_FALSE);
	pAPI->Object_SetRenderTexture(m_Data.uObjID, m_Data.ahTex[0]);

	s32 ox, oy;
	u32 w, h;
	float fw, fh;
	pAPI->Texture_GetSize(ox, oy, w, h, fw, fh);

	ObjectPhysicsData *physics = pAPI->Object_GetPhysicsData(m_Data.uObjID);

	//sprite scale...
	s16 *pSpriteScale = (s16 *)pAPI->Texture_GetExtraData();
	s32 newWidth  = w*(256+pSpriteScale[0])>>8;
	s32 newHeight = h*(256+pSpriteScale[1])>>8;

	Vector3 vScale;
	vScale.x = (f32)newWidth  / 8.0f;
	vScale.y = vScale.x;
	vScale.z = (f32)newHeight / 8.0f;
	physics->m_Scale = vScale;
	physics->m_Loc.Set(x, y, z + vScale.z);
	physics->m_worldX = worldX;
	physics->m_worldY = worldY;
	physics->m_Dir.Set(dirx,diry,0);
	physics->m_Dir.Normalize();
	physics->m_Up.Set(0,0,1);
	physics->m_uSector = 0;
	physics->m_Velocity.Set(0,0,0);

	Vector3 vMin = physics->m_Loc - vScale;
	Vector3 vMax = physics->m_Loc + vScale;
	pAPI->Object_SetWorldBounds( m_Data.uObjID, vMin.x, vMin.y, vMin.z, vMax.x, vMax.y, vMax.z );
				
	//Load the rest of the textures.
	for (s32 t=1; t<6; t++)
	{
		m_Data.ahTex[t] = pAPI->Texture_LoadTexList(2, 7, 0xff, "", szTexName, t, XL_FALSE);
	}

	Enable(pAPI, false);
}

void NPC::Enable(const XLEngine_Plugin_API *pAPI, bool bEnable)
{
	if ( bEnable )
	{
		m_Data.uState = NPC_STATE_WALK;
		pAPI->Object_SetActive( m_Data.uObjID, XL_TRUE );
	}
	else
	{
		m_Data.uState = NPC_STATE_DISABLED;
		pAPI->Object_SetActive( m_Data.uObjID, XL_FALSE );
	}
}

void NPC::GetWorldPos(const XLEngine_Plugin_API *pAPI, s32& x, s32& y)
{
	ObjectPhysicsData *pPhysics = pAPI->Object_GetPhysicsData(m_Data.uObjID);
	if ( pPhysics )
	{
		x = pPhysics->m_worldX;
		y = pPhysics->m_worldY;
	}
}

bool NPC::IsEnabled()
{
	return m_Data.uState!=NPC_STATE_DISABLED;
}

NPC::~NPC(void)
{
}

LOGIC_CB_MAP(Logic_NPC);

Logic_NPC::Logic_NPC(const XLEngine_Plugin_API *API)
{
	m_pAPI = API;
	
	//Create the NPC logic.
	LOGIC_FUNC_LIST(funcs);
	m_pAPI->Logic_CreateFromCode("LOGIC_NPC", this, funcs);
}

Logic_NPC::~Logic_NPC(void)
{
}

void Logic_NPC::LogicSetup(u32 uObjID, u32 uParamCount, LogicParam *param)
{
	m_pAPI->Logic_SetMessageMask(LMSG_ACTIVATE);
}

void Logic_NPC::ObjectSetup(u32 uObjID, u32 uParamCount, LogicParam *param)
{
}

void Logic_NPC::Update(u32 uObjID, u32 uParamCount, LogicParam *param)
{
	NPC::GameData *pData = (NPC::GameData *)m_pAPI->Object_GetGameData(uObjID);

	//do nothing if the NPC is disabled.
	if ( pData->uState == NPC_STATE_DISABLED )
		return;

	ObjectPhysicsData *pPhysics = m_pAPI->Object_GetPhysicsData(uObjID);

	float x, y, z;
	m_pAPI->Object_GetCameraVector(uObjID, x, y, z);

	if ( Math::abs(x) < 40.0f && Math::abs(y) < 40.0f )
	{
		pData->uState = NPC_STATE_IDLE;

		m_pAPI->Object_SetRenderFlip(uObjID, XL_FALSE, XL_FALSE);
		m_pAPI->Object_SetRenderTexture(uObjID, pData->ahTex[5]);
	}
	else
	{
		pData->uState = NPC_STATE_WALK;

		Vector3 vDir(x,y,0);
		vDir.Normalize();

		XL_BOOL bFlipX = XL_FALSE;
		float angle = -pPhysics->m_Dir.Dot(vDir);
		s32 image = (s32)( (angle*0.5f+0.5f) * 5.0f );
		if ( image > 4 ) image = 4;

		Vector3 vPerp;
		vPerp.Cross(pPhysics->m_Dir, vDir);
		if ( vPerp.z < 0.0f && image > 0 && image < 4 )
			 bFlipX = XL_TRUE;
		
		m_pAPI->Object_SetRenderFlip(uObjID, bFlipX, XL_FALSE);
		m_pAPI->Object_SetRenderTexture(uObjID, pData->ahTex[image]);
	}
}

void Logic_NPC::Message(u32 uObjID, u32 uParamCount, LogicParam *param)
{
	//If this is an ACTIVATE message start up the door animation
	//if the door is not already animating.
	if ( uParamCount && param[0].nParam == LMSG_ACTIVATE )
	{
		//Open the NPC conversation dialog.
	}
}
