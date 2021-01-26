#include "../../Public/Translators/sfBlueprintTranslator.h"
#include "../../Public/Translators/sfActorTranslator.h"
#include "../../Public/Translators/sfComponentTranslator.h"
#include "../../Public/Consts.h"
#include "../../Public/sfPropertyUtil.h"
#include "../../Public/sfPropertyManager.h"
#include "../../Public/sfObjectMap.h"
#include "../../Public/SceneFusion.h"
#include "../../Public/sfUnrealUtils.h"
#include "../UI/sfDetailsPanelManager.h"
#include "../../Public/sfConfig.h"

#include <Engine/Blueprint.h>
#include <Engine/LevelScriptBlueprint.h>
#include <Kismet2/BlueprintEditorUtils.h>
#include <Kismet2/KismetEditorUtilities.h>
#include <Engine/InheritableComponentHandler.h>

#define LOG_CHANNEL "sfBlueprintTranslator"

sfBlueprintTranslator::sfBlueprintTranslator() :
    m_sessionPtr { nullptr }
{
    RegisterPropertyChangeHandlers();
}

sfBlueprintTranslator::~sfBlueprintTranslator()
{

}

void sfBlueprintTranslator::Initialize()
{
    if (!sfConfig::Get().SyncBlueprint ||
        SceneFusion::Get().Service->Session()->GetObjectLimit(sfType::Blueprint) == 0)
    {
        return;
    }
    m_sessionPtr = SceneFusion::Service->Session();
    m_tickHandle = SceneFusion::Get().OnTick.AddRaw(this, &sfBlueprintTranslator::Tick);
    m_onPropertyChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
        this, &sfBlueprintTranslator::OnUObjectPropertyChange);
}

void sfBlueprintTranslator::CleanUp()
{
    if (!sfConfig::Get().SyncBlueprint ||
        SceneFusion::Get().Service->Session()->GetObjectLimit(sfType::Blueprint) == 0)
    {
        return;
    }
    m_dirtyBlueprints.Empty();
    m_structurallyModifiedBlueprints.Empty();
    SceneFusion::Get().OnTick.Remove(m_tickHandle);
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(m_onPropertyChangeHandle);
}

void sfBlueprintTranslator::Tick(float deltaTime)
{
    SendChanges();

    // Mark blueprint as structurally modified if its component hierarchy changed.
    if (m_structurallyModifiedBlueprints.Num() > 0)
    {
        FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(m_onPropertyChangeHandle);
        for (UBlueprint* blueprintPtr : m_structurallyModifiedBlueprints)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(blueprintPtr);
            FBlueprintEditorUtils::PostEditChangeBlueprintActors(blueprintPtr, true);
        }
        SceneFusion::RedrawActiveViewport();
        m_structurallyModifiedBlueprints.Empty();
        m_onPropertyChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
            this, &sfBlueprintTranslator::OnUObjectPropertyChange);
    }
}

void sfBlueprintTranslator::UploadBlueprint(UBlueprint* blueprintPtr)
{
    if (!sfConfig::Get().SyncBlueprint ||
        SceneFusion::Get().Service->Session()->GetObjectLimit(sfType::Blueprint) == 0 ||
        blueprintPtr == nullptr ||
        sfObjectMap::Contains(blueprintPtr))
    {
        return;
    }

    // Upload parent blueprint first
    UClass* parentClass = blueprintPtr->ParentClass;
    if (parentClass == nullptr)
    {
        parentClass = blueprintPtr->GeneratedClass->GetSuperClass();
    }
    if (parentClass != nullptr)
    {
        UBlueprint* parentPtr = UBlueprint::GetBlueprintFromClass(parentClass);
        UploadBlueprint(parentPtr);
    }

    // Create sfObject for the UBlueprint object
    sfDictionaryProperty::SPtr blueprintPropertiesPtr = sfDictionaryProperty::Create();
    sfObject::SPtr blueprintObjPtr = sfObject::Create(sfType::Blueprint, blueprintPropertiesPtr);
    blueprintPropertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(blueprintPtr->GetOuter()->GetName()));
    sfPropertyManager::Get().CreateProperties(blueprintPtr, blueprintPropertiesPtr);
    sfObjectMap::Add(blueprintObjPtr, blueprintPtr);

    // Create sfObject for the default object
    if (blueprintPtr->GeneratedClass != nullptr)
    {
        UObject* defaultObjectPtr = blueprintPtr->GeneratedClass->GetDefaultObject();
        if (defaultObjectPtr != nullptr)
        {
            sfObject::SPtr sfObjPtr = nullptr;
            AActor* actorPtr = Cast<AActor>(defaultObjectPtr);
            if (actorPtr != nullptr)
            {
                TSharedPtr<sfActorTranslator> actorTranslatorPtr
                    = SceneFusion::Get().GetTranslator<sfActorTranslator>(sfType::Actor);
                if (actorTranslatorPtr.IsValid())
                {
                    // Upload default actor and inherited components
                    sfObjPtr = actorTranslatorPtr->CreateObject(actorPtr);
                }
            }
            if (sfObjPtr != nullptr)
            {
                blueprintObjPtr->AddChild(sfObjPtr);
            }

            // Sync component templates (archetypes).
            // Native(C++) compoents are handled with the default actor.
            // A non-native component has its USCS_NODE under the USimpleConstructionScript.
            // A non-native inherited component is recorded in the UInheritableComponentHandler.

            UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(blueprintPtr->GeneratedClass);

            // Overriden component templates (non-native inherited component)
            if (UInheritableComponentHandler* inheritableComponentHandler = bgcPtr->GetInheritableComponentHandler())
            {
                TArray<UActorComponent*> overridenComponentTemplates;
                inheritableComponentHandler->GetAllTemplates(overridenComponentTemplates);
                for (UActorComponent* templateComponentPtr : overridenComponentTemplates)
                {
                    if (templateComponentPtr != nullptr)
                    {
                        sfObject::SPtr componentObjPtr = CreateNonNativeInheritedComponentObject(
                            templateComponentPtr, blueprintPtr);
                        blueprintObjPtr->AddChild(componentObjPtr);
                    }
                }
            }

            // Components that have USCS_NODEs
            USimpleConstructionScript* scsPtr = blueprintPtr->SimpleConstructionScript;
            if (scsPtr != nullptr)
            {
                for (USCS_Node* rootNodePtr : scsPtr->GetRootNodes())
                {
                    CreateComponentObjectRecursive(blueprintObjPtr, rootNodePtr, bgcPtr);
                }
            }
        }
    }

    m_sessionPtr->Create(blueprintObjPtr);
}

