/**
 * EventManager.cc
 * Created by kate on 10/7/23.
 * */

// C++ Standard Library
#include <utility>
#include <algorithm>
#include <iterator>

// Project Headers
#include <Common/ConfigLoader.hh>
#include <Core/System/EventSystem.hh>
#include <Library/Utility/Types.hh>
#include <ranges>

namespace Mikoto {
    // For now Container is expected to be a vector,
    // will probably change to another container
    template<typename Container, typename Iterator>
    concept HasEraseMember = std::random_access_iterator<Iterator> &&
                            requires (Container cont, Iterator it) { cont.erase(it); };

    EventSystem::EventSystem(const EngineConfig& options) {

    }

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

    auto EventSystem::Unsubscribe( const UInt64_T subId, const EventType type) -> void {
        auto it{ m_Subscribers.find(subId) };
        if (it != m_Subscribers.end()) {
            // TODO: maybe change container to erase friendly container
            EraseWithFilter(it->second, [&](const EventHandlerWrapper& wrapper) -> bool { return wrapper.GetType() == type; });
        }
    }

    auto EventSystem::Unsubscribe( const UInt64_T subId, const EventCategory category) -> void {
        auto it{ m_Subscribers.find(subId) };
        if (it != m_Subscribers.end()) {
            // TODO: maybe change container to erase friendly container
            EraseWithFilter(it->second, [&](const EventHandlerWrapper& wrapper) -> bool { return wrapper.GetCategory() == category; });
        }
    }

    auto EventSystem::ProcessEvents() -> void {
        for ( auto begin{ m_EventQueue.begin() }; begin != m_EventQueue.end(); ++begin) {
            auto& eventPtr{ *begin };

            for ( auto& listOfHandlers: m_Subscribers | std::views::values ) {
                const auto handlerWrapperIt{ std::ranges::find_if( listOfHandlers,
                    [&](const EventHandlerWrapper& a) -> bool { return eventPtr->IsInCategory(a.GetCategory()) && a.GetType() == eventPtr->GetType(); })
                };

                if (handlerWrapperIt != listOfHandlers.end()) {
                    handlerWrapperIt->GetHandler()(*eventPtr);
                }
            }
        }

        m_EventQueue.clear();
    }

    auto EventSystem::Init() -> void {

    }

    auto EventSystem::Update() -> void {

    }

    auto EventSystem::Shutdown() -> void {
        // Process pending events if any
        ProcessEvents();

        m_EventQueue.clear();
        m_Subscribers.clear();
    }
}
