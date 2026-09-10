#pragma once

// Всё, что приложению нужно от wxl, и ни одного заголовка winrt: доступ к
// декларативной поверхности не должен стоить потребителю разбора проекции.

#include "Nullable.h"
#include "aliases.h"
#include "Card.h"
#include "generated/Members.h"
// Точка указателя: её отдаёт PointerRoutedEventArgs::getCurrentPoint, а
// объявлена она здесь -- без этого заголовка тип виден только по имени.
#include "generated/Microsoft.UI.Input.h"
// Очередь интерфейсного потока и её таймер: ими откладывается запись
// настроек.
#include "generated/Microsoft.UI.Dispatching.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/brushes.h"
#include "RsdnBlock.h"
#include "ShowDialog.h"
#include "launch.h"
