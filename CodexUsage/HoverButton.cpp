#include "pch.h"
#include "HoverButton.h"
BEGIN_MESSAGE_MAP(HoverButton, CButton)
    ON_WM_MOUSEMOVE()
    ON_MESSAGE(WM_MOUSELEAVE, &HoverButton::OnMouseLeave)
    ON_WM_ENABLE()
END_MESSAGE_MAP()
void HoverButton::OnMouseMove(UINT flags, CPoint point)
{
    if (!tracking_)
    {
        TRACKMOUSEEVENT event{ sizeof(event), TME_LEAVE, m_hWnd, 0 };
        tracking_ = TrackMouseEvent(&event) != FALSE;
        Invalidate(FALSE);
    }
    CButton::OnMouseMove(flags, point);
}
LRESULT HoverButton::OnMouseLeave(WPARAM, LPARAM)
{
    tracking_ = false; Invalidate(FALSE); return 0;
}
void HoverButton::OnEnable(BOOL enabled)
{
    CButton::OnEnable(enabled); Invalidate(FALSE);
}
