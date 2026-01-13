#include "TextService.h"
#include "EditSession.h"
#include "GetTextExtEditSession.h"
#include "DictionaryEngine.h"
#include <fstream>
#include <winuser.h>

// Enhanced DebugLog with timestamp and thread safety
static CRITICAL_SECTION g_LogCS;
static bool g_LogCSInitialized = false;

void DebugLog(const wchar_t* format, ...)
{
    // Initialize critical section on first use
    if (!g_LogCSInitialized)
    {
        InitializeCriticalSection(&g_LogCS);
        g_LogCSInitialized = true;
    }
    
    EnterCriticalSection(&g_LogCS);
    
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, 1024, format, args);
    va_end(args);
    
    // Get timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timestamp[64];
    swprintf_s(timestamp, 64, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d]", 
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    // Get thread ID
    DWORD threadId = GetCurrentThreadId();
    
    // Build final log message
    wchar_t finalMsg[1200];
    swprintf_s(finalMsg, 1200, L"%s [TID:%d] %s", timestamp, threadId, buffer);

    // Output to debugger
    OutputDebugString(finalMsg);
    OutputDebugString(L"\n");
    
    // Write to file (with error handling)
    std::wofstream logFile("C:\\Windows\\Temp\\UTIME_Debug.log", std::ios::app);
    if (logFile.is_open())
    {
        logFile << finalMsg << std::endl;
        logFile.close();
    }
    // If file write fails, at least we have OutputDebugString
    
    LeaveCriticalSection(&g_LogCS);
}

CTextService::CTextService() 
    : _cRef(1), 
      _pThreadMgr(NULL), 
      _tfClientId(TF_CLIENTID_NULL), 
      _dwCookieKey(TF_INVALID_COOKIE),
      _pComposition(NULL),
      _pCandidateWindow(NULL),
      _selectedCandidateIndex(0),
      _lastCandidateX(0),
      _lastCandidateY(0)
{
    DllAddRef();
    _pCandidateWindow = new CCandidateWindow();
    
    // Initialize Dictionary
    CDictionaryEngine::Instance().Initialize();
}

CTextService::~CTextService()
{
    if (_pCandidateWindow)
    {
        delete _pCandidateWindow;
        _pCandidateWindow = NULL;
    }
    if (_pComposition)
    {
        _pComposition->Release();
        _pComposition = NULL;
    }
    DllRelease();
}

