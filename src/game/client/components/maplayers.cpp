/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/demo.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/layers.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>
#include <game/client/render.h>

#include <game/client/components/camera.h>
#include <game/client/components/mapimages.h>
#include <game/client/components/effects.h> //H-Client

#include "maplayers.h"

CMapLayers::CMapLayers(int t)
{
	m_Type = t;
	m_pLayers = 0;
	m_EnvStart = 0;
}

void CMapLayers::OnInit()
{
    m_MineTeeIsDay = false;
	m_pLayers = Layers();
	m_EnvStart = time_get();
}

void CMapLayers::MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, bool isBkg)
{
	float Points[4];

	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX/100.0f, pGroup->m_ParallaxY/100.0f,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), (Graphics()->Tumbtail())?4.0f:1.0f, Points);

    if (Graphics()->ShowInfoKills() && !isBkg)
        Graphics()->MapScreen(0, 0, Collision()->GetWidth()*32, Collision()->GetHeight()*32);
    else
        Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CMapLayers::EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser)
{
	CMapLayers *pThis = (CMapLayers *)pUser;
	pChannels[0] = 0;
	pChannels[1] = 0;
	pChannels[2] = 0;
	pChannels[3] = 0;

	CEnvPoint *pPoints = 0;

	{
		int Start, Num;
		pThis->m_pLayers->Map()->GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);
		if(Num)
			pPoints = (CEnvPoint *)pThis->m_pLayers->Map()->GetItem(Start, 0, 0);
	}

	int Start, Num;
	pThis->m_pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);

	if(Env >= Num)
		return;

	CMapItemEnvelope *pItem = (CMapItemEnvelope *)pThis->m_pLayers->Map()->GetItem(Start+Env, 0, 0);

	static float Time = 0;
	if(pThis->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = pThis->DemoPlayer()->BaseInfo();
		static int LastLocalTick = pInfo->m_CurrentTick;

		if(!pInfo->m_Paused)
			Time += (pInfo->m_CurrentTick-LastLocalTick) / (float)pThis->Client()->GameTickSpeed() * pInfo->m_Speed;

		pThis->RenderTools()->RenderEvalEnvelope(pPoints+pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);

		LastLocalTick = pInfo->m_CurrentTick;
	}
	else if (pThis->Client()->State() == IClient::STATE_OFFLINE)
	{
        //static float LastLocalTick = 0;
        Time = (time_get()-pThis->m_EnvStart)/(float)time_freq();
        //Time += AnimateTime-LastLocalTick;
		pThis->RenderTools()->RenderEvalEnvelope(pPoints+pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);
		//LastLocalTick = AnimateTime;
	}
	else
	{
		if(pThis->m_pClient->m_Snap.m_pGameInfoObj)
			Time = (pThis->Client()->GameTick()-pThis->m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / (float)pThis->Client()->GameTickSpeed();
		pThis->RenderTools()->RenderEvalEnvelope(pPoints+pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);
	}
}

