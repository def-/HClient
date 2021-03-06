/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/irc.h> //H-Client
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h> //H-Client

#include <game/client/components/scoreboard.h>
#include <game/client/components/sounds.h>
#include <game/localization.h>

#include <string> //H-Client

#include "chat.h"


CChat::CChat()
{
	OnReset();

    //H-Client
    AssignChatEmote("x_x",      0);
    AssignChatEmote("-.-",      1);
    AssignChatEmote("B)",       2);
    AssignChatEmote("^^",       3);
    AssignChatEmote("nerd",     4);
    AssignChatEmote(":P",       5);
    AssignChatEmote(":'(",      6);
    AssignChatEmote(":brown:",  7);
    AssignChatEmote(":S",       8);
    AssignChatEmote("O_x",      9);
    AssignChatEmote("&_&",      10);
    AssignChatEmote("xD",       11);
    AssignChatEmote(":3",       12);
    AssignChatEmote(":blue:",   13);
    AssignChatEmote(":red:",    14);
    AssignChatEmote(":chomp:",  15);
    AssignChatEmote(":cookie:", 16);
    AssignChatEmote("e_e",      17);
    AssignChatEmote(":emo:",    18);
    AssignChatEmote(">_<",      19);
    AssignChatEmote(":ganja:",  20);
    AssignChatEmote(":x",       21);
    AssignChatEmote(":!:",      22);
    AssignChatEmote(":?:",      23);
    AssignChatEmote(":B",       24);
    AssignChatEmote("o_o",      25);
    AssignChatEmote(":)",       26);
    AssignChatEmote(":D",       27);
    AssignChatEmote("!o!",      28);
    AssignChatEmote(":|",       29);
    //
}

void CChat::OnReset()
{
	for(int i = 0; i < MAX_LINES; i++)
	{
		m_aLines[i].m_Time = 0;
		m_aLines[i].m_aText[0] = 0;
		m_aLines[i].m_aName[0] = 0;
		m_aLines[i].m_aChan[0] = 0;
		m_aLines[i].m_Type = 0;
	}

	m_Show = false;
	m_InputUpdate = false;
	m_ChatStringOffset = 0;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = 0x0;
	m_ChatRoom = -1;
}

void CChat::OnRelease()
{
	m_Show = false;
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
	{
		m_Mode = MODE_NONE;
		for(int i = 0; i < MAX_LINES; i++)
			m_aLines[i].m_Time = 0;
		m_CurrentLine = 0;
	}
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat*)pUserData)->Say(MODE_ALL, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat*)pUserData)->Say(MODE_TEAM, pResult->GetString(0));
}

