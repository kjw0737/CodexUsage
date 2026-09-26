#pragma once
// Owner-drawn buttons need explicit mouse-leave tracking to repaint hover state.
class HoverButton : public CButton
{
    bool tracking_ = false;
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg LRESULT OnMouseLeave(WPARAM, LPARAM);
    afx_msg void OnEnable(BOOL enabled);
    DECLARE_MESSAGE_MAP()
};
