#include "pch_script.h"
#include "UIEncyclopediaArticleWnd.h"
#include "UIXmlInit.h"
#include "encyclopedia_article.h"
#include "xrUICore/Static/UIStatic.h"
#include "xrEngine/StringTable/StringTable.h"

CUIEncyclopediaArticleWnd::CUIEncyclopediaArticleWnd() : CUIWindow("CUIEncyclopediaArticleWnd") {}

void CUIEncyclopediaArticleWnd::Init(pcstr xml_name, pcstr start_from)
{
    CUIXml xml;
    xml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, xml_name);

    string512 path;
    CUIXmlInit::InitWindow(xml, start_from, 0, this);

    m_UIImage = xr_new<CUIStatic>("image");
    m_UIImage->SetAutoDelete(true);
    CUIXmlInit::InitStatic(xml, strconcat(path, start_from, ":image"), 0, m_UIImage);
    AttachChild(m_UIImage);

    m_UIText = xr_new<CUIStatic>("text_cont");
    m_UIText->SetAutoDelete(true);
    CUIXmlInit::InitStatic(xml, strconcat(path, start_from, ":text_cont"), 0, m_UIText);
    AttachChild(m_UIText);
}

void CUIEncyclopediaArticleWnd::SetArticle(CEncyclopediaArticle* article)
{
    CUIStatic& image = article->data()->image;
    if (image.GetShader() && image.GetShader()->inited())
    {
        m_UIImage->SetShader(image.GetShader());
        m_UIImage->SetTextureRect(image.GetTextureRect());
        m_UIImage->SetWndSize(image.GetWndSize());
        const float img_x = std::max(0.0f, (GetWidth() - m_UIImage->GetWidth()) / 2.0f);
        m_UIImage->SetWndPos({ img_x, m_UIImage->GetWndPos().y });
    }
    else
        m_UIImage->SetWndSize({ m_UIImage->GetWidth(), 0.0f });

    m_UIText->SetText(StringTable().translate(article->data()->text.c_str()).c_str());
    m_UIText->AdjustHeightToText();
    AdjustLayout();
}

void CUIEncyclopediaArticleWnd::AdjustLayout()
{
    const float image_bottom = m_UIImage->GetWndPos().y + m_UIImage->GetHeight();
    m_UIText->SetWndPos({ m_UIText->GetWndPos().x, image_bottom });
    SetHeight(image_bottom + m_UIText->GetHeight());
}
