/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_GAMECLIENT_H
#define GAME_CLIENT_GAMECLIENT_H

#include <base/vmath.h>
#include <engine/client.h>
#include <engine/console.h>
#include <game/layers.h>
#include <game/gamecore.h>
#include "render.h"
#include <vector>
#include <string.h>
#include <string>

class CGameClient : public IGameClient
{
	class CStack
	{
	public:
		enum
		{
			MAX_COMPONENTS = 64,
		};

		CStack();
		void Add(class CComponent *pComponent);

		class CComponent *m_paComponents[MAX_COMPONENTS];
		int m_Num;
	};

	CStack m_All;
	CStack m_Input;
	CNetObjHandler m_NetObjHandler;

	class IEngine *m_pEngine;
	class IInput *m_pInput;
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;
	class IClient *m_pClient;
	class ISound *m_pSound;
	class IConsole *m_pConsole;
	class IStorageTW *m_pStorage;
	class IDemoPlayer *m_pDemoPlayer;
	class IDemoRecorder *m_pDemoRecorder;
	class IServerBrowser *m_pServerBrowser;
	class IServerManager *m_pServerManager; //H-Client
	class IAutoUpdate *m_pAutoUpdate; //H-Client
	class INews *m_pNews; //H-Client
	class IIrc *m_pIrc; //H-Client
	class IEditor *m_pEditor;
	class IFriends *m_pFriends;

	CLayers m_Layers;
	class CCollision m_Collision;
	CUI m_UI;

	void DispatchInput();
	void ProcessEvents();
	void UpdatePositions();

	int m_PredictedTick;
	int m_LastNewPredictedTick;

	int64 m_LastSendInfo;

	static void ConTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConKill(IConsole::IResult *pResult, void *pUserData);

	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	IKernel *Kernel() { return IInterface::Kernel(); }
	IEngine *Engine() const { return m_pEngine; }
	class IGraphics *Graphics() const { return m_pGraphics; }
	class IClient *Client() const { return m_pClient; }
	class CUI *UI() { return &m_UI; }
	class ISound *Sound() const { return m_pSound; }
	class IInput *Input() const { return m_pInput; }
	class IStorageTW *Storage() const { return m_pStorage; }
	class IConsole *Console() { return m_pConsole; }
	class ITextRender *TextRender() const { return m_pTextRender; }
	class IDemoPlayer *DemoPlayer() const { return m_pDemoPlayer; }
	class IDemoRecorder *DemoRecorder() const { return m_pDemoRecorder; }
	class IServerBrowser *ServerBrowser() const { return m_pServerBrowser; }
	class IServerManager *ServerManager() const { return m_pServerManager; } //H-Client
	class IAutoUpdate *AutoUpdate() const { return m_pAutoUpdate; } //H-Client
	class INews *News() const { return m_pNews; } //H-Client
	class IIrc *Irc() const { return m_pIrc; } //H-Client
	class CRenderTools *RenderTools() { return &m_RenderTools; }
	class CLayers *Layers() { return &m_Layers; };
	class CCollision *Collision() { return &m_Collision; };
	class IEditor *Editor() { return m_pEditor; }
	class IFriends *Friends() { return m_pFriends; }

	int NetobjNumCorrections() { return m_NetObjHandler.NumObjCorrections(); }
	const char *NetobjCorrectedOn() { return m_NetObjHandler.CorrectedObjOn(); }

	bool m_SuppressEvents;
	bool m_NewTick;
	bool m_NewPredictedTick;
	int m_FlagDropTick[2];

    //H-CLient: TODO: move this
	struct {
	    int m_Items[NUM_ITEMS_INVENTORY];
	    int m_Ammo[NUM_ITEMS_INVENTORY];
        int m_Selected;
	} m_Inventory;

	std::vector<std::string> m_vMaps; //H-Client Maps
	struct KillInfo
	{
	    int m_X;
	    int m_Y;
	    char m_aName[MAX_NAME_LENGTH];
	    bool m_isDead;

        KillInfo()
        {
            m_aName[0] = 0;
            m_isDead = false;
        }
        KillInfo(vec2 pos, char name[], bool isdead)
        {
            m_X = pos.x;
            m_Y = pos.y;
            str_copy(m_aName, name, strlen(m_aName));
            m_isDead = isdead;
        }
	};
    std::list<KillInfo> m_KillInfo;

	// TODO: move this
	CTuningParams m_Tuning;

	enum
	{
		SERVERMODE_PURE=0,
		SERVERMODE_MOD,
		SERVERMODE_PUREMOD,
	};
	int m_ServerMode;

	int m_DemoSpecID;

	vec2 m_LocalCharacterPos;

	// predicted players
	CCharacterCore m_PredictedPrevChar;
	CCharacterCore m_PredictedChar;

	// snap pointers
	struct CSnapState
	{
		const CNetObj_Character *m_pLocalCharacter;
		const CNetObj_Character *m_pLocalPrevCharacter;
		const CNetObj_PlayerInfo *m_pLocalInfo;
		const CNetObj_SpectatorInfo *m_pSpectatorInfo;
		const CNetObj_SpectatorInfo *m_pPrevSpectatorInfo;
		const CNetObj_Flag *m_paFlags[2];
		const CNetObj_GameInfo *m_pGameInfoObj;
		const CNetObj_GameData *m_pGameDataObj;
		int m_GameDataSnapID;

