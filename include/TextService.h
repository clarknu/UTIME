#pragma once
#include "Globals.h"
#include "CandidateWindow.h"

class CUpdateCompositionEditSession;
class CEndCompositionEditSession;

class CTextService : public ITfTextInputProcessor,
                     public ITfKeyEventSink,
                     public ITfCompositionSink
{
    friend class CUpdateCompositionEditSession;
    friend class CEndCompositionEditSession;

public:
    CTextService();
    ~CTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr *ptim, TfClientId tid);
    STDMETHODIMP Deactivate();

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground);
    STDMETHODIMP OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten);

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition);

    // Helper
    TfClientId GetClientId() { return _tfClientId; }

private:
    HRESULT _InitKeyEventSink();
    void _UninitKeyEventSink();

    HRESULT _UpdateComposition(ITfContext *pContext);
    HRESULT _EndComposition(ITfContext *pContext);
    
    // New: Update Candidate Window
    void _UpdateCandidateWindow(ITfContext *pContext);

    long _cRef;
    ITfThreadMgr *_pThreadMgr;
    TfClientId _tfClientId;
    DWORD _dwCookieKey;

    ITfComposition *_pComposition;
    std::wstring _sComposition; // Current internal buffer (e.g. "nihao")
    
    // UI
    CCandidateWindow *_pCandidateWindow;
    std::vector<std::wstring> _candidateList;
};
