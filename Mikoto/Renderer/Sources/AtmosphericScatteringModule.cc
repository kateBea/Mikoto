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

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Profiler.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

// Original implementation from https://github.com/SausageTaste/mirinae
// based on Epic Games paper https://sebh.github.io/publications/egsr2020.pdf

namespace mikoto::renderer {

    using namespace core;
    using namespace rhi;

    static auto CreateAtmosphereTexture( FrameGraph& graph, core::cstr name, core::u32 width, core::u32 height,
        rhi::TextureDimension dimension = rhi::TextureDimension::eTexture2D ) -> FGTextureHandle {
        return graph.Create( FGTextureDescription{}
            .SetName( name )
            .SetWidth( width )
            .SetHeight( height )
            .SetDimensions( dimension )
            .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource ) );
    }


    AtmosphericScatteringModule::AtmosphericScatteringModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto AtmosphericScatteringModule::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }

    auto AtmosphericScatteringModule::SetSkyVisible( bool visible ) -> void {
        mSkyVisible = visible;
    }

    auto AtmosphericScatteringModule::IsEnabled() const -> bool {
        return mEnabled;
    }

    auto AtmosphericScatteringModule::Prepare( const scene::Scene* scene, FrameGraph& graph ) -> bool {
        MKT_BEGIN_PROFILER();
        auto& sunLight{ graph.GetOrCreate<AtmosphereModuleInfo>().mSunLight };
        sunLight.reset();

        const bool wasEnabled{ mEnabled };
        mEnabled = scene && mCamera && scene->GetEnvironmentLightingSource() == scene::EnvironmentLightingSource::eAtmosphere;

        if ( !mEnabled ) {
            if ( mConfigured && !wasEnabled ) {
                return false;
            }

            std::initializer_list<cstr> names{ 
                "Atmosphere_Upload", 
                "Atmosphere_Transmittance", 
                "Atmosphere_MultipleScattering",
                "Atmosphere_SkyView", 
                "Atmosphere_Environment", 
                "Atmosphere_AerialPerspective",
                "Atmosphere_SceneCopy",
                "Atmosphere_Composite", 
                "Atmosphere_Sun",
            };

            for ( cstr name : names ) {
                if ( graph.IsPassPresent( name ) ) {
                    graph.DisablePass( name );
                }
            }

            mInitialized = false;
            mConfigured = true;

            return wasEnabled;
        }

        auto settings{ scene->GetAtmosphereSettings() };
        settings.Sanitize();

        const core::f32 azimuth{ glm::radians( settings.mSunAzimuth ) };
        const core::f32 elevation{ glm::radians( settings.mSunElevation ) };

        core::float3 sun{ 
            glm::cos( elevation ) * glm::cos( azimuth ),
            glm::sin( elevation ), 
            glm::cos( elevation ) * glm::sin( azimuth ) };
        core::float3 illuminance{ settings.mSunIlluminance };

        if ( settings.mUseDirectionalLight ) {
            // Stable GUID ordering prevents another sun being chosen when ECS
            // storage order changes. Light direction points away from the sun.
            for ( const auto [entity, tag, transform, light]: scene->GetRegistry().view<scene::TagComponent, scene::TransformComponent, scene::LightComponent>().each() ) {
                if ( !tag.IsActive() || light.GetActiveType() != LightType::eDirectional ||
                    ( sunLight && tag.GetGuid() >= *sunLight ) ) {
                    continue;
                }

                const auto& directional{ light.Get<DirectionalLight>() };
                const core::float3 direction{ transform.GetWorldTransform() * core::float4{ directional.GetDirection(), 0.0f } };
                const core::f32 lengthSquared{ glm::dot( direction, direction ) };

                if ( lengthSquared < 1e-8f ) {
                    continue;
                }

                sunLight = tag.GetGuid();
                sun = -direction / glm::sqrt( lengthSquared );
                illuminance = glm::clamp( directional.GetColor() * directional.GetIntensity() * settings.mSunIlluminance, core::float3{ 0.0f }, core::float3{ 1000.0f } );
            }
        }

        // Only the medium affects these two tables. Changing camera scale,
        // haze range or a sun angle must not rebuild angle-independent LUTs.
        const bool mediumChanged{ !mInitialized || settings.mPlanetRadius != mSettings.mPlanetRadius ||
                                  settings.mAtmosphereHeight != mSettings.mAtmosphereHeight || settings.mRayleighScale != mSettings.mRayleighScale ||
                                  settings.mMieScale != mSettings.mMieScale || settings.mOzoneScale != mSettings.mOzoneScale ||
                                  settings.mMieAnisotropy != mSettings.mMieAnisotropy || settings.mGroundAlbedo != mSettings.mGroundAlbedo };

        const bool environmentChanged{ mediumChanged || core::float3{ mData.mSunDirection } != sun || core::float3{ mData.mSunIlluminance } != illuminance };
        const bool aerialChanged{ !mInitialized || mSettings.mAerialPerspective != settings.mAerialPerspective };

        // The sky LUT depends on the observer's radius and the sun angle
        // relative to the observer's radial up.
        const core::float3 observer{ core::float3{ 0.0f, settings.mPlanetRadius, 0.0f } +
            ( mCamera->GetPosition() - core::float3{ 0.0f, settings.mGroundHeight, 0.0f } ) / settings.mWorldUnitsPerKilometer };
        const core::f32 radius{ glm::length( observer ) };
        const core::f32 altitude{ glm::max( radius - settings.mPlanetRadius, 0.01f ) };
        const core::f32 sunZenith{ radius > 1e-4f ? glm::dot( observer / radius, sun ) : sun.y };
        const bool skyChanged{ environmentChanged || mData.mView.x != altitude || mData.mView.w != sunZenith };

        mSettings = settings;
        mData.mRadii = core::float4{ 
            settings.mPlanetRadius, 
            settings.mPlanetRadius + settings.mAtmosphereHeight,
            1.0f / settings.mWorldUnitsPerKilometer, 
            settings.mGroundHeight 
        };

        mData.mRayleigh = core::float4{ core::float3{ 0.005802f, 0.013558f, 0.0331f } * settings.mRayleighScale, 8.0f };
        mData.mMie = core::float4{ 0.003996f * settings.mMieScale, 0.00444f * settings.mMieScale, 1.2f, settings.mMieAnisotropy };
        mData.mOzone = core::float4{ core::float3{ 0.00065f, 0.001881f, 0.000085f } * settings.mOzoneScale, 0.0f };
        mData.mGround = core::float4{ core::float3{ settings.mGroundAlbedo }, settings.mAerialDistance };
        mData.mSunDirection = core::float4{ sun, glm::radians( settings.mSunAngularRadius ) };
        mData.mSunIlluminance = core::float4{ illuminance, mSkyVisible ? 1.0f : 0.0f };
        mData.mView = core::float4{ altitude, settings.mAerialPerspective ? 1.0f : 0.0f, mCamera->IsOrthographic() ? 1.0f : 0.0f, sunZenith };

        if ( !mInitialized ) {
            graph.EnablePass( "Atmosphere_Upload" );

            if ( graph.IsPassPresent( "Atmosphere_Composite" ) ) {
                graph.EnablePass( "Atmosphere_SceneCopy" );
                graph.EnablePass( "Atmosphere_Composite" );
                graph.EnablePass( "Atmosphere_Sun" );
            }
        }

        if ( mediumChanged ) {
            graph.EnablePass( "Atmosphere_Transmittance" );
            graph.EnablePass( "Atmosphere_MultipleScattering" );
        }

        if ( skyChanged ) {
            graph.EnablePass( "Atmosphere_SkyView" );
        }

        if ( environmentChanged && graph.IsPassPresent( "Atmosphere_Environment" ) ) {
            graph.EnablePass( "Atmosphere_Environment" );
        }

        if ( aerialChanged && graph.IsPassPresent( "Atmosphere_AerialPerspective" ) ) {
            if ( settings.mAerialPerspective ) {
                graph.EnablePass( "Atmosphere_AerialPerspective" );
            }
            else {
                graph.DisablePass( "Atmosphere_AerialPerspective" );
            }
        }

        mInitialized = true;
        mConfigured = true;

        return environmentChanged;
    }


    auto AtmosphericScatteringModule::RegisterParameterUpload( FrameGraph& graph ) -> void {
        mInitialized = false;
        mConfigured = false;
        mEnabled = false;

        auto& info{ graph.GetOrCreate<AtmosphereModuleInfo>() };
        info.mParameters = graph.Create( FGBufferDescription{}
            .SetName( "Atmosphere_Parameters" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( sizeof( AtmosphereData ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        graph.RegisterPass(
            "Atmosphere_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<AtmosphereModuleInfo>().mParameters, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                ctx.Copy( b.Get<AtmosphereModuleInfo>().mParameters, 0, &mData, sizeof( mData ) );
            } );
    }

    auto AtmosphericScatteringModule::RegisterTransmittance( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<AtmosphereModuleInfo>() };

        info.mSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Atmosphere_ClampLinear" )
            .SetFilter( renderer::rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToEdge ) );

        info.mTransmittance = CreateAtmosphereTexture( graph, "Atmosphere_TransmittanceLUT", 256, 64 );

        auto pipeline{ FGPipelineDescription{}
            .SetName( "Atmosphere_LUT" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "Atmosphere_Lut_Frag.slang", FGStageType::ePixel ) };
        info.mLutPipeline = graph.Create( pipeline );

        RegisterLookupTablePass( graph, "Atmosphere_Transmittance", info.mTransmittance, 0 );
    }

    auto AtmosphericScatteringModule::RegisterMultipleScattering( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<AtmosphereModuleInfo>() };

        constexpr core::u32 resolution{ 32 };
        info.mMultipleScattering = graph.Create( FGTextureDescription{}
            .SetName( "Atmosphere_MultipleScatteringLUT" )
            .SetWidth( resolution )
            .SetHeight( resolution )
            .SetFormat( rhi::Format::eRGBA16_FLOAT )
            .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::UnorderedAccess ) );

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Atmosphere_MultipleScattering_Pipeline" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "Atmosphere_MultipleScattering_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "Atmosphere_MultipleScattering",
            FGPassType::eCompute,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& data{ blackboard.Get<AtmosphereModuleInfo>() };
                builder.UseResource( data.mParameters, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( data.mTransmittance, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( data.mMultipleScattering, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [pipeline]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& data{ blackboard.Get<AtmosphereModuleInfo>() };

                struct Parameters {
                    SPointer mAtmosphere{};
                    core::u32 mTransmittance{};
                    core::u32 mSampler{};
                    core::u32 mOutput{};
                } params{
                    .mAtmosphere = ctx.GetDeviceBufferAddress( data.mParameters ),
                    .mTransmittance = ctx.PushSRV( data.mTransmittance ),
                    .mSampler = ctx.PushSampler( data.mSampler ),
                    .mOutput = ctx.PushUAV( data.mMultipleScattering )
                };

                ctx.PushConstants( params );
                ctx.BindPipeline( pipeline );
                ctx.Dispatch( resolution, resolution, 1 );
            } );

        graph.SetExecutionPolicy( "Atmosphere_MultipleScattering", FGExecutionPolicy::eOnWake );
    }

    auto AtmosphericScatteringModule::RegisterSkyView( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<AtmosphereModuleInfo>() };
        info.mSkyView = CreateAtmosphereTexture( graph, "Atmosphere_SkyViewLUT", 192, 108 );
        RegisterLookupTablePass( graph, "Atmosphere_SkyView", info.mSkyView, 2 );
    }

    auto AtmosphericScatteringModule::RegisterEnvironment( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<AtmosphereModuleInfo>() };
        info.mEnvironmentCube = CreateAtmosphereTexture( graph, "Atmosphere_EnvironmentCube", 128, 128, rhi::TextureDimension::eTextureCube );
        RegisterLookupTablePass( graph, "Atmosphere_Environment", info.mEnvironmentCube, 3 );
    }

    auto AtmosphericScatteringModule::RegisterAerialPerspective( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<AtmosphereModuleInfo>() };
        // Thirty-two 32x32 depth slices packed vertically, as before.

        info.mAerialRadiance = CreateAtmosphereTexture( graph, "Atmosphere_AerialRadiance", 32, 32 * 32 );
        info.mAerialTransmittance = CreateAtmosphereTexture( graph, "Atmosphere_AerialTransmittance", 32, 32 * 32 );

        // The second attachment retains colored extinction, instead of reducing
        // transmittance to a gray alpha that loses sunset coloration on geometry.
        auto aerialPipeline{ FGPipelineDescription{}
            .SetName( "Atmosphere_Aerial" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "Atmosphere_Aerial_Frag.slang", FGStageType::ePixel ) };
        info.mAerialPipeline = graph.Create( aerialPipeline );

        RegisterLookupTablePass( graph, "Atmosphere_AerialPerspective", info.mAerialRadiance, 4 );
    }

    auto AtmosphericScatteringModule::RegisterLookupTablePass( FrameGraph& graph, core::cstr name, FGTextureHandle output, core::u32 stage ) -> void {
        graph.RegisterPass(
            name,
            FGPassType::eGraphics,
            [output, stage]( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& data{ b.Get<AtmosphereModuleInfo>() };

                builder.UseResource( output, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( data.mParameters, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                if ( stage > 0 ) {
                    builder.UseResource( data.mTransmittance, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }

                if ( stage > 1 ) {
                    builder.UseResource( data.mMultipleScattering, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }

                if ( stage == 2 || stage == 4 ) {
                    builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }

                if ( stage == 4 ) {
                    builder.UseResource( data.mAerialTransmittance, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                }
            },
            [this, stage]( CommandContext& ctx, core::Blackboard& b ) {
                const core::u32 faces{ stage == 3 ? 6u : 1u };

                for ( core::u32 face{}; face < faces; ++face ) {
                    Draw( ctx, b, stage, face );
                }
            } );

        if ( stage < 4 ) {
            graph.SetExecutionPolicy( name, FGExecutionPolicy::eOnWake );
        }
    }

    auto AtmosphericScatteringModule::RegisterSceneCopy( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<AtmosphereModuleInfo>() };
        
        const auto [width, height]{ InferDimensions( mResolution ) };
        
        info.mSceneCopy = CreateAtmosphereTexture( graph, "Atmosphere_SceneCopyHDR", width, height );

        info.mCompositePipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Atmosphere_Composite" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "Atmosphere_Composite_Frag.slang", FGStageType::ePixel ) );

        RegisterCompositePass( graph, 5 );
    }

    auto AtmosphericScatteringModule::RegisterComposite( FrameGraph& graph ) -> void {
        RegisterCompositePass( graph, 6 );
    }

    auto AtmosphericScatteringModule::RegisterSun( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<AtmosphereModuleInfo>() };
        info.mSunPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Atmosphere_Sun" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( false )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "Atmosphere_Sun_Vert.slang", FGStageType::eVertex )
            .PushShader( "Atmosphere_Composite_Frag.slang", FGStageType::ePixel ) );
        RegisterCompositePass( graph, 7 );
    }

    auto AtmosphericScatteringModule::RegisterCompositePass( FrameGraph& graph, core::u32 stage ) -> void {

        graph.RegisterPass( stage == 5 ? "Atmosphere_SceneCopy" : 
            stage == 6 ? 
            "Atmosphere_Composite" : 
            "Atmosphere_Sun",
            FGPassType::eGraphics,
            [stage]( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& info{ b.Get<AtmosphereModuleInfo>() };
                const auto color{ b.Get<GeomShadingModuleInfo>().mColorImage };

                builder.UseResource( stage == 5 ? color : info.mSceneCopy, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( stage == 5 ? info.mSceneCopy : color, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                if ( stage >= 6 ) {
                    builder.UseResource( info.mParameters, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget,
                        stage == 7 ? FGPipelineStage::eDepthTarget : FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                    for ( const auto input: { info.mSkyView, info.mTransmittance, info.mAerialRadiance, info.mAerialTransmittance } ) {
                        builder.UseResource( input, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }
                }
            },
            [this, stage]( CommandContext& ctx, core::Blackboard& b ) {
                Draw( ctx, b, stage );
            } );
    }

    auto AtmosphericScatteringModule::Draw( CommandContext& ctx, core::Blackboard& b, core::u32 stage, core::u32 face ) -> void {
        const auto& info{ b.Get<AtmosphereModuleInfo>() };

        struct DrawParams {
            SPointer mAtmosphere{};
            SPointer mCamera{};
            core::u32 mTransmittance{};
            core::u32 mMultipleScattering{};
            core::u32 mSkyView{};
            core::u32 mSource{};
            core::u32 mDepth{};
            core::u32 mSampler{};
            core::u32 mStage{};
            core::u32 mFace{};
            core::u32 mAerialRadiance{};
            core::u32 mAerialTransmittance{};
        } params{
            .mAtmosphere = ctx.GetDeviceBufferAddress( info.mParameters ),
            .mCamera = stage == 2 || stage >= 4 ? ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ) : SPointer{},
            .mTransmittance = stage == 0 || stage == 5 ? 0 : ctx.PushSRV( info.mTransmittance ),
            .mMultipleScattering = stage >= 2 && stage <= 4 ? ctx.PushSRV( info.mMultipleScattering ) : 0,
            .mSkyView = stage >= 6 ? ctx.PushSRV( info.mSkyView ) : 0,
            .mSource = stage >= 5 ? ctx.PushSRV( stage == 5 ? b.Get<GeomShadingModuleInfo>().mColorImage : info.mSceneCopy ) : 0,
            .mDepth = stage == 6 ? ctx.PushSRV( b.Get<PrepassModuleInfo>().mPrepassDepthTarget ) : 0,
            .mSampler = ctx.PushSampler( info.mSampler ),
            .mStage = stage,
            .mFace = face,
            .mAerialRadiance = stage == 6 ? ctx.PushSRV( info.mAerialRadiance ) : 0,
            .mAerialTransmittance = stage == 6 ? ctx.PushSRV( info.mAerialTransmittance ) : 0
        };

        ctx.PushConstants( params );

        auto [width, height]{ InferDimensions( mResolution ) };

        FGTextureHandle target{};
        FGPipelineHandle pipeline{ info.mLutPipeline };

        switch ( stage ) {
            case 0: {
                width = 256;
                height = 64;
                target = info.mTransmittance;
                break;
            }

            case 2: {
                width = 192;
                height = 108;
                target = info.mSkyView;
                break;
            }

            case 3: {
                width = 128;
                height = 128;
                target = info.mEnvironmentCube;
                break;
            }

            case 4: {
                width = 32;
                height = 1024;
                target = info.mAerialRadiance;
                pipeline = info.mAerialPipeline;
                break;
            }

            case 5: {
                target = info.mSceneCopy;
                pipeline = info.mCompositePipeline;
                break;
            }

            default: {
                target = b.Get<GeomShadingModuleInfo>().mColorImage;
                pipeline = stage == 7 ? info.mSunPipeline : info.mCompositePipeline;
                break;
            }
        }

        auto render{ RenderBeginDescription{}
            .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( width ), static_cast<core::i32>( height ) } )
            .AddRenderTarget( target, renderer::rhi::Color{}, stage == 7 ? renderer::rhi::LoadOp::eLoad : renderer::rhi::LoadOp::eClear, face ) };

        if ( stage == 7 ) {
            render.AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad );
        }

        if ( stage == 4 ) {
            render.AddRenderTarget( info.mAerialTransmittance, renderer::rhi::Color{}, renderer::rhi::LoadOp::eClear );
        }

        ctx.BeginRender( render );
        ctx.SetViewportState( renderer::rhi::ViewportState{}
            .AddViewportAndScissorRect( renderer::rhi::Viewport( width, height ) ) );
        ctx.BindPipeline( pipeline );
        ctx.Draw( 3 );
        ctx.EndRender();
    }

    auto AtmosphericScatteringModule::GetData() const -> const AtmosphereData& {
        return mData;
    }

    auto AtmosphereSettings::Sanitize() -> void {
        const auto clamp{ []( core::f32 &value, core::f32 minimum, core::f32 maximum ) {
            value = glm::clamp( value, minimum, maximum );
        } };

        clamp( mPlanetRadius, 100.0f, 10000.0f );
        clamp( mAtmosphereHeight, 10.0f, 200.0f );
        clamp( mWorldUnitsPerKilometer, 1.0f, 1000000.0f );
        clamp( mGroundHeight, -1000000.0f, 1000000.0f );
        clamp( mRayleighScale, 0.0f, 10.0f );
        clamp( mMieScale, 0.0f, 10.0f );
        clamp( mOzoneScale, 0.0f, 10.0f );
        clamp( mMieAnisotropy, -0.95f, 0.95f );
        clamp( mGroundAlbedo, 0.0f, 0.95f );
        clamp( mSunElevation, -90.0f, 90.0f );
        clamp( mSunAzimuth, -360.0f, 360.0f );
        clamp( mSunIlluminance, 0.0f, 1000.0f );
        clamp( mSunAngularRadius, 0.01f, 2.0f );
        clamp( mAerialDistance, 1.0f, 256.0f );
    }
}