		const CNetObj_PlayerInfo *m_paPlayerInfos[MAX_CLIENTS];
		const CNetObj_PlayerInfo *m_paInfoByScore[MAX_CLIENTS];
		const CNetObj_PlayerInfo *m_paInfoByTeam[MAX_CLIENTS];

		int m_LocalClientID;
		int m_NumPlayers;
		int m_aTeamSize[2];

		// spectate data
		struct CSpectateInfo
		{
			bool m_Active;
			int m_SpectatorID;
			bool m_UsePosition;
			vec2 m_Position;
		} m_SpecInfo;

		//
		struct CCharacterInfo
		{
			bool m_Active;

			// snapshots
			CNetObj_Character m_Prev;
			CNetObj_Character m_Cur;

			// interpolated position
			vec2 m_Position;
		};

		CCharacterInfo m_aCharacters[MAX_CLIENTS];
	};

	CSnapState m_Snap;

	// client data
	struct CClientData
	{
		int m_UseCustomColor;
		int m_ColorBody;
		int m_ColorFeet;

		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		char m_aSkinName[64];
		int m_SkinID;
		int m_SkinColor;
		int m_Team;
		int m_Emoticon;
		int m_EmoticonStart;
		CCharacterCore m_Predicted;

		CTeeRenderInfo m_SkinInfo; // this is what the server reports
		CTeeRenderInfo m_RenderInfo; // this is what we use

		float m_Angle;
		bool m_Active;
		bool m_ChatIgnore;
		bool m_Friend;

		//H-Client
		struct CTeeRenderFreeze
		{
            bool m_Freezed;
            float m_TimerFreeze;
            float m_Alpha;

            void Reset() { m_Freezed = false; m_TimerFreeze = 0; m_Alpha = 0.0f; }
		} m_FreezedState;

		bool m_InWater;
		vec2 m_LastPos;
		float m_BlurEffectTimer;
        //

		void UpdateRenderInfo();
		void Reset();
	};

	CClientData m_aClients[MAX_CLIENTS];

	CRenderTools m_RenderTools;

	void OnReset();

	// hooks
	virtual void OnConnected();
	virtual void OnRender();
	virtual void OnRelease();
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnStateChange(int NewState, int OldState);
	virtual void OnMessage(int MsgId, CUnpacker *pUnpacker);
	virtual void OnMessageIrc(const char *pFrom, const char *pUser, const char *pText);
	virtual void OnNewSnapshot();
	virtual void OnPredict();
	virtual void OnActivateEditor();
	virtual int OnSnapInput(int *pData);
	virtual void OnShutdown();
	virtual void OnEnterGame();
	virtual void OnRconLine(const char *pLine);
	virtual void OnGameOver();
	virtual void OnStartGame();

	virtual const char *GetItemName(int Type);
	virtual const char *Version();
	virtual const char *NetVersion();
	virtual const char *HClientNetVersion(); //H-Client


	// actions
	// TODO: move these
	void SendSwitchTeam(int Team);
	void SendInfo(bool Start);
	void SendKill(int ClientID);

	// pointers to all systems
	class CGameConsole *m_pGameConsole;
	class CBinds *m_pBinds;
	class CParticles *m_pParticles;
	class CMenus *m_pMenus;
	class CSkins *m_pSkins;
	class CCountryFlags *m_pCountryFlags;
	class CFlow *m_pFlow;
	class CChat *m_pChat;
	class CDamageInd *m_pDamageind;
	class CCamera *m_pCamera;
	class CControls *m_pControls;
	class CEffects *m_pEffects;
	class CSounds *m_pSounds;
	class CMotd *m_pMotd;
	class CMapImages *m_pMapimages;
	class CVoting *m_pVoting;
	class CScoreboard *m_pScoreboard;
	class CItems *m_pItems;
	class CPlayers *m_pPlayers;

    //H-Client
	class CServerAdmin *m_pServerAdmin;
	class CGhost *m_pGhost; //Ghost

	bool m_TakeInitScreenShot;
	int IntersectCharacter(vec2 HookPos, vec2 NewPos, vec2& NewPos2, int ownID);
	int LoadBackgroundMap(const char *map);
	//

private:
    bool m_DDRaceMsgSent; //H-Client: DDRace
};


inline float HueToRgb(float v1, float v2, float h)
{
	if(h < 0.0f) h += 1;
	if(h > 1.0f) h -= 1;
	if((6.0f * h) < 1.0f) return v1 + (v2 - v1) * 6.0f * h;
	if((2.0f * h) < 1.0f) return v2;
	if((3.0f * h) < 2.0f) return v1 + (v2 - v1) * ((2.0f/3.0f) - h) * 6.0f;
	return v1;
}

inline vec3 HslToRgb(vec3 HSL)
{
	if(HSL.s == 0.0f)
		return vec3(HSL.l, HSL.l, HSL.l);
	else
	{
		float v2 = HSL.l < 0.5f ? HSL.l * (1.0f + HSL.s) : (HSL.l+HSL.s) - (HSL.s*HSL.l);
		float v1 = 2.0f * HSL.l - v2;

		return vec3(HueToRgb(v1, v2, HSL.h + (1.0f/3.0f)), HueToRgb(v1, v2, HSL.h), HueToRgb(v1, v2, HSL.h - (1.0f/3.0f)));
	}
}


extern const char *Localize(const char *Str);
int SearchMapsCallback(const char *pName, int IsDir, int StorageType, void *pUser); //H-Client

#endif
