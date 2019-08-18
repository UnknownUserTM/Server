#include "stdafx.h"

#ifdef __GUILD_SAFEBOX__
#include "GuildSafeboxManager.h"
#include "DBManager.h"
#include "QID.h"
#include "Peer.h"
#include "ClientManager.h"
#include "Cache.h"

extern BOOL g_test_server;

/////////////////////////////////////////
// CGuildSafebox
/////////////////////////////////////////
CGuildSafebox::CGuildSafebox(DWORD dwGuildID)
{
	m_dwGuildID = dwGuildID;
	m_bSize = 0;
	m_szPassword[0] = '\0';
	m_dwGold = 0;

	memset(m_pItems, 0, sizeof(m_pItems));
	memset(m_bItemGrid, 0, sizeof(m_bItemGrid));
}
CGuildSafebox::~CGuildSafebox()
{
	for (int i = 0; i < GUILD_SAFEBOX_MAX_NUM; ++i)
	{
		if (m_pItems[i])
		{
			delete m_pItems[i];
			m_pItems[i] = NULL;
		}
	}
}

void CGuildSafebox::ChangeSize(BYTE bNewSize, CPeer* pkPeer)
{
	SetSize(bNewSize);

	ForwardPacket(HEADER_DG_GUILD_SAFEBOX_SIZE, &bNewSize, sizeof(BYTE), pkPeer);

	CGuildSafeboxManager::Instance().SaveSafebox(this);
}

void CGuildSafebox::ChangeGold(int iChange)
{
	SetGold(GetGold() + iChange);

	DWORD dwGold = GetGold();
	ForwardPacket(HEADER_DG_GUILD_SAFEBOX_GOLD, &dwGold, sizeof(DWORD));

	if (iChange)
		CGuildSafeboxManager::Instance().SaveSafebox(this);
}

void CGuildSafebox::LoadItems(SQLMsg* pMsg)
{
	int iNumRows = pMsg->Get()->uiNumRows;

	if (iNumRows)
	{
		TPlayerItem item;
		memset(&item, 0, sizeof(item));

		item.owner = GetGuildID();
		item.window = GUILD_SAFEBOX;

		for (int i = 0; i < iNumRows; ++i)
		{
			MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
			int col = 0;
			
			str_to_number(item.id, row[col++]);
			str_to_number(item.pos, row[col++]);
			str_to_number(item.count, row[col++]);
			str_to_number(item.vnum, row[col++]);
			str_to_number(item.is_gm_owner, row[col++]);
#ifdef __EXTRA_ITEM_UPGRADE__
			str_to_number(item.extra_upgrade_level, row[col++]);
#endif
#ifdef __JEWELRY_ADDON__
			str_to_number(item.jewelry_addon, row[col++]);
			str_to_number(item.jewelry_addon_time_left, row[col++]);
#endif
			
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				str_to_number(item.alSockets[i], row[col++]);

			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			{
				str_to_number(item.aAttr[i].bType, row[col++]);
				str_to_number(item.aAttr[i].sValue, row[col++]);
			}

			if (item.pos >= GUILD_SAFEBOX_MAX_NUM)
			{
				sys_err("cannot load item by ID %u for guild %u (cell out of range)", item.id, GetGuildID());
				continue;
			}

			const TItemTable* pProto = CClientManager::Instance().GetItemTable(item.vnum);
			if (!pProto)
			{
				sys_err("cannot load item by ID %u for guild %u (wrong vnum %u)", item.id, GetGuildID(), item.vnum);
				continue;
			}

			m_pItems[item.pos] = new TPlayerItem(item);
			for (int i = 0; i < pProto->bSize; ++i)
				m_bItemGrid[item.pos + 5 * i] = true;

			if (g_test_server)
				sys_log(0, "CGuildSafebox::LoadItems %u: LoadItem %u vnum %u %s %dx",
					GetGuildID(), item.id, item.vnum, pProto->szLocaleName, item.count);
		}
	}
}

void CGuildSafebox::DeleteItems()
{
	char szDeleteQuery[256];
	snprintf(szDeleteQuery, sizeof(szDeleteQuery), "DELETE FROM item WHERE window = %u AND owner_id = %u", GUILD_SAFEBOX, GetGuildID());
	CDBManager::Instance().AsyncQuery(szDeleteQuery);

	for (int i = 0; i < GUILD_SAFEBOX_MAX_NUM; ++i)
	{
		if (GetItem(i))
		{
			CGuildSafeboxManager::Instance().FlushItem(m_pItems[i], false);
			delete m_pItems[i];
			m_pItems[i] = NULL;
		}
	}
	memset(m_bItemGrid, 0, sizeof(m_bItemGrid));
}