void sfBlueprintTranslator::OnCreate(sfObject::SPtr objPtr, int childIndex)
{
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(m_onPropertyChangeHandle);
    if (objPtr->Type() == sfType::Blueprint)
    {
        OnCreateBlueprint(objPtr);
    }
    else if (objPtr->Type() == sfType::TemplateComponent)
    {
        OnCreateTemplateComponent(objPtr, true);
    }
    m_onPropertyChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
        this, &sfBlueprintTranslator::OnUObjectPropertyChange);
}

void sfBlueprintTranslator::OnCreateBlueprint(sfObject::SPtr objPtr)
{
    // Load blueprint
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    GIsSlowTask = true;
    GIsEditorLoadingPackage = true;
    UBlueprint* blueprintPtr = nullptr;
    if (name == "PersistentLevel")
    {
        // Load the level blueprint
        UWorld* worldPtr = GEditor->GetEditorWorldContext().World();
        blueprintPtr = worldPtr->PersistentLevel->GetLevelScriptBlueprint();
    }
    else
    {
        blueprintPtr = LoadObject<UBlueprint>(nullptr, *name);
    }
    GIsEditorLoadingPackage = false;
    GIsSlowTask = false;
    if (blueprintPtr == nullptr)
    {
        SceneFusion::MissingObjectManager->AddMissingObject(name, objPtr);
        KS::Log::Warning("Could not find blueprint " + sfUnrealUtils::FToStdString(name), LOG_CHANNEL);
        return;
    }
    sfObjectMap::Add(objPtr, blueprintPtr);
    sfPropertyManager::Get().ApplyProperties(blueprintPtr, propertiesPtr);

    bool hierarchyChanged = false;
    for (sfObject::SPtr childPtr : objPtr->Children())
    {
        if (childPtr->Type() == sfType::Actor)
        {
            TSharedPtr<sfActorTranslator> actorTranslatorPtr
                = SceneFusion::Get().GetTranslator<sfActorTranslator>(sfType::Actor);
            if (actorTranslatorPtr.IsValid())
            {
                actorTranslatorPtr->OnCreate(childPtr, 0);
            }
            continue;
        }

        if (childPtr->Type() == sfType::TemplateComponent)
        {
            hierarchyChanged |= OnCreateTemplateComponent(childPtr);
        }
    }

    hierarchyChanged |= RemoveUnsyncedComponents(blueprintPtr);

    if (hierarchyChanged)
    {
        m_structurallyModifiedBlueprints.Add(blueprintPtr);
    }
}

