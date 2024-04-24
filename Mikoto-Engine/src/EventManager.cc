/**
 * EventManager.cc
 * Created by kate on 10/7/23.
 * */

// C++ Standard Library
#include <utility>
#include <algorithm>
#include <iterator>

// Project Headers
#include "../Common/Types.hh"
#include "Core/EventManager.hh"

namespace Mikoto::EventManager {
    // For now Container is expected to be a vector,
    // will probably change to another container
    template<typename Container, typename Iterator>
    concept HasEraseMember = std::random_access_iterator<Iterator> &&
                            requires (Container cont, Iterator it) { cont.erase(it); };


    /**
     * Erase elements from a container that meets the Unary predicate "filter".
     * Container must be iterable and offer erase() operation which takes an iterator and returns
     * an iterator to the erased element
     * */
    template<typename FilterFuncType, typename Container>
        requires HasEraseMember<Container, typename Container::iterator>
    static auto EraseWithFilter(Container& container, const FilterFuncType& filter) -> void {
        auto begin{ container.begin() };

        for ( ; begin != container.end(); ++begin) {
            if (filter(*begin)) {
                begin = container.erase(begin);
            }
        }
    }


    auto Unsubscribe(UInt64_T subId, EventType type) -> void {
        auto& subscribers{ GetSubscribers() };

        auto it{ subscribers.find(subId) };
        if (it != subscribers.end()) {
            // TODO: maybe change container to set
            EraseWithFilter(it->second, [&](const EventHandlerWrapper& wrapper) -> bool { return wrapper.GetType() == type; });
        }
    }


    auto Unsubscribe(UInt64_T subId, EventCategory category) -> void {
        auto& subscribers{ GetSubscribers() };

        auto it{ subscribers.find(subId) };
        if (it != subscribers.end()) {
            // TODO: maybe change container to set
            EraseWithFilter(it->second, [&](const EventHandlerWrapper& wrapper) -> bool { return wrapper.GetCategory() == category; });
        }
    }


    auto ProcessEvents() -> void {
        auto& subscribers{ GetSubscribers() };
        auto& eventQueue{ GetEventQueue() };

        // Traverse event queue
        auto begin{ eventQueue.begin() };
        for ( ; begin != eventQueue.end(); ++begin) {
            auto& eventPtr{ *begin };

            for (auto& [subId, listOfHandlers] : subscribers) {
                const auto handlerWrapper{ std::find_if(listOfHandlers.begin(),
                                              listOfHandlers.end(),
                                              [&](const EventHandlerWrapper& a) -> bool { return eventPtr->IsInCategory(a.GetCategory()) && a.GetType() == eventPtr->GetType(); }) };

                if (handlerWrapper != listOfHandlers.end()) { handlerWrapper->GetHandler()(*eventPtr); }
            }
        }

        eventQueue.clear();
    }


    auto Shutdown() -> void {
        GetEventQueue().clear();
        GetSubscribers().clear();
    }
}
