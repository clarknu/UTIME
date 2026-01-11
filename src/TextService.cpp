#include "TextService.h"
#include "EditSession.h"
#include "GetTextExtEditSession.h"
#include "DictionaryEngine.h"

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
            std::wstring commitText = _sComposition;
            if (_candidateList.size() > 0)
            {
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

    // 1. Query candidates from DB
    _candidateList = CDictionaryEngine::Instance().Query(_sComposition);
    
    // Fallback if no result
    if (_candidateList.empty()) {
        _candidateList.push_back(_sComposition);
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
        pView->Release();
    }
    
    // 3. Show Window
    // Convert client coordinates to screen coordinates
    // Wait, GetTextExt returns Screen Coordinates? TSF documentation says:
    // "The returned rectangle is in screen coordinates." - Yes.
    
    if (rc.left == 0 && rc.top == 0)
    {
        // Fallback if we can't get position (e.g. console apps sometimes)
        POINT pt;
        GetCursorPos(&pt);
        rc.left = pt.x;
        rc.bottom = pt.y + 20;
    }

    _pCandidateWindow->Show(rc.left, rc.bottom, _candidateList, 0);
}
