#include "EditSession.h"

// CEditSessionBase
CEditSessionBase::CEditSessionBase(CTextService *pTextService, ITfContext *pContext) 
    : _cRef(1), _pTextService(pTextService), _pContext(pContext)
{
    _pTextService->AddRef();
    _pContext->AddRef();
}

CEditSessionBase::~CEditSessionBase()
{
    _pTextService->Release();
    _pContext->Release();
}

STDMETHODIMP CEditSessionBase::QueryInterface(REFIID riid, void **ppvObj)
{
    if (ppvObj == NULL) return E_INVALIDARG;
    *ppvObj = NULL;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession))
    {
        *ppvObj = (void *)this;
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CEditSessionBase::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CEditSessionBase::Release()
{
    long cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

// CUpdateCompositionEditSession
CUpdateCompositionEditSession::CUpdateCompositionEditSession(CTextService *pTextService, ITfContext *pContext, const std::wstring &text)
    : CEditSessionBase(pTextService, pContext), _text(text)
{
}

STDMETHODIMP CUpdateCompositionEditSession::DoEditSession(TfEditCookie ec)
{
    // If no composition, start one
    if (_pTextService->_pComposition == NULL)
    {
        DebugLog(L"CUpdateCompositionEditSession: No composition, starting new one");
        
        // First get the current selection to use as composition range
        ITfRange *pRangeInsert = NULL;
        TF_SELECTION tfSel;
        ULONG cFetched;
        
        if (SUCCEEDED(_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSel, &cFetched)) && cFetched > 0)
        {
            pRangeInsert = tfSel.range;
            DebugLog(L"CUpdateCompositionEditSession: Got selection range");
        }
        else
        {
            DebugLog(L"CUpdateCompositionEditSession: Failed to get selection, trying InsertAtSelection");
            // Fallback: use InsertAtSelection
            ITfInsertAtSelection *pInsertAtSelection;
            if (SUCCEEDED(_pContext->QueryInterface(IID_ITfInsertAtSelection, (void **)&pInsertAtSelection)))
            {
                pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pRangeInsert);
                pInsertAtSelection->Release();
            }
        }
        
        if (pRangeInsert)
        {
            ITfContextComposition *pContextComposition;
            if (SUCCEEDED(_pContext->QueryInterface(IID_ITfContextComposition, (void **)&pContextComposition)))
            {
                HRESULT hr = pContextComposition->StartComposition(ec, pRangeInsert, (ITfCompositionSink *)_pTextService, &_pTextService->_pComposition);
                DebugLog(L"CUpdateCompositionEditSession: StartComposition returned hr=0x%08X, _pComposition=%p", hr, _pTextService->_pComposition);
                pContextComposition->Release();
            }
            pRangeInsert->Release();
        }
        else
        {
            DebugLog(L"CUpdateCompositionEditSession: Failed to get insert range");
        }
    }

    // 2. Set text in composition range
    if (_pTextService->_pComposition)
    {
        ITfRange *pRange;
        HRESULT hr = _pTextService->_pComposition->GetRange(&pRange);
        if (SUCCEEDED(hr) && pRange)
        {
            hr = pRange->SetText(ec, 0, _text.c_str(), (LONG)_text.length());
            DebugLog(L"CUpdateCompositionEditSession: SetText returned hr=0x%08X", hr);
            
            // Adjust selection to end of composition
            ITfRange *pSelection;
            if (SUCCEEDED(pRange->Clone(&pSelection)))
            {
                pSelection->Collapse(ec, TF_ANCHOR_END);
                TF_SELECTION tfSelection;
                tfSelection.range = pSelection;
                tfSelection.style.ase = TF_AE_NONE;
                tfSelection.style.fInterimChar = FALSE;
                _pContext->SetSelection(ec, 1, &tfSelection);
                pSelection->Release();
            }
            pRange->Release();
        }
        else
        {
            DebugLog(L"CUpdateCompositionEditSession: GetRange failed, hr=0x%08X", hr);
        }
    }
    else
    {
        DebugLog(L"CUpdateCompositionEditSession: _pComposition is still NULL after StartComposition");
    }
    return S_OK;
}