bool sfBlueprintTranslator::OnCreateTemplateComponent(sfObject::SPtr objPtr, bool refreshAllNodes)
{
    sfObject::SPtr parentObjPtr = objPtr->Parent();
    if (parentObjPtr == nullptr)
    {
        KS::Log::Warning("Could not find parent of component object " + std::to_string(objPtr->Id()), LOG_CHANNEL);
        return false;
    }

    // Get blueprint
    sfObject::SPtr rootObjPtr = parentObjPtr;
    while (rootObjPtr->Parent() != nullptr)
    {
        rootObjPtr = rootObjPtr->Parent();
    }
    UBlueprint* blueprintPtr = sfObjectMap::Get<UBlueprint>(rootObjPtr);
    if (blueprintPtr == nullptr ||
        blueprintPtr->GeneratedClass == nullptr ||
        blueprintPtr->SimpleConstructionScript == nullptr)
    {
        return false;
    }

    UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(blueprintPtr->GeneratedClass);

    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    sfProperty::SPtr nodeNamePropPtr;
    if (!propertiesPtr->TryGet(sfProp::NodeName, nodeNamePropPtr))
    {
        // There is no node name property. This is a non-native inherited component.
        FString className = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Class));
        UClass* classPtr = sfUnrealUtils::LoadClass(className);
        if (classPtr != nullptr)
        {
            FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
            if (UInheritableComponentHandler* inheritableComponentHandler = bgcPtr->GetInheritableComponentHandler())
            {
                TArray<UActorComponent*> overridenComponentTemplates;
                inheritableComponentHandler->GetAllTemplates(overridenComponentTemplates);
                if (overridenComponentTemplates.Num() == 0)
                {
                    // A non-native inherited component will not be created unless it has some property values that
                    // override the parent blueprint's values or it was opened in the editor once.
                    // Iterate through parent blueprints and create override component templates
                    TArray<UBlueprint*> parentBPStack;
                    UBlueprint::GetBlueprintHierarchyFromClass(blueprintPtr->GeneratedClass, parentBPStack);
                    for (int stackIndex = parentBPStack.Num() - 1; stackIndex > 0; stackIndex--)
                    {
                        USimpleConstructionScript* parentSCSPtr = parentBPStack[stackIndex]->SimpleConstructionScript;
                        if (parentSCSPtr == nullptr)
                        {
                            continue;
                        }
                        const TArray<USCS_Node*>& rootNodes = parentSCSPtr->GetRootNodes();
                        for (int32 nodeIndex = 0; nodeIndex < rootNodes.Num(); nodeIndex++)
                        {
                            USCS_Node* nodePtr = rootNodes[nodeIndex];
                            if (nodePtr != nullptr)
                            {
                                inheritableComponentHandler->CreateOverridenComponentTemplate(FComponentKey(nodePtr));
                            }
                        }
                    }
                    inheritableComponentHandler->GetAllTemplates(overridenComponentTemplates);
                }
                for (UActorComponent* componentPtr : overridenComponentTemplates)
                {
                    if (componentPtr != nullptr && componentPtr->GetName() == name)
                    {
                        sfObjectMap::Add(objPtr, componentPtr);

                        // Set category
                        sfProperty::SPtr propPtr;
                        if (propertiesPtr->TryGet(sfProp::Category, propPtr))
                        {
                            name = name.LeftChop(USimpleConstructionScript::ComponentTemplateNameSuffix.Len());
                            FBlueprintEditorUtils::SetBlueprintVariableCategory(
                                blueprintPtr, FName(*name), nullptr, sfPropertyUtil::ToText(propPtr));
                        }

                        sfPropertyManager::Get().ApplyProperties(componentPtr, propertiesPtr);
                        break;
                    }
                }
            }
        }
        return false;
    }

    // Find or create scs node
    bool hierarchyChanged = false;
    USCS_Node* nodePtr = GetOrCreateNode(blueprintPtr->SimpleConstructionScript, objPtr, hierarchyChanged);
    if (nodePtr == nullptr)
    {
        return false;
    }

    // Set node name
    FName nodeName = *sfPropertyUtil::ToString(nodeNamePropPtr);
    if (nodePtr->GetVariableName() != nodeName)
    {
        nodePtr->SetVariableName(nodeName, false);
    }

    // Set tooltip
    sfProperty::SPtr propPtr;
    if (propertiesPtr->TryGet(sfProp::Tooltip, propPtr))
    {
        nodePtr->SetMetaData(TEXT("tooltip"), sfPropertyUtil::ToString(propPtr));
    }

    UActorComponent* componentPtr = nodePtr->GetActualComponentTemplate(bgcPtr);
    if (componentPtr != nullptr)
    {
        sfObjectMap::Add(objPtr, componentPtr);
        sfPropertyManager::Get().ApplyProperties(componentPtr, propertiesPtr);

        // Set parent
        USceneComponent* sceneComponentPtr = Cast<USceneComponent>(componentPtr);
        if (sceneComponentPtr != nullptr)
        {
            hierarchyChanged |= SetParent(sceneComponentPtr, parentObjPtr, nodePtr);
            if (propertiesPtr->TryGet(sfProp::Parent, propPtr))
            {
                hierarchyChanged |= SetInheritedParent(nodePtr, propPtr);
            }
        }
    }

    // Iterate children
    for (sfObject::SPtr childPtr : objPtr->Children())
    {
        if (childPtr->Type() == sfType::Actor)
        {
            TSharedPtr<sfActorTranslator> actorTranslatorPtr
                = SceneFusion::Get().GetTranslator<sfActorTranslator>(sfType::Actor);
            if (actorTranslatorPtr.IsValid())
            {
                actorTranslatorPtr->OnCreate(childPtr, 0);
            }
            continue;
        }

        if (childPtr->Type() == sfType::TemplateComponent)
        {
            hierarchyChanged |= OnCreateTemplateComponent(childPtr);
        }
    }

    if (refreshAllNodes && hierarchyChanged)
    {
        m_structurallyModifiedBlueprints.Add(blueprintPtr);
    }

    return hierarchyChanged;
}

bool sfBlueprintTranslator::SetParent(USceneComponent* componentPtr, sfObject::SPtr parentObjPtr, USCS_Node* nodePtr)
{
    USimpleConstructionScript* scsPtr = nodePtr->GetSCS();
    if (scsPtr == nullptr)
    {
        KS::Log::Warning("SimpleConstructionScript is nullptr.", LOG_CHANNEL);
        return false;
    }

    if (parentObjPtr->Type() == sfType::TemplateComponent)
    {
        UActorComponent* parentComponentPtr = sfObjectMap::Get<UActorComponent>(parentObjPtr);
        // If its parent object is a template component object, attach its node to the parent node.
        if (parentComponentPtr != nullptr)
        {
            USCS_Node* parentNodePtr = scsPtr->FindSCSNode(parentComponentPtr->GetFName());
            if (parentNodePtr == nullptr)
            {
                KS::Log::Warning("Cannot find parent component.", LOG_CHANNEL);
                return false;
            }
            if (parentNodePtr->GetChildNodes().Contains(nodePtr))
            {
                return false;
            }

            // Resolve loop
            for (USCS_Node* childNodePtr : nodePtr->GetChildNodes())
            {
                if (childNodePtr == nullptr)
                {
                    continue;
                }
                if (parentNodePtr->IsChildOf(childNodePtr))
                {
                    nodePtr->RemoveChildNode(childNodePtr, true);
                    scsPtr->AddNode(childNodePtr);
                    break;
                }
            }
            scsPtr->RemoveNode(nodePtr);
            parentNodePtr->AddChildNode(nodePtr, true);
            return true;
        }
        else
        {
            KS::Log::Warning("Cannot find parent component.", LOG_CHANNEL);
        }
    }
    // If its parent object is the blueprint object, add its node to root nodes.
    else if (parentObjPtr->Type() == sfType::Blueprint && !scsPtr->GetRootNodes().Contains(nodePtr))
    {
        // Remove and add node again to make the node a root node
        scsPtr->RemoveNode(nodePtr);
        scsPtr->AddNode(nodePtr);
        return true;
    }
    return false;
}

