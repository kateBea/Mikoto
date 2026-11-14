#    Copyright 2026 ケイト
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Slang 2026.17.1 expands the parent's next macro while recording the source
# location following an #include. The expanded token can belong to another
# SourceView, violating addAbsoluteSegment's range assertion. Record the raw
# token's location instead; normal preprocessing still expands it afterwards.
function(mikoto_fix_slang_include_source_location source_dir)
    set(preprocessor "${source_dir}/source/slang/slang-preprocessor.cpp")
    file(READ "${preprocessor}" contents)
    set(original "parentFile->getExpansionStream()->peekLoc(),")
    set(corrected "parentFile->getExpansionStream()->peekRawToken().loc,")
    string(FIND "${contents}" "${original}" original_position)
    if(NOT original_position EQUAL -1)
        string(REPLACE "${original}" "${corrected}" contents "${contents}")
        file(WRITE "${preprocessor}" "${contents}")
    else()
        string(FIND "${contents}" "${corrected}" corrected_position)
        if(corrected_position EQUAL -1)
            message(FATAL_ERROR "Slang's include source-location code changed; review the Mikoto patch for the new version.")
        endif()
    endif()
endfunction()
