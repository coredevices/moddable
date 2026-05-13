/*
 * Copyright (c) 2016-2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Runtime.
 * 
 *   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 * 
 *   The Moddable SDK Runtime is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 * 
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "xsAll.h"
#include "xsmc.h"
#include "mc.xs.h"

#include "xsffi.h"
#include "moddableAppState.h"

#include "mcu/privilege.h"

#define FFI_MAX_BINDINGS 32

typedef struct {
	txSlot		*instance;
	txCallback	user_cb;
} FFIBinding;

static FFIBinding s_ffi_bindings[FFI_MAX_BINDINGS];
static int s_ffi_binding_count;

typedef struct {
	txMachine	*the;
	txCallback	fn;
} FFIInvokeCtx;

static void prv_invoke_unprivileged(void *raw)
{
	FFIInvokeCtx *ctx = raw;
	(ctx->fn)(ctx->the);
}

static void prv_ffi_shim(txMachine *the)
{
	txSlot *instance = mxFunction->value.reference;
	txCallback user_cb = C_NULL;
	for (int i = 0; i < s_ffi_binding_count; i++) {
		if (s_ffi_bindings[i].instance == instance) {
			user_cb = s_ffi_bindings[i].user_cb;
			break;
		}
	}
	if (C_NULL == user_cb)
		xsUnknownError("FFI binding not resolved");

	FFIInvokeCtx ctx = { the, user_cb };
	mcu_call_unprivileged(prv_invoke_unprivileged, &ctx);
}

static txSlot *prv_newHostFunctionFFI(txMachine *the, txCallback user_cb,
		txInteger length, txInteger name, txInteger profileID)
{
	if (s_ffi_binding_count >= FFI_MAX_BINDINGS)
		xsRangeError("too many FFI bindings");

	txSlot *instance = fxNewHostFunction(the, prv_ffi_shim, length, name, profileID);
	s_ffi_bindings[s_ffi_binding_count].instance = instance;
	s_ffi_bindings[s_ffi_binding_count].user_cb = user_cb;
	s_ffi_binding_count++;
	return instance;
}

void FFI_constructor(xsMachine* the)
{
	extern txAPI gxAPI;

	txBuildFFI fxBuildFFI = getModdableAppState(fxBuildFFI);
	if (!fxBuildFFI) {
		xsResult = xsUndefined;
		return;
	}

	s_ffi_binding_count = 0;

	static txAPI gxAPI_ffi;
	gxAPI_ffi = gxAPI;
	gxAPI_ffi.newHostFunction = prv_newHostFunctionFFI;

	(fxBuildFFI)(the, &gxAPI_ffi);
}

void FFI_destructor(void* /* it */)
{
}
