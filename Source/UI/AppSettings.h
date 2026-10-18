#pragma once
// Глобальные настройки интерфейса (язык, скин): один экземпляр на процесс (SharedResourcePointer),
// файл <AppData>/NUWMUN/FM RX/FM RX.settings. Все открытые окна плагина подписаны и обновляются вместе.
#include "Strings.h"
#include "Theme.h"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace fmrx::ui
{
    class AppSettings : public juce::ChangeBroadcaster
    {
    public:
        AppSettings()
        {
            juce::PropertiesFile::Options o;
            o.applicationName = "FM RX";
            o.folderName = "NUWMUN/FM RX";
            o.filenameSuffix = ".settings";
            o.osxLibrarySubFolder = "Application Support";
            props_ = std::make_unique<juce::PropertiesFile>(o);

            const auto sys = juce::SystemStats::getUserLanguage().toLowerCase();
            const auto lang = props_->getValue("language", sys.startsWith("ru") ? "ru" : "en");
            currentLang() = lang == "ru" ? Lang::Ru : Lang::En;
            applySkin(skinIndexForId(props_->getValue("skin", kSkins[0].id)));
        }

        Lang language() const { return currentLang(); }
        int skin() const { return currentSkin(); }

        void setLanguage(Lang l)
        {
            currentLang() = l;
            props_->setValue("language", l == Lang::Ru ? "ru" : "en");
            props_->saveIfNeeded();
            sendChangeMessage();
        }

        void setSkin(int index)
        {
            applySkin(index);
            props_->setValue("skin", kSkins[currentSkin()].id);
            props_->saveIfNeeded();
            sendChangeMessage();
        }

    private:
        std::unique_ptr<juce::PropertiesFile> props_;
    };
}
