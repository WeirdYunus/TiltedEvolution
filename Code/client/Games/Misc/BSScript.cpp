#include <Misc/BSScript.h>
#include <Misc/NativeFunction.h>

#include <World.h>
#include <Events/PapyrusFunctionRegisterEvent.h>
#include <Forms/TESForm.h>
#include <Misc/GameVM.h>
#include <Actor.h>
#include <PlayerCharacter.h>
#include <Games/ActorExtension.h>
#include <Games/PapyrusFunctions.h>

struct Variables
{
    void* pUnk0;
    BSScript::Variable* pVariables;
    uint64_t capacity;
    uint64_t size;
};

TP_THIS_FUNCTION(TRegisterPapyrusFunction, void, BSScript::IVirtualMachine, NativeFunction*);
// TODO: ft
#if TP_SKYRIM64
TP_THIS_FUNCTION(TBindEverythingToScript, void, BSScript::IVirtualMachine*);
TP_THIS_FUNCTION(TSignaturesMatch, bool, BSScript::NativeFunction, BSScript::NativeFunction*);
TP_THIS_FUNCTION(TCreateStack, void, BSScript::IVirtualMachine, int, int, BSScript::Stack**);
TP_THIS_FUNCTION(TPushFrame, uint64_t, BSScript::Stack, BSScript::IFunction**, BSScript::ObjectTypeInfo**, BSScript::Variable*, Variables*);
#endif
TP_THIS_FUNCTION(TCompareVariables, int64_t, void, BSScript::Variable*, BSScript::Variable*);

TRegisterPapyrusFunction* RealRegisterPapyrusFunction = nullptr;
#if TP_SKYRIM64
TBindEverythingToScript* RealBindEverythingToScript = nullptr;
TSignaturesMatch* RealSignaturesMatch = nullptr;
TCreateStack* RealCreateStack = nullptr;
TPushFrame* RealPushFrame = nullptr;
#endif
TCompareVariables* RealCompareVariables = nullptr;

void TP_MAKE_THISCALL(HookRegisterPapyrusFunction, BSScript::IVirtualMachine, NativeFunction* apFunction)
{
    auto& runner = World::Get().GetRunner();

    PapyrusFunctionRegisterEvent event(apFunction->functionName.AsAscii(), apFunction->typeName.AsAscii(), apFunction->functionAddress);

    runner.Trigger(std::move(event));

    TiltedPhoques::ThisCall(RealRegisterPapyrusFunction, apThis, apFunction);
}

#if TP_SKYRIM64
void TP_MAKE_THISCALL(HookBindEverythingToScript, BSScript::IVirtualMachine*)
{
    (*apThis)->BindNativeMethod(new BSScript::IsRemotePlayerFunc("IsRemotePlayer", "SkyrimTogetherUtils", PapyrusFunctions::IsRemotePlayer, BSScript::Variable::kBoolean));

    (*apThis)->BindNativeMethod(new BSScript::IsPlayerFunc("IsPlayer", "SkyrimTogetherUtils", PapyrusFunctions::IsPlayer, BSScript::Variable::kBoolean));

    TiltedPhoques::ThisCall(RealBindEverythingToScript, apThis);
}

bool TP_MAKE_THISCALL(HookSignaturesMatch, BSScript::NativeFunction, BSScript::NativeFunction* apOther)
{
    /*
    if (!strcmp(apThis->GetName().AsAscii(), "IsRemotePlayer"))
        DebugBreak();
    */

    return TiltedPhoques::ThisCall(RealSignaturesMatch, apThis, apOther);
}

void TP_MAKE_THISCALL(HookCreateStack, BSScript::IVirtualMachine, int aUnk1, int aUnk2, BSScript::Stack** appStack)
{
    TiltedPhoques::ThisCall(RealCreateStack, apThis, aUnk1, aUnk2, appStack);

    auto stackId = (*appStack)->uiStackID;

    World::Get().GetRunner().Queue([stackId]() {
        BSScript::VirtualMachine* pVm = (BSScript::VirtualMachine*)GameVM::Get()->virtualMachine;
        std::optional<creation::BSTScatterTableDefaultKVStorage<uint32_t, BSScript::Stack*>> stack = std::nullopt;
        for (const auto& stackIt : pVm->kAllRunningStacks)
        {
            if (stackIt.key == stackId)
            {
                stack = stackIt;
                break;
            }
        }

        if (!stack)
        {
            spdlog::error("Could not find stack");
            return;
        }

        BSScript::Stack* pStack = stack->value;

        if (pStack->pTop && pStack->pTop->pOwningObjectType)
            spdlog::critical("{}", pStack->pTop->pOwningObjectType->name.AsAscii());
        else
            spdlog::error("Missing some shit, top: {}", pStack->pTop ? "yes" : "no");
    });
}

