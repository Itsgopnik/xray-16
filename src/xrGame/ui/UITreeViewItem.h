//=============================================================================
//  TreeView item for CUIListWnd (SoC PDA diary and encyclopedia trees).
//  Ported from the SoC/OGSR CUITreeViewItem by Roman E. Marchenko (GSC).
//=============================================================================
#pragma once

#include "xrUICore/ListWnd/UIListItem.h"
#include "xrUICore/Static/UIStatic.h"

class CUIListWnd;

class CUITreeViewItem : public CUIListItem
{
    using inherited = CUIListItem;

    bool isRoot{};
    bool isOpened{};
    // Text offset in spaces
    int iTextShift{};
    CUITreeViewItem* pOwner{};

    void CheckParentMark(CUITreeViewItem* pOwner);

    u32 m_uUnreadedColor;
    u32 m_uReadedColor;
    bool m_bArticleRead{};
    // If true, MarkArticleAsRead() doesn't call SetItemColor()
    bool m_bManualSetColor{};
    bool m_toggle_pending{};

protected:
    virtual void OnRootChanged();
    virtual void OnOpenClose();

public:
    using SubItems = xr_vector<CUITreeViewItem*>;
    SubItems vSubItems;
    CUIStatic UIBkg;

    CUITreeViewItem();
    ~CUITreeViewItem() override;

    void SetRoot(bool set);
    bool IsRoot() const { return isRoot; }
    void SetTextShift(int delta) { iTextShift += delta; }
    CUITreeViewItem* GetOwner() const { return pOwner; }
    void SetOwner(CUITreeViewItem* owner) { pOwner = owner; }

    void Open();
    void Close();
    bool IsOpened() const { return isOpened; }

    void AddItem(CUITreeViewItem* pItem);
    void DeleteAllSubItems();
    CUITreeViewItem* Find(pcstr text) const;
    CUITreeViewItem* Find(int value) const;

    // Call SetText() before SetRoot(), the [+-] signs depend on it
    void SetText(pcstr str) override;
    void SendMessage(CUIWindow* pWnd, s16 msg, void* pData) override;
    // Opening/closing changes the list's children. The click arrives while the list iterates
    // them (buttons don't capture the mouse), so it is only scheduled here; the list owner
    // calls ToggleIfPending() outside of the list's loops.
    void OnClick() override;
    bool HasPendingToggle() const { return m_toggle_pending; }
    void ToggleIfPending();

    void MarkArticleAsRead(bool value);
    bool IsArticleReaded() const { return m_bArticleRead; }
    void SetReadedColor(u32 cl) { m_uReadedColor = cl; }
    void SetUnreadedColor(u32 cl) { m_uUnreadedColor = cl; }
    void SetManualSetColor(bool val) { m_bManualSetColor = val; }
    void SetItemColor() { SetTextColor(m_bArticleRead ? m_uReadedColor : m_uUnreadedColor); }

    pcstr GetDebugType() override { return "CUITreeViewItem"; }
};

// Opens/closes the tree items clicked since the last call. Call it outside of the list's loops,
// e.g. from the owner's Update() after inherited::Update().
void ToggleTreeItemsIfPending(CUIListWnd* list);

// Adds leafName under the slash separated group path (e.g. "journal/escape") to the list,
// creating the root and intermediate items as needed
void CreateTreeBranch(shared_str nesting, shared_str leafName, CUIListWnd* pListToAdd, int leafProperty,
    CGameFont* pRootFont, u32 rootColor, CGameFont* pLeafFont, u32 leafColor, bool markRead);
