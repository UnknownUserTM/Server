/*********************************************************************
* title_name		: Language System
* date_created		: 2017.10.21
* filename			: MultiLanguage.cpp
* author			: VegaS
* version_actual	: Version 0.1.0
*/
#include "stdafx.h"

#include "constants.h"
#include "banword.h"
#include "utils.h"
#include "mob_manager.h"
#include "empire_text_convert.h"
#include "config.h"
#include "skill_power.h"
#include "locale.hpp"
#include <boost/format.hpp>
#include "desc.h"
#include "desc_manager.h"
#include "char.h"
#include "item.h"
#include "char_manager.h"
#include "affect.h"
//#include "start_position.h"
#include "p2p.h"
#include "db.h"
#include "skill.h"
#include "dungeon.h"
#include "castle.h"
#include <string>
#include "buffer_manager.h"
#include <algorithm>
#include <iostream>
#include "questmanager.h"
#include "desc_client.h"
#include "sectree_manager.h"
#include "regen.h"
#include "target.h"
#include "party.h"
#include "OXEvent.h"
#include "MultiLanguage.h"
#include <cstdarg>
#include <common.h>

using namespace std;

BYTE PK_PROTECT_LEVEL = 15;
eLocalization g_eLocalType = LC_GERMANY;
std::string g_stLocale = "latin1";

extern set<string> g_setQuestObjectDir;
extern std::string g_stQuestDir;

int (*check_name) (const char * str) = NULL;

/*******************************************************************\
|				CLanguageManager - CLASS							|
\*******************************************************************/

CLanguageManager::CLanguageManager()
{
}

CLanguageManager::~CLanguageManager()
{
}

/*******************************************************************\
|				CHARACTER - CLASS									|
\*******************************************************************/

void CHARACTER::SetLanguage(const char* szLanguage)
{
	if (GetDesc())
		strlcpy(GetDesc()->GetAccountTable().szLanguage, szLanguage, sizeof(GetDesc()->GetAccountTable().szLanguage));
}

const char * CHARACTER::GetLanguage() const
{
	if (GetDesc())
		return GetDesc()->GetAccountTable().szLanguage;
		
	return g_stDefaultLanguage;
}

void CHARACTER::LanguageChatPacket(BYTE bType, const char * format, ...)
{
	LPDESC d = GetDesc();
	if (!d || !format)
		return;
	
	std::string c_strLocale = LC_TEXT_CONVERT_LANGUAGE(GetLanguage(), format);
	
	char chatbuf[CHAT_MAX_LEN + 1];
	va_list args;

	va_start(args, format);
	int32_t len = vsnprintf(chatbuf, sizeof(chatbuf), c_strLocale.c_str(), args);
	va_end(args);

	struct packet_chat pack_chat;

	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(struct packet_chat) + len;
	pack_chat.type = bType;
	pack_chat.id = 0;
	pack_chat.bEmpire = d->GetEmpire();

	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(struct packet_chat));
	buf.write(chatbuf, len);

	d->Packet(buf.read_peek(), buf.size());
}

void CHARACTER::PacketNotifyPickup(const char * szBuf, DWORD dwVnum)
{
	if (GetDesc())
	{
		TPacketGCItemPickup pack;
		pack.header = HEADER_GC_ITEM_PICKUP;
		strlcpy(pack.szBuf, szBuf, sizeof(pack.szBuf));
		pack.vnum = dwVnum;
		GetDesc()->Packet(&pack, sizeof(TPacketGCItemPickup));
	}
}

/*******************************************************************\
| [PUBLIC] OXEvent Languages Features
\*******************************************************************/

struct SLanguageNoticeMapPacketFunc
{
	int32_t iNumberQuiz;
	std::vector<std::vector<tag_Quiz> > m_vecQuizLanguage;

