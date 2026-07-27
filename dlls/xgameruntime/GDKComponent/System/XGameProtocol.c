/*
 * Xbox Game Runtime: XGameProtocol
 *
 * Derived from LukasPAH/WineGDK under the LGPL-2.1-or-later license.
 */

#include "XGameProtocol.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);

static inline struct x_gameprotocol *impl_from_IXGameProtocolImpl(IXGameProtocolImpl *iface)
{
    return CONTAINING_RECORD(iface, struct x_gameprotocol, IXGameProtocolImpl_iface);
}

static HRESULT WINAPI x_gameprotocol_QueryInterface(IXGameProtocolImpl *iface, REFIID iid, void **out)
{
    struct x_gameprotocol *impl = impl_from_IXGameProtocolImpl(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);
    if (!out) return E_POINTER;

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IXGameProtocolImpl))
    {
        *out = &impl->IXGameProtocolImpl_iface;
        IXGameProtocolImpl_AddRef(&impl->IXGameProtocolImpl_iface);
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI x_gameprotocol_AddRef(IXGameProtocolImpl *iface)
{
    struct x_gameprotocol *impl = impl_from_IXGameProtocolImpl(iface);
    return InterlockedIncrement(&impl->ref);
}

static ULONG WINAPI x_gameprotocol_Release(IXGameProtocolImpl *iface)
{
    struct x_gameprotocol *impl = impl_from_IXGameProtocolImpl(iface);
    return InterlockedDecrement(&impl->ref);
}

static HRESULT WINAPI x_gameprotocol_RegisterForActivation(IXGameProtocolImpl *iface,
        XTaskQueueHandle queue, void *context, void *callback,
        XTaskQueueRegistrationToken *token)
{
    FIXME("iface %p, queue %p, context %p, callback %p, token %p stub.\n",
          iface, queue, context, callback, token);
    if (token) memset(token, 0, sizeof(*token));
    return S_OK;
}

static BOOLEAN WINAPI x_gameprotocol_UnregisterForActivation(IXGameProtocolImpl *iface,
        XTaskQueueRegistrationToken *token, boolean wait)
{
    FIXME("iface %p, token %p, wait %d stub.\n", iface, token, wait);
    return TRUE;
}

static void WINAPI x_gameprotocol_ActivationCallback(IXGameProtocolImpl *iface,
        void *context, LPSTR protocol_uri)
{
    FIXME("iface %p, context %p, protocol_uri %s stub.\n",
          iface, context, debugstr_a(protocol_uri));
}

static const IXGameProtocolImplVtbl x_gameprotocol_vtbl =
{
    x_gameprotocol_QueryInterface,
    x_gameprotocol_AddRef,
    x_gameprotocol_Release,
    x_gameprotocol_RegisterForActivation,
    x_gameprotocol_UnregisterForActivation,
    x_gameprotocol_ActivationCallback,
};

static struct x_gameprotocol x_gameprotocol =
{
    { &x_gameprotocol_vtbl },
    1
};

IXGameProtocolImpl *x_gameprotocol_impl = &x_gameprotocol.IXGameProtocolImpl_iface;
