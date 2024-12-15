//
// Created by kate on 12/15/24.
//

#ifndef MIKOTO_EDITOR_ENUMS_H
#define MIKOTO_EDITOR_ENUMS_H

namespace Mikoto {

    /**
     * @brief Represents the current state of a system.
     * Can be used to control whether it is running, stopped or idling.
     * */
    enum class Status {
        /** Application is running. */
        RUNNING,

        /** Application has stopped. */
        STOPPED,

        /** Application is idle. */
        IDLE,
    };
}

#endif //MIKOTO_EDITOR_ENUMS_H