// IUnknown implementation
STDMETHODIMP CTextService::QueryInterface(REFIID riid, void **ppvObj)
{
    if (ppvObj == NULL) return E_INVALIDARG;
    *ppvObj = NULL;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
    {
        *ppvObj = (void *)(ITfTextInputProcessor *)this;
    }
    else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    {
        *ppvObj = (void *)(ITfKeyEventSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfCompositionSink))
    {
        *ppvObj = (void *)(ITfCompositionSink *)this;
    }
    else
    {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CTextService::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CTextService::Release()
{
    long cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

// ITfTextInputProcessor implementation
STDMETHODIMP CTextService::Activate(ITfThreadMgr *ptim, TfClientId tid)
{
    _pThreadMgr = ptim;
    _pThreadMgr->AddRef();
    _tfClientId = tid;
    
    if (_pCandidateWindow)
    {
        _pCandidateWindow->Initialize(g_hInst);
        // Set callback for mouse click events - pass NULL for context as it will be provided on each click
        _pCandidateWindow->SetCallback(this, NULL);
    }

    return _InitKeyEventSink();
}

STDMETHODIMP CTextService::Deactivate()
{
    _UninitKeyEventSink();

    if (_pCandidateWindow)
    {
        _pCandidateWindow->Destroy();
    }

    if (_pThreadMgr)
    {
        _pThreadMgr->Release();
        _pThreadMgr = NULL;
    }
    _tfClientId = TF_CLIENTID_NULL;
    
    return S_OK;
}

// ITfKeyEventSink Implementation
STDMETHODIMP CTextService::OnSetFocus(BOOL fForeground)
{
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    *pfEaten = FALSE;
    
    // Don't eat if Ctrl or Alt is pressed
    if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000)) {
        return S_OK;
    }

    // Simple logic: if 'a'-'z', eat it.
    if ((wParam >= 'A' && wParam <= 'Z')) {
        *pfEaten = TRUE;
    }
    // If composition is active, eat Space/Enter/Esc/Back, number keys, and up/down arrow keys
    if (_sComposition.length() > 0) {
        if (wParam == VK_SPACE || wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_BACK) {
            *pfEaten = TRUE;
        }
        // Eat number keys 1-9 when composition is active
        if (wParam >= '1' && wParam <= '9') {
            *pfEaten = TRUE;
        }
        // Eat up/down arrow keys for candidate navigation
        if (wParam == VK_UP || wParam == VK_DOWN) {
            *pfEaten = TRUE;
        }
    }
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    *pfEaten = FALSE;
    
    BOOL fTestEaten = FALSE;
    OnTestKeyDown(pic, wParam, lParam, &fTestEaten);
    if (!fTestEaten) return S_OK;

    *pfEaten = TRUE;
    
    DebugLog(L"OnKeyDown: wParam=%X", wParam);

    if (wParam >= 'A' && wParam <= 'Z') {
        // Simple lowercase mapping
        wchar_t ch = (wchar_t)(wParam - 'A' + 'a');
        _sComposition += ch;
        _selectedCandidateIndex = 0;  // Reset selection when composition changes
        _UpdateComposition(pic);
        _UpdateCandidateWindow(pic);
    }
    else if (wParam == VK_BACK) {
        if (!_sComposition.empty()) {
            _sComposition.pop_back();
            _selectedCandidateIndex = 0;  // Reset selection when composition changes
            if (_sComposition.empty()) 
            {
                _EndComposition(pic);
                if (_pCandidateWindow) _pCandidateWindow->Hide();
            }
            else 
            {
                _UpdateComposition(pic);
                _UpdateCandidateWindow(pic);
            }
        }
    }
    else if (wParam >= '1' && wParam <= '9') {
        // Handle number key to select candidate
        if (!_sComposition.empty() && _candidateList.size() > 0) {
            int index = (int)(wParam - '1'); // 1 -> 0, 2 -> 1, etc.
            
            if (index < (int)_candidateList.size()) {
                std::wstring commitText = _candidateList[index];
                DebugLog(L"OnKeyDown Number Key %d: Committing candidate='%s'", index + 1, commitText.c_str());
                
                // Use synchronous commit session
                CCommitCompositionEditSession *pCommit = new CCommitCompositionEditSession(this, pic, commitText);
                HRESULT hrSession;
                HRESULT hr = pic->RequestEditSession(_tfClientId, pCommit, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
                
                if (hr == TF_E_SYNCHRONOUS)
                {
                    DebugLog(L"OnKeyDown Number: Application rejected synchronous request, falling back to async");
                    hr = pic->RequestEditSession(_tfClientId, pCommit, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, NULL);
                }
                
                if (FAILED(hr))
                {
                    DebugLog(L"OnKeyDown Number: RequestEditSession failed, hr=0x%08X", hr);
                }
                else
                {
                    DebugLog(L"OnKeyDown Number: RequestEditSession succeeded, hrSession=0x%08X", hrSession);
                }
                
                pCommit->Release();
                
                // Clear state
                _sComposition.clear();
                _candidateList.clear();
                _selectedCandidateIndex = 0;
                if (_pCandidateWindow) _pCandidateWindow->Hide();
            }
            else
            {
                DebugLog(L"OnKeyDown Number: Index %d out of range (size=%d)", index, _candidateList.size());
            }
        }
    }
    else if (wParam == VK_SPACE) {
        if (!_sComposition.empty()) {
            // Commit selected candidate or raw pinyin
            std::wstring commitText = _sComposition; // Default: commit raw pinyin
            
            if (_candidateList.size() > 0 && _selectedCandidateIndex >= 0 && _selectedCandidateIndex < (int)_candidateList.size())
            {
                // Use selected candidate
                commitText = _candidateList[_selectedCandidateIndex];
            }
            
            DebugLog(L"OnKeyDown VK_SPACE: Committing text='%s', selectedIndex=%d", commitText.c_str(), _selectedCandidateIndex);
            
            // Use new synchronous commit session
            CCommitCompositionEditSession *pCommit = new CCommitCompositionEditSession(this, pic, commitText);
            HRESULT hrSession;
            HRESULT hr = pic->RequestEditSession(_tfClientId, pCommit, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            
            if (hr == TF_E_SYNCHRONOUS)
            {
                DebugLog(L"OnKeyDown VK_SPACE: Application rejected synchronous request, falling back to async");
                // Fallback to async mode
                hr = pic->RequestEditSession(_tfClientId, pCommit, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, NULL);
            }
            
            if (FAILED(hr))
            {
                DebugLog(L"OnKeyDown VK_SPACE: RequestEditSession failed, hr=0x%08X", hr);
            }
            else
            {
                DebugLog(L"OnKeyDown VK_SPACE: RequestEditSession succeeded, hrSession=0x%08X", hrSession);
            }
            
            pCommit->Release();
            
            // Clear state
            _sComposition.clear();
            _candidateList.clear();
            _selectedCandidateIndex = 0;
            if (_pCandidateWindow) _pCandidateWindow->Hide();
        }
        else
        {
            // Pass space to application
            *pfEaten = FALSE;
        }
    }
    else if (wParam == VK_RETURN) {
        if (!_sComposition.empty()) {
            // Commit pinyin raw text (not candidate)
            std::wstring commitText = _sComposition;
            
            DebugLog(L"OnKeyDown VK_RETURN: Committing raw text='%s'", commitText.c_str());
            
            // Use synchronous commit session
            CCommitCompositionEditSession *pCommit = new CCommitCompositionEditSession(this, pic, commitText);
            HRESULT hrSession;
            HRESULT hr = pic->RequestEditSession(_tfClientId, pCommit, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            
            if (hr == TF_E_SYNCHRONOUS)
            {
                DebugLog(L"OnKeyDown VK_RETURN: Application rejected synchronous request, falling back to async");
                hr = pic->RequestEditSession(_tfClientId, pCommit, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, NULL);
            }
            
            if (FAILED(hr))
            {
                DebugLog(L"OnKeyDown VK_RETURN: RequestEditSession failed, hr=0x%08X", hr);
            }
            else
            {
                DebugLog(L"OnKeyDown VK_RETURN: RequestEditSession succeeded, hrSession=0x%08X", hrSession);
            }
            
            pCommit->Release();
            
            // Clear state
            _sComposition.clear();
            _candidateList.clear();
            _selectedCandidateIndex = 0;
            if (_pCandidateWindow) _pCandidateWindow->Hide();
        }
        else
        {
            // Pass enter to application
            *pfEaten = FALSE;
        }
    }
    else if (wParam == VK_UP) {
        // Navigate up in candidate list
        if (!_sComposition.empty() && _candidateList.size() > 0) {
            if (_selectedCandidateIndex == 0) {
                _selectedCandidateIndex = (int)_candidateList.size() - 1;
            } else {
                _selectedCandidateIndex--;
            }
            
            DebugLog(L"OnKeyDown VK_UP: selectedIndex=%d", _selectedCandidateIndex);
            
            // Refresh candidate window with new selection
            if (_pCandidateWindow && _pCandidateWindow->IsVisible()) {
                _pCandidateWindow->Show(_lastCandidateX, _lastCandidateY, _candidateList, _selectedCandidateIndex);
            }
        }
    }
    else if (wParam == VK_DOWN) {
        // Navigate down in candidate list
        if (!_sComposition.empty() && _candidateList.size() > 0) {
            _selectedCandidateIndex = (_selectedCandidateIndex + 1) % _candidateList.size();
            
            DebugLog(L"OnKeyDown VK_DOWN: selectedIndex=%d", _selectedCandidateIndex);
            
            // Refresh candidate window with new selection
            if (_pCandidateWindow && _pCandidateWindow->IsVisible()) {
                _pCandidateWindow->Show(_lastCandidateX, _lastCandidateY, _candidateList, _selectedCandidateIndex);
            }
        }
    }
    else if (wParam == VK_ESCAPE) {
        _sComposition.clear();
        _candidateList.clear();
        _selectedCandidateIndex = 0;
        _EndComposition(pic);
        if (_pCandidateWindow) _pCandidateWindow->Hide();
    }
    
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

// ITfCompositionSink Implementation
STDMETHODIMP CTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition)
{
    if (_pComposition) {
        _pComposition->Release();
        _pComposition = NULL;
    }
    _sComposition.clear();
    return S_OK;
}

// Helpers
HRESULT CTextService::_InitKeyEventSink()
{
    ITfKeystrokeMgr *pKeystrokeMgr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr)))
    {
        pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink *)this, TRUE);
        pKeystrokeMgr->Release();
        return S_OK;
    }
    return E_FAIL;
}

