//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_APPLICATION_HH
#define MIKOTO_APPLICATION_HH

#include <Core/LayerStack.hh>
#include <Core/Singleton.hh>

namespace mikoto::core {

    /**
     * @brief Represents the current state of a system.
     * Can be used to control whether it is running, stopped or idling.
     * */
    enum class ApplicationStatus {
        eRunning,
        eStopped,
        eIdle,
    };

    /**
     * @brief Manages the editor application lifecycle.
     * */
    class Application : public Singleton<Application> {
    public:
        /**
         * @brief Constructs an empty application.
         * */
        explicit Application() = default;

        /**
         * @brief Creates and initializes the editor app and runs the main loop.
         * */
        virtual auto Run() -> void = 0;

        /**
         * @brief Destructs this application after exiting its scope.
         * */
        ~Application() override = default;

        /**
         * @brief Initializes this application. Must call once when
         * the application is created. Initializes the app.
         * */
        virtual auto Init() -> void = 0;

        /**
         * @brief Shuts down this application. Call once
         * to terminate the application and free all of its
         * resources.
         * */
        virtual auto Shutdown() -> void = 0;

        /**
         * @brief Updates the application state.
         * */
        virtual auto Update() -> void = 0;


        template<typename LayerType, typename... Args>
        auto PushLayer(Args&&... args) -> void {
            mLayerStack.PushLayer<LayerType>( eastl::forward<Args>(args)... );
        }

        template<typename LayerType>
        auto PushLayer() -> void {
            mLayerStack.PopLayer<LayerType>();
        }

        /**
         * @brief Checks if the application is running.
         * @returns True if the application is running, false otherwise.
         * */
        MKT_NODISCARD auto IsRunning() const -> bool {
            return mState == ApplicationStatus::eRunning || mState == ApplicationStatus::eIdle;
        }

    protected:
        LayerStack mLayerStack{};
        ApplicationStatus mState{ ApplicationStatus::eRunning };
    };

}// namespace Mikoto

#endif// MIKOTO_APPLICATION_HH