void sfBlueprintTranslator::OnDelete(sfObject::SPtr objPtr)
{
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(m_onPropertyChangeHandle);
    if (objPtr->Type() == sfType::Blueprint)
    {
        objPtr->ForSelfAndDescendants([](sfObject::SPtr objectPtr)
        {
            sfObjectMap::Remove(objectPtr);
            return true;
        });
    }
    else if (objPtr->Type() == sfType::TemplateComponent)
    {
        RemoveSCSNode(objPtr, true);
    }
    m_onPropertyChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
        this, &sfBlueprintTranslator::OnUObjectPropertyChange);
}

void sfBlueprintTranslator::RegisterPropertyChangeHandlers()
{
    m_propertyChangeHandlers[sfProp::Name] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        sfUnrealUtils::TryRename(uobjPtr, sfPropertyUtil::ToString(propertyPtr),
            REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
        return true;
    };

    m_propertyChangeHandlers[sfProp::NodeName] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        USCS_Node* nodePtr = GetSCSNode(uobjPtr);
        if (nodePtr != nullptr)
        {
            nodePtr->SetVariableName(*sfPropertyUtil::ToString(propertyPtr), false);
            m_structurallyModifiedBlueprints.Add(nodePtr->GetSCS()->GetBlueprint());
        }
        return true;
    };

    m_propertyChangeHandlers[sfProp::Category] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        FString name = uobjPtr->GetName();
        name = name.LeftChop(USimpleConstructionScript::ComponentTemplateNameSuffix.Len());
        UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(uobjPtr->GetOuter());
        UBlueprint* blueprintPtr = Cast<UBlueprint>(bgcPtr->ClassGeneratedBy);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(
            blueprintPtr, FName(*name), nullptr, sfPropertyUtil::ToText(propertyPtr));
        return true;
    };

    m_propertyChangeHandlers[sfProp::Tooltip] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        USCS_Node* nodePtr = GetSCSNode(uobjPtr);
        if (nodePtr != nullptr)
        {
            if (propertyPtr != nullptr)
            {
                nodePtr->SetMetaData(TEXT("tooltip"), sfPropertyUtil::ToString(propertyPtr));
            }
            else
            {
                nodePtr->RemoveMetaData(TEXT("tooltip"));
            }
        }
        return true;
    };

    m_propertyChangeHandlers[sfProp::Parent] = [this](UObject* uobjPtr, sfProperty::SPtr propertyPtr)
    {
        USCS_Node* nodePtr = GetSCSNode(uobjPtr);
        if (nodePtr == nullptr)
        {
            return true;
        }

        if (propertyPtr == nullptr)
        {
            nodePtr->ParentComponentOrVariableName = NAME_None;
            nodePtr->ParentComponentOwnerClassName = NAME_None;
            nodePtr->Modify();
            m_structurallyModifiedBlueprints.Add(nodePtr->GetSCS()->GetBlueprint());
            return true;
        }

        if (SetInheritedParent(nodePtr, propertyPtr))
        {
            m_structurallyModifiedBlueprints.Add(nodePtr->GetSCS()->GetBlueprint());
        }
        return true;
    };
}

void sfBlueprintTranslator::OnParentChange(sfObject::SPtr objPtr, int childIndex)
{
    if (objPtr->Type() != sfType::TemplateComponent)
    {
        return;
    }

    USceneComponent* componentPtr = sfObjectMap::Get<USceneComponent>(objPtr);
    if (componentPtr == nullptr)
    {
        return;
    }

    UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(componentPtr->GetOuter());
    if (bgcPtr == nullptr)
    {
        return;
    }
    UBlueprint* blueprintPtr = Cast<UBlueprint>(bgcPtr->ClassGeneratedBy);
    if (blueprintPtr == nullptr || blueprintPtr->SimpleConstructionScript == nullptr)
    {
        return;
    }

    bool hierarchyChanged = false;
    USCS_Node* nodePtr = GetOrCreateNode(blueprintPtr->SimpleConstructionScript, objPtr, hierarchyChanged);
    if (nodePtr == nullptr)
    {
        return;
    }

    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(m_onPropertyChangeHandle);
    hierarchyChanged |= SetParent(componentPtr, objPtr->Parent(), nodePtr);
    m_onPropertyChangeHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
        this, &sfBlueprintTranslator::OnUObjectPropertyChange);

    // If the new parent is the blueprint, that means the root node is replaced by another node.
    // In this case, we don't want to refresh the nodes until the old root node's parent get set.
    // Otherwise, this node will be duplicated and the duplicate will be added as a child of the old root.
    if (hierarchyChanged && objPtr->Parent()->Type() != sfType::Blueprint)
    {
        m_structurallyModifiedBlueprints.Add(blueprintPtr);
    }
}

