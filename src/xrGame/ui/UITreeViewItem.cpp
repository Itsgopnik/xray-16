//=============================================================================
//  TreeView item for CUIListWnd (SoC PDA diary and encyclopedia trees).
//  Ported from the SoC/OGSR CUITreeViewItem by Roman E. Marchenko (GSC).
//=============================================================================
#include "pch_script.h"
#include "UITreeViewItem.h"
#include "xrUICore/ListWnd/UIListWnd.h"
#include "xrEngine/StringTable/StringTable.h"

namespace
{
// Offset of sub items relative to the parent, in spaces
constexpr int subShift = 1;
constexpr pcstr treeItemBackgroundTexture = "ui\\ui_pda_over_list";
constexpr u32 UNREAD_COLOR = 0xff00ff00;
constexpr u32 READ_COLOR = 0xffffffff;
} // namespace

CUITreeViewItem::CUITreeViewItem()
    : m_uUnreadedColor(UNREAD_COLOR), m_uReadedColor(READ_COLOR), UIBkg("TreeViewItemBkg")
{
    AttachChild(&UIBkg);
    UIBkg.InitTexture(treeItemBackgroundTexture);
    UIBkg.TextureOff();
    UIBkg.SetTextureOffset(-20, 0);
    SetHighlightText(false);
    // CUIButton centers its text; tree items are left aligned as in SoC
    SetTextAlignment(CGameFont::alLeft);
}

CUITreeViewItem::~CUITreeViewItem()
{
    DetachChild(&UIBkg);
    DeleteAllSubItems();
}

void CUITreeViewItem::OnRootChanged()
{
    xr_string str = GetText();
    if (isRoot)
    {
        // Put + or - right before the text, after the last leading space
        xr_string::size_type pos = str.find_first_not_of(' ');
        if (xr_string::npos == pos)
            pos = 0;

        if (pos == 0)
        {
            ++iTextShift;
            str.insert(0, " ");
        }
        else
            --pos;

        str.replace(pos, 1, isOpened ? "-" : "+");
    }
    else
    {
        const xr_string::size_type pos = str.find_first_of("+-");
        if (pos == 0)
        {
            for (int i = 0; i < iTextShift; ++i)
                str.insert(pos, " ");
        }
        else if (pos != xr_string::npos)
            str.replace(pos, 1, " ");
    }
    inherited::SetText(str.c_str());
}

void CUITreeViewItem::OnOpenClose()
{
    if (!isRoot)
        return;

    xr_string str = GetText();
    const xr_string::size_type pos = str.find_first_of("+-");
    if (xr_string::npos != pos)
        str.replace(pos, 1, isOpened ? "-" : "+");
    inherited::SetText(str.c_str());
}

void CUITreeViewItem::Open()
{
    if (!isRoot || isOpened)
        return;
    isOpened = true;
    OnOpenClose();

    // Attach all sub items to the parent list right after us
    CUIListWnd* pList = smart_cast<CUIListWnd*>(GetParent());
    R_ASSERT(pList);
    if (!pList)
        return;

    int pos = pList->GetItemPos(this);
    for (CUITreeViewItem* item : vSubItems)
        pList->AddItem(item, ++pos);
}

void CUITreeViewItem::Close()
{
    if (!isRoot || !isOpened)
        return;
    isOpened = false;
    OnOpenClose();

    CUIListWnd* pList = smart_cast<CUIListWnd*>(GetParent());
    R_ASSERT(pList);
    if (!pList)
        return;

    for (CUITreeViewItem* item : vSubItems)
        item->Close();
    for (CUITreeViewItem* item : vSubItems)
        pList->RemoveItem(pList->GetItemPos(item));
}

void CUITreeViewItem::AddItem(CUITreeViewItem* pItem)
{
    R_ASSERT(pItem);
    pItem->SetTextShift(subShift + iTextShift);
    vSubItems.push_back(pItem);
    pItem->SetAutoDelete(false);
    pItem->SetOwner(this);
    pItem->SetText(pItem->GetText());
}

void CUITreeViewItem::DeleteAllSubItems()
{
    for (CUITreeViewItem* item : vSubItems)
    {
        // Attached items are deleted by the list
        if (!item->GetParent())
            xr_delete(item);
    }
    vSubItems.clear();
}

void CUITreeViewItem::SetRoot(bool set)
{
    if (isRoot)
        return;
    isRoot = set;
    OnRootChanged();
}

void CUITreeViewItem::SetText(pcstr str)
{
    xr_string s = str;
    const xr_string::size_type pos = s.find_first_not_of(" +-");
    if (pos != xr_string::npos)
    {
        if (pos < xr_string::size_type(iTextShift))
            s.insert(0, iTextShift - pos, ' ');
        else if (pos > xr_string::size_type(iTextShift))
            s.erase(0, pos - iTextShift);
    }
    inherited::SetText(s.c_str());
}

void CUITreeViewItem::OnClick()
{
    if (IsRoot())
        m_toggle_pending = true;
    else
        MarkArticleAsRead(true);
    inherited::OnClick();
}

void CUITreeViewItem::ToggleIfPending()
{
    if (!m_toggle_pending)
        return;
    m_toggle_pending = false;
    IsOpened() ? Close() : Open();
}

void ToggleTreeItemsIfPending(CUIListWnd* list)
{
    xr_vector<CUITreeViewItem*> pending;
    for (int i = 0; i < list->GetItemsCount(); ++i)
    {
        auto* item = smart_cast<CUITreeViewItem*>(list->GetItem(i));
        if (item && item->HasPendingToggle())
            pending.push_back(item);
    }
    for (CUITreeViewItem* item : pending)
        item->ToggleIfPending();
}