void CTextService::_UninitKeyEventSink()
{
    ITfKeystrokeMgr *pKeystrokeMgr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr)))
    {
        pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
        pKeystrokeMgr->Release();
    }
}

HRESULT CTextService::_UpdateComposition(ITfContext *pContext)
{
    CUpdateCompositionEditSession *pEditSession = new CUpdateCompositionEditSession(this, pContext, _sComposition);
    HRESULT hr = pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, NULL);
    pEditSession->Release();
    return hr;
}

HRESULT CTextService::_EndComposition(ITfContext *pContext)
{
    CEndCompositionEditSession *pEditSession = new CEndCompositionEditSession(this, pContext);
    HRESULT hr = pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, NULL);
    pEditSession->Release();
    return hr;
}

void CTextService::_UpdateCandidateWindow(ITfContext *pContext)
{
    if (!_pCandidateWindow) return;

    DebugLog(L"_UpdateCandidateWindow: Composition=%s", _sComposition.c_str());

    // 1. Query candidates from DB
    _candidateList = CDictionaryEngine::Instance().Query(_sComposition);
    
    // Reset selection index when candidate list changes
    _selectedCandidateIndex = 0;
    
    DebugLog(L"_UpdateCandidateWindow: Query returned %d candidates", _candidateList.size());

    // Fallback if no result
    if (_candidateList.empty()) {
        _candidateList.push_back(_sComposition);
        DebugLog(L"_UpdateCandidateWindow: Added fallback candidate");
    }

    // 2. Get cursor position using multiple strategies
    RECT rc = {0, 0, 0, 0};
    ITfContextView *pView;
    
    if (SUCCEEDED(pContext->GetActiveView(&pView)))
    {
        DebugLog(L"_UpdateCandidateWindow: Got active view");
        
        // Strategy 1: Try GetTextExt with synchronous session
        CGetTextExtEditSession *pSession = new CGetTextExtEditSession(this, pContext, pView);
        HRESULT hrSession;
        HRESULT hr = pContext->RequestEditSession(_tfClientId, pSession, TF_ES_SYNC | TF_ES_READ, &hrSession);
        
        if (hr == TF_E_SYNCHRONOUS)
        {
            DebugLog(L"_UpdateCandidateWindow: Sync request rejected, trying async");
            hr = pContext->RequestEditSession(_tfClientId, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READ, NULL);
        }
        
        if (SUCCEEDED(hr))
        {
            rc = pSession->GetRect();
            DebugLog(L"_UpdateCandidateWindow: GetTextExt returned rect: (%d, %d, %d, %d)", 
                rc.left, rc.top, rc.right, rc.bottom);
        }
        else
        {
            DebugLog(L"_UpdateCandidateWindow: RequestEditSession failed, hr=0x%08X", hr);
        }
        
        pSession->Release();
        
        // Strategy 2: If rect is still empty, try GetGUIThreadInfo
        if (rc.left == 0 && rc.top == 0 && rc.right == 0 && rc.bottom == 0)
        {
            DebugLog(L"_UpdateCandidateWindow: GetTextExt failed, trying GetGUIThreadInfo");
            
            HWND hwnd = NULL;
            HRESULT hrGetWnd = pView->GetWnd(&hwnd);
            
            if (SUCCEEDED(hrGetWnd) && hwnd != NULL)
            {
                DebugLog(L"_UpdateCandidateWindow: pView->GetWnd returned hwnd=0x%p", hwnd);
                
                DWORD threadId = GetWindowThreadProcessId(hwnd, NULL);
                DebugLog(L"_UpdateCandidateWindow: Target window threadId=%d", threadId);
                
                GUITHREADINFO gti = {0};
                gti.cbSize = sizeof(gti);
                BOOL gtiRes = FALSE;
                
                if (threadId != 0)
                {
                    gtiRes = GetGUIThreadInfo(threadId, &gti);
                }
                
                DebugLog(L"_UpdateCandidateWindow: GetGUIThreadInfo returned %d, lastError=%d", gtiRes, GetLastError());
                
                if (gtiRes && gti.hwndFocus != NULL)
                {
                    // rcCaret is in client coordinates, convert to screen
                    POINT ptTopLeft = { gti.rcCaret.left, gti.rcCaret.top };
                    POINT ptBottomRight = { gti.rcCaret.right, gti.rcCaret.bottom };
                    
                    // Use focus window instead of hwnd for coordinate conversion
                    if (ClientToScreen(gti.hwndFocus, &ptTopLeft) && ClientToScreen(gti.hwndFocus, &ptBottomRight))
                    {
                        rc.left = ptTopLeft.x;
                        rc.top = ptTopLeft.y;
                        rc.right = ptBottomRight.x;
                        rc.bottom = ptBottomRight.y;
                        
                        DebugLog(L"_UpdateCandidateWindow: Used GetGUIThreadInfo caret (screen): (%d, %d, %d, %d)", 
                            rc.left, rc.top, rc.right, rc.bottom);
                    }
                    else
                    {
                        DebugLog(L"_UpdateCandidateWindow: ClientToScreen failed, error=%d", GetLastError());
                    }
                }
            }
            else
            {
                DebugLog(L"_UpdateCandidateWindow: pView->GetWnd failed or returned NULL, hr=0x%08X", hrGetWnd);
            }
            
            // Strategy 3: Try using foreground window if we still don't have position
            if (rc.left == 0 && rc.top == 0 && rc.right == 0 && rc.bottom == 0)
            {
                DebugLog(L"_UpdateCandidateWindow: Trying foreground window");
                
                HWND hwndFG = GetForegroundWindow();
                if (hwndFG != NULL)
                {
                    DWORD threadId = GetWindowThreadProcessId(hwndFG, NULL);
                    GUITHREADINFO gti = {0};
                    gti.cbSize = sizeof(gti);
                    
                    if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus != NULL)
                    {
                        POINT ptTopLeft = { gti.rcCaret.left, gti.rcCaret.top };
                        POINT ptBottomRight = { gti.rcCaret.right, gti.rcCaret.bottom };
                        
                        if (ClientToScreen(gti.hwndFocus, &ptTopLeft) && ClientToScreen(gti.hwndFocus, &ptBottomRight))
                        {
                            rc.left = ptTopLeft.x;
                            rc.top = ptTopLeft.y;
                            rc.right = ptBottomRight.x;
                            rc.bottom = ptBottomRight.y;
                            
                            DebugLog(L"_UpdateCandidateWindow: Used foreground window caret: (%d, %d, %d, %d)", 
                                rc.left, rc.top, rc.right, rc.bottom);
                        }
                    }
                }
            }
            
            // Strategy 4: Try GetCaretPos as last resort before mouse fallback
            if (rc.left == 0 && rc.top == 0 && rc.right == 0 && rc.bottom == 0)
            {
                DebugLog(L"_UpdateCandidateWindow: Trying GetCaretPos");
                
                POINT ptCaret;
                if (GetCaretPos(&ptCaret))
                {
                    HWND hwndFocus = GetFocus();
                    if (hwndFocus != NULL && ClientToScreen(hwndFocus, &ptCaret))
                    {
                        rc.left = ptCaret.x;
                        rc.top = ptCaret.y;
                        rc.right = rc.left + 2;
                        rc.bottom = rc.top + 20;
                        DebugLog(L"_UpdateCandidateWindow: Used GetCaretPos: (%d, %d)", rc.left, rc.top);
                    }
                }
                else
                {
                    DebugLog(L"_UpdateCandidateWindow: GetCaretPos failed, lastError=%d", GetLastError());
                }
            }
        }
        
        pView->Release();
    }
    else
    {
        DebugLog(L"_UpdateCandidateWindow: GetActiveView failed");
    }
    
    DebugLog(L"_UpdateCandidateWindow: Final cursor rect: (%d, %d, %d, %d)", rc.left, rc.top, rc.right, rc.bottom);

    // 3. Prepare show coordinates with fallback
    bool valid = !(rc.left == 0 && rc.top == 0 && rc.right == 0 && rc.bottom == 0);
    POINT showPt = {0, 0};
    
    if (valid)
    {
        // Prefer bottom-left of caret so candidate window appears below the caret
        showPt.x = rc.left;
        showPt.y = (rc.bottom != 0) ? rc.bottom : rc.top;
        DebugLog(L"_UpdateCandidateWindow: Using caret position: (%d, %d)", showPt.x, showPt.y);
    }
    else
    {
        // Strategy 5: Fallback to mouse cursor position
        POINT pt;
        if (GetCursorPos(&pt))
        {
            showPt = pt;
            showPt.y += 20; // offset to avoid covering text
            DebugLog(L"_UpdateCandidateWindow: Falling back to mouse position: (%d, %d)", showPt.x, showPt.y);
        }
        else
        {
            // Last resort: small offset from (0,0) to avoid top-left
            showPt.x = 100;
            showPt.y = 100;
            DebugLog(L"_UpdateCandidateWindow: GetCursorPos failed, using default position: (%d, %d)", showPt.x, showPt.y);
        }
    }

    DebugLog(L"_UpdateCandidateWindow: Final candidate window position: (%d, %d)", showPt.x, showPt.y);
    
    // Cache position for up/down key navigation
    _lastCandidateX = showPt.x;
    _lastCandidateY = showPt.y;
    
    _pCandidateWindow->Show(showPt.x, showPt.y, _candidateList, _selectedCandidateIndex);
}

