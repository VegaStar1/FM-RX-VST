#pragma once
// Локализация интерфейса: tr("key") -> строка на текущем языке (EN / RU). Только message-поток.
#include <juce_core/juce_core.h>
#include <map>
#include <string>

namespace fmrx::ui
{
    enum class Lang { En = 0, Ru = 1 };
    inline Lang& currentLang() { static Lang l = Lang::En; return l; }

    namespace detail
    {
        struct Pair { const char* en; const char* ru; };
        inline const std::map<std::string, Pair>& table()
        {
            static const std::map<std::string, Pair> t = {
                // шапка и общие
                { "subtitle",        { "8-STATION BAND RECEIVER", "8-СТАНЦИОННЫЙ FM-ПРИЁМНИК" } },
                { "preset",          { "Preset", "Пресет" } },
                { "preset_none",     { "- no preset -", "- без пресета -" } },
                { "preset_save",     { "Save preset...", "Сохранить пресет..." } },
                { "preset_delete",   { "Delete preset", "Удалить пресет" } },
                { "preset_factory",  { "Factory", "Заводские" } },
                { "preset_user",     { "User", "Мои" } },
                { "preset_name",     { "Preset name", "Имя пресета" } },
                { "save",            { "Save", "Сохранить" } },
                { "cancel",          { "Cancel", "Отмена" } },
                { "settings",        { "Settings", "Настройки" } },
                // кнопки управления
                { "search",          { "SEARCH", "ПОИСК" } },
                { "scan",            { "SCAN", "СКАН" } },
                { "stop",            { "STOP", "СТОП" } },
                { "lock",            { "LOCK", "LOCK" } },
                { "mute",            { "MUTE", "MUTE" } },
                { "tip_search_up",   { "Search up: sweeps the band until it finds a station", "Поиск вверх: листает диапазон, пока не найдёт станцию" } },
                { "tip_search_dn",   { "Search down", "Поиск вниз" } },
                { "tip_scan",        { "Scan: stops on every station for a few seconds", "Скан: останавливается на каждой станции на несколько секунд" } },
                { "tip_stop",        { "Stop search; the station you hear stops and rewinds. Double click: everything from the start", "Стоп поиска; слышимая станция останавливается и встаёт в начало. Двойной клик - всё в самое начало" } },
                { "tip_lock",        { "Lock the frequency", "Зафиксировать частоту" } },
                { "tip_mute",        { "Mute the whole receiver, noise included", "Полностью выключить приёмник, вместе с шумом" } },
                { "tip_step_dn",     { "-0.05 MHz", "-0.05 МГц" } },
                { "tip_step_up",     { "+0.05 MHz", "+0.05 МГц" } },
                { "tip_tune",        { "TUNE - automate it to sweep the dial with an envelope", "TUNE - автоматизируйте, чтобы «крутить эфир» огибающей" } },
                { "tip_afc",         { "Automatic frequency control: pulls onto the station", "Автоподстройка частоты: «дотягивает» на станцию" } },
                { "tip_stmode",      { "Force mono", "Принудительное моно" } },
                { "tip_gear",        { "Settings: language, skin, help", "Настройки: язык, скин, помощь" } },
                // помощь
                { "help_title",      { "HOW TO USE FM\xc2\xb7RX", "КАК ПОЛЬЗОВАТЬСЯ FM\xc2\xb7RX" } },
                { "help_dont_show",  { "Don't show at startup", "Больше не показывать при запуске" } },
                { "help_pdf",        { "Full manual (PDF)", "Полная инструкция (PDF)" } },
                { "help_pdf_tip",    { "Opens the PDF manual installed with the plugin", "Открывает PDF-инструкцию, установленную вместе с плагином" } },
                { "help_pdf_none",   { "The PDF manual was not found - reinstall the plugin with the installer", "PDF-инструкция не найдена - переустановите плагин установщиком" } },
                { "help_prev",       { "\xe2\x97\x80 BACK", "\xe2\x97\x80 НАЗАД" } },
                { "help_next",       { "NEXT \xe2\x96\xb6", "ДАЛЕЕ \xe2\x96\xb6" } },
                { "help_done",       { "GOT IT", "ПОНЯТНО" } },
                { "set_help",        { "Help", "Помощь" } },
                { "set_help_btn",    { "Open help", "Открыть помощь" } },
                // строка параметров поиска
                { "row_search",      { "SEARCH", "ПОИСК" } },
                { "row_speed",       { "SPEED", "СКОРОСТЬ" } },
                { "row_sens",        { "SENSITIVITY", "ЧУВСТВИТЕЛЬНОСТЬ" } },
                { "row_clicks",      { "BUTTON CLICKS", "ЩЕЛЧКИ КНОПОК" } },
                { "tip_speed",       { "How fast the dial moves while searching and scanning", "Как быстро ручка «едет» при поиске и скане" } },
                { "tip_sens",        { "Higher - search also stops on weak stations", "Выше - поиск останавливается и на слабых станциях" } },
                { "tip_clicks",      { "Volume of the mechanical button clicks; 0 - off", "Громкость механических щелчков кнопок; 0 - выкл" } },
                // панели
                { "stations",        { "STATIONS", "СТАНЦИИ" } },
                { "stations_hint",   { "drop audio files onto the rows", "перетащите файлы на строки" } },
                { "air",             { "AIR · CHANNEL", "ЭФИР · КАНАЛ" } },
                { "air_hint",        { "what happens on the air", "что происходит в эфире" } },
                { "receiver",        { "RECEIVER", "ПРИЁМНИК" } },
                // ручки
                { "k_noise",         { "NOISE", "ШУМ" } },
                { "k_fading",        { "FADING", "ЗАМИРАНИЯ" } },
                { "k_faderate",      { "FADE RATE", "СКОРОСТЬ" } },
                { "k_multipath",     { "MULTIPATH", "ОТРАЖЕНИЯ" } },
                { "k_impulse",       { "IMPULSE", "ЩЕЛЧКИ" } },
                { "k_drift",         { "DRIFT", "ДРЕЙФ" } },
                { "k_clean",         { "CLEAN ZONE", "ЧИСТАЯ ЗОНА" } },
                { "k_noisezone",     { "NOISE ZONE", "ШУМОВАЯ ЗОНА" } },
                { "k_ifbw",          { "IF BW", "ПОЛОСА ПЧ" } },
                { "k_blend",         { "BLEND", "БЛЕНД" } },
                { "k_softmute",      { "SOFT MUTE", "ШУМОДАВ" } },
                { "k_hicut",         { "HI-CUT", "СРЕЗ ВЧ" } },
                { "k_speaker",       { "SPEAKER", "ДИНАМИК" } },
                { "k_output",        { "OUTPUT", "ВЫХОД" } },
                { "t_noise",         { "Air noise level", "Уровень шума эфира" } },
                { "t_fading",        { "Depth of Rayleigh fading", "Глубина рэлеевских замираний" } },
                { "t_faderate",      { "Fading speed", "Скорость замираний" } },
                { "t_multipath",     { "Multipath: a delayed reflection", "Многолучёвость: отражённый сигнал с задержкой" } },
                { "t_impulse",       { "Impulse interference (clicks)", "Импульсные помехи (щелчки)" } },
                { "t_drift",         { "Slow local oscillator drift", "Медленный уход частоты гетеродина" } },
                { "t_clean",         { "Half-width of the zone around each station where it sounds clean", "Половина ширины зоны вокруг станции, где она звучит чисто" } },
                { "t_noisezone",     { "Width of the transition where the station slowly emerges from the noise", "Ширина перехода, где станция постепенно проступает из шума" } },
                { "t_ifbw",          { "IF filter width: narrower - fewer neighbours, more distortion", "Ширина ПЧ-фильтра: уже - меньше соседей, но больше искажений" } },
                { "t_blend",         { "How early stereo blends to mono on a weak signal", "Как рано стерео уходит в моно на слабом сигнале" } },
                { "t_softmute",      { "Mutes the hiss between stations and on weak signals", "Приглушение шума между станциями и на слабом сигнале" } },
                { "t_hicut",         { "Treble cut on weak signals", "Срез верхов на слабом сигнале" } },
                { "t_speaker",       { "Small speaker colouration", "Окраска маленьким динамиком" } },
                { "t_output",        { "Output level", "Выходной уровень" } },
                { "t_demod",         { "Demodulator: quadrature or PLL - they sound different when detuned", "Демодулятор: квадратурный или ФАПЧ - по-разному звучат на расстройке" } },
                { "t_deemph",        { "De-emphasis: 50 us (Europe) or 75 us (USA)", "Деэмфазис: 50 мкс (Европа) или 75 мкс (США)" } },
                // строки станций
                { "st_tune",         { "Tune to this station", "Настроиться на станцию" } },
                { "st_rename",       { "Double-click to rename (RDS)", "Двойной клик - переименовать (RDS)" } },
                { "st_src",          { "Audio source: file or track input", "Источник звука: файл или вход дорожки" } },
                { "st_load",         { "Choose an audio file (WAV, AIFF, MP3, FLAC, OGG) - or drop it onto the row", "Выбрать аудиофайл (WAV, AIFF, MP3, FLAC, OGG) - или перетащите его на строку" } },
                { "st_clear",        { "Clear slot", "Очистить слот" } },
                { "st_freq",         { "Station frequency (drag; Shift - finer; double-click - type)", "Частота станции (тащите; Shift - точнее; двойной клик - ввести)" } },
                { "st_level",        { "Transmitter power: a weak station is noisier, not quieter (it's FM)", "Мощность передатчика: слабая станция шумит, но не тише (это FM)" } },
                { "st_stereo",       { "Stereo broadcast (19 kHz pilot)", "Стерео-вещание (пилот 19 кГц)" } },
                { "st_on",           { "Station on air", "Станция в эфире" } },
                { "st_mode",         { "LOOP - plays in a circle; 1-SHOT - plays once, then the receiver searches for the next station", "LOOP - по кругу; 1-SHOT - один раз, затем приёмник ищет следующую станцию" } },
                { "st_empty",        { "- empty -", "- пусто -" } },
                { "st_input",        { "Track input", "Вход дорожки" } },
                { "st_loading",      { "loading...", "загрузка..." } },
                { "st_missing",      { "file not found: ", "файл не найден: " } },
                { "st_failed",       { "cannot read: ", "не удалось прочитать: " } },
                { "st_drop",         { "drop a file or LOAD", "перетащите файл или LOAD" } },
                { "st_parked",       { "stopped", "стоп" } },
                { "st_done",         { "finished", "доиграла" } },
                { "st_file_title",   { "Audio file for station ", "Файл для станции " } },
                { "src_file",        { "File", "Файл" } },
                { "src_input",       { "Input", "Вход" } },
                // редактор станции
                { "ed_back",         { "\xe2\x86\x90 STATIONS", "\xe2\x86\x90 СТАНЦИИ" } },
                { "ed_reset",        { "RESET", "СБРОС" } },
                { "ed_autotrim",     { "AUTO-TRIM SILENCE", "АВТО-ОБРЕЗКА ТИШИНЫ" } },
                { "ed_play",         { "\xe2\x96\xb6 FROM START", "\xe2\x96\xb6 С НАЧАЛА" } },
                { "ed_fadein",       { "FADE-IN", "FADE-IN" } },
                { "ed_fadeout",      { "FADE-OUT", "FADE-OUT" } },
                { "ed_thr",          { "SILENCE THRESHOLD", "ПОРОГ ТИШИНЫ" } },
                { "ed_start",        { "START", "НАЧАЛО" } },
                { "ed_end",          { "END", "КОНЕЦ" } },
                { "ed_length",       { "LENGTH", "ДЛИНА" } },
                { "ed_of",           { "of", "из" } },
                { "ed_hint",         { "Wheel - zoom, Shift+wheel - scroll. Double-click a marker - reset, a time - type it.", "Колесо - зум, Shift+колесо - прокрутка. Двойной клик по маркеру - сброс, по времени - ввести." } },
                { "ed_nofile",       { "Load an audio file into this station to edit it", "Загрузите файл в станцию, чтобы его редактировать" } },
                { "ed_rename",       { "Double-click to rename (RDS)", "Двойной клик - переименовать (RDS)" } },
                { "tip_reset",       { "Whole file, default fades", "Весь файл, fade по умолчанию" } },
                { "tip_autotrim",    { "Cut the silence at the start and the end by the threshold", "Срезать тишину в начале и в конце по порогу" } },
                { "tip_play",        { "Tune to this station and start it from the START marker", "Настроиться на станцию и запустить с маркера НАЧАЛО" } },
                { "st_open",         { "Click - file editor (trim start and end)", "Клик - редактор файла (обрезка начала и конца)" } },
                // настройки
                { "set_title",       { "SETTINGS", "НАСТРОЙКИ" } },
                { "set_lang",        { "Language", "Язык" } },
                { "set_skin",        { "Skin", "Скин" } },
                { "set_clicks",      { "Button clicks", "Щелчки кнопок" } },
                { "set_speed",       { "Search speed", "Скорость поиска" } },
                { "set_sens",        { "Search sensitivity", "Чувствительность поиска" } },
                { "set_note",        { "Language and skin apply to all projects.", "Язык и скин действуют во всех проектах." } },
                { "close",           { "Close", "Закрыть" } },
                // табло
                { "tuned",           { "TUNED", "НАСТР." } },
                { "searching",       { "SEARCH", "ПОИСК" } },
                { "scanning",        { "SCAN", "СКАН" } },
                { "muted",           { "MUTED", "MUTE" } },
                { "locked",          { "LOCKED", "LOCK" } },
                { "band",            { "BAND 87.5 - 108.0 MHz", "ДИАПАЗОН 87.5 - 108.0 МГц" } },
            };
            return t;
        }
    }

    inline juce::String tr(const char* key)
    {
        const auto& t = detail::table();
        const auto it = t.find(key);
        if (it == t.end()) return juce::String(key);
        return juce::String::fromUTF8(currentLang() == Lang::Ru ? it->second.ru : it->second.en);
    }
}