bool CGuildSafebox::IsValidCell(BYTE bCell, BYTE bSize) const
{
	// pos out of max window size
	if (bCell >= GUILD_SAFEBOX_MAX_NUM - GUILD_SAFEBOX_ITEM_WIDTH * (bSize - 1))
		return false;

	// all good
	return true;
}

bool CGuildSafebox::IsFree(BYTE bPos, BYTE bSize) const
{
	if (!IsValidCell(bPos, bSize))
	{
		sys_err("IsFree: invalid cell %u", bPos);
		return false;
	}

	// item cannot split over two pages
	const BYTE bPageSize = GUILD_SAFEBOX_ITEM_WIDTH * GUILD_SAFEBOX_ITEM_HEIGHT;
	if (bPos / bPageSize != (bPos + GUILD_SAFEBOX_ITEM_WIDTH * (bSize - 1)) / bPageSize)
	{
		sys_err("IsFree: different pages");
		return false;
	}

	// not enough pages owned
	if (bPos / bPageSize >= m_bSize)
	{
		sys_err("not enough pages (page %u requested, owned %u)", bPos / bPageSize + 1, m_bSize);
		return false;
	}

	// there is already an item on this slot
	for (int i = 0; i < bSize; ++i)
	{
		if (m_bItemGrid[bPos + i * GUILD_SAFEBOX_ITEM_WIDTH])
			return false;
	}

	// all good
	return true;
}

void CGuildSafebox::RequestAddItem(CPeer* pkPeer, DWORD dwHandle, const TPlayerItem* pItem)
{
	const TItemTable* pProto = CClientManager::instance().GetItemTable(pItem->vnum);
	if (!pProto)
	{
		sys_err("cannot get proto %u", pItem->vnum);
		CDBManager::instance().AsyncQuery("DELETE FROM item WHERE id = %u", pItem->id);
		GiveItemToPlayer(pkPeer, dwHandle, pItem);
		return;
	}

	if (!IsFree(pItem->pos, pProto->bSize))
	{
		sys_err("no free space at pos %u size %u", pItem->pos, pProto->bSize);
		CDBManager::instance().AsyncQuery("DELETE FROM item WHERE id = %u", pItem->id);
		GiveItemToPlayer(pkPeer, dwHandle, pItem);
		return;
	}

	sys_log(0, "CGuildSafebox::RequestAddItem: item %u %-20s to %u (guild %u)",
		pItem->id, pProto->szLocaleName, pItem->pos, GetGuildID());

	m_pItems[pItem->pos] = new TPlayerItem(*pItem);
	m_pItems[pItem->pos]->owner = GetGuildID();
	m_pItems[pItem->pos]->window = GUILD_SAFEBOX;
	for (int i = 0; i < pProto->bSize; ++i)
		m_bItemGrid[pItem->pos + i * GUILD_SAFEBOX_ITEM_WIDTH] = true;

	CGuildSafeboxManager::Instance().SaveItem(m_pItems[pItem->pos]);

	SendItemPacket(pItem->pos);
}

void CGuildSafebox::RequestMoveItem(BYTE bSrcSlot, BYTE bTargetSlot)
{
	if (!IsValidCell(bSrcSlot))
		return;

	TPlayerItem* pItem = m_pItems[bSrcSlot];
	if (!pItem)
		return;

	const TItemTable* pProto = CClientManager::instance().GetItemTable(pItem->vnum);
	if (!pProto)
		return;

	if (!IsFree(bTargetSlot, pProto->bSize))
		return;

	sys_log(0, "CGuildSafebox::RequestMoveItem: item %u %-20s from %u to %u (guild %u)",
		pItem->id, pProto->szLocaleName, bSrcSlot, bTargetSlot, GetGuildID());

	m_pItems[bSrcSlot] = NULL;
	for (int i = 0; i < pProto->bSize; ++i)
		m_bItemGrid[bSrcSlot + i * GUILD_SAFEBOX_ITEM_WIDTH] = false;

	pItem->pos = bTargetSlot;
	m_pItems[bTargetSlot] = pItem;
	for (int i = 0; i < pProto->bSize; ++i)
		m_bItemGrid[bTargetSlot + i * GUILD_SAFEBOX_ITEM_WIDTH] = true;

	CGuildSafeboxManager::Instance().SaveItem(pItem);

	SendItemPacket(bSrcSlot);
	SendItemPacket(bTargetSlot);
}

