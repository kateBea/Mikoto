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

#pragma once

#include "base/Thumbnail_Helpers.slang"

[shader( "fragment" )]
void main( PreviewVertex input ) {
    PreviewDraw draw = ( ( BufferReference<PreviewDraw> )preview.mDraws )[preview.mDrawIndex];
    if ( draw.mFlags.x == 1.0 && PreviewBaseColor( draw, input ).a < draw.mFlags.y ) {
        discard;
    }
}
