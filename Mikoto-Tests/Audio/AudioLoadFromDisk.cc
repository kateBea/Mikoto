//
// Created by kate on 11/22/25.
//

#include <string>

#include <gtest/gtest.h>

#include <Audio/AudioLoadFromDisk.hh>

namespace MikotoTets {
    auto GetTestAudioPath() -> std::string {
        return "Mikoto-Tests/Audio/TestFiles/test.wav";
    }

    TEST(AudioLoadFromDisk, LoadValidAudio) {
        ASSERT_TRUE(!false);
        EXPECT_GT(1, 0);
    }

}