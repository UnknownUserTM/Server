#ifndef __HEADER_GUILD_SAFEBOX__
#define __HEADER_GUILD_SAFEBOX__

#include "stdafx.h"

#ifdef __GUILD_SAFEBOX__
typedef struct _SQLMsg SQLMsg;

class CGuild;

class CGuildSafeBox
{
public:
	CGuildSafeBox(CGuild* pOwnerGuild);
	~CGuildSafeBox();

	BYTE	GetSize() const	{ return m_bSize; }
	DWORD	GetGold() const { return m_dwGold; }

	void	Load(BYTE bSize, const char* szPassword, DWORD dwGold);
	void	LoadItem(TPlayerItem* pItems, DWORD dwSize);

	bool	IsValidPosition(WORD wPos) const;
	bool	IsEmpty(WORD wPos, BYTE bSize) const;
	bool	CanAddItem(LPITEM pkItem, WORD wPos) const;

	bool	HasSafebox() const { return m_bSize > 0; }
	void	GiveSafebox(BYTE bSize = GUILD_SAFEBOX_DEFAULT_SIZE);
	void	ChangeSafeboxSize(BYTE bNewSize);
	void	OpenSafebox(LPCHARACTER ch);
	void	CheckInItem(LPCHARACTER ch, LPITEM pkItem, int iDestCell);
	void	CheckOutItem(LPCHARACTER ch, int iSourcePos, BYTE bTargetWindow, int iTargetPos);
	void	MoveItem(LPCHARACTER ch, int iSourcePos, int iTargetPos, BYTE bCount);
	void	GiveGold(LPCHARACTER ch, DWORD dwGold);
	void	TakeGold(LPCHARACTER ch, DWORD dwGold);
	void	CloseSafebox(LPCHARACTER ch);

	void	ViewerPacket(const void* c_pData, size_t size) { __ViewerPacket(c_pData, size); }

	void	DB_SetItem(TPlayerItem* pkItem);
	void	DB_DelItem(BYTE bSlot);
	void	DB_SetGold(DWORD dwGold);
	void	DB_SetOwned(BYTE bSize);

private:
	bool			__AddItem(LPITEM pkItem, WORD wPos, bool bIsLoading = false);
	void			__AddItem(const TPlayerItem& rItem, WORD wPos);
	void			__AddItem(DWORD dwID, WORD wPos, DWORD dwVnum, BYTE bCount, BYTE bSpecialLevel,
						const long* alSockets, const TPlayerItemAttribute* aAttr);

	void			__RemoveItem(LPITEM pkItem);
	TPlayerItem*	__GetItem(WORD wPos);

	bool	__IsViewer(LPCHARACTER ch);
	void	__AddViewer(LPCHARACTER ch);
	void	__RemoveViewer(LPCHARACTER ch);
	void	__ViewerPacket(const void* c_pData, size_t size);
	void	__ClientPacket(BYTE subheader, const void* c_pData, size_t size, LPCHARACTER ch = NULL);

private:
	CGuild*					m_pkOwnerGuild;

	BYTE					m_bSize;
	char					m_szPassword[GUILD_SAFEBOX_PASSWORD_MAX_LEN + 1];
	DWORD					m_dwGold;

	bool					m_bItemLoaded;
	TPlayerItem*			m_pkItems[GUILD_SAFEBOX_MAX_NUM];
	BYTE					m_bItemGrid[GUILD_SAFEBOX_MAX_NUM];

	std::set<LPCHARACTER>	m_set_pkCurrentViewer;
};
#endif

#endif
