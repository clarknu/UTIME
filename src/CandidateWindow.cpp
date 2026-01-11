#include "CandidateWindow.h"

#define CANDIDATE_WINDOW_CLASS L"UTIME_CandidateWindow"

CCandidateWindow::CCandidateWindow() : _hwnd(NULL), _selectedIndex(0), _hFont(NULL)
{
}

CCandidateWindow::~CCandidateWindow()
{
    Destroy();
}

bool CCandidateWindow::Initialize(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_IME;
    wc.lpfnWndProc = _WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = CANDIDATE_WINDOW_CLASS;

    RegisterClassEx(&wc);

    _hFont = CreateFont(
        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");

    // Create the window but don't show it yet
    // WS_POPUP: No border
    // WS_EX_TOPMOST: Always on top
    // WS_EX_NOACTIVATE: Don't steal focus
    // WS_EX_TOOLWINDOW: Don't show in taskbar
    _hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        CANDIDATE_WINDOW_CLASS,
        L"UTIME Candidates",
        WS_POPUP | WS_BORDER,
        0, 0, 100, 30, // Initial size
        NULL, NULL, hInstance, this);

    if (_hwnd)
    {
        SetWindowLongPtr(_hwnd, GWLP_USERDATA, (LONG_PTR)this);
    }

    return _hwnd != NULL;
}

void CCandidateWindow::Destroy()
{
    if (_hwnd)
    {
        DestroyWindow(_hwnd);
        _hwnd = NULL;
    }
    if (_hFont)
    {
        DeleteObject(_hFont);
        _hFont = NULL;
    }
    UnregisterClass(CANDIDATE_WINDOW_CLASS, GetModuleHandle(NULL));
}

void CCandidateWindow::Show(int x, int y, const std::vector<std::wstring>& candidates, int selectedIndex)
{
    if (!_hwnd) return;

    _candidates = candidates;
    _selectedIndex = selectedIndex;

    // Calculate window size based on content
    HDC hdc = GetDC(_hwnd);
    HFONT hOldFont = (HFONT)SelectObject(hdc, _hFont);

    int maxWidth = 0;
    int totalHeight = _padding * 2;

    for (size_t i = 0; i < _candidates.size(); ++i)
    {
        std::wstring line = std::to_wstring(i + 1) + L". " + _candidates[i];
        SIZE sz;
        GetTextExtentPoint32(hdc, line.c_str(), (int)line.length(), &sz);
        if (sz.cx > maxWidth) maxWidth = sz.cx;
        totalHeight += _lineHeight;
    }
    
    // Add some padding
    maxWidth += _padding * 4;

    SelectObject(hdc, hOldFont);
    ReleaseDC(_hwnd, hdc);

    // Position and resize
    SetWindowPos(_hwnd, HWND_TOPMOST, x, y + 25, maxWidth, totalHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    
    // Trigger repaint
    InvalidateRect(_hwnd, NULL, TRUE);
}

void CCandidateWindow::Hide()
{
    if (_hwnd)
    {
        ShowWindow(_hwnd, SW_HIDE);
    }
}

LRESULT CALLBACK CCandidateWindow::_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CCandidateWindow* pThis = (CCandidateWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_PAINT:
        if (pThis)
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            pThis->_OnPaint(hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

void CCandidateWindow::_OnPaint(HDC hdc)
{
    RECT rc;
    GetClientRect(_hwnd, &rc);
    
    // Fill background
    HBRUSH hBrushBg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rc, hBrushBg);
    DeleteObject(hBrushBg);

    // Draw candidates
    HFONT hOldFont = (HFONT)SelectObject(hdc, _hFont);
    SetBkMode(hdc, TRANSPARENT);

    int y = _padding;
    for (size_t i = 0; i < _candidates.size(); ++i)
    {
        std::wstring line = std::to_wstring(i + 1) + L". " + _candidates[i];
        
        // Highlight selected
        if ((int)i == _selectedIndex)
        {
            RECT rcLine = { 0, y, rc.right, y + _lineHeight };
            HBRUSH hBrushSel = CreateSolidBrush(RGB(230, 240, 255)); // Light blue
            FillRect(hdc, &rcLine, hBrushSel);
            DeleteObject(hBrushSel);
            SetTextColor(hdc, RGB(0, 120, 215)); // Blue text
        }
        else
        {
            SetTextColor(hdc, RGB(0, 0, 0)); // Black text
        }

        TextOut(hdc, _padding * 2, y + 2, line.c_str(), (int)line.length());
        y += _lineHeight;
    }

    SelectObject(hdc, hOldFont);
}
