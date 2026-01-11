#pragma once
#include "Globals.h"
#include "TextService.h"

class CGetTextExtEditSession : public CEditSessionBase
{
public:
    CGetTextExtEditSession(CTextService *pTextService, ITfContext *pContext, ITfContextView *pView)
        : CEditSessionBase(pTextService, pContext), _pView(pView)
    {
        _pView->AddRef();
        memset(&_rc, 0, sizeof(_rc));
    }
    
    ~CGetTextExtEditSession()
    {
        _pView->Release();
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec)
    {
        TF_SELECTION tfSelection;
        ULONG cFetched;
        if (SUCCEEDED(_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSelection, &cFetched)) && cFetched > 0)
        {
            BOOL fClipped;
            _pView->GetTextExt(ec, tfSelection.range, &_rc, &fClipped);
            tfSelection.range->Release();
        }
        return S_OK;
    }

    RECT GetRect() { return _rc; }

private:
    ITfContextView *_pView;
    RECT _rc;
};