void CGuildSafebox::RequestTakeItem(CPeer* pkPeer, DWORD dwHandle, BYTE bSlot, BYTE bTargetSlot)
{
	if (!IsValidCell(bSlot))
		return;

	TPlayerItem* pItem = m_pItems[bSlot];
	if (!pItem)
		return;

	const TItemTable* pProto = CClientManager::Instance().GetItemTable(pItem->vnum);
	if (!pProto)
		return;

	m_pItems[bSlot] = NULL;
	for (int i = 0; i < pProto->bSize; ++i)
		m_bItemGrid[bSlot + i * GUILD_SAFEBOX_ITEM_WIDTH] = false;

	SendItemPacket(bSlot);

	pItem->window = INVENTORY;
	pItem->pos = bTargetSlot;
	GiveItemToPlayer(pkPeer, dwHandle, pItem);

	CGuildSafeboxManager::Instance().FlushItem(pItem, false);
	delete pItem;
}

void CGuildSafebox::GiveItemToPlayer(CPeer* pkPeer, DWORD dwHandle, const TPlayerItem* pItem)
{
	sys_log(0, "CGuildSafebox::GiveItemToPlayer: id %u vnum %u socket %u %u %u", pItem->id, pItem->vnum,
		pItem->alSockets[0], pItem->alSockets[1], pItem->alSockets[2]);

	pkPeer->EncodeHeader(HEADER_DG_GUILD_SAFEBOX_GIVE, dwHandle, sizeof(TPlayerItem));
	pkPeer->Encode(pItem, sizeof(TPlayerItem));
}

void CGuildSafebox::SendItemPacket(BYTE bCell)
{
	if (const TPlayerItem* pItem = m_pItems[bCell])
		ForwardPacket(HEADER_DG_GUILD_SAFEBOX_SET, pItem, sizeof(TPlayerItem));
	else
		ForwardPacket(HEADER_DG_GUILD_SAFEBOX_DEL, &bCell, sizeof(BYTE));
}

void CGuildSafebox::LoadItems(CPeer* pkPeer, DWORD dwHandle)
{
	if (m_set_ForwardPeer.find(pkPeer) != m_set_ForwardPeer.end())
	{
		sys_err("already loaded items for channel %u %s", pkPeer->GetChannel(), pkPeer->GetHost());

		pkPeer->EncodeHeader(HEADER_DG_GUILD_SAFEBOX_LOAD, dwHandle, sizeof(DWORD) + sizeof(DWORD) + sizeof(WORD));
		pkPeer->EncodeDWORD(GetGuildID());
		pkPeer->EncodeDWORD(GetGold());
		pkPeer->EncodeWORD(0);

		return;
	}

	std::vector<TPlayerItem> vecItems;
	for (int i = 0; i < GUILD_SAFEBOX_MAX_NUM; ++i)
	{
		if (const TPlayerItem* pkItem = GetItem(i))
			vecItems.push_back(*pkItem);
	}

	sys_log(0, "CGuildSafebox::LoadItems [%u] for channel %u %s", vecItems.size(), pkPeer->GetChannel(), pkPeer->GetHost());

	pkPeer->EncodeHeader(HEADER_DG_GUILD_SAFEBOX_LOAD, dwHandle, sizeof(DWORD) + sizeof(DWORD) + sizeof(WORD) + sizeof(TPlayerItem) * vecItems.size());
	pkPeer->EncodeDWORD(GetGuildID());
	pkPeer->EncodeDWORD(GetGold());
	pkPeer->EncodeWORD(vecItems.size());
	pkPeer->Encode(&vecItems[0], sizeof(TPlayerItem) * vecItems.size());

	AddPeer(pkPeer);
}

void CGuildSafebox::AddPeer(CPeer* pkPeer)
{
	if (m_set_ForwardPeer.find(pkPeer) == m_set_ForwardPeer.end())
		m_set_ForwardPeer.insert(pkPeer);
}

