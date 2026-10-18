#pragma once
// Глобальные настройки интерфейса (язык, скин, помощь при запуске): один экземпляр на процесс (SharedResourcePointer),
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

        // Окно помощи при запуске: показывается, пока пользователь не отметит «Больше не показывать».
        bool showHelpOnStart() const { return props_->getBoolValue("showHelpOnStart", true); }
        void setShowHelpOnStart(bool on)
        {
            props_->setValue("showHelpOnStart", on);
            props_->saveIfNeeded();
        }

        // Не больше одного автопоказа за запуск DAW (окно плагина могут открывать и закрывать много раз).
        static bool& helpShownThisSession() { static bool shown = false; return shown; }

        // PDF-инструкция, которую кладёт установщик: Windows - Program Files\NUWMUN\FM RX,
        // macOS - /Library/Application Support/NUWMUN/FM RX. Сначала на текущем языке, затем на другом.
        static juce::File findManual(Lang lang)
        {
            juce::Array<juce::File> dirs;
           #if JUCE_WINDOWS
            dirs.add(juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory).getChildFile("NUWMUN").getChildFile("FM RX"));
            dirs.add(juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("NUWMUN").getChildFile("FM RX"));
           #elif JUCE_MAC
            dirs.add(juce::File("/Library/Application Support/NUWMUN/FM RX"));
            dirs.add(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support/NUWMUN/FM RX"));
           #else
            dirs.add(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NUWMUN/FM RX"));
           #endif
            const char* first  = lang == Lang::Ru ? "FM RX Manual RU.pdf" : "FM RX Manual EN.pdf";
            const char* second = lang == Lang::Ru ? "FM RX Manual EN.pdf" : "FM RX Manual RU.pdf";
            for (auto* name : { first, second })
                for (auto& d : dirs)
                    if (d.getChildFile(name).existsAsFile()) return d.getChildFile(name);
            return {};
        }
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
