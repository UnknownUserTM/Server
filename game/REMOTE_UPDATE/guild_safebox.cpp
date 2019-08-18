#include "stdafx.h"

#ifdef __GUILD_SAFEBOX__
#include "../common/tables.h"
#include "guild_safebox.h"
#include "guild.h"
#include "db.h"
#include "desc_client.h"
#include "item.h"
#include "char.h"
#include "item_manager.h"
#include "desc.h"
#include "config.h"
#include "buffer_manager.h"

/********************************\
** PUBLIC LOADING
\********************************/

CGuildSafeBox::CGuildSafeBox(CGuild* pOwnerGuild) : m_pkOwnerGuild(pOwnerGuild), m_bSize(0), m_dwGold(0), m_bItemLoaded(false)
{
	memset(m_szPassword, 0, sizeof(m_szPassword));
	memset(m_pkItems, 0, sizeof(m_pkItems));
	memset(m_bItemGrid, 0, sizeof(m_bItemGrid));
}

CGuildSafeBox::~CGuildSafeBox()
{
	for (int i = 0; i < GUILD_SAFEBOX_MAX_NUM; ++i)
	{
		if (m_pkItems[i])
		{
			delete m_pkItems[i];
			m_pkItems[i] = NULL;
		}
	}
}

void CGuildSafeBox::Load(BYTE bSize, const char* szPassword, DWORD dwGold)
{
	sys_log(0, "LoadGuildSafeBox: %u", m_pkOwnerGuild->GetID());

	m_bSize = bSize;
	strlcpy(m_szPassword, szPassword, sizeof(m_szPassword));
	m_dwGold = dwGold;
}

void CGuildSafeBox::LoadItem(TPlayerItem* pItems, DWORD dwSize)
{
	if (m_bItemLoaded)
		return;

	memset(m_pkItems, 0, sizeof(m_pkItems));
	memset(m_bItemGrid, 0, sizeof(m_bItemGrid));
	for (int i = 0; i < dwSize; ++i)
	{
		const TItemTable* pProto = ITEM_MANAGER::instance().GetTable(pItems[i].vnum);
		if (!pProto)
		{
			sys_err("cannot load guild item %u vnum %u for guild %u", pItems[i].id, pItems[i].vnum, m_pkOwnerGuild->GetID());
			continue;
		}

		if (pItems[i].pos + GUILD_SAFEBOX_ITEM_WIDTH * (pProto->bSize - 1) >= GUILD_SAFEBOX_MAX_NUM)
		{
			sys_err("cannot lod guild item %u vnum %u for guild %u (out of position %u)",
				pItems[i].id, pItems[i].vnum, m_pkOwnerGuild->GetID(), pItems[i].pos);
			continue;
		}

		m_pkItems[pItems[i].pos] = new TPlayerItem(pItems[i]);
		for (int iSize = 0; iSize < pProto->bSize; ++iSize)
			m_bItemGrid[pItems[i].pos + GUILD_SAFEBOX_ITEM_WIDTH * iSize] = 1;
	}

	m_bItemLoaded = true;
}

/********************************\
** PUBLIC CHECK
\********************************/

bool CGuildSafeBox::IsValidPosition(WORD wPos) const
{
	if (!m_bSize)
		return false;

	if (wPos >= GUILD_SAFEBOX_MAX_NUM)
		return false;

	return true;
}

bool CGuildSafeBox::IsEmpty(WORD wPos, BYTE bSize) const
{
	if (!m_bSize)
		return false;

	if (wPos + 5 * (bSize - 1) >= GUILD_SAFEBOX_MAX_NUM)
		return false;

	for (int i = wPos; i < wPos + 5 * bSize; i += 5)
	{
		if (m_bItemGrid[i])
			return false;
	}

	return true;
}

bool CGuildSafeBox::CanAddItem(LPITEM pkItem, WORD wPos) const
{
	if (!IsValidPosition(wPos))
		return false;

	if (!IsEmpty(wPos, pkItem->GetSize()))
		return false;

	return true;
}

/********************************\
** PUBLIC GENERAL
\********************************/

void CGuildSafeBox::GiveSafebox(BYTE bSize)
{
	if (HasSafebox())
		return;

	m_bSize = bSize;
	m_dwGold = 0;

	DWORD dwGuildID = m_pkOwnerGuild->GetID();

	db_clientdesc->DBPacketHeader(HEADER_GD_GUILD_SAFEBOX_CREATE, 0, sizeof(DWORD) + sizeof(BYTE));
	db_clientdesc->Packet(&dwGuildID, sizeof(DWORD));
	db_clientdesc->Packet(&m_bSize, sizeof(BYTE));
}