void CChat::ConSayIrc(IConsole::IResult *pResult, void *pUserData)
{
	((CChat*)pUserData)->Say(MODE_IRC, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		((CChat*)pUserData)->EnableMode(0);
	else if(str_comp(pMode, "team") == 0)
		((CChat*)pUserData)->EnableMode(1);
	else if(str_comp(pMode, "irc") == 0)
		((CChat*)pUserData)->EnableMode(2);
	else
		((CChat*)pUserData)->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "expected all or team as mode");
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("say_irc", "r", CFGFLAG_CLIENT, ConSayIrc, this, "Say in irc chat");
	Console()->Register("chat", "s", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
}

bool CChat::OnInput(IInput::CEvent Event)
{
	if(m_Mode == MODE_NONE)
		return false;

	if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		m_Mode = MODE_NONE;
		m_pClient->OnRelease();
	}
	else if(Event.m_Flags&IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_Input.GetString()[0])
		{
			Say(m_Mode, m_Input.GetString());
			char *pEntry = m_History.Allocate(m_Input.GetLength()+1);
			mem_copy(pEntry, m_Input.GetString(), m_Input.GetLength()+1);
		}
		m_pHistoryEntry = 0x0;
		m_Mode = MODE_NONE;
		m_pClient->OnRelease();
	}
	if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		// fill the completion buffer
		if(m_CompletionChosen < 0)
		{
			const char *pCursor = m_Input.GetString()+m_Input.GetCursorOffset();
			for(int Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor-1) != ' '; --pCursor, ++Count);
			m_PlaceholderOffset = pCursor-m_Input.GetString();

			for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
				++m_PlaceholderLength;

			str_copy(m_aCompletionBuffer, m_Input.GetString()+m_PlaceholderOffset, min(static_cast<int>(sizeof(m_aCompletionBuffer)), m_PlaceholderLength+1));
		}

		// find next possible name
		const char *pCompletionString = 0;
		m_CompletionChosen = (m_CompletionChosen+1)%(2*MAX_CLIENTS);
		for(int i = 0; i < 2*MAX_CLIENTS; ++i)
		{
			int SearchType = ((m_CompletionChosen+i)%(2*MAX_CLIENTS))/MAX_CLIENTS;
			int Index = (m_CompletionChosen+i)%MAX_CLIENTS;
			if(!m_pClient->m_Snap.m_paPlayerInfos[Index])
				continue;

			bool Found = false;
			if(SearchType == 1)
			{
				if(str_comp_nocase_num(m_pClient->m_aClients[Index].m_aName, m_aCompletionBuffer, str_length(m_aCompletionBuffer)) &&
					str_find_nocase(m_pClient->m_aClients[Index].m_aName, m_aCompletionBuffer))
					Found = true;
			}
			else if(!str_comp_nocase_num(m_pClient->m_aClients[Index].m_aName, m_aCompletionBuffer, str_length(m_aCompletionBuffer)))
				Found = true;

			if(Found)
			{
				pCompletionString = m_pClient->m_aClients[Index].m_aName;
				m_CompletionChosen = Index+SearchType*MAX_CLIENTS;
				break;
			}
		}

		// insert the name
		if(pCompletionString)
		{
			char aBuf[256];
			// add part before the name
			str_copy(aBuf, m_Input.GetString(), min(static_cast<int>(sizeof(aBuf)), m_PlaceholderOffset+1));

			// add the name
			str_append(aBuf, pCompletionString, sizeof(aBuf));

			// add seperator
			const char *pSeparator = "";
			if(*(m_Input.GetString()+m_PlaceholderOffset+m_PlaceholderLength) != ' ')
				pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
			else if(m_PlaceholderOffset == 0)
				pSeparator = ":";
			if(*pSeparator)
				str_append(aBuf, pSeparator, sizeof(aBuf));

			// add part after the name
			str_append(aBuf, m_Input.GetString()+m_PlaceholderOffset+m_PlaceholderLength, sizeof(aBuf));

			m_PlaceholderLength = str_length(pSeparator)+str_length(pCompletionString);
			m_OldChatStringLength = m_Input.GetLength();
			m_Input.Set(aBuf);
			m_Input.SetCursorOffset(m_PlaceholderOffset+m_PlaceholderLength);
			m_InputUpdate = true;
			m_OldChatStringLength = m_Input.GetLength();
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key != KEY_TAB)
			m_CompletionChosen = -1;

		m_OldChatStringLength = m_Input.GetLength();
		m_Input.ProcessInput(Event);
		m_InputUpdate = true;
	}

    if(m_Mode == MODE_IRC && Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_LALT)
	    m_pClient->Irc()->NextRoom();

	//H-Client
	#if defined(CONF_FAMILY_WINDOWS)
	static bool makeCopy = false;
	if(!makeCopy && Input()->KeyPressed(KEY_LCTRL) && Input()->KeyDown('v'))
	{
	    dbg_msg("HC", "PASA!");
	    char aBuf[256]={0}, str[256]={0};

        str_copy(aBuf, m_Input.GetString(), sizeof(aBuf));

	    GetClipBoardText(str, sizeof(str));
	    str_append(aBuf, str, sizeof(aBuf));

        m_Input.Set(aBuf);
        m_Input.SetCursorOffset(str_length(aBuf));
        m_InputUpdate = true;
        makeCopy = true;
	} else if (makeCopy)
        makeCopy = false;
	#endif
	//

	if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
		if (m_pHistoryEntry)
		{
			char *pTest = m_History.Prev(m_pHistoryEntry);

			if (pTest)
				m_pHistoryEntry = pTest;
		}
		else
			m_pHistoryEntry = m_History.Last();

		if (m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry);
	}
	else if (Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if (m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if (m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry);
		else
			m_Input.Clear();
	}

	return true;
}


