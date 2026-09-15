// Перевод ответа сервера в модель: из дерева wxl::json в структуры :model.
//
// Отдельно от :api и наружу нарочно. Разбор -- единственное место, где
// написано, как называются поля у сервера, и проверять его надо на готовом
// ответе, а не на живом форуме: тест кладёт кусок настоящего JSON и
// смотрит, что вышло.
//
// Ни одна из этих функций не отказывается читать. Поле, которого нет,
// читается умолчанием -- так устроен и wxl::json, где отсутствующее поле
// отвечает `null`, -- потому что обмен растёт: сервер добавляет поля и
// перестаёт присылать те, которых больше нет, и клиент, падающий от этого,
// ломается в тот день, когда сервер обновили. Испорченным считается только
// то, что не разобралось как JSON вовсе, и об этом скажет сам читатель.
//
// **Поток.** Дерево wxl::json живёт в пуле STA, а пул привязан к одному
// потоку -- тому, где он создан. Значит, и разбор, и всё здесь идёт на
// потоке пула, то есть на потоке интерфейса. Ответ по сети приходит куда
// угодно, а вот превращается в модель только там.

export module besedka.forum:parse;

import :model;
import std;
import wxl.json;
import wxl.core;

export namespace besedka::forum {

/// Время из ответа: ISO 8601 с дробной частью и смещением, как его пишет
/// сервер -- `2026-08-29T16:11:36.023+03:00`. Ответом всегда UTC.
///
/// Непрочитанное время -- начало эпохи, а не отказ: одна кривая дата не
/// стоит потерянной страницы сообщений, а в списке она сразу видна.
std::chrono::system_clock::time_point readTimestamp(wxl::core::u8_view stamp);

ForumGroup readForumGroup(const wxl::json::value& from);
ForumDescription readForum(const wxl::json::value& from);

/// Витрина целиком: `/forums` отвечает массивом.
std::vector<ForumDescription> readForums(const wxl::json::value& from);

Author readAuthor(const wxl::json::value& from);
MessageInfo readMessageInfo(const wxl::json::value& from);

/// Страница `/messages`: `items`, `total`, `offset`.
MessagePage readMessagePage(const wxl::json::value& from);

/// Сообщение с телом -- ответ `/messages/{id}?withBodies=true`.
Message readMessage(const wxl::json::value& from);

Account readAccount(const wxl::json::value& from);
ServiceInfo readServiceInfo(const wxl::json::value& from);

}  // namespace besedka::forum