void CGuildSafebox::ForwardPacket(BYTE bHeader, const void* c_pData, DWORD dwSize, CPeer* pkExceptPeer)
{
	for (itertype(m_set_ForwardPeer) it = m_set_ForwardPeer.begin(); it != m_set_ForwardPeer.end(); ++it)
	{
		if (*it == pkExceptPeer)
			continue;

		(*it)->EncodeHeader(bHeader, 0, sizeof(DWORD) + dwSize);
		(*it)->EncodeDWORD(GetGuildID());
		(*it)->Encode(c_pData, dwSize);
	}
}

/////////////////////////////////////////
// CGuildSafeboxManager
/////////////////////////////////////////
CGuildSafeboxManager::CGuildSafeboxManager()
{
	m_dwLastFlushItemTime = time(0);
	m_dwLastFlushSafeboxTime = time(0);
}
CGuildSafeboxManager::~CGuildSafeboxManager()
{
	for (itertype(m_map_GuildSafebox) it = m_map_GuildSafebox.begin(); it != m_map_GuildSafebox.end(); ++it)
		delete (*it).second;
	m_map_GuildSafebox.clear();
	m_map_DelayedItemSave.clear();
}

void CGuildSafeboxManager::Initialize()
{
	std::unique_ptr<SQLMsg> pMsg(CDBManager::Instance().DirectQuery("SELECT guild_id, size, password, gold FROM guild_safebox"));
	
	uint32_t uiNumRows = pMsg->Get()->uiNumRows;
	if (uiNumRows)
	{
		for (int i = 0; i < uiNumRows; ++i)
		{
			MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
			int col = 0;

			// id
			DWORD dwGuildID;
			str_to_number(dwGuildID, row[col++]);
			// size
			BYTE bSize;
			str_to_number(bSize, row[col++]);
			// passwd
			const char* szPassword = row[col++];
			// gold
			DWORD dwGold;
			str_to_number(dwGold, row[col++]);

			// create class
			CGuildSafebox* pGuildSafebox = new CGuildSafebox(dwGuildID);
			pGuildSafebox->SetSize(bSize);
			pGuildSafebox->SetPassword(szPassword);
			pGuildSafebox->SetGold(dwGold);
			m_map_GuildSafebox.insert(std::pair<DWORD, CGuildSafebox*>(dwGuildID, pGuildSafebox));

			// load items
			char szQueryExtra[256];
			int len = snprintf(szQueryExtra, sizeof(szQueryExtra), "");
#ifdef __EXTRA_ITEM_UPGRADE__
			len += snprintf(szQueryExtra + len, sizeof(szQueryExtra) - len, ", extra_upgrade_level");
#endif
#ifdef __JEWELRY_ADDON__
			len += snprintf(szQueryExtra + len, sizeof(szQueryExtra) - len, ", jewelry_addon, jewelry_addon_time_left");
#endif

			char szItemQuery[512];
			snprintf(szItemQuery, sizeof(szItemQuery), "SELECT id, pos, count, vnum, is_gm_owner%s, socket0, socket1, socket2, "
				"attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, attrtype4, attrvalue4, "
				"attrtype5, attrvalue5, attrtype6, attrvalue6 FROM item WHERE window = %u AND owner_id = %u",
				szQueryExtra, GUILD_SAFEBOX, dwGuildID);
			CDBManager::Instance().ReturnQuery(szItemQuery, QID_GUILD_SAFEBOX_ITEM_LOAD, 0, new GuildHandleInfo(dwGuildID));

			// log
			sys_log(0, "CGuildSafeboxManager::Initialize: Load Guildsafebox: %u (size %d)", dwGuildID, bSize);
		}
	}
}

void CGuildSafeboxManager::Destroy()
{
	FlushItems(true);
	FlushSafeboxes(true);
}

CGuildSafebox* CGuildSafeboxManager::GetSafebox(DWORD dwGuildID)
{
	itertype(m_map_GuildSafebox) it = m_map_GuildSafebox.find(dwGuildID);
	if (it == m_map_GuildSafebox.end())
		return NULL;
	return it->second;
}

void CGuildSafeboxManager::DestroySafebox(DWORD dwGuildID)
{
	CGuildSafebox* pSafebox;
	if (!(pSafebox = GetSafebox(dwGuildID)))
		return;

	pSafebox->DeleteItems();

	char szDeleteQuery[256];
	snprintf(szDeleteQuery, sizeof(szDeleteQuery), "DELETE FROM guild_safebox WHERE guild_id = %u", dwGuildID);
	CDBManager::Instance().AsyncQuery(szDeleteQuery);

	delete pSafebox;
	m_map_GuildSafebox.erase(dwGuildID);
}