void CChat::EnableMode(int Team)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(m_Mode == MODE_NONE)
	{
		if(Team == 1)
			m_Mode = MODE_TEAM;
        else if (Team == 2)
            m_Mode = MODE_IRC;
		else
			m_Mode = MODE_ALL;

		m_Input.Clear();
		Input()->ClearEvents();
		m_CompletionChosen = -1;
	}
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		AddLine(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}

void CChat::OnMessageIrc(const char *pFrom, const char *pUser, const char *pText)
{
    AddLine(pFrom, pUser, pText);
}

void CChat::AddLine(const char *pFrom, const char *pUser, const char *pLine)
{
	bool Highlighted = false;
	char *p = const_cast<char*>(pLine);
	while(*p)
	{
		Highlighted = false;
		pLine = p;
		// find line seperator and strip multiline
		while(*p)
		{
			if(*p++ == '\n')
			{
				*(p-1) = 0;
				break;
			}
		}

		m_CurrentLine = (m_CurrentLine+1)%MAX_LINES;
		m_aLines[m_CurrentLine].m_Time = time_get();
		m_aLines[m_CurrentLine].m_YOffset[0] = -1.0f;
		m_aLines[m_CurrentLine].m_YOffset[1] = -1.0f;

		// check for highlighted name
		const char *pHL = str_find_nocase(pLine, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName);
		if(pHL)
		{
			int Length = str_length(m_pClient->Irc()->GetNick());
			if((pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || (pHL[Length] == ':' && pHL[Length+1] == ' ')))
				Highlighted = true;
		}

		m_aLines[m_CurrentLine].m_Highlighted =  Highlighted;
		m_aLines[m_CurrentLine].m_Type = 1;
        str_copy(m_aLines[m_CurrentLine].m_aChan, pFrom, sizeof(m_aLines[m_CurrentLine].m_aChan));
        str_copy(m_aLines[m_CurrentLine].m_aName, pUser, sizeof(m_aLines[m_CurrentLine].m_aName));
        str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), "%s", pLine);
	}

	// play sound
    if(Highlighted)
		m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 0, vec2(0.0f, 0.0f));
	else
		m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 0, vec2(0,0));
}

void CChat::AddLine(int ClientID, int Team, const char *pLine)
{
	if(ClientID != -1 && (m_pClient->m_aClients[ClientID].m_aName[0] == '\0' || // unknown client
		m_pClient->m_aClients[ClientID].m_ChatIgnore))
		return;

	bool Highlighted = false;
	char *p = const_cast<char*>(pLine);
	while(*p)
	{
		Highlighted = false;
		pLine = p;
		// find line seperator and strip multiline
		while(*p)
		{
			if(*p++ == '\n')
			{
				*(p-1) = 0;
				break;
			}
		}

		m_CurrentLine = (m_CurrentLine+1)%MAX_LINES;
		m_aLines[m_CurrentLine].m_Time = time_get();
		m_aLines[m_CurrentLine].m_YOffset[0] = -1.0f;
		m_aLines[m_CurrentLine].m_YOffset[1] = -1.0f;
		m_aLines[m_CurrentLine].m_ClientID = ClientID;
		m_aLines[m_CurrentLine].m_Team = Team;
		m_aLines[m_CurrentLine].m_NameColor = -2;

		// check for highlighted name
		const char *pHL = str_find_nocase(pLine, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName);
		if(pHL)
		{
			int Length = str_length(m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName);
			if((pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || (pHL[Length] == ':' && pHL[Length+1] == ' ')))
				Highlighted = true;
		}
		m_aLines[m_CurrentLine].m_Highlighted =  Highlighted;
		m_aLines[m_CurrentLine].m_Type = 0;

		if(ClientID == -1) // server message
		{
			str_copy(m_aLines[m_CurrentLine].m_aName, "*** ", sizeof(m_aLines[m_CurrentLine].m_aName));
			str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), "%s", pLine);
		}
		else
		{
			if(m_pClient->m_aClients[ClientID].m_Team == TEAM_SPECTATORS)
				m_aLines[m_CurrentLine].m_NameColor = TEAM_SPECTATORS;

			if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
			{
				if(m_pClient->m_aClients[ClientID].m_Team == TEAM_RED)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_RED;
				else if(m_pClient->m_aClients[ClientID].m_Team == TEAM_BLUE)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_BLUE;
			}

			str_copy(m_aLines[m_CurrentLine].m_aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(m_aLines[m_CurrentLine].m_aName));
			str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), ": %s", pLine);
		}

		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "%s%s", m_aLines[m_CurrentLine].m_aName, m_aLines[m_CurrentLine].m_aText);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_aLines[m_CurrentLine].m_Team?"teamchat":"chat", aBuf);
	}

	// play sound
	if(ClientID == -1)
		m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 0, vec2(0,0));
	else if(Highlighted)
		m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 0, vec2(0.0f, 0.0f));
	else
		m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 0, vec2(0,0));
}

