#ifndef __HEADER_GUILD_SAFEBOX_MANAGER__
#define __HEADER_GUILD_SAFEBOX_MANAGER__

#include "stdafx.h"

#ifdef __GUILD_SAFEBOX__
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include <map>
#include "DBManager.h"

class CPeer;

typedef struct SGuildHandleInfo
{
	DWORD	dwGuildID;

	SGuildHandleInfo(DWORD dwGuildID) : dwGuildID(dwGuildID)
	{
	}
} GuildHandleInfo;

class CGuildSafebox
{
public:
	CGuildSafebox(DWORD dwGuildID);
	~CGuildSafebox();

	DWORD	GetGuildID() const								{ return m_dwGuildID; }

	void	SetSize(BYTE bSize)								{ m_bSize = bSize; }
	void	ChangeSize(BYTE bNewSize, CPeer* pkPeer = NULL);
	BYTE	GetSize() const									{ return m_bSize; }

	void	SetPassword(const char* szPassword)				{ strlcpy(m_szPassword, szPassword, sizeof(m_szPassword)); }
	const char* GetPassword()								{ return m_szPassword; }
	bool	CheckPassword(const char* szPassword) const		{ return !strcmp(m_szPassword, szPassword); }

	void	SetGold(DWORD dwGold)							{ m_dwGold = dwGold; }
	void	ChangeGold(int iChange);
	DWORD	GetGold() const									{ return m_dwGold; }

	void	LoadItems(SQLMsg* pMsg);
	void	DeleteItems();

	bool	IsValidCell(BYTE bCell, BYTE bSize = 1) const;
	bool	IsFree(BYTE bPos, BYTE bSize) const;

	void	RequestAddItem(CPeer* pkPeer, DWORD dwHandle, const TPlayerItem* pItem);
	void	RequestMoveItem(BYTE bSrcSlot, BYTE bTargetSlot);
	void	RequestTakeItem(CPeer* pkPeer, DWORD dwHandle, BYTE bSlot, BYTE bTargetSlot);

	void	GiveItemToPlayer(CPeer* pkPeer, DWORD dwHandle, const TPlayerItem* pItem);
	void	SendItemPacket(BYTE bCell);

	void	LoadItems(CPeer* pkPeer, DWORD dwHandle = 0);

	const TPlayerItem*	GetItem(BYTE bCell) const { return m_pItems[bCell]; }

	void	AddPeer(CPeer* pkPeer);
	void	ForwardPacket(BYTE bHeader, const void* c_pData, DWORD dwSize, CPeer* pkExceptPeer = NULL);

private:
	DWORD			m_dwGuildID;
	BYTE			m_bSize;
	char			m_szPassword[GUILD_SAFEBOX_PASSWORD_MAX_LEN + 1];
	DWORD			m_dwGold;

	TPlayerItem*	m_pItems[GUILD_SAFEBOX_MAX_NUM];
	bool			m_bItemGrid[GUILD_SAFEBOX_MAX_NUM];

	boost::unordered_set<CPeer*>	m_set_ForwardPeer;
};

class CGuildSafeboxManager : public singleton<CGuildSafeboxManager>
{
public:
	CGuildSafeboxManager();
	~CGuildSafeboxManager();
	void Initialize();
	void Destroy();

	CGuildSafebox*	GetSafebox(DWORD dwGuildID);
	void			DestroySafebox(DWORD dwGuildID);

	void			SaveItem(TPlayerItem* pItem);
	void			FlushItem(TPlayerItem* pItem, bool bSave = true);
	void			FlushItems(bool bForce = false);
	void			SaveSingleItem(TPlayerItem* pItem);

	void			SaveSafebox(CGuildSafebox* pSafebox);
	void			FlushSafebox(CGuildSafebox* pSafebox, bool bSave = true);
	void			FlushSafeboxes(bool bForce = false);
	void			SaveSingleSafebox(CGuildSafebox* pSafebox);

	void			QueryResult(CPeer* pkPeer, SQLMsg* pMsg, int iQIDNum);
	void			ProcessPacket(CPeer* pkPeer, DWORD dwHandle, BYTE bHeader, const void* c_pData);

	void			InitSafeboxCore(CPeer* pkPeer);
	WORD			GetSafeboxCount() const { return m_map_GuildSafebox.size(); }

	void			Update();

private:
	std::map<DWORD, CGuildSafebox*>			m_map_GuildSafebox;
	boost::unordered_set<TPlayerItem*>		m_map_DelayedItemSave;
	boost::unordered_set<CGuildSafebox*>	m_map_DelayedSafeboxSave;

	DWORD									m_dwLastFlushItemTime;
	DWORD									m_dwLastFlushSafeboxTime;
};

#endif // __GUILD_SAFEBOX__
#endif // __HEADER_GUILD_SAFEBOX_MANAGER__