	SLanguageNoticeMapPacketFunc(std::vector<std::vector<tag_Quiz> > m_vecReceived, int32_t val) : iNumberQuiz(val), m_vecQuizLanguage(m_vecReceived)
	{
	}

	void operator() (LPDESC d)
	{
		if (!d->GetCharacter() || d->GetCharacter()->GetMapIndex() != OXEVENT_MAP_INDEX)
			return;

		int32_t iLanguageIndex = CLanguageManager::instance().GetKeyInstanceVectorByLang(d->GetCharacter()->GetLanguage());
		if (!CLanguageManager::instance().GetVectorInstanceKey(m_vecQuizLanguage, iLanguageIndex))
			return;

		d->GetCharacter()->ChatPacket(CHAT_TYPE_BIG_NOTICE, m_vecQuizLanguage[iLanguageIndex][iNumberQuiz].Quiz);
	}
};

void CLanguageManager::SendLanguageNoticeMap(std::vector<std::vector<tag_Quiz> > m_vecQuizLanguage, int32_t iNumberQuiz)
{
	const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();
	std::for_each(c_ref_set.begin(), c_ref_set.end(), SLanguageNoticeMapPacketFunc(m_vecQuizLanguage, iNumberQuiz));
}

int32_t CLanguageManager::GetKeyInstanceVectorByLang(const char* pszLanguage)
{
	for (size_t bLanguageID = 0; bLanguageID < _countof(g_stLanguagesName); bLanguageID++)
	{
	    if (!strcmp(pszLanguage, g_stLanguagesName[bLanguageID].c_str()))
	        return bLanguageID;
	}

	return 0;
}

bool CLanguageManager::GetVectorInstanceKey(std::vector<std::vector<tag_Quiz> > m_vecQuiz, int32_t keyA)
{
	return (!m_vecQuiz.size() || !m_vecQuiz[keyA].size()) ? false : true;
}

/*******************************************************************\
| [PUBLIC] Chat Shout Language Filter Features
\*******************************************************************/

const char * CLanguageManager::GetLanguageStringByIndex(BYTE bLanguageID)
{
	if (bLanguageID == LANGUAGE_MAX_NUM)
		return g_stGlobalLanguage;
	
	return g_stLanguagesName[bLanguageID].c_str();
}

int CHARACTER::GetShoutLanguageByID(DWORD bLanguageID)
{
	char buf[CHAT_MAX_LEN + 1];
	snprintf(buf, sizeof(buf), LANGUAGE_FILTER_CHAT, bLanguageID);
	return GetQuestFlag(buf);
}

void CHARACTER::SetShoutLanguageByID(BYTE bLanguageID, BYTE bValue)
{
	if (GetShoutLanguageByID(bLanguageID) == bValue)
		return;

	char buf[CHAT_MAX_LEN + 1];
	snprintf(buf, sizeof(buf), LANGUAGE_FILTER_CHAT, bLanguageID);
	SetQuestFlag(buf, bValue);
}

void CHARACTER::SetShoutLanguageGlobal()
{
	for (int32_t bLanguageID = 0; bLanguageID < LANGUAGE_MAX_NUM + 1; bLanguageID++)
	{
		if (bLanguageID == LANGUAGE_MAX_NUM)
			SetShoutLanguageByID(LANGUAGE_MAX_NUM, LANGUAGE_SELECTED);
		else
			SetShoutLanguageByID(bLanguageID, LANGUAGE_UNSELECTED);
	}
}

