#pragma once
#include "experiments/csharp/NativeStaticValue.h"
#include "experiments/csharp/NativeTooltipPanelTexturePath.h"
#include <string>

namespace bl::csharp {

struct NativeTooltipPanelSettings {
    native_static::Value instance;
    Il2CppClass* owner=nullptr;

    bool bind(Il2CppClass* cls,std::string* error) {
        *this={};
        if(!error)return false;
        auto& a=il2cpp::api();
        if(!cls||!a.class_get_namespace||!a.class_get_name||
           !a.class_is_valuetype||!a.object_get_class||!a.class_is_assignable_from) {
            *error="Reason=NativeTooltipSettingsMetadataUnavailable";
            return false;
        }
        const char* ns=a.class_get_namespace(cls);
        const char* name=a.class_get_name(cls);
        if(!ns||!name||std::strcmp(ns,"")||std::strcmp(name,"Settings")||
           a.class_is_valuetype(cls)) {
            *error="Reason=NativeTooltipSettingsOwnerMismatch";
            return false;
        }
        auto source=native_static::resolve(cls,"Instance");
        if(!source||source.type!=cls||native_static::size(source)!=sizeof(Il2CppObject*)) {
            *error="Reason=NativeTooltipSettingsInstanceUnavailable";
            return false;
        }
        instance=source;owner=cls;return true;
    }

    bool read(Il2CppObject** output) const {
        auto& a=il2cpp::api();
        if(!output||!instance||!owner||!a.object_get_class||
           !a.class_is_assignable_from)return false;
        Il2CppObject* value=nullptr;
        if(!native_static::read(instance,&value,sizeof(value))||!value||
           !a.class_is_assignable_from(owner,a.object_get_class(value)))return false;
        *output=value;return true;
    }
};

struct NativeTooltipPanelResources {
    Il2CppObject* settings=nullptr;
    Il2CppObject* tooltipBacking=nullptr;
    Il2CppObject* fillTexture=nullptr;
    Il2CppObject* borderTexture=nullptr;
};

struct NativeTooltipPanelResourcePath {
    NativeTooltipPanelSettings root;
    NativeTooltipPanelTexturePath textures;

    bool bind(Il2CppClass* settingsClass,std::string* error) {
        *this={};
        if(!root.bind(settingsClass,error)||!textures.bind(settingsClass,error))return false;
        return true;
    }

    bool read(NativeTooltipPanelResources* result) const {
        if(!result)return false;
        NativeTooltipPanelResources staged;
        Il2CppObject* tips=nullptr;
        if(!root.read(&staged.settings)||
           !textures.tooltips.read(staged.settings,&tips)||
           !textures.backing.read(tips,&staged.tooltipBacking))return false;
        Il2CppObject *fill=nullptr,*border=nullptr;
        if(!textures.fill.read(staged.tooltipBacking,&fill)||
           !textures.border.read(staged.tooltipBacking,&border)||
           !textures.fillTexture.read(fill,&staged.fillTexture)||
           !textures.borderTexture.read(border,&staged.borderTexture))return false;
        *result=staged;return true;
    }
};

}