uint64_t TP_MAKE_THISCALL(HookPushFrame, BSScript::Stack, BSScript::IFunction** appOwningFunction,
                          BSScript::ObjectTypeInfo** appOwningObject, BSScript::Variable* apSelf,
                          Variables* apArguments)
{
    //if (String("SkyrimTogetherUtils") == String((*appOwningFunction)->GetObjectTypeName().AsAscii()))
    {
        auto* pOwningFunction = *appOwningFunction;
        String function = "";
        function += pOwningFunction->GetObjectTypeName().AsAscii();
        function += "::";
        function += pOwningFunction->GetName().AsAscii();
        function += "(";

        for (int i = 0; i < pOwningFunction->GetParamCount(); i++)
        {
            if (i != 0)
                function += ", ";

            BSFixedString name{};
            BSScript::Variable::Type type{};
            pOwningFunction->GetParam(i, name, type);

            String typeString = "UNDEFINED";
            if (i < apArguments->size)
            {
                BSScript::Variable* pVariable = &apArguments->pVariables[i];
                typeString = pVariable->GetTypeString();
            }

            function += typeString;
            function += " ";
            function += name.AsAscii();

            name.Release();
        }

        function += ")";

        spdlog::info("{}", function);

    #if 0
        TESForm* pComplexType = apSelf->ExtractComplexType<TESForm>();
        if (pComplexType)
            spdlog::error("ComplexType extracted, form id: {:X}", pComplexType->formID);
        else
            spdlog::warn("Failed to extract complex type");

        for (int i = 0; i < apArguments->size; i++)
        {
            BSScript::Variable* pVariable = &apArguments->pVariables[i];
            TESForm* pVarForm = pVariable->ExtractComplexType<TESForm>();
            spdlog::info("{:X}", pVarForm->formID);
        }
    #endif
    }

    auto result =
        TiltedPhoques::ThisCall(RealPushFrame, apThis, appOwningFunction, appOwningObject, apSelf, apArguments);

    #if 0
    if (result == 0)
    {
        if (appOwningFunction && *appOwningFunction)
        {
            spdlog::critical("{}:{}", (*appOwningFunction)->GetObjectTypeName().AsAscii(),
                             (*appOwningFunction)->GetName().AsAscii());
        }
        else
            spdlog::error("Missing function type");
    }
    else
        spdlog::warn("Failed to push frame");
    #endif

    return result;
}
#endif

// This is a neat hack, but it has been disabled since it messes up other things like beastform.
// These kinds of issues should be solved with custom scripts now that we have SkyrimTogether.esp anyway.
int64_t TP_MAKE_THISCALL(HookCompareVariables, void, BSScript::Variable* apVar1, BSScript::Variable* apVar2)
{
#if TP_SKYRIM64
    BSScript::Object* pObject1 = apVar1->GetObject();
    BSScript::Object* pObject2 = apVar2->GetObject();

    if (!pObject1 || !pObject2)
        return TiltedPhoques::ThisCall(RealCompareVariables, apThis, apVar1, apVar2);

    uint64_t handle1 = pObject1->GetHandle();
    uint64_t handle2 = pObject2->GetHandle();

    auto* pPolicy = GameVM::Get()->virtualMachine->GetObjectHandlePolicy();

    if (!pPolicy || !handle1 || !handle2 || !pPolicy->HandleIsType((uint32_t)Actor::Type, handle1) || !pPolicy->HandleIsType((uint32_t)Actor::Type, handle2) || !pPolicy->IsHandleObjectAvailable(handle1) || !pPolicy->IsHandleObjectAvailable(handle2))
    {
        return TiltedPhoques::ThisCall(RealCompareVariables, apThis, apVar1, apVar2);
    }

    Actor* pActor1 = pPolicy->GetObjectForHandle<Actor>(handle1);
    Actor* pActor2 = pPolicy->GetObjectForHandle<Actor>(handle2);

    if (!pActor1 || !pActor2)
        return TiltedPhoques::ThisCall(RealCompareVariables, apThis, apVar1, apVar2);

    if (pActor1 == PlayerCharacter::Get())
    {
        auto* pExtension = pActor2->GetExtension();
        if (pExtension && pExtension->IsPlayer())
            return 0;
    }
    else if (pActor2 == PlayerCharacter::Get())
    {
        auto* pExtension = pActor1->GetExtension();
        if (pExtension && pExtension->IsPlayer())
            return 0;
    }

    return TiltedPhoques::ThisCall(RealCompareVariables, apThis, apVar1, apVar2);
#else
    return 0;
#endif
}

static TiltedPhoques::Initializer s_vmHooks(
    []()
    {
        POINTER_SKYRIMSE(TRegisterPapyrusFunction, s_registerPapyrusFunction, 104788);
        POINTER_FALLOUT4(TRegisterPapyrusFunction, s_registerPapyrusFunction, 919894);

#if TP_SKYRIM64
        POINTER_SKYRIMSE(TBindEverythingToScript, s_bindEverythingToScript, 55739);
        POINTER_SKYRIMSE(TSignaturesMatch, s_signaturesMatch, 104359);
        POINTER_SKYRIMSE(TCreateStack, s_createStack, 104870);
        POINTER_SKYRIMSE(TPushFrame, s_pushFrame, 104482);
#endif

        // POINTER_SKYRIMSE(TCompareVariables, s_compareVariables, 105220);

        RealRegisterPapyrusFunction = s_registerPapyrusFunction.Get();
#if TP_SKYRIM64
        RealBindEverythingToScript = s_bindEverythingToScript.Get();
        RealSignaturesMatch = s_signaturesMatch.Get();
        RealCreateStack = s_createStack.Get();
        RealPushFrame = s_pushFrame.Get();
#endif
        // RealCompareVariables = s_compareVariables.Get();

        TP_HOOK(&RealRegisterPapyrusFunction, HookRegisterPapyrusFunction);
#if TP_SKYRIM64
        TP_HOOK(&RealBindEverythingToScript, HookBindEverythingToScript);
        TP_HOOK(&RealSignaturesMatch, HookSignaturesMatch);
        //TP_HOOK(&RealCreateStack, HookCreateStack);
        TP_HOOK(&RealPushFrame, HookPushFrame);
#endif
        // TP_HOOK(&RealCompareVariables, HookCompareVariables);
    });
