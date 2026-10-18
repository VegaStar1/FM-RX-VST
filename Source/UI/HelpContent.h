#pragma once
// Текст встроенной помощи (RU/EN). Разметка тела раздела: строка «# ...» - подзаголовок, «- ...» - пункт
// списка, остальное - абзац. Полная инструкция - PDF (docs/manual), устанавливается вместе с плагином.
#include <iterator>

namespace fmrx::ui::help
{
    struct Section { const char* titleEn; const char* titleRu; const char* bodyEn; const char* bodyRu; };

    inline const Section kSections[] = {
        { "Quick start", "Быстрый старт",
          "FM RX is a virtual FM band: up to eight stations broadcast at once, and you tune between them like on a real radio. "
          "The sound goes through real FM modulation, the air and a receiver, so hiss, whistles and stereo dropouts happen by themselves.\n"
          "# In four steps\n"
          "- Drop audio files onto the station rows (or press LOAD).\n"
          "- Drag the dial on the band display, turn the wheel or use the TUNE slider.\n"
          "- Press SEARCH to jump to the next station automatically.\n"
          "- Shape the air (NOISE, FADING, zones) and the receiver to taste, or pick a preset.\n"
          "# Tip\n"
          "Automate TUNE in your DAW to \"turn the dial\" with an envelope.",
          "FM RX - это виртуальный FM-диапазон: до восьми станций вещают одновременно, а вы настраиваетесь между ними, как на настоящем приёмнике. "
          "Звук проходит настоящую ЧМ-модуляцию, эфир и приёмник, поэтому шипение, свист и распад стерео получаются сами.\n"
          "# За четыре шага\n"
          "- Перетащите аудиофайлы на строки станций (или нажмите LOAD).\n"
          "- Тащите стрелку на шкале диапазона, крутите колесо или ползунок TUNE.\n"
          "- Нажмите ПОИСК - приёмник сам перейдёт к следующей станции.\n"
          "- Настройте эфир (ШУМ, ЗАМИРАНИЯ, зоны) и приёмник по вкусу или выберите пресет.\n"
          "# Совет\n"
          "Автоматизируйте TUNE в DAW, чтобы «крутить эфир» огибающей." },

        { "Stations", "Станции",
          "Each of the eight rows is a transmitter.\n"
          "- Number button - tune to this station.\n"
          "- Name - double-click to rename (shown as RDS on the display).\n"
          "- FILE / INPUT - play a file or the audio of the track the plugin sits on.\n"
          "- LOAD and the × button - choose or clear the file. WAV, AIFF, MP3, FLAC and OGG are supported.\n"
          "- LOOP / 1-SHOT - play in a circle, or once: when a 1-SHOT station ends, the receiver searches for the next one by itself.\n"
          "- MHz - station frequency (drag; Shift - finer; double-click - type).\n"
          "- dB - transmitter power. In FM a weak station is noisier, not quieter.\n"
          "- ST - stereo broadcast, ON - station on air.\n"
          "# File editor\n"
          "Click the mini waveform (or the file name) to open the editor in place of the list.",
          "Каждая из восьми строк - это передатчик.\n"
          "- Кнопка с номером - настроиться на станцию.\n"
          "- Имя - двойной клик, чтобы переименовать (показывается на табло как RDS).\n"
          "- ФАЙЛ / ВХОД - играть файл или звук дорожки, на которой стоит плагин.\n"
          "- LOAD и кнопка × - выбрать или убрать файл. Поддерживаются WAV, AIFF, MP3, FLAC и OGG.\n"
          "- LOOP / 1-SHOT - играть по кругу или один раз: когда станция 1-SHOT доиграла, приёмник сам ищет следующую.\n"
          "- MHz - частота станции (тащите; Shift - точнее; двойной клик - ввести).\n"
          "- dB - мощность передатчика. В FM слабая станция шумит, а не становится тише.\n"
          "- ST - стерео-вещание, ON - станция в эфире.\n"
          "# Редактор файла\n"
          "Клик по мини-волне (или по имени файла) открывает редактор на месте списка." },

        { "Tuning and search", "Настройка и поиск",
          "# Dial\n"
          "- Click a station peak to tune to it; click empty space to jump there.\n"
          "- Drag to turn the dial (Shift - finer), wheel - 50 kHz steps.\n"
          "- The arrow buttons next to the slider step by 0.05 MHz.\n"
          "# Buttons\n"
          "- SEARCH ◀ ▶ - sweeps the band until it finds a station the receiver can hear, then settles on it. Weak stations are skipped.\n"
          "- SCAN - stops on every station for 5 seconds and moves on.\n"
          "- STOP - stops the search; the station you hear stops and rewinds. Double STOP - all stations from the start, dial to 87.5 MHz.\n"
          "- LOCK - holds the frequency: the dial and search are ignored.\n"
          "- MUTE - silences the whole receiver, noise included.\n"
          "# SEARCH row\n"
          "SPEED - how fast the dial moves, SENSITIVITY - whether search stops on weak stations, BUTTON CLICKS - volume of the mechanical clicks (0 - off).\n"
          "# HAND-OVER row (after a 1-SHOT)\n"
          "- PAUSE - how long to wait after a 1-SHOT station ends before searching for the next one (0 - at once, up to 5 s).\n"
          "- HAND-OVER NOISE - how loud the noise is while switching; 0 - a silent switch.\n"
          "- KEEP TUNE AUTOMATION - search, scan and hand-over never write the TUNE parameter, so your DAW automation (e.g. in Ableton) is not switched off.",
          "# Шкала\n"
          "- Клик по пику станции - настройка на неё; клик по пустому месту - переход туда.\n"
          "- Перетаскивание - вращение ручки (Shift - точнее), колесо - шаг 50 кГц.\n"
          "- Стрелки рядом с ползунком - шаг 0.05 МГц.\n"
          "# Кнопки\n"
          "- ПОИСК ◀ ▶ - листает диапазон, пока не найдёт станцию, которую слышно, и доводит на неё. Слабые пропускает.\n"
          "- СКАН - останавливается на каждой станции на 5 секунд и едет дальше.\n"
          "- СТОП - останавливает поиск; слышимая станция останавливается и встаёт в начало. Двойной СТОП - все станции с начала, стрелка на 87.5 МГц.\n"
          "- LOCK - фиксирует частоту: ручка и поиск не действуют.\n"
          "- MUTE - полностью выключает приёмник, вместе с шумом.\n"
          "# Строка ПОИСК\n"
          "СКОРОСТЬ - как быстро едет ручка, ЧУВСТВИТЕЛЬНОСТЬ - останавливаться ли на слабых станциях, ЩЕЛЧКИ КНОПОК - громкость механических щелчков (0 - выкл).\n"
          "# Строка ПЕРЕХОД (после 1-SHOT)\n"
          "- ПАУЗА - сколько ждать после окончания станции 1-SHOT, прежде чем искать следующую (0 - сразу, до 5 с).\n"
          "- ШУМ ПЕРЕХОДА - громкость шума во время переключения; 0 - тихое переключение.\n"
          "- ЗАЩИТА АВТОМАТИЗАЦИИ - поиск, скан и автопереход не пишут в параметр TUNE, и автоматизация в DAW (например, в Ableton) не отключается." },

        { "Station editor", "Редактор станции",
          "Trims the file without changing it on disk; the trim is saved with your project.\n"
          "- Green START and red END markers - drag them. The darkened parts do not play. Markers snap to zero crossings so there is no click.\n"
          "- Double-click a marker - reset it; double-click a time - type it (0:02.500).\n"
          "- Wheel - zoom, Shift+wheel - scroll; the strip below shows the whole file.\n"
          "- AUTO-TRIM SILENCE - cuts silence at both ends by the SILENCE THRESHOLD.\n"
          "- FADE-IN / FADE-OUT - short fades at the edges of the region.\n"
          "- ▶ FROM START - tunes the receiver to the station and starts it from START, so you hear it through the radio.\n"
          "- RESET - whole file, default fades. ← STATIONS or Esc - back to the list.\n"
          "LOOP plays the region in a circle with a crossfade; 1-SHOT plays it once. STOP returns the station to START.",
          "Обрезает файл, не меняя его на диске; обрезка сохраняется в проекте.\n"
          "- Зелёный маркер НАЧАЛО и красный КОНЕЦ - тяните их. Затемнённые части не звучат. Маркеры прилипают к нулю волны, чтобы не было щелчка.\n"
          "- Двойной клик по маркеру - сброс; двойной клик по времени - ввести вручную (0:02.500).\n"
          "- Колесо - зум, Shift+колесо - прокрутка; полоса снизу показывает весь файл.\n"
          "- АВТО-ОБРЕЗКА ТИШИНЫ - срезает тишину с обоих концов по ПОРОГУ ТИШИНЫ.\n"
          "- FADE-IN / FADE-OUT - короткие появление и затухание на краях отрезка.\n"
          "- ▶ С НАЧАЛА - настраивает приёмник на станцию и запускает её с НАЧАЛА: слушаете через радио.\n"
          "- СБРОС - весь файл, fade по умолчанию. ← СТАНЦИИ или Esc - назад к списку.\n"
          "LOOP крутит отрезок по кругу с кроссфейдом, 1-SHOT играет его один раз. СТОП возвращает станцию на НАЧАЛО." },

        { "Air and receiver", "Эфир и приёмник",
          "# Air · channel\n"
          "- NOISE - air noise level. FADING and FADE RATE - depth and speed of signal fades.\n"
          "- MULTIPATH - a delayed reflection (distortion on fades). IMPULSE - crackles. DRIFT - slow oscillator drift.\n"
          "- CLEAN ZONE - where around each station it sounds clean. NOISE ZONE - how long it slowly emerges from the hiss as you tune in. Both zones are drawn on the band.\n"
          "# Receiver\n"
          "- IF BW - IF filter width: narrower - fewer neighbours, more distortion.\n"
          "- BLEND - how early stereo falls to mono on a weak signal. SOFT MUTE - hushes the hiss between stations. HI-CUT - treble cut on weak signals.\n"
          "- SPEAKER - small speaker colour. OUTPUT - output level.\n"
          "- QUAD / PLL - demodulator type, 50 / 75 µs - de-emphasis, STEREO / MONO, AFC - automatic frequency control.\n"
          "# Noise colour\n"
          "Shapes the hiss only - clean stations do not change; the noisier the signal, the stronger the effect.\n"
          "- TONE - left: dull, right: bright.\n"
          "- FREQUENCY and SHARPNESS - where the noise is emphasised and how sharp, whistling it sounds there.",
          "# Эфир · канал\n"
          "- ШУМ - уровень шума эфира. ЗАМИРАНИЯ и СКОРОСТЬ - глубина и скорость провалов сигнала.\n"
          "- ОТРАЖЕНИЯ - многолучёвость (искажения на провалах). ЩЕЛЧКИ - импульсные помехи. ДРЕЙФ - медленный уход частоты.\n"
          "- ЧИСТАЯ ЗОНА - где вокруг станции она звучит чисто. ШУМОВАЯ ЗОНА - как долго станция проступает из шума при подстройке. Обе зоны видны на шкале.\n"
          "# Приёмник\n"
          "- ПОЛОСА ПЧ - ширина ПЧ-фильтра: уже - меньше соседей, но больше искажений.\n"
          "- БЛЕНД - как рано стерео уходит в моно на слабом сигнале. ШУМОДАВ - глушит шум между станциями. СРЕЗ ВЧ - срез верхов на слабом сигнале.\n"
          "- ДИНАМИК - окраска маленьким динамиком. ВЫХОД - выходной уровень.\n"
          "- QUAD / PLL - тип демодулятора, 50 / 75 мкс - деэмфазис, STEREO / MONO, AFC - автоподстройка частоты.\n"
          "# Цвет шума\n"
          "Меняет только шипение - чистые станции не меняются; чем больше шума в сигнале, тем сильнее действие.\n"
          "- ТОН - влево глухо, вправо звонко.\n"
          "- ЧАСТОТА и РЕЗКОСТЬ - где подчёркивается шум и насколько резко, «свистяще» он там звучит." },

        { "Presets and settings", "Пресеты и настройки",
          "# Presets\n"
          "- The arrows in the header step through presets; click the name for the full list.\n"
          "- 12 factory presets: from CLEAN FM to STORM and DX OVERSEAS. A * after the name means you changed something.\n"
          "- Save preset... keeps your own; they are stored for all projects. Presets change the air and receiver only - stations and frequency stay.\n"
          "# Settings (gear)\n"
          "- Language: Russian or English. Skin: five colour themes. Both apply to all projects.\n"
          "- Help - opens this window again.",
          "# Пресеты\n"
          "- Стрелки в шапке листают пресеты; клик по имени - полный список.\n"
          "- 12 заводских: от CLEAN FM до STORM и DX OVERSEAS. Звёздочка после имени - вы что-то изменили.\n"
          "- «Сохранить пресет...» - свой пресет, доступен во всех проектах. Пресет меняет только эфир и приёмник: станции и частота остаются.\n"
          "# Настройки (шестерёнка)\n"
          "- Язык: русский или английский. Скин: пять цветовых тем. Действуют во всех проектах.\n"
          "- Помощь - снова открывает это окно." },
    };
    inline constexpr int kNumSections = (int) std::size(kSections);
}
