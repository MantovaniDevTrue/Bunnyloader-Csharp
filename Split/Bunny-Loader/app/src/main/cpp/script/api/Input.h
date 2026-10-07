#pragma once
#include "script/bridge/ScriptEngine.h"

#include <string>
#include <vector>

#if BL_HAVE_QUICKJS
#include "quickjs.h"
#endif

namespace bl::script {

std::vector<std::string> modInputButtons();
void setModInputState(const std::string& id, bool down);

#if BL_HAVE_QUICKJS
void installInputApi(JSContext* ctx, JSValue bl);
#endif

}
