// Беседка -- клиент форума RSDN на wxl.winui.
//
// Здесь нет ни wWinMain, ни поднятия Windows App Runtime, ни наследника
// Application, ни XAML: всё это делает wxl и потом зовёт эту функцию.
//
// Что здесь есть -- сборка приложения из экранов и решения о том, что чем
// сменяется. Экранов четыре, и они те же, что у jana: заставка, витрина
// форумов, темы форума, сообщения темы. Порядок перехода тот же, а показ
// сообщения -- свой, через wxl::RsdnBlock.
//
// Запросы задаются отсюда, с потока интерфейса, и сюда же C++/WinRT
// возвращает ответ -- на том потоке он и разбирается. Почему иначе нельзя,
// написано в docs/decisions.md.

// Свои заголовки со стандартными внутри -- до всего, что тянет import
// wxl.core: заголовок, включённый после импорта, MSVC уже не принимает.
#include <windows.h>

#include <exception>
#include <filesystem>
#include <memory>
#include <string>

#include "WindowBackdrop.h"
#include "forum_screen.h"
#include "message_screen.h"
#include "splash_screen.h"
#include "topic_screen.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

using namespace besedka::app;

namespace forum = besedka::forum;

// Каким окно открывается. Место окна пока не запоминается -- это следующий
// шаг вместе с остальным хранилищем.
constexpr int32_t kInitialWidth = 1280;
constexpr int32_t kInitialHeight = 860;

/// Каталог рядом с исполняемым файлом. Путь без схемы XAML разрешает именно
/// оттуда, и ресурсы туда же кладёт сборка.
std::filesystem::path exeDirectory() {
    wchar_t path[MAX_PATH] = {};

    ::GetModuleFileNameW(nullptr, path, MAX_PATH);

    return std::filesystem::path(path).parent_path();
}

/// Отказ словами, которые можно показать. Транспорт называет причину сам
/// (сеть, сертификат, код ответа); всё прочее -- редкость, и о ней честнее
/// сказать «не вышло», чем выдумывать объяснение.
std::wstring reasonOf(const std::exception_ptr& why) {
    try {
        std::rethrow_exception(why);
    } catch (const forum::HttpError& refused) {
        const std::string said = refused.what();

        std::wstring wide(said.begin(), said.end());

        return refused.status() == 0 ? L"Сервер недоступен: " + wide
                                     : std::wstring(L"Сервер отказал: ") + wide;
    } catch (const std::exception&) {
        return L"Не вышло поговорить с сервером.";
    }
}

}  // namespace

wxl::Teardown wxl_launched() {
    auto window = Window{
        title = L"Беседка",
        minSize = {820, 560},
    };

    window.appWindow().resize({kInitialWidth, kInitialHeight});

    // Заставка -- ещё и задником окна. Остров XAML при быстрой растяжке
    // отстаёт от рамки на такт-другой, и в просвете видна поверхность окна,
    // стёртая белой кистью WinUI. wxl забирает её себе и рисует ту же
    // картинку синхронно, с тем же кадрированием.
    wxl::window_backdrop_image(window, (exeDirectory() / L"Assets/splash-screen.png").c_str());

    auto api = std::make_shared<forum::Api>();

    auto splash = std::make_shared<SplashScreen>();
    auto forums = std::make_shared<ForumScreen>();
    auto topics = std::make_shared<TopicScreen>();
    auto messages = std::make_shared<MessageScreen>();

    messages->setBaseDirectory((exeDirectory() / L"Assets").wstring());

    // ---- витрина форумов ----
    //
    // Запрос уходит с этого потока, и продолжения приходят на него же:
    // C++/WinRT возвращает корутину в апартамент, из которого её начали.
    // Поэтому внутри обработчика можно и трогать XAML, и разбирать ответ.
    const auto loadForums = [api, splash, forums, window] {
        splash->setStatus(L"Читаю витрину форумов…");
        window.content(splash->root());

        api->forums()
            .when_succeeded([splash, forums, window](
                                const std::vector<forum::ForumDescription>& list) noexcept {
                forums->show(list);
                window.content(forums->root());
            })
            .when_failed([splash](const std::exception_ptr& why) noexcept {
                splash->setError(reasonOf(why));
            });
    };

    splash->onRetry = loadForums;
    forums->onRefresh = loadForums;

    // ---- темы форума ----
    forums->onOpen = [api, topics, window](const forum::ForumDescription& forum) {
        topics->setForum(forum);
        window.content(topics->root());

        api->topics(forum.id, 50)
            .when_succeeded([topics](const forum::MessagePage& page) noexcept { topics->show(page); })
            .when_failed([topics](const std::exception_ptr& why) noexcept {
                topics->setError(reasonOf(why));
            });
    };

    topics->onBack = [forums, window] { window.content(forums->root()); };

    // ---- сообщения темы ----
    //
    // Тела приезжают вместе со списком: одна поездка на всю тему.
    forums->onRefresh = loadForums;

    topics->onOpen = [api, messages, window](const forum::MessageInfo& topic) {
        messages->setTopic(topic);
        window.content(messages->root());

        api->answers(topic.id, 200)
            .when_succeeded(
                [messages](const forum::MessagePage& page) noexcept { messages->show(page); })
            .when_failed([messages](const std::exception_ptr& why) noexcept {
                messages->setError(reasonOf(why));
            });
    };

    messages->onBack = [topics, window] { window.content(topics->root()); };

    window.content(splash->root());
    window.activate();

    loadForums();

    // Обработчик держит окно и экраны живыми ровно столько, сколько живёт
    // приложение.
    return [window, api, splash, forums, topics, messages](Reason) {};
}
