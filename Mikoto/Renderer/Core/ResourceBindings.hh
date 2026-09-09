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

#ifndef MIKOTO_RESOURCE_BINDINGS_HH
#define MIKOTO_RESOURCE_BINDINGS_HH

// See shaders: slang/ResourceBindings.slang

#define MKT_DEFAULT_REGISTER_SPACE 0

#define MKT_STRUCTURED_SRV_BINDING 0
#define MKT_STRUCTURED_UAV_BINDING 1

#define MKT_SAMPLER_BINDING 2

#define MKT_TEXTURE_SRV_BINDING 3
#define MKT_TEXTURE_UAV_BINDING 4

#define MKT_BUFFER_DEVICE_ADDRESS_BINDING 5

#define MKT_ACCELERATION_STRUCTURE_BINDING 5

#define MKT_SHADER_TRUE 1U
#define MKT_SHADER_FALSE 0U

#endif//MIKOTO_RESOURCE_BINDINGS_HH
