//    Copyright 2025 ケイト
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

#include <iostream>

#include <Threading/TaskGraph.hh>

namespace Mikoto {

    auto TaskGraph::Dump( DumpDst out ) -> void {
        switch (out) {
            case DumpDst::STANDARD_OUTPUT:
                m_Taskflow.dump(std::cout);
                break;
            case DumpDst::STANDARD_ERROR:
                m_Taskflow.dump(std::cerr);
                break;
        }
    }
}