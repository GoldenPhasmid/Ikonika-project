#pragma once

#include "AppSpeech.h"

#include <vector>

namespace
{
    class FMockAppSpeech : public AppSpeech
    {
    public:
        void SpeakAtRate(const QString& Text, const QString& LanguageCode, float RateScale) override
        {
            ++SpeakCount;
            LastText = Text;
            LastLanguage = LanguageCode;
            LastRateScale = RateScale;
        }

        void SetEnabled(bool bValue) override
        {
            bEnabled = bValue;

            emit enabledChanged(bValue);
        }

        bool IsEnabled() const override
        {
            return bEnabled;
        }

        int GetVoiceCount() const override
        {
            return static_cast<int>(Voices.size());
        }

        FTVoiceInfo GetVoiceInfo(int Index) const override
        {
            return Index >= 0 && Index < GetVoiceCount() ? Voices[static_cast<size_t>(Index)] : FTVoiceInfo{};
        }

        int GetActiveVoice() const override
        {
            return ActiveVoice;
        }

        std::vector<FTVoiceInfo> Voices;
        int ActiveVoice = 0;
        bool bEnabled = true;
        int SpeakCount = 0;
        QString LastText;
        QString LastLanguage;
        float LastRateScale = 0.0f;
    };

    struct FAppSpeechGuard
    {
        FMockAppSpeech Mock;

        FAppSpeechGuard()
        {
            AppSpeech::SetInstance(&Mock);
        }

        ~FAppSpeechGuard()
        {
            AppSpeech::SetInstance(nullptr);
        }
    };
}