void CHARACTER::FilterChatPacket(const char * szNameSender, const char * szLanguageSender, BYTE bEmpireSender, const char * szTextLine)
{
	LPDESC d = GetDesc();
	if (!d)
		return;
	
	if (!GetOptionFilterChatByLanguage(szLanguageSender) && strcmp(szNameSender, GetName()))
		return;	

	char chatBuf[CHAT_MAX_LEN + 1];
	int len = snprintf(chatBuf, sizeof(chatBuf), szTextLine);

	struct packet_chat pack_chat;
	pack_chat.header    = HEADER_GC_CHAT;
	pack_chat.size      = sizeof(struct packet_chat) + len;
	pack_chat.type      = CHAT_TYPE_SHOUT;
	pack_chat.id        = 0;
	pack_chat.bEmpire   = bEmpireSender;
	strlcpy(pack_chat.szLanguage, szLanguageSender, sizeof(pack_chat.szLanguage));

	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(struct packet_chat));
	buf.write(chatBuf, len);
	d->Packet(buf.read_peek(), buf.size());	
}

bool CHARACTER::GetOptionFilterChatByLanguage(const char * szLanguageSender)
{
	if (!strcmp(GetLanguage(), szLanguageSender))
		return true;
	
	if (GetShoutLanguageByID(LANGUAGE_MAX_NUM) == LANGUAGE_SELECTED)
		return true;

	for (int32_t bLanguageID = 0; bLanguageID < LANGUAGE_MAX_NUM; ++bLanguageID)
	{
		if (GetShoutLanguageByID(bLanguageID) == LANGUAGE_SELECTED)
		{
			const char * szLanguage = CLanguageManager::instance().GetLanguageStringByIndex(bLanguageID);
			if (!strcmp(szLanguage, szLanguageSender))
				return true;
		}
	}

	return false;
}

void CHARACTER::SendShoutFilterListPacket()
{
	if (GetDesc())
	{
		BYTE bShoutFilterList[LANGUAGE_MAX_NUM + 1];
		for (int32_t bLanguageID = 0; bLanguageID < LANGUAGE_MAX_NUM + 1; bLanguageID++)
			bShoutFilterList[bLanguageID] = GetShoutLanguageByID(bLanguageID);

		TPacketGCLoadShoutFilterList pack;
		pack.header = HEADER_GC_SHOUT_FILTER_LIST;
		thecore_memcpy(&pack.bFilterList, bShoutFilterList, sizeof(pack.bFilterList));
		GetDesc()->Packet(&pack, sizeof(TPacketGCLoadShoutFilterList));
	}
}

/*******************************************************************\
| [PUBLIC] Announcement GM Features
\*******************************************************************/

struct SAnnouncementByLanguagePacket
{
	int iType;
	int iMapIndex;
	const char * szText;
	const char * szLanguage;

	SAnnouncementByLanguagePacket(int type, int mapIndex, const char * text, const char * lang) : iType(type), iMapIndex(mapIndex), szText(text), szLanguage(lang)
	{
	}

	void operator() (LPDESC d)
	{
		LPCHARACTER ch = d->GetCharacter();
		if (!ch)
			return;
		
		if (!strcmp(ch->GetLanguage(), szLanguage) || !strcmp(g_stGlobalLanguage, szLanguage) || ch->IsGM())
		{
			switch (iType)
			{
				case ANNOUNCEMENT_TYPE_NOTICE:
					ch->ChatPacket(CHAT_TYPE_NOTICE, szText);
					break;

				case ANNOUNCEMENT_TYPE_BIG_NOTICE:
					ch->ChatPacket(CHAT_TYPE_BIG_NOTICE, szText);
					break;

				case ANNOUNCEMENT_TYPE_MAP_NOTICE:
				{
					if (ch->GetMapIndex() == iMapIndex)
						ch->ChatPacket(CHAT_TYPE_BIG_NOTICE, szText);
				}
				break;
					
				case ANNOUNCEMENT_TYPE_WHISPER:
				{
					if (ch->GetDesc())
					{
						char buf[CHAT_MAX_LEN + 1];
						int len = snprintf(buf, sizeof(buf), szText);
						
						TPacketGCWhisper pack;
						pack.bHeader = HEADER_GC_WHISPER;
						pack.wSize = sizeof(TPacketGCWhisper) + len;
						pack.bType = WHISPER_TYPE_GM;
						strlcpy(pack.szNameFrom, g_stServerNameWhisper, sizeof(pack.szNameFrom));

						ch->GetDesc()->BufferedPacket(&pack, sizeof(pack));
						ch->GetDesc()->Packet(buf, len);
					}
				}
				break;				
				default:
					break;
			}
		}
	}
};