void CGuildSafeBox::ChangeSafeboxSize(BYTE bNewSize)
{
	m_bSize = bNewSize;

	DWORD dwGuildID = m_pkOwnerGuild->GetID();

	db_clientdesc->DBPacketHeader(HEADER_GD_GUILD_SAFEBOX_SIZE, 0, sizeof(DWORD)+sizeof(BYTE));
	db_clientdesc->Packet(&dwGuildID, sizeof(DWORD));
	db_clientdesc->Packet(&m_bSize, sizeof(BYTE));
}

void CGuildSafeBox::OpenSafebox(LPCHARACTER ch)
{
	if (!HasSafebox())
		return;

	if (ch->GetGuild() != m_pkOwnerGuild)
	{
		sys_err("cannot open guild safebox %d for player %d %s", m_pkOwnerGuild->GetID(), ch->GetPlayerID(), ch->GetName());
		return;
	}

	if (!m_bItemLoaded)
	{
		if (test_server)
			sys_log(0, "CGuildSafeBox::OpenSafebox: Request loading from DB");
		DWORD dwGuildID = m_pkOwnerGuild->GetID();
		db_clientdesc->DBPacket(HEADER_GD_GUILD_SAFEBOX_LOAD, ch->GetDesc()->GetHandle(), &dwGuildID, sizeof(DWORD));
		return;
	}

	__AddViewer(ch);

	__ClientPacket(GUILD_SAFEBOX_SUBHEADER_OPEN, &m_bSize, sizeof(BYTE), ch);
	__ClientPacket(GUILD_SAFEBOX_SUBHEADER_GOLD, &m_dwGold, sizeof(DWORD), ch);

	for (int i = 0; i < GUILD_SAFEBOX_MAX_NUM; ++i)
	{
		if (TPlayerItem* pkItem = __GetItem(i))
		{
			TGuildSafeboxItem item;
			item.vnum = pkItem->vnum;
			item.count = pkItem->count;
			item.pos = pkItem->pos;
			thecore_memcpy(item.sockets, pkItem->alSockets, sizeof(item.sockets));
			thecore_memcpy(item.attr, pkItem->aAttr, sizeof(item.attr));

			__ClientPacket(GUILD_SAFEBOX_SUBHEADER_SET_ITEM, &item, sizeof(TGuildSafeboxItem), ch);
		}
	}
}

void CGuildSafeBox::CheckInItem(LPCHARACTER ch, LPITEM pkItem, int iDestCell)
{
	if (!__IsViewer(ch))
	{
		sys_err("no viewer character (%d %s) try to check in item %d %s pos %d",
			ch->GetPlayerID(), ch->GetName(), pkItem->GetID(), pkItem->GetName(), iDestCell);
		return;
	}

	if (pkItem->GetWindow() != INVENTORY || pkItem->IsExchanging() || pkItem->isLocked())
	{
		sys_err("wrong item selected");
		return;
	}

	if (!CanAddItem(pkItem, iDestCell))
	{
		sys_err("cannot add item to %d", iDestCell);
		return;
	}

	if (test_server)
		sys_log(0, "CGuildSafeBox::CheckInItem %u: source %u destination %u", m_pkOwnerGuild->GetID(), pkItem->GetCell(), iDestCell);

	TPlayerItem item;
	item.id = pkItem->GetID();
	item.owner = m_pkOwnerGuild->GetID();
	item.window = GUILD_SAFEBOX;
	item.pos = iDestCell;
	item.vnum = pkItem->GetVnum();
	item.count = pkItem->GetCount();
	memcpy(item.alSockets, pkItem->GetSockets(), sizeof(item.alSockets));
	memcpy(item.aAttr, pkItem->GetAttributes(), sizeof(item.aAttr));

	// dont send destroy packet to db (guild safebox will do this by itself)
	pkItem->SetSkipSave(true);
	ITEM_MANAGER::instance().RemoveItem(pkItem, "GUILD_SAFEBOX_CHECK_IN");

	db_clientdesc->DBPacket(HEADER_GD_GUILD_SAFEBOX_ADD, ch->GetDesc()->GetHandle(), &item, sizeof(TPlayerItem));
}

void CGuildSafeBox::CheckOutItem(LPCHARACTER ch, int iSourcePos, BYTE bTargetWindow, int iTargetPos)
{
	if (!__IsViewer(ch))
	{
		sys_err("no viewer character (%d %s) try to check out item from pos %d",
			ch->GetPlayerID(), ch->GetName(), iSourcePos);
		return;
	}

	if (!IsValidPosition(iSourcePos))
	{
		sys_err("invalid cell %u", iSourcePos);
		return;
	}

	if (IsEmpty(iSourcePos, 1))
	{
		sys_err("empty pos %u", iSourcePos);
		return;
	}

	if (iTargetPos >= INVENTORY_MAX_NUM || bTargetWindow != INVENTORY)
	{
		sys_err("invalid target pos %u or targetWindow %u", iTargetPos, bTargetWindow);
		return;
	}

	if (test_server)
		sys_log(0, "CGuildSafeBox::CheckOutItem %u: source %u destination %u", m_pkOwnerGuild->GetID(), iSourcePos, iTargetPos);

	TPacketGDGuildSafeboxTake packet;
	packet.dwGuild = m_pkOwnerGuild->GetID();
	packet.bSlot = iSourcePos;
	packet.bTargetSlot = iTargetPos;

	db_clientdesc->DBPacket(HEADER_GD_GUILD_SAFEBOX_TAKE, ch->GetDesc()->GetHandle(), &packet, sizeof(TPacketGDGuildSafeboxTake));
}

