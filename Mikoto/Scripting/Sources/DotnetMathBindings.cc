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

#include <glm/glm.hpp>

#include <Scripting/Dotnet/DotnetBindings.hh>


namespace mikoto::scripting {
    using namespace core;

    template<glm::length_t Dimension>
    static auto EvaluateVector( ManagedVectorOperation operation, const core::f32* left, const core::f32* right, core::f32* output ) noexcept -> core::i32 {
        using Vector = glm::vec<Dimension, core::f32, glm::defaultp>;

        // Copy components explicitly: GLM alignment settings must not change
        // the managed ABI, including the 12-byte Vector3 used by transforms.
        Vector a{ 0.0f };
        Vector b{ 0.0f };
        Vector result{ 0.0f };

        for ( glm::length_t index{}; index < Dimension; ++index ) {
            a[index] = left[index];
            b[index] = right[index];
        }

        switch ( operation ) {
            case ManagedVectorOperation::eConstruct:
                result = Vector{ a };
                break;
            case ManagedVectorOperation::eAdd:
                result = a + b;
                break;
            case ManagedVectorOperation::eSubtract:
                result = a - b;
                break;
            case ManagedVectorOperation::eMultiply:
                result = a * b;
                break;
            case ManagedVectorOperation::eDivide:
                result = a / b;
                break;
            case ManagedVectorOperation::eNegate:
                result = -a;
                break;
            case ManagedVectorOperation::eEqual:
                output[0] = a == b ? 1.0f : 0.0f;
                return 1;
            case ManagedVectorOperation::eLength:
                output[0] = glm::length( a );
                return 1;
            case ManagedVectorOperation::eDot:
                output[0] = glm::dot( a, b );
                return 1;
            case ManagedVectorOperation::eNormalize:
                result = glm::normalize( a );
                break;
            case ManagedVectorOperation::eCross:
                if constexpr ( Dimension == 3 ) {
                    result = glm::cross( a, b );
                    break;
                }
                else {
                    return 0;
                }

            default:
                return 0;
        }

        for ( glm::length_t index{}; index < Dimension; ++index ) {
            output[index] = result[index];
        }

        return 1;
    }

    template<glm::length_t Dimension>
    static auto EvaluateMatrix( ManagedMatrixOperation operation, const core::f32* left, const core::f32* right, core::f32* output ) noexcept -> core::i32 {
        using Matrix = glm::mat<Dimension, Dimension, core::f32, glm::defaultp>;

        Matrix a{ 0.0f };
        Matrix b{ 0.0f };
        Matrix result{ 0.0f };

        for ( glm::length_t column{}; column < Dimension; ++column ) {
            for ( glm::length_t row{}; row < Dimension; ++row ) {
                const auto index{ column * Dimension + row };
                a[column][row] = left[index];
                b[column][row] = right[index];
            }
        }

        switch ( operation ) {
            case ManagedMatrixOperation::eConstruct:
                result = Matrix{ a };
                break;
            case ManagedMatrixOperation::eMultiply:
                result = a * b;
                break;
            case ManagedMatrixOperation::eEqual:
                output[0] = a == b ? 1.0f : 0.0f;
                return 1;
            default:
                return 0;
        }

        for ( glm::length_t column{}; column < Dimension; ++column ) {
            for ( glm::length_t row{}; row < Dimension; ++row ) {
                output[column * Dimension + row] = result[column][row];
            }
        }

        return 1;
    }


    auto DotnetMathBinding::Init( ManagedNativeApi& api ) -> void {
        api.mMath = &GetApi();
    }


    auto DotnetMathBinding::GetApi() -> const ManagedMathApi& {
        static const ManagedMathApi api{
            .mVector = &VectorOperation,
            .mMatrix = &MatrixOperation
        };
        return api;
    }

    auto DotnetMathBinding::VectorOperation( ManagedVectorOperation operation, core::u32 dimension,
        const core::f32* left, const core::f32* right, core::f32* output ) noexcept -> core::i32 {
        if ( !left || !right || !output ) {
            return 0;
        }

        switch ( dimension ) {
            case 2:
                return EvaluateVector<2>( operation, left, right, output );
            case 3:
                return EvaluateVector<3>( operation, left, right, output );
            case 4:
                return EvaluateVector<4>( operation, left, right, output );
            default:
                return 0;
        }
    }

    auto DotnetMathBinding::MatrixOperation( ManagedMatrixOperation operation, core::u32 dimension,
        const core::f32* left, const core::f32* right, core::f32* output ) noexcept -> core::i32 {
        if ( !left || !right || !output ) {
            return 0;
        }

        switch ( dimension ) {
            case 2:
                return EvaluateMatrix<2>( operation, left, right, output );
            case 3:
                return EvaluateMatrix<3>( operation, left, right, output );
            case 4:
                return EvaluateMatrix<4>( operation, left, right, output );
            default:
                return 0;
        }
    }
}// namespace mikoto::scripting
