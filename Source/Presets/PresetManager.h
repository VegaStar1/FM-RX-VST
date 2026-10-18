#pragma once
// Пресеты (только message-поток): заводские + пользовательские файлы *.fmrxpreset (XML) в
// <AppData>/NUWMUN/FM RX/Presets. Пресет меняет параметры через жесты хоста; имя текущего пресета
// хранится в состоянии проекта (свойство "presetName").
#include "Presets/FactoryPresets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace fmrx
{
    class PresetManager
    {
    public:
        struct Entry { juce::String name; bool factory = true; int factoryIndex = -1; juce::File file; };

        explicit PresetManager(juce::AudioProcessorValueTreeState& s) : apvts_(s) { refresh(); }

        static juce::File userDirectory()
        {
            return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("NUWMUN").getChildFile("FM RX").getChildFile("Presets");
        }

        void refresh()
        {
            entries_.clear();
            for (int i = 0; i < preset::kNumFactory; ++i)
                entries_.push_back({ preset::kFactory[i].name, true, i, {} });
            auto files = userDirectory().findChildFiles(juce::File::findFiles, false, "*.fmrxpreset");
            files.sort();
            for (auto& f : files)
                if (auto xml = juce::parseXML(f))
                    if (xml->hasTagName("FMRX_PRESET"))
                        entries_.push_back({ xml->getStringAttribute("name", f.getFileNameWithoutExtension()), false, -1, f });
        }

        const std::vector<Entry>& entries() const { return entries_; }
        juce::String currentName() const { return apvts_.state.getProperty("presetName", "").toString(); }

        int currentIndex() const
        {
            const auto n = currentName();
            for (int i = 0; i < (int) entries_.size(); ++i) if (entries_[(size_t) i].name == n) return i;
            return -1;
        }

        bool load(int index)
        {
            if (index < 0 || index >= (int) entries_.size()) return false;
            const auto& e = entries_[(size_t) index];
            std::array<float, preset::kNumIds> v {};
            if (! valuesFor(e, v)) return false;
            for (int i = 0; i < preset::kNumIds; ++i) setPlain(preset::kIds[i], v[(size_t) i]);
            apvts_.state.setProperty("presetName", e.name, nullptr);
            return true;
        }

        bool step(int delta)
        {
            if (entries_.empty()) return false;
            const int n = (int) entries_.size();
            int i = currentIndex();
            i = i < 0 ? (delta > 0 ? 0 : n - 1) : ((i + delta) % n + n) % n;
            return load(i);
        }

        // Параметры отличаются от сохранённых значений текущего пресета.
        bool isModified() const
        {
            const int idx = currentIndex();
            if (idx < 0) return false;
            std::array<float, preset::kNumIds> v {};
            if (! valuesFor(entries_[(size_t) idx], v)) return false;
            for (int i = 0; i < preset::kNumIds; ++i)
            {
                const auto* raw = apvts_.getRawParameterValue(preset::kIds[i]);
                auto* prm = apvts_.getParameter(preset::kIds[i]);
                if (raw == nullptr || prm == nullptr) continue;
                const auto range = prm->getNormalisableRange();
                const float span = juce::jmax(1e-6f, range.end - range.start);
                if (std::fabs(raw->load() - v[(size_t) i]) > span * 0.004f) return true;
            }
            return false;
        }

        bool saveUser(const juce::String& rawName)
        {
            const auto name = rawName.trim().substring(0, 48);
            if (name.isEmpty()) return false;
            auto dir = userDirectory();
            if (! dir.createDirectory()) return false;
            juce::XmlElement xml("FMRX_PRESET");
            xml.setAttribute("name", name);
            xml.setAttribute("version", 1);
            for (int i = 0; i < preset::kNumIds; ++i)
                if (auto* raw = apvts_.getRawParameterValue(preset::kIds[i]))
                {
                    auto* p = xml.createNewChildElement("PARAM");
                    p->setAttribute("id", preset::kIds[i]);
                    p->setAttribute("value", (double) raw->load());
                }
            const auto file = dir.getChildFile(juce::File::createLegalFileName(name) + ".fmrxpreset");
            if (! xml.writeTo(file)) return false;
            refresh();
            apvts_.state.setProperty("presetName", name, nullptr);
            return true;
        }

        bool deleteUser(int index)
        {
            if (index < 0 || index >= (int) entries_.size() || entries_[(size_t) index].factory) return false;
            const bool ok = entries_[(size_t) index].file.deleteFile();
            refresh();
            if (ok) apvts_.state.setProperty("presetName", "", nullptr);
            return ok;
        }

    private:
        bool valuesFor(const Entry& e, std::array<float, preset::kNumIds>& v) const
        {
            if (e.factory)
            {
                if (e.factoryIndex < 0 || e.factoryIndex >= preset::kNumFactory) return false;
                v = preset::kFactory[e.factoryIndex].v;
                return true;
            }
            auto xml = juce::parseXML(e.file);
            if (xml == nullptr || ! xml->hasTagName("FMRX_PRESET")) return false;
            for (int i = 0; i < preset::kNumIds; ++i)                      // отсутствующее -> текущее значение
                if (auto* raw = apvts_.getRawParameterValue(preset::kIds[i])) v[(size_t) i] = raw->load();
            for (auto* p : xml->getChildWithTagNameIterator("PARAM"))
            {
                const auto id = p->getStringAttribute("id");
                for (int i = 0; i < preset::kNumIds; ++i)
                    if (id == preset::kIds[i])
                    {
                        const double val = p->getDoubleAttribute("value", (double) v[(size_t) i]);
                        if (std::isfinite(val)) v[(size_t) i] = (float) val;
                    }
            }
            return true;
        }

        void setPlain(const char* id, float plain)
        {
            if (auto* prm = apvts_.getParameter(id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost(prm->convertTo0to1(plain));
                prm->endChangeGesture();
            }
        }

        juce::AudioProcessorValueTreeState& apvts_;
        std::vector<Entry> entries_;
    };
}