// Highlight only this item: the list forwards focus messages of any item to all items
void CUITreeViewItem::OnFocusReceive()
{
    inherited::OnFocusReceive();
    UIBkg.TextureOn();
}

void CUITreeViewItem::OnFocusLost()
{
    inherited::OnFocusLost();
    UIBkg.TextureOff();
}

CUITreeViewItem* CUITreeViewItem::Find(pcstr text) const
{
    for (CUITreeViewItem* item : vSubItems)
    {
        xr_string caption = item->GetText();
        const xr_string::size_type pos = caption.find_first_not_of(" +-");
        if (pos != xr_string::npos)
            caption.erase(0, pos);
        if (xr_strcmp(caption.c_str(), text) == 0)
            return item;
        if (item->IsRoot())
            if (CUITreeViewItem* found = item->Find(text))
                return found;
    }
    return nullptr;
}

CUITreeViewItem* CUITreeViewItem::Find(int value) const
{
    for (CUITreeViewItem* item : vSubItems)
    {
        if (item->GetValue() == value)
            return item;
        if (item->IsRoot())
            if (CUITreeViewItem* found = item->Find(value))
                return found;
    }
    return nullptr;
}

void CUITreeViewItem::MarkArticleAsRead(bool value)
{
    m_bArticleRead = value;
    if (!m_bManualSetColor)
        SetItemColor();

    if (IsRoot())
    {
        // A root marks itself and all children
        for (CUITreeViewItem* item : vSubItems)
        {
            item->m_bArticleRead = value;
            item->SetItemColor();
            if (item->IsRoot())
                item->MarkArticleAsRead(value);
        }
    }
    else
        CheckParentMark(GetOwner());
}

void CUITreeViewItem::CheckParentMark(CUITreeViewItem* owner)
{
    // A branch is read when all its children are read; propagate upwards
    if (!owner || !owner->IsRoot())
        return;

    bool all_read = true;
    for (CUITreeViewItem* item : owner->vSubItems)
        all_read &= item->IsArticleReaded();
    owner->m_bArticleRead = all_read;
    owner->SetItemColor();
    owner->CheckParentMark(owner->GetOwner());
}

void CreateTreeBranch(shared_str nesting, shared_str leafName, CUIListWnd* pListToAdd, int leafProperty,
    CGameFont* pRootFont, u32 rootColor, CGameFont* pLeafFont, u32 leafColor, bool markRead)
{
    R_ASSERT(nesting.size());
    R_ASSERT(pListToAdd && pLeafFont && pRootFont);

    // Insert the not yet existing tail of the group path below pItemToIns
    const auto add_tree_tail = [&](xr_vector<shared_str>::const_iterator it,
                                   const xr_vector<shared_str>& tree, CUITreeViewItem* pItemToIns) {
        CUITreeViewItem* pNewItem = nullptr;
        for (; it != tree.end(); ++it)
        {
            pNewItem = xr_new<CUITreeViewItem>();
            pItemToIns->AddItem(pNewItem);
            pNewItem->SetFont(pRootFont);
            pNewItem->SetText(it->c_str());
            pNewItem->SetReadedColor(rootColor);
            pNewItem->SetRoot(true);
            pItemToIns = pNewItem;
        }
        return pNewItem;
    };

    // Split the group path into translated levels
    xr_vector<shared_str> groupTree;
    xr_string group = nesting.c_str();
    for (xr_string::size_type pos; (pos = group.find('/')) != xr_string::npos; group.erase(0, pos + 1))
        groupTree.emplace_back(StringTable().translate(group.substr(0, pos).c_str()));
    groupTree.emplace_back(StringTable().translate(group.c_str()));

    // Look for the path among the existing roots
    CUITreeViewItem* pTVItemChilds = nullptr;
    for (int i = 0; i < pListToAdd->GetItemsCount(); ++i)
    {
        CUITreeViewItem* pTVItem = smart_cast<CUITreeViewItem*>(pListToAdd->GetItem(i));
        R_ASSERT(pTVItem);
        pTVItem->Close();

        xr_string caption = pTVItem->GetText();
        caption.erase(0, 1); // "+" sign
        if (xr_strcmp(caption.c_str(), groupTree.front().c_str()) != 0)
            continue;

        pTVItemChilds = pTVItem;
        for (auto it = groupTree.begin() + 1; it != groupTree.end(); ++it)
        {
            CUITreeViewItem* found = pTVItemChilds->Find(it->c_str());
            if (!found)
            {
                pTVItemChilds = add_tree_tail(it, groupTree, pTVItemChilds);
                break;
            }
            pTVItemChilds = found;
        }
        break;
    }

    // Not found at all: add the whole path as a new root
    if (!pTVItemChilds)
    {
        pTVItemChilds = xr_new<CUITreeViewItem>();
        pTVItemChilds->SetFont(pRootFont);
        pTVItemChilds->SetText(groupTree.front().c_str());
        pTVItemChilds->SetReadedColor(rootColor);
        pTVItemChilds->SetRoot(true);
        pListToAdd->AddItem<CUITreeViewItem>(pTVItemChilds);
        if (groupTree.size() > 1)
            pTVItemChilds = add_tree_tail(groupTree.begin() + 1, groupTree, pTVItemChilds);
    }
    R_ASSERT(pTVItemChilds);

    CUITreeViewItem* pLeaf = xr_new<CUITreeViewItem>();
    pLeaf->SetFont(pLeafFont);
    pLeaf->SetReadedColor(leafColor);
    pLeaf->SetText(StringTable().translate(leafName).c_str());
    pLeaf->SetValue(leafProperty);
    pTVItemChilds->AddItem(pLeaf);
    pLeaf->MarkArticleAsRead(markRead);
}