sfObject::SPtr sfBlueprintTranslator::CreateComponentObjectRecursive(
    sfObject::SPtr parentObjPtr,
    USCS_Node* nodePtr,
    UBlueprintGeneratedClass* bgcPtr)
{
    sfObject::SPtr componentObjPtr = nullptr;
    UActorComponent* templateComponentPtr = nodePtr->GetActualComponentTemplate(bgcPtr);
    if (templateComponentPtr != nullptr)
    {
        componentObjPtr = sfObjectMap::GetOrCreateSFObject(
            templateComponentPtr, sfType::TemplateComponent);
        sfDictionaryProperty::SPtr  componentPropertiesPtr = componentObjPtr->Property()->AsDict();

        componentPropertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(templateComponentPtr->GetName()));
        componentPropertiesPtr->Set(sfProp::NodeName,
            sfPropertyUtil::FromString(nodePtr->GetVariableName().ToString()));

        FString className = sfUnrealUtils::ClassToFString(templateComponentPtr->GetClass());
        componentPropertiesPtr->Set(sfProp::Class, sfPropertyUtil::FromString(className));

        if (nodePtr->FindMetaDataEntryIndexForKey(TEXT("tooltip")) != INDEX_NONE)
        {
            componentPropertiesPtr->Set(sfProp::Tooltip,
                sfPropertyUtil::FromString(nodePtr->GetMetaData(TEXT("tooltip"))));
        }

        sfPropertyManager::Get().CreateProperties(templateComponentPtr, componentPropertiesPtr);

        // Set parent
        parentObjPtr->AddChild(componentObjPtr);
        if (parentObjPtr->Type() == sfType::Blueprint)
        {
            UBlueprint* blueprintPtr = sfObjectMap::Get<UBlueprint>(parentObjPtr);
            // For root nodes, set inherited parent component reference
            USceneComponent* parentComponentPtr = nullptr;
            if (nodePtr->bIsParentComponentNative)
            {
                parentComponentPtr = nodePtr->GetParentComponentTemplate(blueprintPtr);
            }
            else
            {
                // Parent is not Native.
                // Find the parent component defined in the parent blueprint instead of the overriden one.
                parentComponentPtr = FindParentComponentInAncestors(nodePtr);
            }
            if (parentComponentPtr != nullptr)
            {
                parentObjPtr = sfObjectMap::GetSFObject(parentComponentPtr);
                componentPropertiesPtr->Set(sfProp::Parent, sfReferenceProperty::Create(parentObjPtr->Id()));
            }
        }

        for (USCS_Node* childNodePtr : nodePtr->GetChildNodes())
        {
            CreateComponentObjectRecursive(componentObjPtr, childNodePtr, bgcPtr);
        }
    }
    return componentObjPtr;
}


sfObject::SPtr sfBlueprintTranslator::CreateNonNativeInheritedComponentObject(
    UActorComponent* templateComponentPtr,
    UBlueprint* blueprintPtr)
{
    // Create sfObject for the give non-native inherited component
    sfObject::SPtr componentObjPtr = sfObjectMap::GetOrCreateSFObject(
        templateComponentPtr, sfType::TemplateComponent);
    sfDictionaryProperty::SPtr  componentPropertiesPtr = componentObjPtr->Property()->AsDict();

    FString name = templateComponentPtr->GetName();
    componentPropertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(name));

    FString className = sfUnrealUtils::ClassToFString(templateComponentPtr->GetClass());
    componentPropertiesPtr->Set(sfProp::Class, sfPropertyUtil::FromString(className));

    name = name.LeftChop(USimpleConstructionScript::ComponentTemplateNameSuffix.Len());
    FText category = FBlueprintEditorUtils::GetBlueprintVariableCategory(
        blueprintPtr, FName(*name), nullptr);
    componentPropertiesPtr->Set(sfProp::Category, sfPropertyUtil::FromText(category));

    sfPropertyManager::Get().CreateProperties(templateComponentPtr, componentPropertiesPtr);
    return componentObjPtr;
}

// Listen to property event change. If the UObject is a UBlueprint and the member proeprty is null,
// iterate all nodes to check for component parent change.
void sfBlueprintTranslator::OnUObjectPropertyChange(UObject* uobjPtr, FPropertyChangedEvent& ev)
{
    UBlueprint* blueprintPtr = Cast<UBlueprint>(uobjPtr); // Try to cast to a blueprint
    if ((blueprintPtr == nullptr && !uobjPtr->GetClass()->IsInBlueprint()) || ev.MemberProperty != nullptr)
    {
        return;
    }

    if (blueprintPtr == nullptr && uobjPtr->GetOutermost() == GetTransientPackage())
    {
        // It is not a blueprint but is in the transient package. It may be a preview actor of a blueprint actor.
        // Try to cast the object its class was generated by to a blueprint.
        blueprintPtr = Cast<UBlueprint>(uobjPtr->GetClass()->ClassGeneratedBy);
    }

    if (blueprintPtr != nullptr)
    {
        m_dirtyBlueprints.Add(blueprintPtr);
    }
}

void sfBlueprintTranslator::SendChanges()
{
    if (m_dirtyBlueprints.Num() == 0)
    {
        return;
    }

    TSet<AActor*> selectedActors;
    sfDetailsPanelManager::Get().GetSelectedActors(selectedActors);
    for (UBlueprint* blueprintPtr : m_dirtyBlueprints)
    {
        USimpleConstructionScript* scsPtr = blueprintPtr->SimpleConstructionScript;
        if (scsPtr == nullptr)
        {
            continue;
        }

        sfObject::SPtr blueprintObjPtr = sfObjectMap::GetSFObject(blueprintPtr);
        if (blueprintObjPtr == nullptr)
        {
            UploadBlueprint(blueprintPtr);
        }
        else
        {
            // Check components parent and property changes
            SendChangesRecursive(blueprintObjPtr, blueprintPtr, scsPtr->GetRootNodes());

            // Overriden component templates
            UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(blueprintPtr->GeneratedClass);
            if (UInheritableComponentHandler* inheritableComponentHandler = bgcPtr->GetInheritableComponentHandler())
            {
                TArray<UActorComponent*> overridenComponentTemplates;
                inheritableComponentHandler->GetAllTemplates(overridenComponentTemplates);
                for (UActorComponent* templateComponentPtr : overridenComponentTemplates)
                {
                    if (templateComponentPtr != nullptr)
                    {
                        sfObject::SPtr componentObjPtr = sfObjectMap::GetSFObject(templateComponentPtr);
                        if (componentObjPtr == nullptr)
                        {
                            // Upload new component
                            componentObjPtr = CreateNonNativeInheritedComponentObject(
                                templateComponentPtr, blueprintPtr);
                            blueprintObjPtr->AddChild(componentObjPtr);
                            // Send create request to server. Keep the actor to be the first child.
                            m_sessionPtr->Create(componentObjPtr, blueprintObjPtr, 1);
                            continue;
                        }

                        // Send category change
                        sfDictionaryProperty::SPtr propertiesPtr = componentObjPtr->Property()->AsDict();
                        FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
                        name = name.LeftChop(USimpleConstructionScript::ComponentTemplateNameSuffix.Len());
                        sfProperty::SPtr categoryPropPtr;
                        const FText currentCategory = FBlueprintEditorUtils::GetBlueprintVariableCategory(
                            blueprintPtr, FName(*name), nullptr);
                        if (!propertiesPtr->TryGet(sfProp::Category, categoryPropPtr) ||
                            !currentCategory.EqualTo(sfPropertyUtil::ToText(categoryPropPtr)))
                        {
                            propertiesPtr->Set(sfProp::Category, sfPropertyUtil::FromText(currentCategory));
                        }

                        // Send other property changes
                        sfPropertyManager::Get().SendPropertyChanges(templateComponentPtr, propertiesPtr);
                    }
                }
            }
        }

        // This fixes an unreal bug where when you have an actor selected and its blueprint is modified,
        // the actor's component hierarchy does not refresh.
        for (AActor* actorPtr : selectedActors)
        {
            if (actorPtr->GetClass()->ClassGeneratedBy == blueprintPtr)
            {
                selectedActors.Remove(actorPtr);
                sfDetailsPanelManager::Get().UpdateDetailsPanelTree();
                break;
            }
        }
    }
    m_dirtyBlueprints.Empty();
}