void CMapLayers::OnRender()
{
	if (Client()->State() != IClient::STATE_OFFLINE && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
    else if (Client()->State() == IClient::STATE_OFFLINE && !Client()->BackgroundLoaded())
        return;

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);

	vec2 Center = m_pClient->m_pCamera->m_Center;
	//float center_x = gameclient.camera->center.x;
	//float center_y = gameclient.camera->center.y;

    CMapItemLayerTilemap *pGTMap = m_pLayers->GameLayer();
    CTile *pGameTiles = (CTile *)m_pLayers->Map()->GetData(pGTMap->m_Data);

	bool PassedGameLayer = false;
	for(int g = 0; g < m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);
		if (!pGroup)
            continue;

		if(!g_Config.m_GfxNoclip && pGroup->m_Version >= 2 && pGroup->m_UseClipping)
		{
			// set clipping
			float Points[4];
			MapScreenToGroup(Center.x, Center.y, m_pLayers->GameGroup());
			Graphics()->GetScreen(&Points[0], &Points[1], &Points[2], &Points[3]);
			float x0 = (pGroup->m_ClipX - Points[0]) / (Points[2]-Points[0]);
			float y0 = (pGroup->m_ClipY - Points[1]) / (Points[3]-Points[1]);
			float x1 = ((pGroup->m_ClipX+pGroup->m_ClipW) - Points[0]) / (Points[2]-Points[0]);
			float y1 = ((pGroup->m_ClipY+pGroup->m_ClipH) - Points[1]) / (Points[3]-Points[1]);

			Graphics()->ClipEnable((int)(x0*Graphics()->ScreenWidth()), (int)(y0*Graphics()->ScreenHeight()),
				(int)((x1-x0)*Graphics()->ScreenWidth()), (int)((y1-y0)*Graphics()->ScreenHeight()));
		}

		MapScreenToGroup(Center.x, Center.y, pGroup, g==0);

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+l);
			bool Render = false;
			bool IsGameLayer = false;

			if(pLayer == (CMapItemLayer*)m_pLayers->GameLayer())
			{
				IsGameLayer = true;
				PassedGameLayer = 1;
			}

			// skip rendering if detail layers if not wanted
			if(pLayer->m_Flags&LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && !IsGameLayer)
				continue;

			if(m_Type == -1)
				Render = true;
			else if(m_Type == 0)
			{
				if(PassedGameLayer)
					return;
				Render = true;
			}
			else
			{
				if(PassedGameLayer && !IsGameLayer)
					Render = true;
			}

			if(Render && pLayer->m_Type == LAYERTYPE_TILES && Input()->KeyPressed(KEY_LCTRL) && Input()->KeyPressed(KEY_LSHIFT) && Input()->KeyDown(KEY_KP0))
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				CTile *pTiles = (CTile *)m_pLayers->Map()->GetData(pTMap->m_Data);
				CServerInfo CurrentServerInfo;
				Client()->GetServerInfo(&CurrentServerInfo);
				char aFilename[256];
				str_format(aFilename, sizeof(aFilename), "dumps/tilelayer_dump_%s-%d-%d-%dx%d.txt", CurrentServerInfo.m_aMap, g, l, pTMap->m_Width, pTMap->m_Height);
				IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorageTW::TYPE_SAVE);
				if(File)
				{
					#if defined(CONF_FAMILY_WINDOWS)
						static const char Newline[] = "\r\n";
					#else
						static const char Newline[] = "\n";
					#endif
					for(int y = 0; y < pTMap->m_Height; y++)
					{
						for(int x = 0; x < pTMap->m_Width; x++)
							io_write(File, &(pTiles[y*pTMap->m_Width + x].m_Index), sizeof(pTiles[y*pTMap->m_Width + x].m_Index));
						io_write(File, Newline, sizeof(Newline)-1);
					}
					io_close(File);
				}
			}

			if(Render && !IsGameLayer)
			{
				//layershot_begin();

				if(pLayer->m_Type == LAYERTYPE_TILES)
				{
					CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
					if(pTMap->m_Image == -1)
						Graphics()->TextureSet(-1);
					else
						Graphics()->TextureSet(m_pClient->m_pMapimages->Get(pTMap->m_Image));

					CTile *pTiles = (CTile *)m_pLayers->Map()->GetData(pTMap->m_Data);
					Graphics()->BlendNone();
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f);

                    CServerInfo Info;
                    Client()->GetServerInfo(&Info);
                    int MineTeeLayer = 0;

                    if (str_find_nocase(Info.m_aGameType, "minetee") || str_find_nocase(Info.m_aGameType, "ctf-break"))
                    {
                        if (pTMap == m_pLayers->MineTeeLayer())
                            MineTeeLayer = 1;
                        else if (pTMap == m_pLayers->MineTeeBGLayer())
                            MineTeeLayer = 2;
                        else if (pTMap == m_pLayers->MineTeeFGLayer())
                            MineTeeLayer = 3;
                    }

					if (m_pLayers->GameLayer() && g_Config.m_ddrShowHiddenWays && str_find_nocase(Info.m_aGameType, "race"))
					{
                        Graphics()->BlendNormal();
                        RenderTools()->RenderTilemap(pGameTiles, pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE|LAYERRENDERFLAG_TRANSPARENT,
                                                     EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset, MineTeeLayer, false, m_pClient->m_pEffects);
					}
                    else
                    {
                        Graphics()->BlendNone();
                        RenderTools()->RenderTilemap(0x0, pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
                                                        EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset, MineTeeLayer, false, m_pClient->m_pEffects);
                        Graphics()->BlendNormal();
                        RenderTools()->RenderTilemap(0x0, pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
                                                        EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset, MineTeeLayer, false, m_pClient->m_pEffects);
                    }

                    if (MineTeeLayer == 1)
                        RenderTools()->RenderTilemap(0x0, pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
                                                        EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset, MineTeeLayer, true, 0x0);
				}
				else if(pLayer->m_Type == LAYERTYPE_QUADS)
				{
					CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
					if(pQLayer->m_Image == -1)
						Graphics()->TextureSet(-1);
					else
						Graphics()->TextureSet(m_pClient->m_pMapimages->Get(pQLayer->m_Image));

					CQuad *pQuads = (CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);

					Graphics()->BlendNone();
					RenderTools()->RenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_OPAQUE, EnvelopeEval, this);
					Graphics()->BlendNormal();
					RenderTools()->RenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this);
				}

				//layershot_end();
			}
		}
		if(!g_Config.m_GfxNoclip)
			Graphics()->ClipDisable();
	}

    //H-Client
    //NIGHT-DAY
    CServerInfo Info;
    Client()->GetServerInfo(&Info);
    if (str_find_nocase(Info.m_aGameType,"minetee") && m_pLayers->TileLights() && m_pLayers->MineTeeLayer() && !Graphics()->Tumbtail())
    {
        CTile *pMTLTiles = 0x0;
        CTile *pMTTiles = 0x0;
        static int s_LightLevel = 0;

        if (300 < 0)
            s_LightLevel = 0;
        else
        {
            int Time = -1;
            if (m_pClient->m_Snap.m_pGameInfoObj)
                Time = (Client()->GameTick()-m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / (float)Client()->GameTickSpeed();
            m_MineTeeIsDay = (static_cast<int>(Time/300)%2)?false:true;

            float tt = Time;
            int itt = tt/300;
            tt/=300;

            if (tt-itt < 0.02)
                s_LightLevel=(m_MineTeeIsDay)?0:3;
            else if (tt-itt < 0.025)
                s_LightLevel=(m_MineTeeIsDay)?1:2;
            else if (tt-itt < 0.035)
                s_LightLevel=(m_MineTeeIsDay)?2:1;
            else if (tt-itt < 0.045)
                s_LightLevel=(m_MineTeeIsDay)?3:0;
            else
                s_LightLevel=(m_MineTeeIsDay)?4:0;
        }



        if (s_LightLevel >= 0)
        {
            CMapItemLayerTilemap *pMTMap = m_pLayers->MineTeeLayer();
            if (pMTMap)
                pMTTiles = (CTile *)m_pLayers->Map()->GetData(pMTMap->m_Data);

            if (pMTTiles)
                RenderTools()->UpdateLights(Collision(), pMTTiles, m_pLayers->TileLights(), Layers()->Lights()->m_Width, Layers()->Lights()->m_Height, s_LightLevel);
        }
    }

	if(!g_Config.m_GfxNoclip)
		Graphics()->ClipDisable();

	// reset the screen like it was before
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}

