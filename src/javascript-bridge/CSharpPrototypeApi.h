#pragma once

#include "script/bridge/ScriptEngine.h"

#if BL_HAVE_QUICKJS
#include "quickjs.h"
#endif

namespace bl::script {

#if BL_HAVE_QUICKJS
void installCSharpPrototypeApi(JSContext* ctx, JSValueConst bl);
#endif

}