void CGuildSafeboxManager::SaveItem(TPlayerItem* pItem)
{
	if (m_map_DelayedItemSave.find(pItem) == m_map_DelayedItemSave.end())
		m_map_DelayedItemSave.insert(pItem);
}

void CGuildSafeboxManager::FlushItem(TPlayerItem* pItem, bool bSave)
{
	if (m_map_DelayedItemSave.find(pItem) != m_map_DelayedItemSave.end())
	{
		if (bSave)
			SaveSingleItem(pItem);
		m_map_DelayedItemSave.erase(pItem);
	}
}

void CGuildSafeboxManager::FlushItems(bool bForce)
{
	if (!bForce && time(0) - m_dwLastFlushItemTime <= 5 * 60)
		return;
	m_dwLastFlushItemTime = time(0);

	sys_log(0, "CGuildSafeboxManager::FlushItems: flush %u", m_map_DelayedItemSave.size());

	for (itertype(m_map_DelayedItemSave) it = m_map_DelayedItemSave.begin(); it != m_map_DelayedItemSave.end(); ++it)
		SaveSingleItem(*it);
	m_map_DelayedItemSave.clear();
}

void CGuildSafeboxManager::SaveSingleItem(TPlayerItem* pItem)
{
	char szSaveQuery[QUERY_MAX_LEN];

	if (!pItem->owner)
	{
		snprintf(szSaveQuery, sizeof(szSaveQuery), "DELETE FROM player.item WHERE id = %u AND window = %u", pItem->id, GUILD_SAFEBOX);
		delete pItem;
	}
	else
	{
		char szQueryExtra[256];
		int len = snprintf(szQueryExtra, sizeof(szQueryExtra), "");
#ifdef __EXTRA_ITEM_UPGRADE__
		len += snprintf(szQueryExtra + len, sizeof(szQueryExtra) - len, ",extra_upgrade_level=%u", pItem->extra_upgrade_level);
#endif
#ifdef __JEWELRY_ADDON__
		len += snprintf(szQueryExtra + len, sizeof(szQueryExtra) - len, ",jewelry_addon=%u,jewelry_addon_time_left=%u",
			pItem->jewelry_addon, pItem->jewelry_addon_time_left);
#endif

		snprintf(szSaveQuery, sizeof(szSaveQuery), "REPLACE INTO player.item SET id=%u,owner_id=%u,window=%u,pos=%u,count=%u,vnum=%u,"
			"is_gm_owner=%u%s,socket0=%ld,socket1=%ld,socket2=%ld,attrtype0=%u,attrvalue0=%d,attrtype1=%u,attrvalue1=%d,attrtype2=%u,attrvalue2=%d,"
			"attrtype3=%u,attrvalue3=%d,attrtype4=%u,attrvalue4=%d,attrtype5=%u,attrvalue5=%d,attrtype6=%u,attrvalue6=%d",
			pItem->id, pItem->owner, pItem->window, pItem->pos, pItem->count, pItem->vnum, pItem->is_gm_owner, szQueryExtra,
			pItem->alSockets[0], pItem->alSockets[1], pItem->alSockets[2], pItem->aAttr[0].bType, pItem->aAttr[0].sValue,
			pItem->aAttr[1].bType, pItem->aAttr[1].sValue, pItem->aAttr[2].bType, pItem->aAttr[2].sValue, pItem->aAttr[3].bType,
			pItem->aAttr[3].sValue, pItem->aAttr[4].bType, pItem->aAttr[4].sValue, pItem->aAttr[5].bType, pItem->aAttr[5].sValue,
			pItem->aAttr[6].bType, pItem->aAttr[6].sValue);
	}

	if (g_test_server)
		sys_log(0, "CGuildSafeboxManager::SaveSingleItem: %s", szSaveQuery);

	CDBManager::Instance().AsyncQuery(szSaveQuery);
}

void CGuildSafeboxManager::SaveSafebox(CGuildSafebox* pSafebox)
{
	if (m_map_DelayedSafeboxSave.find(pSafebox) == m_map_DelayedSafeboxSave.end())
		m_map_DelayedSafeboxSave.insert(pSafebox);
}