// Public method for candidate window callback
void CTextService::CommitCandidate(ITfContext *pContext, int index)
{
    DebugLog(L"CommitCandidate: index=%d", index);
    
    // Validate parameters
    if (!pContext || index < 0 || index >= (int)_candidateList.size())
    {
        DebugLog(L"CommitCandidate: Invalid parameters, context=%p, index=%d, candidateList.size=%d", 
            pContext, index, _candidateList.size());
        return;
    }
    
    // Get the candidate text
    std::wstring commitText = _candidateList[index];
    DebugLog(L"CommitCandidate: Committing candidate='%s'", commitText.c_str());
    
    // Create commit session
    CCommitCompositionEditSession *pCommit = new CCommitCompositionEditSession(this, pContext, commitText);
    HRESULT hrSession;
    HRESULT hr = pContext->RequestEditSession(_tfClientId, pCommit, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
    
    if (hr == TF_E_SYNCHRONOUS)
    {
        DebugLog(L"CommitCandidate: Application rejected synchronous request, falling back to async");
        hr = pContext->RequestEditSession(_tfClientId, pCommit, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, NULL);
    }
    
    if (FAILED(hr))
    {
        DebugLog(L"CommitCandidate: RequestEditSession failed, hr=0x%08X", hr);
    }
    else
    {
        DebugLog(L"CommitCandidate: RequestEditSession succeeded, hrSession=0x%08X", hrSession);
    }
    
    pCommit->Release();
    
    // Clear state
    _sComposition.clear();
    _candidateList.clear();
    _selectedCandidateIndex = 0;
    if (_pCandidateWindow) _pCandidateWindow->Hide();
}

// Get current active context
ITfContext* CTextService::GetCurrentContext()
{
    if (!_pThreadMgr)
        return NULL;
        
    ITfDocumentMgr *pDocMgr = NULL;
    if (FAILED(_pThreadMgr->GetFocus(&pDocMgr)) || !pDocMgr)
        return NULL;
        
    ITfContext *pContext = NULL;
    pDocMgr->GetTop(&pContext);
    pDocMgr->Release();
    
    return pContext; // Caller must release
}
