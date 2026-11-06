# ClannadKI — VST3 плагин для бочки (FL Studio)

Интерфейс в стиле визуальной новеллы: ручки находятся в диалоговом окне внизу,
а фон за ним плавно меняется (около 3 секунд), когда вы выбираете стиль (Character).

## Обновить плагин на GitHub (если репозиторий уже есть)

1. Распакуйте архив и откройте свой репозиторий на github.com.
2. **Add file → Upload files**, перетащите всё содержимое папки `ClannadKI`
   (`Source`, `Resources`, `.github`, `CMakeLists.txt`, `README.md`).
   Файлы с теми же именами заменятся.
3. **Commit changes**, затем вкладка **Actions**: сборка запустится сама.
4. После зелёной галочки скачайте артефакт `ClannadKI-VST3` (раздел **Artifacts** внизу страницы запуска).
5. Скопируйте `ClannadKI.vst3` в `C:\Program Files\Common Files\VST3\`.
   Старый `Sora Kick.vst3` оттуда можно удалить.
6. FL Studio: **Options → Manage plugins → Find installed plugins**.

## Свои картинки

Подробности в `Resources/README.txt`: PNG с именами `scene1.png` … `scene9.png`
заменяют фоны, `heroine.png` — спрайт персонажа. Без них используются
нарисованные по умолчанию небо, закат, ночь, сакура и т.д.

## Шрифт

Используется Tahoma (есть в Windows). Чтобы поменять, откройте `Source/PluginEditor.h`
и замените `"Tahoma"` в функции `vnFont` на любой другой установленный шрифт.

## Параметры

Те же, что и раньше: Attack, Sustain, Length, Sub, Click, Low/High Cut, Drive (+тип),
Bits, Rate, Screech, Sub Layer (+форма волны, частота, затухание), Pitch Drop, Drop Time,
Trigger, Rumble, Mix, Output. Порог Trigger должен быть ниже пика бочки.