void CGuildSafeBox::MoveItem(LPCHARACTER ch, int iSourcePos, int iTargetPos, BYTE bCount)
{
	if (!__IsViewer(ch))
	{
		sys_err("no viewer character (%d %s) try to move item from pos %d to pos %d count %d",
			ch->GetPlayerID(), ch->GetName(), iSourcePos, iTargetPos, bCount);
		return;
	}

	if (!IsValidPosition(iSourcePos) || !IsValidPosition(iTargetPos))
	{
		sys_err("source or target not valid (src %u dst %u)", iSourcePos, iTargetPos);
		return;
	}

	if (iSourcePos < 0 || iSourcePos >= GUILD_SAFEBOX_MAX_NUM || iTargetPos < 0 || iTargetPos >= GUILD_SAFEBOX_MAX_NUM)
		return;

	if (IsEmpty(iSourcePos, 1))
	{
		sys_err("cannot move none item (cell %u)", iSourcePos);
		return;
	}

	if (test_server)
		sys_log(0, "CGuildSafeBox::MoveItem %u: source %u destination %u", m_pkOwnerGuild->GetID(), iSourcePos, iTargetPos);

	TPacketGDGuildSafeboxMove packet;
	packet.dwGuild = m_pkOwnerGuild->GetID();
	packet.bSrcSlot = iSourcePos;
	packet.bTargetSlot = iTargetPos;

	db_clientdesc->DBPacket(HEADER_GD_GUILD_SAFEBOX_MOVE, 0, &packet, sizeof(TPacketGDGuildSafeboxMove));
}

void CGuildSafeBox::GiveGold(LPCHARACTER ch, DWORD dwGold)
{
	if (!__IsViewer(ch))
	{
		sys_err("no viewer character (%d %s) try to give gold %u",
			ch->GetPlayerID(), ch->GetName(), dwGold);
		return;
	}

	if (m_dwGold + dwGold >= GOLD_MAX)
	{
		if (m_dwGold >= GOLD_MAX)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_CONVERT_LANGUAGE(ch->GetLanguage(), "You cannot safe any more gold."));
			return;
		}

		dwGold = GOLD_MAX - m_dwGold;
	}

	if (ch->GetGold() < dwGold)
		dwGold = ch->GetGold();

	if (!dwGold)
		return;

	if (test_server)
		sys_log(0, "CGuildSafeBox::GiveGold %u: %u", m_pkOwnerGuild->GetID(), dwGold);

	ch->PointChange(POINT_GOLD, -dwGold);

	TPacketGuildSafeboxGold packet;
	packet.dwGuild = m_pkOwnerGuild->GetID();
	packet.dwGold = dwGold;

	db_clientdesc->DBPacket(HEADER_GD_GUILD_SAFEBOX_GIVE_GOLD, 0, &packet, sizeof(TPacketGuildSafeboxGold));
}

void CGuildSafeBox::TakeGold(LPCHARACTER ch, DWORD dwGold)
{
	if (!__IsViewer(ch))
	{
		sys_err("no viewer character (%d %s) try to give gold %u",
			ch->GetPlayerID(), ch->GetName(), dwGold);
		return;
	}

	if (m_dwGold < dwGold)
		dwGold = m_dwGold;

	if (dwGold && ch->GetGold() + dwGold >= GOLD_MAX)
	{
		if (ch->GetGold() >= GOLD_MAX)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT_CONVERT_LANGUAGE(ch->GetLanguage(), "You cannot take any more gold."));
			return;
		}

		dwGold = GOLD_MAX - ch->GetGold();
	}

	if (!dwGold)
		return;

	if (test_server)
		sys_log(0, "CGuildSafeBox::TakeGold %u: %u", m_pkOwnerGuild->GetID(), dwGold);

	TPacketGuildSafeboxGold packet;
	packet.dwGuild = m_pkOwnerGuild->GetID();
	packet.dwGold = dwGold;

	db_clientdesc->DBPacket(HEADER_GD_GUILD_SAFEBOX_GET_GOLD, ch->GetDesc()->GetHandle(), &packet, sizeof(TPacketGuildSafeboxGold));
}

