/*
 * Xbox Game Runtime: XGameProtocol
 *
 * Derived from LukasPAH/WineGDK under the LGPL-2.1-or-later license.
 */

#ifndef XGAMEPROTOCOL_H
#define XGAMEPROTOCOL_H

#include "../../private.h"
#include <string.h>

struct x_gameprotocol
{
    IXGameProtocolImpl IXGameProtocolImpl_iface;
    LONG ref;
};

#endif