void CGuildSafeboxManager::FlushSafebox(CGuildSafebox* pSafebox, bool bSave)
{
	if (m_map_DelayedSafeboxSave.find(pSafebox) != m_map_DelayedSafeboxSave.end())
	{
		if (bSave)
			SaveSingleSafebox(pSafebox);
		m_map_DelayedSafeboxSave.erase(pSafebox);
	}
}

void CGuildSafeboxManager::FlushSafeboxes(bool bForce)
{
	if (!bForce && time(0) - m_dwLastFlushSafeboxTime <= 5 * 60)
		return;
	m_dwLastFlushSafeboxTime = time(0);

	sys_log(0, "CGuildSafeboxManager::FlushSafeboxes: flush %u", m_map_DelayedSafeboxSave.size());

	for (itertype(m_map_DelayedSafeboxSave) it = m_map_DelayedSafeboxSave.begin(); it != m_map_DelayedSafeboxSave.end(); ++it)
		SaveSingleSafebox(*it);
	m_map_DelayedSafeboxSave.clear();
}

void CGuildSafeboxManager::SaveSingleSafebox(CGuildSafebox* pSafebox)
{
	char szSaveQuery[QUERY_MAX_LEN];

	snprintf(szSaveQuery, sizeof(szSaveQuery), "UPDATE player.guild_safebox SET size = %u, gold = %u WHERE guild_id = %u",
		pSafebox->GetSize(), pSafebox->GetGold(), pSafebox->GetGuildID());

	if (g_test_server)
		sys_log(0, "CGuildSafeboxManager::SaveSingleSafebox: %s", szSaveQuery);

	CDBManager::Instance().AsyncQuery(szSaveQuery);
}

void CGuildSafeboxManager::QueryResult(CPeer* pkPeer, SQLMsg* pMsg, int iQIDNum)
{
	std::unique_ptr<GuildHandleInfo> pInfo((GuildHandleInfo*)((CQueryInfo*)pMsg->pvUserData)->pvData);
	CGuildSafebox* pSafebox = GetSafebox(pInfo->dwGuildID);

	if (!pSafebox)
	{
		sys_err("safebox of guild %u does not exist anymore", pInfo->dwGuildID);
		return;
	}

	switch (iQIDNum)
	{
		case QID_GUILD_SAFEBOX_ITEM_LOAD:
			pSafebox->LoadItems(pMsg);
			break;

		default:
			sys_err("unkown qid %u", iQIDNum);
			break;
	}
}