void sfBlueprintTranslator::SendChangesRecursive(
    sfObject::SPtr currentParentObjPtr,
    UBlueprint* blueprintPtr,
    const TArray<USCS_Node*>& nodesToCheck)
{
    UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(blueprintPtr->GeneratedClass);
    bool isRootNode = currentParentObjPtr->Type() == sfType::Blueprint;
    TSharedPtr<sfComponentTranslator> componentTranslatorPtr
        = SceneFusion::Get().GetTranslator<sfComponentTranslator>(sfType::Component);
    for (USCS_Node* nodePtr : nodesToCheck)
    {
        if (nodePtr == nullptr)
        {
            continue;
        }

        UActorComponent* componentPtr = nodePtr->GetActualComponentTemplate(bgcPtr);
        if (componentPtr == nullptr)
        {
            continue;
        }
        sfObject::SPtr componentObjPtr = sfObjectMap::GetSFObject(componentPtr);
        if (componentObjPtr == nullptr)
        {
            // Upload new component
            sfObject::SPtr objPtr = CreateComponentObjectRecursive(currentParentObjPtr, nodePtr, bgcPtr);
            if (objPtr != nullptr)
            {
                // Send create request to server. Keep the actor to be the first child.
                m_sessionPtr->Create(objPtr, currentParentObjPtr, isRootNode ? 1 : 0);
            }
            continue;
        }

        // Send component name change
        sfDictionaryProperty::SPtr propertiesPtr = componentObjPtr->Property()->AsDict();
        FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
        if (componentPtr->GetName() != name)
        {
            propertiesPtr->Set(sfProp::Name, sfPropertyUtil::FromString(componentPtr->GetName()));
        }

        // Send node name change
        FString nodeName = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::NodeName));
        if (nodePtr->GetVariableName() != *nodeName)
        {
            propertiesPtr->Set(sfProp::NodeName, sfPropertyUtil::FromString(nodePtr->GetVariableName().ToString()));
        }

        // Send tooltip change
        if (nodePtr->FindMetaDataEntryIndexForKey(TEXT("tooltip")) == INDEX_NONE)
        {
            propertiesPtr->Remove(sfProp::Tooltip);
        }
        else
        {
            sfProperty::SPtr tooltipPropPtr;
            FString currentTooltip = nodePtr->GetMetaData(TEXT("tooltip"));
            if (!propertiesPtr->TryGet(sfProp::Tooltip, tooltipPropPtr) ||
                currentTooltip != sfPropertyUtil::ToString(tooltipPropPtr))
            {
                propertiesPtr->Set(sfProp::Tooltip, sfPropertyUtil::FromString(currentTooltip));
            }
        }

        // Send other property changes
        sfPropertyManager::Get().SendPropertyChanges(componentPtr, propertiesPtr);

        USceneComponent* sceneComponentPtr = Cast<USceneComponent>(componentPtr);
        if (sceneComponentPtr == nullptr) //If it is not a scene compoennt, then it has no descendants. Continue.
        {
            continue;
        }

        // Send parent change
        if (currentParentObjPtr != componentObjPtr->Parent())
        {
            currentParentObjPtr->AddChild(componentObjPtr);
            if (componentTranslatorPtr.IsValid())
            {
                componentTranslatorPtr->SyncTransform(sceneComponentPtr);
            }
        }
        sceneComponentPtr = nullptr;
        if (nodePtr->bIsParentComponentNative)
        {
            sceneComponentPtr = nodePtr->GetParentComponentTemplate(blueprintPtr);
        }
        else
        {
            // Parent is not Native.
            // Find the parent component defined in the parent blueprint instead of the overriden one.
            sceneComponentPtr = FindParentComponentInAncestors(nodePtr);
        }
        if (sceneComponentPtr == nullptr || !isRootNode)
        {
            propertiesPtr->Remove(sfProp::Parent);
        }
        else
        {
            sfProperty::SPtr inheritedParentPropPtr;
            uint32_t oldInheritedParentId = 0;
            if (propertiesPtr->TryGet(sfProp::Parent, inheritedParentPropPtr))
            {
                oldInheritedParentId = inheritedParentPropPtr->AsReference()->GetObjectId();
            }
            uint32_t newInheritedParentId = sfObjectMap::GetSFObject(sceneComponentPtr)->Id();
            if (newInheritedParentId != oldInheritedParentId)
            {
                propertiesPtr->Set(sfProp::Parent, sfReferenceProperty::Create(newInheritedParentId));
            }
        }

        // Iterate child hierarchy
        SendChangesRecursive(componentObjPtr, blueprintPtr, nodePtr->GetChildNodes());
    }

    // Delete destroyed children
    USimpleConstructionScript* scsPtr = bgcPtr->SimpleConstructionScript;
    auto iter = currentParentObjPtr->Children().begin();
    while (iter != currentParentObjPtr->Children().end())
    {
        sfObject::SPtr childObjPtr = *iter;
        iter++;
        if (childObjPtr->Type() != sfType::TemplateComponent)
        {
            continue;
        }

        sfDictionaryProperty::SPtr propertiesPtr = childObjPtr->Property()->AsDict();
        sfProperty::SPtr nodeNamePropPtr;
        if (!propertiesPtr->TryGet(sfProp::NodeName, nodeNamePropPtr))
        {
            // Overriden component template
            continue;
        }

        UObject* uobjPtr = sfObjectMap::GetUObject(childObjPtr);
        if (uobjPtr != nullptr && !uobjPtr->IsPendingKill())
        {
            // Try to find component node in all nodes
            USCS_Node* nodePtr = scsPtr->FindSCSNode(uobjPtr->GetFName());
            if (nodePtr != nullptr)
            {
                continue;
            }
        }
        sfObjectMap::Remove(childObjPtr);
        m_sessionPtr->Delete(childObjPtr); // Component is removed. Delete sfObject.
    }
}

