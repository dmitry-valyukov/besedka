#pragma once

// Всё, что приложению нужно от wxl, и ни одного заголовка winrt: доступ к
// декларативной поверхности не должен стоить потребителю разбора проекции.

#include "aliases.h"
#include "Card.h"
#include <wxl/Members.h>
// Точка указателя: её отдаёт PointerRoutedEventArgs::getCurrentPoint, а
// объявлена она здесь -- без этого заголовка тип виден только по имени.
#include <wxl/Microsoft.UI.Input.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include <wxl/brushes.h>
#include "RsdnBlock.h"
#include "ShowDialog.h"
#include "launch.h"
