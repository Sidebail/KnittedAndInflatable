#include "sfConfigTranslator.h"
#include "../../Public/sfConfig.h"
#include "../../Public/Consts.h"
#include "../../Public/SceneFusion.h"


void sfConfigTranslator::Initialize()
{
    if (SceneFusion::IsSessionCreator)
    {
        sfDictionaryProperty::SPtr propsPtr = sfDictionaryProperty::Create();
        sfObject::SPtr objPtr = sfObject::Create(sfType::Config, propsPtr);
        propsPtr->Set(sfProp::SyncBlueprint, sfValueProperty::Create(sfConfig::Get().SyncBlueprint));
        propsPtr->Set(sfProp::SyncLandscape, sfValueProperty::Create(sfConfig::Get().SyncLandscape));
        SceneFusion::Service->Session()->Create(objPtr);
    }
}

void sfConfigTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    bool changed = false;
    sfDictionaryProperty::SPtr propsPtr = objPtr->Property()->AsDict();

    bool syncBlueprint = propsPtr->Get(sfProp::SyncBlueprint)->AsValue()->GetValue();
    if (syncBlueprint != sfConfig::Get().SyncBlueprint)
    {
        sfConfig::Get().SyncBlueprint = syncBlueprint;
        changed = true;
    }

    bool syncLandscape = propsPtr->Get(sfProp::SyncLandscape)->AsValue()->GetValue();
    if (syncLandscape != sfConfig::Get().SyncLandscape)
    {
        sfConfig::Get().SyncLandscape = syncLandscape;
        changed = true;
    }

    if (changed)
    {
        sfConfig::Get().Save();
    }
}