void CChat::OnRender()
{
	float Width = 300.0f*Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, 300.0f);
	float x = 5.0f;
	float y = 300.0f-30.0f;
	if(m_Mode != MODE_NONE)
	{
		// render chat input
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, x, y, 8.0f, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = Width-190.0f;
		Cursor.m_MaxLines = 2;

		if(m_Mode == MODE_ALL)
			TextRender()->TextEx(&Cursor, Localize("All"), -1);
		else if(m_Mode == MODE_TEAM)
			TextRender()->TextEx(&Cursor, Localize("Team"), -1);
        else if (m_Mode == MODE_IRC)
        {
            if (m_pClient->Irc()->GetState() == IIrc::STATE_CONNECTED)
            {
                CIrcCom *pCom = m_pClient->Irc()->GetActiveCom();
                if (!pCom)
                    TextRender()->TextEx(&Cursor, Localize("UNKOWN ROOM"), -1);
                else
                {
                    if (pCom->GetType() == CIrcCom::TYPE_CHANNEL)
                    {
                        CComChan *pChan = static_cast<CComChan*>(pCom);
                        TextRender()->TextEx(&Cursor, pChan->m_Channel, -1);
                    }
                    else if (pCom->GetType() == CIrcCom::TYPE_QUERY)
                    {
                        CComQuery *pQuery = static_cast<CComQuery*>(pCom);
                        TextRender()->TextEx(&Cursor, pQuery->m_User, -1);
                    }
                }
            }
            else
               TextRender()->TextEx(&Cursor, Localize("IRC DISCONNECTED"), -1);
        }
		else
			TextRender()->TextEx(&Cursor, Localize("Chat"), -1);

		TextRender()->TextEx(&Cursor, ": ", -1);

		// check if the visible text has to be moved
		if(m_InputUpdate)
		{
			if(m_ChatStringOffset > 0 && m_Input.GetLength() < m_OldChatStringLength)
				m_ChatStringOffset = max(0, m_ChatStringOffset-(m_OldChatStringLength-m_Input.GetLength()));

			if(m_ChatStringOffset > m_Input.GetCursorOffset())
				m_ChatStringOffset -= m_ChatStringOffset-m_Input.GetCursorOffset();
			else
			{
				CTextCursor Temp = Cursor;
				Temp.m_Flags = 0;
				TextRender()->TextEx(&Temp, m_Input.GetString()+m_ChatStringOffset, m_Input.GetCursorOffset()-m_ChatStringOffset);
				TextRender()->TextEx(&Temp, "|", -1);
				while(Temp.m_LineCount > 2)
				{
					++m_ChatStringOffset;
					Temp = Cursor;
					Temp.m_Flags = 0;
					TextRender()->TextEx(&Temp, m_Input.GetString()+m_ChatStringOffset, m_Input.GetCursorOffset()-m_ChatStringOffset);
					TextRender()->TextEx(&Temp, "|", -1);
				}
			}
			m_InputUpdate = false;
		}

		TextRender()->TextEx(&Cursor, m_Input.GetString()+m_ChatStringOffset, m_Input.GetCursorOffset()-m_ChatStringOffset);
		static float MarkerOffset = TextRender()->TextWidth(0, 8.0f, "|", -1)/3;
		CTextCursor Marker = Cursor;
		Marker.m_X -= MarkerOffset;
		TextRender()->TextEx(&Marker, "|", -1);
		TextRender()->TextEx(&Cursor, m_Input.GetString()+m_Input.GetCursorOffset(), -1);
	}

	y -= 8.0f;

    int64 Now = time_get();
    float LineWidth = m_pClient->m_pScoreboard->Active() ? 90.0f : 200.0f;
    float HeightLimit = m_pClient->m_pScoreboard->Active() ? 230.0f : m_Show ? 50.0f : 200.0f;
    float Begin = x;
    float FontSize = 6.0f;
    CTextCursor Cursor;
    int OffsetType = m_pClient->m_pScoreboard->Active() ? 1 : 0;

    if (m_Mode != MODE_IRC)
    {
        for(int i = 0; i < MAX_LINES; i++)
        {
            int r = ((m_CurrentLine-i)+MAX_LINES)%MAX_LINES;
            if(Now > m_aLines[r].m_Time+16*time_freq() && !m_Show)
                break;

            // get the y offset (calculate it if we haven't done that yet)
            if(m_aLines[r].m_YOffset[OffsetType] < 0.0f)
            {
                TextRender()->SetCursor(&Cursor, Begin, 0.0f, FontSize, 0);
                Cursor.m_LineWidth = LineWidth;
                if (m_aLines[r].m_Type == 1 && m_aLines[r].m_aChan[0] != 0)
                    TextRender()->TextEx(&Cursor, m_aLines[r].m_aChan, -1);
                TextRender()->TextEx(&Cursor, m_aLines[r].m_aName, -1);
                TextRender()->TextEx(&Cursor, m_aLines[r].m_aText, -1);
                m_aLines[r].m_YOffset[OffsetType] = Cursor.m_Y + Cursor.m_FontSize;
            }
            y -= m_aLines[r].m_YOffset[OffsetType];

            // cut off if msgs waste too much space
            if(y < HeightLimit)
                break;

            float Blend = Now > m_aLines[r].m_Time+14*time_freq() && !m_Show ? 1.0f-(Now-m_aLines[r].m_Time-14*time_freq())/(2.0f*time_freq()) : 1.0f;
            if (Blend <= 0.0f)
                continue;

            // reset the cursor
            //TextRender()->SetCursor(&Cursor, Begin, y, FontSize, TEXTFLAG_RENDER);
            TextRender()->SetCursor(&Cursor, Begin+4, y, FontSize, TEXTFLAG_RENDER); //H-Client
            Cursor.m_LineWidth = LineWidth;

            // render channel
            if (m_aLines[r].m_Type == 1)
            {
                if(m_aLines[r].m_aChan[0] != 0)
                {
                    TextRender()->TextColor(0.45f, 0.45f, 0.45f, Blend);
                    TextRender()->TextEx(&Cursor, m_aLines[r].m_aChan, -1);
                }

                Graphics()->BlendNormal();
                Graphics()->TextureSet(g_pData->m_aImages[IMAGE_IRC].m_Id);
                Graphics()->QuadsBegin();
                    Graphics()->SetColor(1.0f, 1.0f, 1.0f, Blend);
                    Graphics()->QuadsSetRotation(0.0f);
                    IGraphics::CQuadItem QuadItem(Begin, y+FontSize-1.5f, 8.0f, 8.0f);
                    Graphics()->QuadsDraw(&QuadItem, 1);
                Graphics()->QuadsEnd();
            }

            // render name
            if (m_aLines[r].m_Type == 0)
            {
                if(m_aLines[r].m_ClientID == -1)
                    TextRender()->TextColor(1.0f, 1.0f, 0.5f, Blend); // system
                else if(m_aLines[r].m_Team)
                    TextRender()->TextColor(0.45f, 0.9f, 0.45f, Blend); // team message
                else if(m_aLines[r].m_NameColor == TEAM_RED)
                    TextRender()->TextColor(1.0f, 0.5f, 0.5f, Blend); // red
                else if(m_aLines[r].m_NameColor == TEAM_BLUE)
                    TextRender()->TextColor(0.7f, 0.7f, 1.0f, Blend); // blue
                else if(m_aLines[r].m_NameColor == TEAM_SPECTATORS)
                    TextRender()->TextColor(0.75f, 0.5f, 0.75f, Blend); // spectator
                else
                    TextRender()->TextColor(0.8f, 0.8f, 0.8f, Blend);
            }
            else
                TextRender()->TextColor(0.8f, 0.8f, 0.8f, Blend);

            TextRender()->TextEx(&Cursor, m_aLines[r].m_aName, -1);
            vec4 textColor = vec4(1.0f, 1.0f, 1.0f, Blend);

            // render line
            if (m_aLines[r].m_Type == 0)
            {
                if(m_aLines[r].m_ClientID == -1)
                    textColor = vec4(1.0f, 1.0f, 0.5f, Blend); // system
                else if(m_aLines[r].m_Highlighted)
                    textColor = vec4(1.0f, 0.5f, 0.5f, Blend); // highlighted
                else if(m_aLines[r].m_Team)
                    textColor = vec4(0.65f, 1.0f, 0.65f, Blend); // team message
                else
                    textColor = vec4(1.0f, 1.0f, 1.0f, Blend);
            }
            else
                textColor = vec4(1.0f, 1.0f, 1.0f, Blend);

            /** H-Client **/
            //Render Color Line
            if (m_aLines[r].m_Type == 0 && m_aLines[r].m_ClientID != -1)
            {
                CGameClient::CClientData *pClientData = &m_pClient->m_aClients[m_aLines[r].m_ClientID];
                CTeeRenderInfo RenderInfo = pClientData->m_RenderInfo;
                RenderInfo.m_Size = 8.0f;
                RenderInfo.m_ColorBody.a = RenderInfo.m_ColorFeet.a = Blend;

                RenderTools()->RenderTee(CAnimState::GetIdle(), &RenderInfo, 0, vec2(-1.0f, 0.0f), vec2(Begin, y+FontSize-1.5f));

                if (g_Config.m_hcChatColours)
                    textColor = vec4(pClientData->m_RenderInfo.m_ColorBody.r, pClientData->m_RenderInfo.m_ColorBody.g, pClientData->m_RenderInfo.m_ColorBody.b, Blend);
            }
            //Emotes
            std::string sText = m_aLines[r].m_aText;
            TextRender()->TextColor(textColor.r, textColor.g, textColor.b, textColor.a);
            if (g_Config.m_hcChatEmoticons && m_aLines[r].m_ClientID > -1)
            {
                std::list<CChatEmote>::iterator it = m_vChatEmotes.begin();
                while(it != m_vChatEmotes.end())
                {
                    const char *pHL = 0x0;
                    int Length = str_length((it)->m_Emote);
                    do
                    {
                        pHL = str_find_nocase(sText.c_str(), (it)->m_Emote);
                        if(pHL)
                        {
                            HighlightText(sText.substr(0,sText.length() - str_length(pHL)).c_str(), textColor, &Cursor);

                            int emoteSize = 8.0f;
                            Graphics()->BlendNormal();
                            Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CHAT_EMOTICONS].m_Id);
                            Graphics()->QuadsBegin();
                                Graphics()->SetColor(1.0f, 1.0f, 1.0f, Blend);
                                RenderTools()->SelectSprite((it)->m_SpriteID);
                                Graphics()->QuadsSetRotation(0.0f);
                                IGraphics::CQuadItem QuadItem(Cursor.m_X+emoteSize/2, Cursor.m_Y+emoteSize/2, emoteSize, emoteSize);
                                Graphics()->QuadsDraw(&QuadItem, 1);
                            Graphics()->QuadsEnd();

                            Cursor.m_X += emoteSize;
                            sText = sText.substr(sText.length() - (str_length(pHL) - Length), -1);
                        }
                    } while(pHL);

                    it++;
                }

                if (sText.length() > 0)
                    HighlightText(sText.c_str(), textColor, &Cursor);
            }
            else
                TextRender()->TextEx(&Cursor, sText.c_str(), -1);
            /**/

            //TextRender()->TextEx(&Cursor, m_aLines[r].m_aText, -1);
        }

        TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else
    {
        CIrcCom *pCom = m_pClient->Irc()->GetActiveCom();
        if (!pCom || pCom->m_Buffer.size() <= 0)
            return;

        std::vector<std::string>::iterator it = pCom->m_Buffer.end();
        --it;
        while (it != pCom->m_Buffer.begin())
        {
            TextRender()->SetCursor(&Cursor, Begin, 0.0f, FontSize, 0);
            Cursor.m_LineWidth = LineWidth;
            y-=Cursor.m_Y+Cursor.m_FontSize;

            if(y < HeightLimit)
                break;

            TextRender()->SetCursor(&Cursor, Begin+4, y, FontSize, TEXTFLAG_RENDER); //H-Client
            TextRender()->TextEx(&Cursor, (*it).c_str(), -1);

            --it;
        }
    }
}