void CGuildSafeBox::CloseSafebox(LPCHARACTER ch)
{
	if (!__IsViewer(ch))
		return;

	__RemoveViewer(ch);

	__ClientPacket(GUILD_SAFEBOX_SUBHEADER_CLOSE, NULL, 0, ch);
}

void CGuildSafeBox::DB_SetItem(TPlayerItem* pkItem)
{
	const TItemTable* pProto = ITEM_MANAGER::Instance().GetTable(pkItem->vnum);
	if (!pProto)
	{
		sys_err("cannot get proto of vnum %u guild id %u", pkItem->vnum, m_pkOwnerGuild->GetID());
		return;
	}

	m_pkItems[pkItem->pos] = new TPlayerItem(*pkItem);
	for (int i = pkItem->pos; i < pkItem->pos + GUILD_SAFEBOX_ITEM_WIDTH * pProto->bSize; i += GUILD_SAFEBOX_ITEM_WIDTH)
		m_bItemGrid[i] = 1;

	TGuildSafeboxItem item;
	item.vnum = pkItem->vnum;
	item.count = pkItem->count;
	item.pos = pkItem->pos;
	thecore_memcpy(item.sockets, pkItem->alSockets, sizeof(item.sockets));
	thecore_memcpy(item.attr, pkItem->aAttr, sizeof(item.attr));

	__ClientPacket(GUILD_SAFEBOX_SUBHEADER_SET_ITEM, &item, sizeof(TGuildSafeboxItem));
}

void CGuildSafeBox::DB_DelItem(BYTE bSlot)
{
	if (!m_pkItems[bSlot])
	{
		sys_err("DB_DelItem: no item on slot %u", bSlot);
		return;
	}

	const TItemTable* pProto = ITEM_MANAGER::Instance().GetTable(m_pkItems[bSlot]->vnum);
	if (!pProto)
	{
		sys_err("cannot get proto of vnum %u guild id %u", m_pkItems[bSlot]->vnum, m_pkOwnerGuild->GetID());
		return;
	}

	delete m_pkItems[bSlot];
	m_pkItems[bSlot] = NULL;
	for (int i = bSlot; i < bSlot + GUILD_SAFEBOX_ITEM_WIDTH * pProto->bSize; i += GUILD_SAFEBOX_ITEM_WIDTH)
		m_bItemGrid[i] = 0;

	__ClientPacket(GUILD_SAFEBOX_SUBHEADER_REMOVE_ITEM, &bSlot, sizeof(BYTE));
}

void CGuildSafeBox::DB_SetGold(DWORD dwGold)
{
	m_dwGold = dwGold;

	__ClientPacket(GUILD_SAFEBOX_SUBHEADER_GOLD, &m_dwGold, sizeof(DWORD));
}

void CGuildSafeBox::DB_SetOwned(BYTE bSize)
{
	m_bSize = bSize;
}

/********************************\
** PRIVATE SET/GET
\********************************/

TPlayerItem* CGuildSafeBox::__GetItem(WORD wPos)
{
	if (wPos >= GUILD_SAFEBOX_MAX_NUM)
		return NULL;
	return m_pkItems[wPos];
}

/********************************\
** PRIVATE VIEWER
\********************************/

bool CGuildSafeBox::__IsViewer(LPCHARACTER ch)
{
	return m_set_pkCurrentViewer.find(ch) != m_set_pkCurrentViewer.end();
}
void CGuildSafeBox::__AddViewer(LPCHARACTER ch)
{
	if (!__IsViewer(ch))
		m_set_pkCurrentViewer.insert(ch);
}
void CGuildSafeBox::__RemoveViewer(LPCHARACTER ch)
{
	m_set_pkCurrentViewer.erase(ch);
}
void CGuildSafeBox::__ViewerPacket(const void* c_pData, size_t size)
{
	for (std::set<LPCHARACTER>::iterator it = m_set_pkCurrentViewer.begin(); it != m_set_pkCurrentViewer.end(); ++it)
		(*it)->GetDesc()->Packet(c_pData, size);
}
void CGuildSafeBox::__ClientPacket(BYTE subheader, const void* c_pData, size_t size, LPCHARACTER ch)
{
	TPacketGCGuildSafebox packet;
	packet.header = HEADER_GC_GUILD_SAFEBOX;
	packet.size = sizeof(TPacketGCGuildSafebox) + size;
	packet.subheader = subheader;

	TEMP_BUFFER buf;
	buf.write(&packet, sizeof(TPacketGCGuildSafebox));
	if (size)
		buf.write(c_pData, size);

	if (ch)
		ch->GetDesc()->Packet(buf.read_peek(), buf.size());
	else
		__ViewerPacket(buf.read_peek(), buf.size());
}
#endif
