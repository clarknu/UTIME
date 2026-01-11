#include "TextService.h"
#include "EditSession.h"
#include "GetTextExtEditSession.h"
#include "DictionaryEngine.h"
#include <fstream>

void DebugLog(const wchar_t* format, ...)
{
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, 1024, format, args);
    va_end(args);

    OutputDebugString(buffer);
    
    // Also write to file for persistence
    std::wofstream logFile("C:\\Windows\\Temp\\UTIME_Debug.log", std::ios::app);
    if (logFile.is_open())
    {
        logFile << buffer << std::endl;
    }
}

CTextService::CTextService() 
    : _cRef(1), 
      _pThreadMgr(NULL), 
      _tfClientId(TF_CLIENTID_NULL), 
      _dwCookieKey(TF_INVALID_COOKIE),
      _pComposition(NULL),
      _pCandidateWindow(NULL)
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
    // If composition is active, eat Space/Enter/Esc/Back
    if (_sComposition.length() > 0) {
        if (wParam == VK_SPACE || wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_BACK) {
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
        _UpdateComposition(pic);
        _UpdateCandidateWindow(pic);
    }
    else if (wParam == VK_BACK) {
        if (!_sComposition.empty()) {
            _sComposition.pop_back();
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
    else if (wParam == VK_SPACE) {
        if (!_sComposition.empty()) {
            // Commit first candidate
            std::wstring commitText = _sComposition; // Default: commit raw pinyin
            
            if (_candidateList.size() > 0)
            {
                // Format: "1. text" -> we need "text"
                // Assuming _candidateList contains raw text "text"
                
                // If the first candidate is the same as composition (meaning no match found), keep it
                // Otherwise use the candidate
                commitText = _candidateList[0];
            }
            
            // Update text first
            CUpdateCompositionEditSession *pUpdate = new CUpdateCompositionEditSession(this, pic, commitText);
            pic->RequestEditSession(_tfClientId, pUpdate, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, NULL);
            pUpdate->Release();
            
            // Then end composition
            _EndComposition(pic);
            _sComposition.clear();
            _candidateList.clear();
            if (_pCandidateWindow) _pCandidateWindow->Hide();
        }
        else
        {
            // Pass space to application
            *pfEaten = FALSE;
        }
    }
    else if (wParam == VK_ESCAPE) {
        _sComposition.clear();
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
    
    DebugLog(L"Query returned %d candidates", _candidateList.size());

    // Fallback if no result
    if (_candidateList.empty()) {
        _candidateList.push_back(_sComposition);
        DebugLog(L"Added fallback candidate");
    }

    // 2. Get cursor position
    RECT rc = {0, 0, 0, 0};
    ITfContextView *pView;
    if (SUCCEEDED(pContext->GetActiveView(&pView)))
    {
        CGetTextExtEditSession *pSession = new CGetTextExtEditSession(this, pContext, pView);
        pContext->RequestEditSession(_tfClientId, pSession, TF_ES_SYNC | TF_ES_READ, NULL);
        rc = pSession->GetRect();
        pSession->Release();
        
        // Map Client to Screen Coordinates
        // GetTextExt *usually* returns screen coordinates, but if it fails or returns 0, we need to handle it.
        // Also, we need to get the window handle of the active view to map coordinates if necessary.
        HWND hwnd;
        if (SUCCEEDED(pView->GetWnd(&hwnd)))
        {
             // If rc is relative to client, we might need to map it. 
             // However, TSF spec says GetTextExt is in Screen Coordinates.
             // But sometimes it returns 0,0 if the range is not visible or valid.
             if (rc.left == 0 && rc.top == 0)
             {
                 // Try to get caret pos from system
                 POINT ptCaret;
                 if (GetCaretPos(&ptCaret))
                 {
                     ClientToScreen(hwnd, &ptCaret);
                     rc.left = ptCaret.x;
                     rc.bottom = ptCaret.y + 20; // Assume height
                     DebugLog(L"Used GetCaretPos fallback: (%d, %d)", rc.left, rc.bottom);
                 }
             }
        }
        
        pView->Release();
    }
    
    DebugLog(L"Cursor Rect: (%d, %d, %d, %d)", rc.left, rc.top, rc.right, rc.bottom);

    // 3. Show Window
    // Fallback if we can't get position (e.g. console apps sometimes)
    if (rc.left == 0 && rc.top == 0 && rc.right == 0 && rc.bottom == 0)
    {
        POINT pt;
        GetCursorPos(&pt);
        rc.left = pt.x;
        rc.bottom = pt.y + 20;
        DebugLog(L"Using Mouse Position: (%d, %d)", rc.left, rc.bottom);
    }

    _pCandidateWindow->Show(rc.left, rc.bottom, _candidateList, 0);
}