void CGuildSafeboxManager::ProcessPacket(CPeer* pkPeer, DWORD dwHandle, BYTE bHeader, const void* c_pData)
{
	if (g_test_server)
		sys_log(0, "CGuildSafeboxManager::ProcessPacket: %u", bHeader);

	switch (bHeader)
	{
		case HEADER_GD_GUILD_SAFEBOX_ADD:
		{
			const TPlayerItem* p = (TPlayerItem*) c_pData;
			CClientManager::instance().EraseItemCache(p->id);

			CGuildSafebox* pSafebox = GetSafebox(p->owner);
			if (pSafebox)
			{
				pSafebox->RequestAddItem(pkPeer, dwHandle, p);
			}
			else
			{
				CDBManager::instance().AsyncQuery("DELETE FROM item WHERE id = %u", p->id);

				pkPeer->EncodeHeader(HEADER_DG_GUILD_SAFEBOX_GIVE, dwHandle, sizeof(TPlayerItem));
				pkPeer->Encode(p, sizeof(TPlayerItem));
			}
		}
			break;

		case HEADER_GD_GUILD_SAFEBOX_MOVE:
			{
				const TPacketGDGuildSafeboxMove* p = (TPacketGDGuildSafeboxMove*) c_pData;
				CGuildSafebox* pSafebox = GetSafebox(p->dwGuild);
				if (pSafebox)
				{
					pSafebox->RequestMoveItem(p->bSrcSlot, p->bTargetSlot);
				}
			}
				break;

		case HEADER_GD_GUILD_SAFEBOX_TAKE:
			{
				const TPacketGDGuildSafeboxTake* p = (TPacketGDGuildSafeboxTake*) c_pData;
				CGuildSafebox* pSafebox = GetSafebox(p->dwGuild);
				if (pSafebox)
				{
					pSafebox->RequestTakeItem(pkPeer, dwHandle, p->bSlot, p->bTargetSlot);
				}
			}
				break;

		case HEADER_GD_GUILD_SAFEBOX_GIVE_GOLD:
			{
				const TPacketGuildSafeboxGold* p = (TPacketGuildSafeboxGold*) c_pData;
				CGuildSafebox* pSafebox = GetSafebox(p->dwGuild);
				if (pSafebox)
				{
					pSafebox->ChangeGold(p->dwGold);
				}
			}
				break;

		case HEADER_GD_GUILD_SAFEBOX_GET_GOLD:
			{
				const TPacketGuildSafeboxGold* p = (TPacketGuildSafeboxGold*) c_pData;
				CGuildSafebox* pSafebox = GetSafebox(p->dwGuild);
				if (pSafebox)
				{
					pkPeer->EncodeHeader(HEADER_DG_GUILD_SAFEBOX_GOLD, dwHandle, sizeof(DWORD) + sizeof(DWORD));
					pkPeer->EncodeDWORD(p->dwGuild);

					if (pSafebox->GetGold() >= p->dwGold)
					{
						pkPeer->EncodeDWORD(p->dwGold);

						pSafebox->ChangeGold(-p->dwGold);
					}
					else
					{
						pkPeer->EncodeDWORD(0);

						sys_err("not enough gold in safebox");
					}
				}
			}
				break;

		case HEADER_GD_GUILD_SAFEBOX_CREATE:
			{
				const char* c_szData = (const char*) c_pData;
				DWORD dwGuildID = *(DWORD*) c_szData;
				c_szData += sizeof(DWORD);

				CGuildSafebox* pSafebox = GetSafebox(dwGuildID);
				if (!pSafebox)
				{
					sys_log(0, "CreateGuildSafebox: %u", dwGuildID);

					pSafebox = new CGuildSafebox(dwGuildID);
					pSafebox->SetSize(*(BYTE*) c_szData);
					m_map_GuildSafebox.insert(std::pair<DWORD, CGuildSafebox*>(dwGuildID, pSafebox));

					char szQuery[256];
					snprintf(szQuery, sizeof(szQuery), "INSERT INTO guild_safebox (guild_id, size, password, gold) VALUES "
						"(%u, %u, '', 0)", dwGuildID, *(BYTE*) c_szData);
					CDBManager::Instance().AsyncQuery(szQuery);

					struct {
						DWORD	dwGuildID;
						BYTE	bSize;
					} kCreateInfo;
					kCreateInfo.dwGuildID = dwGuildID;
					kCreateInfo.bSize = *(BYTE*) c_szData;

					CClientManager::Instance().ForwardPacket(HEADER_DG_GUILD_SAFEBOX_CREATE, &kCreateInfo, sizeof(kCreateInfo), 0, pkPeer);
				}
				else
					sys_err("safebox already created for guild %u", dwGuildID);
			}
				break;

		case HEADER_GD_GUILD_SAFEBOX_SIZE:
			{
				;
				const char* c_szData = (const char*) c_pData;
				DWORD dwGuildID = *(DWORD*) c_szData;
				c_szData += sizeof(DWORD);

				CGuildSafebox* pSafebox = GetSafebox(dwGuildID);
				if (pSafebox)
				{
					pSafebox->ChangeSize(*(BYTE*) c_szData, pkPeer);
				}
			}
				break;

		case HEADER_GD_GUILD_SAFEBOX_LOAD:
			{
				 DWORD dwGuildID = *(DWORD*) c_pData;
				 CGuildSafebox* pSafebox = GetSafebox(dwGuildID);
				 if (pSafebox)
				 {
					 pSafebox->LoadItems(pkPeer, dwHandle);
				 }
			}
				break;

		default:
			sys_err("unkown packet header %u", bHeader);
			break;
	}
}

void CGuildSafeboxManager::InitSafeboxCore(CPeer* pkPeer)
{
	for (itertype(m_map_GuildSafebox) it = m_map_GuildSafebox.begin(); it != m_map_GuildSafebox.end(); ++it)
	{
		TGuildSafeboxInitial init;
		init.dwGuildID = it->second->GetGuildID();
		init.bSize = it->second->GetSize();
		strlcpy(init.szPassword, it->second->GetPassword(), sizeof(init.szPassword));
		init.dwGold = it->second->GetGold();

		pkPeer->Encode(&init, sizeof(TGuildSafeboxInitial));
	}
}

void CGuildSafeboxManager::Update()
{
	FlushItems();
	FlushSafeboxes();
}
#endif
