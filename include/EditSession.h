#pragma once
#include "Globals.h"
#include "TextService.h"

class CEditSessionBase : public ITfEditSession
{
public:
    CEditSessionBase(CTextService *pTextService, ITfContext *pContext);
    virtual ~CEditSessionBase();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec) = 0;

protected:
    CTextService *_pTextService;
    ITfContext *_pContext;
    long _cRef;
};

class CUpdateCompositionEditSession : public CEditSessionBase
{
public:
    CUpdateCompositionEditSession(CTextService *pTextService, ITfContext *pContext, const std::wstring &text);
    STDMETHODIMP DoEditSession(TfEditCookie ec);
private:
    std::wstring _text;
};

class CEndCompositionEditSession : public CEditSessionBase
{
public:
    CEndCompositionEditSession(CTextService *pTextService, ITfContext *pContext);
    STDMETHODIMP DoEditSession(TfEditCookie ec);
};
