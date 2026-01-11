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
    // 1. If no composition, start one
    if (_pTextService->_pComposition == NULL)
    {
        ITfContextComposition *pContextComposition;
        if (SUCCEEDED(_pContext->QueryInterface(IID_ITfContextComposition, (void **)&pContextComposition)))
        {
            pContextComposition->StartComposition(ec, NULL, (ITfCompositionSink *)_pTextService, &_pTextService->_pComposition);
            pContextComposition->Release();
        }
    }

    // 2. Set text in composition range
    if (_pTextService->_pComposition)
    {
        ITfRange *pRange;
        if (SUCCEEDED(_pTextService->_pComposition->GetRange(&pRange)))
        {
            pRange->SetText(ec, 0, _text.c_str(), (LONG)_text.length());
            
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
    }
    return S_OK;
}