USCS_Node* sfBlueprintTranslator::GetOrCreateNode(
    USimpleConstructionScript* scsPtr,
    sfObject::SPtr objPtr,
    bool& created)
{
    created = false;
    sfDictionaryProperty::SPtr propertiesPtr = objPtr->Property()->AsDict();
    FString name = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Name));
    USCS_Node* nodePtr = scsPtr->FindSCSNode(*name);
    if (nodePtr == nullptr)
    {
        // Create node
        name = name.LeftChop(scsPtr->ComponentTemplateNameSuffix.Len());
        FString className = sfPropertyUtil::ToString(propertiesPtr->Get(sfProp::Class));
        UClass* classPtr = sfUnrealUtils::LoadClass(className);
        nodePtr = scsPtr->CreateNode(classPtr, *name);
        
        if (nodePtr != nullptr)
        {
            created = true;
        }
        else
        {
            KS::Log::Warning("Could not find or create node for " + sfUnrealUtils::FToStdString(name) + ".", LOG_CHANNEL);
        }
    }
    return nodePtr;
}

void sfBlueprintTranslator::RemoveSCSNode(sfObject::SPtr objPtr, bool refreshAllNodes)
{
    UActorComponent* componentPtr = Cast<UActorComponent>(sfObjectMap::Remove(objPtr));
    if (componentPtr == nullptr)
    {
        return;
    }
    UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(componentPtr->GetOuter());
    if (bgcPtr == nullptr)
    {
        return;
    }
    UBlueprint* blueprintPtr = Cast<UBlueprint>(bgcPtr->ClassGeneratedBy);
    if (blueprintPtr == nullptr || blueprintPtr->SimpleConstructionScript == nullptr)
    {
        return;
    }

    USimpleConstructionScript* scsPtr = blueprintPtr->SimpleConstructionScript;
    FString name = componentPtr->GetName().LeftChop(scsPtr->ComponentTemplateNameSuffix.Len());
    USCS_Node* nodePtr = scsPtr->FindSCSNode(FName(*name));
    if (nodePtr != nullptr)
    {
        USceneComponent* sceneComponentPtr = Cast<USceneComponent>(componentPtr);
        bool isSceneRoot = sceneComponentPtr != nullptr && scsPtr->GetRootNodes().Contains(nodePtr) &&
            nodePtr->ParentComponentOrVariableName == NAME_None;
        RemoveSCSNode(blueprintPtr, nodePtr, isSceneRoot);
        if (refreshAllNodes)
        {
            m_structurallyModifiedBlueprints.Add(blueprintPtr);
        }
    }
}

void sfBlueprintTranslator::RemoveSCSNode(UBlueprint* blueprintPtr, USCS_Node* nodePtr, bool isSceneRoot)
{
    // The following code in this if statement is copied from Unreal's SSCSEditor.cpp.
    // It deletes the USCS_Node properly.

    FBlueprintEditorUtils::RemoveVariableNodes(blueprintPtr, nodePtr->GetVariableName());
    // Remove node from SCS tree
    blueprintPtr->SimpleConstructionScript->RemoveNodeAndPromoteChildren(nodePtr);

    // Clear the delegate
    nodePtr->SetOnNameChanged(FSCSNodeNameChanged());

    // on removal, since we don't move the template from the GeneratedClass
    // (which we shouldn't, as it would create a discrepancy with existing instances),
    // we rename it instead so that we can re-use the name without having to compile
    // (we still have a problem if they attempt to name it to what ever we choose here, but that is unlikely)
    // note: skip this for the default scene root; we don't actually destroy that node when it's removed,
    // so we don't need the template to be renamed.
    if (!isSceneRoot && nodePtr->ComponentTemplate != nullptr)
    {
        const FName templateName = nodePtr->ComponentTemplate->GetFName();
        const FString removedName = nodePtr->GetVariableName().ToString() +
            TEXT("_REMOVED_") + FGuid::NewGuid().ToString();

        nodePtr->ComponentTemplate->Modify();
        nodePtr->ComponentTemplate->Rename(*removedName, /*NewOuter =*/nullptr, REN_DontCreateRedirectors);

        // Children need to have their inherited component template instance of the component
        // renamed out of the way as well
        TArray<UClass*> childrenOfClass;
        GetDerivedClasses(blueprintPtr->GeneratedClass, childrenOfClass);

        for (UClass* childClassPtr : childrenOfClass)
        {
            UBlueprintGeneratedClass* bpChildClassPtr = CastChecked<UBlueprintGeneratedClass>(childClassPtr);

            UActorComponent* childClassComponentPtr = (UActorComponent*)FindObjectWithOuter(
                bpChildClassPtr,
                UActorComponent::StaticClass(),
                templateName);
            if (childClassComponentPtr != nullptr)
            {
                childClassComponentPtr->Modify();
                childClassComponentPtr->Rename(*removedName, /*NewOuter =*/nullptr, REN_DontCreateRedirectors);
            }
        }
    }
}