// CEndCompositionEditSession
CEndCompositionEditSession::CEndCompositionEditSession(CTextService *pTextService, ITfContext *pContext)
    : CEditSessionBase(pTextService, pContext)
{
}

STDMETHODIMP CEndCompositionEditSession::DoEditSession(TfEditCookie ec)
{
    if (_pTextService->_pComposition)
    {
        _pTextService->_pComposition->EndComposition(ec);
        _pTextService->_pComposition->Release();
        _pTextService->_pComposition = NULL;
        DebugLog(L"CEndCompositionEditSession: Composition ended");
    }
    return S_OK;
}

// CCommitCompositionEditSession
CCommitCompositionEditSession::CCommitCompositionEditSession(CTextService *pTextService, ITfContext *pContext, const std::wstring &text)
    : CEditSessionBase(pTextService, pContext), _commitText(text)
{
}

STDMETHODIMP CCommitCompositionEditSession::DoEditSession(TfEditCookie ec)
{
    // If no composition exists, try to create one and commit directly
    if (_pTextService->_pComposition == NULL)
    {
        DebugLog(L"CCommitCompositionEditSession: No composition, inserting text directly");
        
        // Try to insert text directly using InsertAtSelection
        ITfInsertAtSelection *pInsertAtSelection;
        if (SUCCEEDED(_pContext->QueryInterface(IID_ITfInsertAtSelection, (void **)&pInsertAtSelection)))
        {
            ITfRange *pRange;
            HRESULT hr = pInsertAtSelection->InsertTextAtSelection(ec, 0, _commitText.c_str(), (LONG)_commitText.length(), &pRange);
            DebugLog(L"CCommitCompositionEditSession: InsertTextAtSelection hr=0x%08X", hr);
            if (SUCCEEDED(hr) && pRange)
            {
                // Move selection to end of inserted text
                pRange->Collapse(ec, TF_ANCHOR_END);
                TF_SELECTION tfSelection;
                tfSelection.range = pRange;
                tfSelection.style.ase = TF_AE_NONE;
                tfSelection.style.fInterimChar = FALSE;
                _pContext->SetSelection(ec, 1, &tfSelection);
                pRange->Release();
            }
            pInsertAtSelection->Release();
            return S_OK;
        }
        else
        {
            DebugLog(L"CCommitCompositionEditSession: Failed to get ITfInsertAtSelection");
            return E_FAIL;
        }
    }

    // 2. Get composition range
    ITfRange *pRange;
    HRESULT hr = _pTextService->_pComposition->GetRange(&pRange);
    if (FAILED(hr) || !pRange)
    {
        DebugLog(L"CCommitCompositionEditSession: GetRange failed, hr=0x%08X", hr);
        return hr;
    }

    // 3. Update composition text to final content
    hr = pRange->SetText(ec, 0, _commitText.c_str(), (LONG)_commitText.length());
    if (FAILED(hr))
    {
        DebugLog(L"CCommitCompositionEditSession: SetText failed, hr=0x%08X", hr);
        // Continue to end composition even if SetText fails
    }
    else
    {
        DebugLog(L"CCommitCompositionEditSession: SetText succeeded, text=%s", _commitText.c_str());
    }

    // 4. Adjust selection to end of composition
    ITfRange *pSelection;
    if (SUCCEEDED(pRange->Clone(&pSelection)))
    {
        pSelection->Collapse(ec, TF_ANCHOR_END);
        TF_SELECTION tfSelection;
        tfSelection.range = pSelection;
        tfSelection.style.ase = TF_AE_NONE;
        tfSelection.style.fInterimChar = FALSE;
        _pContext->SetSelection(ec, 1, &tfSelection);
        pSelection->Release();
    }

    // End composition (using same edit cookie)
    hr = _pTextService->_pComposition->EndComposition(ec);
    if (FAILED(hr))
    {
        DebugLog(L"CCommitCompositionEditSession: EndComposition failed, hr=0x%08X", hr);
    }

    // 6. Cleanup composition pointer
    _pTextService->_pComposition->Release();
    _pTextService->_pComposition = NULL;
    
    // 7. Cleanup
    pRange->Release();
    return S_OK;
}
