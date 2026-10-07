#pragma once
#include "script/bridge/ScriptEngine.h"

#include <string>
#include <vector>

namespace bl::script {

std::vector<std::string> modInputButtons();
void setModInputState(const std::string& id, bool down);

#if BL_HAVE_QUICKJS
#include "quickjs.h"
void installInputApi(JSContext* ctx, JSValue bl);
#endif

}