bool sfBlueprintTranslator::RemoveUnsyncedComponents(UBlueprint* blueprintPtr)
{
    bool nodeRemoved = false;
    USimpleConstructionScript* scsPtr = blueprintPtr->SimpleConstructionScript;
    UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(blueprintPtr->GeneratedClass);
    if (scsPtr != nullptr && bgcPtr != nullptr)
    {
        auto iter = scsPtr->GetAllNodes().CreateConstIterator();
        while (iter)
        {
            USCS_Node* nodePtr = *iter;
            iter++;
            if (nodePtr == nullptr)
            {
                continue;
            }
            UActorComponent* templateComponentPtr = nodePtr->GetActualComponentTemplate(bgcPtr);
            if (!sfObjectMap::Contains(templateComponentPtr))
            {
                RemoveSCSNode(blueprintPtr, nodePtr, false);
                nodeRemoved = true;
            }
        }
    }
    return nodeRemoved;
}

USCS_Node* sfBlueprintTranslator::GetSCSNode(UObject* uobjPtr)
{
    USCS_Node* nodePtr = nullptr;
    UBlueprintGeneratedClass* bgcPtr = Cast<UBlueprintGeneratedClass>(uobjPtr->GetOuter());
    if (bgcPtr != nullptr && bgcPtr->SimpleConstructionScript != nullptr)
    {
        nodePtr = bgcPtr->SimpleConstructionScript->FindSCSNode(uobjPtr->GetFName());;
    }
    return nodePtr;
}

bool sfBlueprintTranslator::SetInheritedParent(USCS_Node* nodePtr, sfProperty::SPtr propertyPtr)
{
    sfObject::SPtr inheritedParentObjPtr = m_sessionPtr->GetObject(propertyPtr->AsReference()->GetObjectId());
    USceneComponent* parentComponentPtr = sfObjectMap::Get<USceneComponent>(inheritedParentObjPtr);
    if (parentComponentPtr == nullptr ||
        parentComponentPtr->GetFName() == nodePtr->ParentComponentOrVariableName)
    {
        return false;
    }

    if (parentComponentPtr->GetOutermost() == nodePtr->GetOutermost())
    {
        // Set native component parent
        nodePtr->SetParent(parentComponentPtr);
        return true;
    }

    // Get parent blueprint
    sfObject::SPtr parentBpObjPtr = inheritedParentObjPtr;
    while (parentBpObjPtr != nullptr && parentBpObjPtr->Type() != sfType::Blueprint)
    {
        parentBpObjPtr = parentBpObjPtr->Parent();
    }
    if (parentBpObjPtr == nullptr)
    {
        return false;
    }
    UBlueprint* parentBPPtr = sfObjectMap::Get<UBlueprint>(parentBpObjPtr);
    if (parentBPPtr == nullptr || parentBPPtr->SimpleConstructionScript == nullptr)
    {
        return false;
    }

    // Iterate through parent blueprint's nodes to find parent node
    TArray<USCS_Node*> parentSCSNodes = parentBPPtr->SimpleConstructionScript->GetAllNodes();
    for (int32 i = 0; i < parentSCSNodes.Num(); ++i)
    {
        if (parentSCSNodes[i]->ComponentTemplate == parentComponentPtr)
        {
            // Found parent component
            USimpleConstructionScript* scsPtr = nodePtr->GetSCS();
            if (!scsPtr->GetRootNodes().Contains(nodePtr))
            {
                // Remove the node from the old parent's children.
                scsPtr->RemoveNode(nodePtr);
                scsPtr->AddNode(nodePtr);
            }
            nodePtr->SetParent(parentSCSNodes[i]); // Set non-native parent
            return true;
        }
    }
    return false;
}

USceneComponent* sfBlueprintTranslator::FindParentComponentInAncestors(USCS_Node* nodePtr)
{
    // Get the Blueprint hierarchy
    TArray<UBlueprint*> parentBPStack;
    USimpleConstructionScript* scsPtr = nodePtr->GetSCS();
    UBlueprint::GetBlueprintHierarchyFromClass(scsPtr->GetBlueprint()->GeneratedClass, parentBPStack);

    // Find the parent Blueprint in the hierarchy
    USceneComponent* parentComponentPtr = nullptr;
    for (int32 StackIndex = parentBPStack.Num() - 1; StackIndex > 0 && !parentComponentPtr; --StackIndex)
    {
        UBlueprint* parentBlueprint = parentBPStack[StackIndex];
        if (parentBlueprint != nullptr
            && parentBlueprint->SimpleConstructionScript != nullptr
            && parentBlueprint->GeneratedClass->GetFName() == nodePtr->ParentComponentOwnerClassName)
        {
            // Find the SCS node with a variable name that matches the specified name
            TArray<USCS_Node*> parentSCSNodes = parentBlueprint->SimpleConstructionScript->GetAllNodes();
            for (int32 i = 0; i < parentSCSNodes.Num(); ++i)
            {
                USceneComponent* compTemplate = Cast<USceneComponent>(parentSCSNodes[i]->ComponentTemplate);
                if (compTemplate != nullptr &&
                    parentSCSNodes[i]->GetVariableName() == nodePtr->ParentComponentOrVariableName)
                {
                    // Found a match; this is our parent, we're done
                    return Cast<USceneComponent>(parentSCSNodes[i]->ComponentTemplate);
                }
            }
        }
    }
    return nullptr;
}

#undef LOG_CHANNEL