#pragma once

struct EffectItemData
{
    float fMagnitude;
    int32_t iArea;
    int32_t iDuration;
};

struct EffectItem
{
    EffectItemData data;
    uint32_t padC;
    EffectSetting* pEffectSetting;
    float fRawCost;
};