void CLanguageManager::AnnouncementP2P(const char * c_pData)
{
	TPacketGGAnnouncement * p = (TPacketGGAnnouncement *) c_pData;
	CLanguageManager::instance().SendAnnouncement(p->iType, p->iMapIndex, p->szText, p->szLanguage);
}

void CLanguageManager::SendAnnouncement(BYTE iType, int32_t iMapIndex, const char * szText, const char * szLanguage)
{
	const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();
	std::for_each(c_ref_set.begin(), c_ref_set.end(), 
		SAnnouncementByLanguagePacket(iType, iMapIndex, szText, szLanguage));
}

void CLanguageManager::ShoutFilter(LPCHARACTER ch, const char* c_pData)
{
	if (!ch)
		return;

	const TPacketCGShoutFilter* p = reinterpret_cast<const TPacketCGShoutFilter*>(c_pData);
	if (!p)
		return;

	switch (p->bSubHeader)
	{
		case LANGUAGE_SUB_HEADER_FILTER_CHAT:
		{
			if (p->bFilterList[LANGUAGE_MAX_NUM] == LANGUAGE_SELECTED)
			{
				ch->SetShoutLanguageGlobal();
				return;
			}

			for (int32_t bLanguageID = 0; bLanguageID < LANGUAGE_MAX_NUM + 1; bLanguageID++)
			{
				if (p->bFilterList[bLanguageID] == LANGUAGE_SELECTED)
					ch->SetShoutLanguageByID(bLanguageID, LANGUAGE_SELECTED);
				else 
					ch->SetShoutLanguageByID(bLanguageID, LANGUAGE_UNSELECTED);
			}
			
			ch->SendShoutFilterListPacket();
		}
		return;
		
		case LANGUAGE_SUB_HEADER_GM_ANNOUNCEMENT:
		{
			if (!ch->IsGM())
				return;
			
			if (p->iType > ANNOUNCEMENT_TYPE_MAX || !strlen(p->szText))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "Invalid size: p->iType(%d), p->szText(%s)", p->iType, p->szText);
				return;
			}

			const char * szLanguageString = CLanguageManager::instance().GetLanguageStringByIndex(p->bFilterList[0]);
			
			TPacketGGAnnouncement pack;
			pack.bHeader = HEADER_GG_ANNOUNCEMENT;
			pack.iType = p->iType;
			strlcpy(pack.szText, p->szText, sizeof(pack.szText));
			strlcpy(pack.szLanguage, szLanguageString, sizeof(pack.szLanguage));

			P2P_MANAGER::instance().Send(&pack, sizeof(TPacketGGAnnouncement));
			CLanguageManager::instance().SendAnnouncement(p->iType, ch->GetMapIndex(), p->szText, szLanguageString);
		}
		return;
		
		default:
			return;
	}
}

int check_name_independent(const char * str)
{
	if (CBanwordManager::instance().CheckString(str, strlen(str)))
		return 0;

	char szTmp[256];
	str_lower(str, szTmp, sizeof(szTmp));

	if (CMobManager::instance().Get(szTmp, false))
		return 0;

	return 1;
}

int check_name_alphabet(const char * str)
{
	const char*	tmp;

	if (!str || !*str)
		return 0;

	if (strlen(str) < 2)
		return 0;

	for (tmp = str; *tmp; ++tmp)
	{
		if (isdigit(*tmp) || isalpha(*tmp))
			continue;
		else
			return 0;
	}

	return check_name_independent(str);
}

