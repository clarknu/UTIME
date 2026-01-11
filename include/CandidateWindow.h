#pragma once
#include <windows.h>
#include <string>
#include <vector>

class CCandidateWindow
{
public:
    CCandidateWindow();
    ~CCandidateWindow();

    bool Initialize(HINSTANCE hInstance);
    void Destroy();

    // Show the window at specific coordinates with candidate strings
    void Show(int x, int y, const std::vector<std::wstring>& candidates, int selectedIndex);
    void Hide();
    
    // Check if window is currently visible
    bool IsVisible() const { return _hwnd != NULL && IsWindowVisible(_hwnd); }

private:
    static LRESULT CALLBACK _WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void _OnPaint(HDC hdc);

    HWND _hwnd;
    std::vector<std::wstring> _candidates;
    int _selectedIndex;
    HFONT _hFont;
    
    // Layout constants
    const int _padding = 5;
    const int _lineHeight = 24;
};