void CChat::HighlightText(const char *pText, vec4 dfcolor, CTextCursor *pCursor)
{
    //Parse Url
    std::string sText(pText);
    int endPos = std::string::npos;
    do
    {
        if ((endPos = sText.find_first_of(" ")) == std::string::npos)
            endPos = sText.length()-1;
        if (g_Config.m_hcChatUrl && (sText.substr(0,endPos).find("www.") != std::string::npos || sText.substr(0,endPos).find("http://") != std::string::npos))
        {
            TextRender()->TextColor(0.0f,0.63f,0.9f,dfcolor.a);
            TextRender()->TextEx(pCursor, sText.substr(0, endPos+1).c_str(), -1);
            TextRender()->TextColor(dfcolor.r,dfcolor.g,dfcolor.b,dfcolor.a);
        }
        else
            TextRender()->TextEx(pCursor, sText.substr(0,endPos+1).c_str(), -1);

        sText = sText.substr(endPos+1);
    } while(sText.length() > 0);
}

void CChat::Say(int Team, const char *pLine)
{
    if (Team != MODE_IRC)
    {
        // send chat message
        if (Team == MODE_ALL)
            Team = 0;
        else
            Team = 1;
        CNetMsg_Cl_Say Msg;
        Msg.m_Team = Team;
        Msg.m_pMessage = pLine;
        Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
    }
    else if (m_pClient->Irc()->GetState() == IIrc::STATE_CONNECTED)
    {
        m_pClient->Irc()->SendMsg("", pLine);
        CIrcCom *pCom = m_pClient->Irc()->GetActiveCom();
        if (pCom && pCom->GetType() == CIrcCom::TYPE_CHANNEL)
        {
            CComChan *pChan = static_cast<CComChan*>(pCom);
            std::string aChan = pChan->m_Channel;
            aChan.insert(0, "[");
            aChan.append("] ");
            std::string aFrom = m_pClient->Irc()->GetNick();
            aFrom.insert(0, "<");
            aFrom.append("> ");
            AddLine(aChan.c_str(), aFrom.c_str(), pLine);
        }
        if (pCom && pCom->GetType() == CIrcCom::TYPE_QUERY)
        {
            CComQuery *pQuery = static_cast<CComQuery*>(pCom);
            std::string aUser = pQuery->m_User;
            aUser.insert(0, "[");
            aUser.append("] ");
            std::string aFrom = m_pClient->Irc()->GetNick();
            aFrom.insert(0, "<");
            aFrom.append("> ");
            AddLine(aUser.c_str(), aFrom.c_str(), pLine);
        }
    }
}

//H-Client
void CChat::AssignChatEmote(const char *emote, int sc_id)
{
    int spriteID = SPRITE_CHAT_EMOTE1 + sc_id;
    if (spriteID < SPRITE_CHAT_EMOTE1 || spriteID > SPRITE_CHAT_EMOTE30)
        return;

    m_vChatEmotes.push_back(CChatEmote(emote, SPRITE_CHAT_EMOTE1 + sc_id));
}
//