void LocaleService_TransferDefaultSetting()
{
	if (!check_name)
		check_name = check_name_alphabet;

	if (!exp_table)
		exp_table = exp_table_common;

	if (!CTableBySkill::instance().Check())
		exit(1);

	if (!aiPercentByDeltaLevForBoss)
		aiPercentByDeltaLevForBoss = aiPercentByDeltaLevForBoss_euckr;

	if (!aiPercentByDeltaLev)
		aiPercentByDeltaLev = aiPercentByDeltaLev_euckr;

	if (!aiChainLightningCountBySkillLevel)
		aiChainLightningCountBySkillLevel = aiChainLightningCountBySkillLevel_euckr;
	
	g_stQuestDir = g_stLocaleQuestPath;
	g_setQuestObjectDir.clear();
	g_setQuestObjectDir.insert(g_stLocaleQuestObjectDirPath.c_str());
}

const std::string& LocaleService_GetBasePath()
{
	return g_stServiceBasePath;
}

const std::string& LocaleService_GetMapPath()
{
	return g_stServiceMapPath;
}

const std::string& LocaleService_GetQuestPath()
{
	return g_stQuestDir;
}

eLocalization LC_GetLocalType() 
{
	return g_eLocalType;
}

bool LC_IsLocale( const eLocalization t ) 
{
	return LC_GetLocalType() == t ? true : false;
}

bool LC_IsYMIR()		{ return LC_GetLocalType() == LC_YMIR ? true : false; }
bool LC_IsJapan()		{ return LC_GetLocalType() == LC_JAPAN ? true : false; }
bool LC_IsEnglish()		{ return LC_GetLocalType() == LC_ENGLISH ? true : false; } 
bool LC_IsHongKong()	{ return LC_GetLocalType() == LC_HONGKONG ? true : false; }
bool LC_IsNewCIBN()		{ return LC_GetLocalType() == LC_NEWCIBN ? true : false; }
bool LC_IsGermany()		{ return LC_GetLocalType() == LC_GERMANY ? true : false; }
bool LC_IsKorea()		{ return LC_GetLocalType() == LC_KOREA ? true : false; }
bool LC_IsCanada()		{ return LC_GetLocalType() == LC_CANADA ? false : false; }
bool LC_IsBrazil()		{ return LC_GetLocalType() == LC_BRAZIL ? true : false; }
bool LC_IsSingapore()	{ return LC_GetLocalType() == LC_SINGAPORE ? true : false; }
bool LC_IsVietnam()		{ return LC_GetLocalType() == LC_VIETNAM ? true : false; }
bool LC_IsThailand()	{ return LC_GetLocalType() == LC_THAILAND ? true : false; }
bool LC_IsWE_Korea()	{ return LC_GetLocalType() == LC_WE_KOREA ? true : false; }
bool LC_IsTaiwan()	{ return LC_GetLocalType() == LC_TAIWAN ? true : false; }

bool LC_IsWorldEdition()
{
	return LC_IsWE_Korea() || LC_IsEurope();
}

bool LC_IsEurope()
{
	eLocalization iLocalization = LC_GetLocalType();

	switch ((int) iLocalization)
	{
		case LC_GERMANY:
		case LC_FRANCE:
		case LC_ITALY:
		case LC_TURKEY:
		case LC_POLAND:
		case LC_UK:
		case LC_SPAIN:
		case LC_PORTUGAL:
		case LC_GREEK:
		case LC_RUSSIA:
		case LC_DENMARK:
		case LC_BULGARIA:
		case LC_CROATIA:
		case LC_MEXICO:
		case LC_ARABIA:
		case LC_CZECH:
		case LC_ROMANIA:
		case LC_HUNGARY:
		case LC_NETHERLANDS:
		case LC_USA:
		case LC_WE_KOREA:
		case LC_TAIWAN:
		case LC_JAPAN:
		case LC_NEWCIBN:
		case LC_CANADA:
			return true;
	}

	return false;
}
