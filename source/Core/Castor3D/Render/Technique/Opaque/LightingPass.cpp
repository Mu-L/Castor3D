#include "Castor3D/Render/Technique/Opaque/LightingPass.hpp"

#include "Castor3D/Engine.hpp"
#include "Castor3D/Cache/LightCache.hpp"
#include "Castor3D/Material/Texture/TextureLayout.hpp"
#include "Castor3D/Material/Texture/TextureUnit.hpp"
#include "Castor3D/Miscellaneous/PipelineVisitor.hpp"
#include "Castor3D/Render/RenderModule.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/RenderPipeline.hpp"
#include "Castor3D/Render/RenderSystem.hpp"
#include "Castor3D/Render/RenderTarget.hpp"
#include "Castor3D/Render/GlobalIllumination/LightPropagationVolumes/LayeredLightPropagationVolumes.hpp"
#include "Castor3D/Render/GlobalIllumination/LightPropagationVolumes/LightPropagationVolumes.hpp"
#include "Castor3D/Render/Technique/RenderTechniquePass.hpp"
#include "Castor3D/Render/Technique/Opaque/OpaquePass.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/DirectionalLightPass.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/LightPassShadow.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/PointLightPass.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/SpotLightPass.hpp"
#include "Castor3D/Render/Technique/Opaque/ReflectiveShadowMapGI/LightPassReflectiveShadow.hpp"
#include "Castor3D/Render/Technique/Opaque/LightVolumeGI/LightPassVolumePropagationShadow.hpp"
#include "Castor3D/Render/Technique/Opaque/LightVolumeGI/LightPassLayeredVolumePropagationShadow.hpp"
#include "Castor3D/Scene/Scene.hpp"
#include "Castor3D/Scene/SceneNode.hpp"
#include "Castor3D/Scene/Camera.hpp"
#include "Castor3D/Scene/Light/PointLight.hpp"
#include "Castor3D/Shader/Shaders/GlslLight.hpp"
#include "Castor3D/Shader/Ubos/MatrixUbo.hpp"
#include "Castor3D/Shader/Ubos/ModelUbo.hpp"

#include <CastorUtils/Graphics/RgbaColour.hpp>

#include <ashespp/Buffer/VertexBuffer.hpp>
#include <ashespp/RenderPass/FrameBuffer.hpp>

#include <ShaderWriter/Source.hpp>

namespace castor3d
{
	namespace
	{
		constexpr uint32_t getDirectionalGIidx( LightingPass::Type type )
		{
			return std::min( uint32_t( type ), uint32_t( LightingPass::Type::eShadowLayeredLpvGGI ) );
		}

		constexpr uint32_t getPointGIidx( LightingPass::Type type )
		{
			return std::min( uint32_t( type ), uint32_t( LightingPass::Type::eShadowRsmGI ) );
		}

		constexpr uint32_t getSpotGIidx( LightingPass::Type type )
		{
			return std::min( uint32_t( type ), uint32_t( LightingPass::Type::eShadowLpvGGI ) );
		}

		constexpr LightingPass::Type getLightType( LightingPass::Type passType
			, LightType lightType )
		{
			switch ( lightType )
			{
			case LightType::eDirectional:
				return LightingPass::Type( getDirectionalGIidx( passType ) );
			case LightType::ePoint:
				return LightingPass::Type( getPointGIidx( passType ) );
			case LightType::eSpot:
				return LightingPass::Type( getSpotGIidx( passType ) );
			default:
				CU_Failure( "Unsupported LightType" );
				return LightingPass::Type( std::min( uint32_t( passType ), uint32_t( LightingPass::Type::eShadowNoGI ) ) );
			}
		}

		LightPass * getLightPass( LightingPass::Type passType
			, LightType lightType
			, LightingPass::LightPasses const & passes )
		{
			auto lt = uint32_t( lightType );

			switch ( lightType )
			{
			case LightType::eDirectional:
				return passes[getDirectionalGIidx( passType )][lt].get();
			case LightType::ePoint:
				return passes[getPointGIidx( passType )][lt].get();
			case LightType::eSpot:
				return passes[getSpotGIidx( passType )][lt].get();
			default:
				CU_Failure( "Unsupported LightType" );
				return nullptr;
			}
		}

		LightPassUPtr & getLightPass( LightingPass::Type passType
			, LightType lightType
			, LightingPass::LightPasses & passes )
		{
			auto lt = uint32_t( lightType );

			switch ( lightType )
			{
			case LightType::eDirectional:
				return passes[getDirectionalGIidx( passType )][lt];
			case LightType::ePoint:
				return passes[getPointGIidx( passType )][lt];
			case LightType::eSpot:
				return passes[getSpotGIidx( passType )][lt];
			default:
				{
					static LightPassUPtr dummy;
					CU_Failure( "Unsupported LightType" );
					return dummy;
				}
			}
		}

		LightingPass::Type getLightingType( RenderDevice const & device
			, bool voxelConeTracing
			, bool shadow
			, GlobalIlluminationType giType )
		{
			if ( voxelConeTracing )
			{
				giType = GlobalIlluminationType::eVoxelConeTracing;
			}

			if ( !device.renderSystem.getGpuInformations().hasShaderType( VK_SHADER_STAGE_GEOMETRY_BIT ) )
			{
				giType = std::min( GlobalIlluminationType::eRsm, giType );
			}

			if ( shadow )
			{
				switch ( giType )
				{
				case GlobalIlluminationType::eNone:
					return LightingPass::Type::eShadowNoGI;
				case GlobalIlluminationType::eVoxelConeTracing:
					return LightingPass::Type::eShadowVoxelConeTracingGI;
				case GlobalIlluminationType::eRsm:
					return LightingPass::Type::eShadowRsmGI;
				case GlobalIlluminationType::eLpv:
					return LightingPass::Type::eShadowLpvGI;
				case GlobalIlluminationType::eLpvG:
					return LightingPass::Type::eShadowLpvGGI;
				case GlobalIlluminationType::eLayeredLpv:
					return LightingPass::Type::eShadowLayeredLpvGI;
				case GlobalIlluminationType::eLayeredLpvG:
					return LightingPass::Type::eShadowLayeredLpvGGI;
				default:
					return LightingPass::Type::eShadowNoGI;
				}
			}

			switch ( giType )
			{
			case GlobalIlluminationType::eNone:
				return LightingPass::Type::eNoShadow;
			case GlobalIlluminationType::eVoxelConeTracing:
				return LightingPass::Type::eNoShadowVoxelConeTracingGI;
			default:
				CU_Failure( "Unsupported GI without shadow maps (WTF are you doing here)" );
				return LightingPass::Type::eNoShadow;
			}
		}

		template< LightingPass::Type PassT, LightType LightT >
		struct PassInfoT;

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadow, LightType::eDirectional >
		{
			using Type = DirectionalLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadow, LightType::ePoint >
		{
			using Type = PointLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadow, LightType::eSpot >
		{
			using Type = SpotLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadowVoxelConeTracingGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadowVoxelConeTracingGI, LightType::ePoint >
		{
			using Type = PointLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadowVoxelConeTracingGI, LightType::eSpot >
		{
			using Type = SpotLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowNoGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowNoGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowNoGI, LightType::eSpot >
		{
			using Type = SpotLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowVoxelConeTracingGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowVoxelConeTracingGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowVoxelConeTracingGI, LightType::eSpot >
		{
			using Type = SpotLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowRsmGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassReflectiveShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowRsmGI, LightType::ePoint >
		{
			using Type = PointLightPassReflectiveShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowRsmGI, LightType::eSpot >
		{
			using Type = SpotLightPassReflectiveShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassLPVShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGI, LightType::eSpot >
		{
			using Type = SpotLightPassLPVShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassLayeredLPVShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGI, LightType::eSpot >
		{
			using Type = SpotLightPassLPVShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassLPVShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGGI, LightType::eSpot >
		{
			using Type = SpotLightPassLPVShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassLayeredLPVShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGGI, LightType::eSpot >
		{
			using Type = SpotLightPassLPVShadow;
		};

		template< LightingPass::Type PassT, LightType LightT >
		using PassTypeT = typename PassInfoT< PassT, LightT >::Type;

		template< LightingPass::Type PassT >
		LightPassUPtr makeLightPassNoGIT( LightType lightType
			, crg::FrameGraph & graph
			, crg::FramePass const *& previousPass
			, RenderDevice const & device
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo
			, bool shadows )
		{
			LightPassConfig lpConfig{ lpResult, gpInfoUbo, shadows, false, false };

			switch ( lightType )
			{
			case LightType::eDirectional:
				return std::make_unique< PassTypeT< PassT, LightType::eDirectional > >( graph
					, previousPass
					, device
					, lpConfig );
			case LightType::ePoint:
				return std::make_unique< PassTypeT< PassT, LightType::ePoint > >( graph
					, previousPass
					, device
					, lpConfig );
			case LightType::eSpot:
				return std::make_unique< PassTypeT< PassT, LightType::eSpot > >( graph
					, previousPass
					, device
					, lpConfig );
			default:
				CU_Failure( "Unsupported LightType" );
				return LightPassUPtr{};
			}
		}

		template< LightingPass::Type PassT >
		LightPassUPtr makeLightPassVctGIT( LightType lightType
			, crg::FrameGraph & graph
			, crg::FramePass const *& previousPass
			, RenderDevice const & device
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo
			, bool shadows
			, VoxelizerUbo const & vctConfig )
		{
			LightPassConfig lpConfig{ lpResult, gpInfoUbo, shadows, true, true };

			switch ( lightType )
			{
			case LightType::eDirectional:
				return std::make_unique< PassTypeT< PassT, LightType::eDirectional > >( graph
					, previousPass
					, device
					, lpConfig
					, &vctConfig );
			case LightType::ePoint:
				return std::make_unique< PassTypeT< PassT, LightType::ePoint > >( graph
					, previousPass
					, device
					, lpConfig
					, &vctConfig );
			case LightType::eSpot:
				return std::make_unique< PassTypeT< PassT, LightType::eSpot > >( graph
					, previousPass
					, device
					, lpConfig
					, &vctConfig );
			default:
				CU_Failure( "Unsupported LightType" );
				return LightPassUPtr{};
			}
		}

		template< LightingPass::Type PassT >
		LightPassUPtr makeLightPassRsmGIT( LightType lightType
			, crg::FrameGraph & graph
			, crg::FramePass const *& previousPass
			, RenderDevice const & device
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smDirectionalResult
			, ShadowMapResult const & smPointResult
			, ShadowMapResult const & smSpotResult )
		{
			switch ( lightType )
			{
			case LightType::eDirectional:
				return std::make_unique< PassTypeT< PassT, LightType::eDirectional > >( graph
					, previousPass
					, device
					, RsmLightPassConfig{ { lpResult, gpInfoUbo }
						, lightCache
						, gpResult
						, smDirectionalResult } );
			case LightType::ePoint:
				return std::make_unique< PassTypeT< PassT, LightType::ePoint > >( graph
					, previousPass
					, device
					, RsmLightPassConfig{ { lpResult, gpInfoUbo }
						, lightCache
						, gpResult
						, smPointResult } );
			case LightType::eSpot:
				return std::make_unique< PassTypeT< PassT, LightType::eSpot > >( graph
					, previousPass
					, device
					, RsmLightPassConfig{ { lpResult, gpInfoUbo }
						, lightCache
						, gpResult
						, smSpotResult } );
			default:
				CU_Failure( "Unsupported LightType" );
				return LightPassUPtr{};
			}
		}

		template< LightingPass::Type PassT >
		LightPassUPtr makeLightPassLpvGIT( LightType lightType
			, crg::FrameGraph & graph
			, crg::FramePass const *& previousPass
			, RenderDevice const & device
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smDirectionalResult
			, ShadowMapResult const & smPointResult
			, ShadowMapResult const & smSpotResult
			, LightVolumePassResult const & lpvResult
			, LpvGridConfigUbo const & lpvConfigUbo )
		{
			switch ( lightType )
			{
			case LightType::eDirectional:
				return std::make_unique< PassTypeT< PassT, LightType::eDirectional > >( graph
					, previousPass
					, device
					, LpvLightPassConfig{ LightPassConfig{ lpResult, gpInfoUbo }
						, lightCache
						, gpResult
						, smDirectionalResult
						, lpvResult
						, lpvConfigUbo } );
			//case LightType::ePoint:
			//	return std::make_unique< PassTypeT< PassT, LightType::ePoint > >( graph
			//		, previousPass
			//		, device
			//		, LpvLightPassConfig{ LightPassConfig{ lpResult, gpInfoUbo }
			//			, lightCache
			//			, gpResult
			//			, smPointResult
			//			, lpvResult
			//			, lpvConfigUbo } );
			case LightType::eSpot:
				return std::make_unique< PassTypeT< PassT, LightType::eSpot > >( graph
					, previousPass
					, device
					, LpvLightPassConfig{ LightPassConfig{ lpResult, gpInfoUbo }
						, lightCache
						, gpResult
						, smSpotResult
						, lpvResult
						, lpvConfigUbo } );
			default:
				CU_Failure( "Unsupported LightType" );
				return LightPassUPtr{};
			}
		}

		template< LightingPass::Type PassT >
		LightPassUPtr makeLightPassLlpvGIT( LightType lightType
			, crg::FrameGraph & graph
			, crg::FramePass const *& previousPass
			, RenderDevice const & device
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smDirectionalResult
			, ShadowMapResult const & smPointResult
			, ShadowMapResult const & smSpotResult
			, LightVolumePassResultArray const & lpvResult
			, LayeredLpvGridConfigUbo const & lpvConfigUbo )
		{
			switch ( lightType )
			{
			case LightType::eDirectional:
				return std::make_unique< PassTypeT< PassT, LightType::eDirectional > >( graph
					, previousPass
					, device
					, LayeredLpvLightPassConfig{ LightPassConfig{ lpResult, gpInfoUbo }
						, lightCache
						, gpResult
						, smDirectionalResult
						, lpvResult
						, lpvConfigUbo } );
			//case LightType::ePoint:
			//	return std::make_unique< PassTypeT< PassT, LightType::ePoint > >( graph
			//		, previousPass
			//		, device
			//		, LayeredLpvLightPassConfig{ LightPassConfig{ lpResult, gpInfoUbo }
			//			, lightCache
			//			, gpResult
			//			, smPointResult
			//			, lpvResult
			//			, lpvConfigUbo
			//			, lpvConfigUbos } );
			//case LightType::eSpot:
			//	return std::make_unique< PassTypeT< PassT, LightType::eSpot > >( graph
			//		, previousPass
			//		, device
			//		, LayeredLpvLightPassConfig{ LightPassConfig{ lpResult, gpInfoUbo }
			//			, lightCache
			//			, gpResult
			//			, smSpotResult
			//			, lpvResult
			//			, lpvConfigUbo
			//			, lpvConfigUbos } );
			default:
				CU_Failure( "Unsupported LightType" );
				return LightPassUPtr{};
			}
		}

		LightPassUPtr makeLightPass( crg::FrameGraph & graph
			, crg::FramePass const *& previousPass
			, LightingPass::Type passType
			, LightType lightType
			, RenderDevice const & device
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smDirectionalResult
			, ShadowMapResult const & smPointResult
			, ShadowMapResult const & smSpotResult
			, LightPassResult const & lpResult
			, LightVolumePassResult const & lpvResult
			, LightVolumePassResultArray const & llpvResult
			, GpInfoUbo const & gpInfoUbo
			, LpvGridConfigUbo const & lpvConfigUbo
			, LayeredLpvGridConfigUbo const & llpvConfigUbo
			, VoxelizerUbo const & vctConfig )
		{
			switch ( passType )
			{
			case LightingPass::Type::eNoShadow:
				return makeLightPassNoGIT< LightingPass::Type::eNoShadow >( lightType
					, graph
					, previousPass
					, device
					, lpResult
					, gpInfoUbo
					, false );
			case LightingPass::Type::eNoShadowVoxelConeTracingGI:
				return makeLightPassVctGIT< LightingPass::Type::eNoShadowVoxelConeTracingGI >( lightType
					, graph
					, previousPass
					, device
					, lpResult
					, gpInfoUbo
					, false
					, vctConfig );
			case LightingPass::Type::eShadowNoGI:
				return makeLightPassNoGIT< LightingPass::Type::eShadowNoGI >( lightType
					, graph
					, previousPass
					, device
					, lpResult
					, gpInfoUbo
					, true );
			case LightingPass::Type::eShadowVoxelConeTracingGI:
				return makeLightPassVctGIT< LightingPass::Type::eShadowVoxelConeTracingGI >( lightType
					, graph
					, previousPass
					, device
					, lpResult
					, gpInfoUbo
					, true
					, vctConfig );
			case LightingPass::Type::eShadowRsmGI:
				return makeLightPassRsmGIT< LightingPass::Type::eShadowRsmGI >( lightType
					, graph
					, previousPass
					, device
					, lpResult
					, gpInfoUbo
					, lightCache
					, gpResult
					, smDirectionalResult
					, smPointResult
					, smSpotResult );
			case LightingPass::Type::eShadowLpvGI:
				if ( device.renderSystem.getGpuInformations().hasShaderType( VK_SHADER_STAGE_GEOMETRY_BIT ) )
				{
					return makeLightPassLpvGIT< LightingPass::Type::eShadowLpvGI >( lightType
						, graph
						, previousPass
						, device
						, lpResult
						, gpInfoUbo
						, lightCache
						, gpResult
						, smDirectionalResult
						, smPointResult
						, smSpotResult
						, lpvResult
						, lpvConfigUbo );
				}
				return LightPassUPtr{};
			case LightingPass::Type::eShadowLpvGGI:
				if ( device.renderSystem.getGpuInformations().hasShaderType( VK_SHADER_STAGE_GEOMETRY_BIT ) )
				{
					return makeLightPassLpvGIT< LightingPass::Type::eShadowLpvGGI >( lightType
						, graph
						, previousPass
						, device
						, lpResult
						, gpInfoUbo
						, lightCache
						, gpResult
						, smDirectionalResult
						, smPointResult
						, smSpotResult
						, lpvResult
						, lpvConfigUbo );
				}
				return LightPassUPtr{};
			case LightingPass::Type::eShadowLayeredLpvGI:
				if ( device.renderSystem.getGpuInformations().hasShaderType( VK_SHADER_STAGE_GEOMETRY_BIT ) )
				{
					return makeLightPassLlpvGIT< LightingPass::Type::eShadowLayeredLpvGI >( lightType
						, graph
						, previousPass
						, device
						, lpResult
						, gpInfoUbo
						, lightCache
						, gpResult
						, smDirectionalResult
						, smPointResult
						, smSpotResult
						, llpvResult
						, llpvConfigUbo );
				}
				return LightPassUPtr{};
			case LightingPass::Type::eShadowLayeredLpvGGI:
				if ( device.renderSystem.getGpuInformations().hasShaderType( VK_SHADER_STAGE_GEOMETRY_BIT ) )
				{
					return makeLightPassLlpvGIT< LightingPass::Type::eShadowLayeredLpvGGI >( lightType
						, graph
						, previousPass
						, device
						, lpResult
						, gpInfoUbo
						, lightCache
						, gpResult
						, smDirectionalResult
						, smPointResult
						, smSpotResult
						, llpvResult
						, llpvConfigUbo );
				}
				return LightPassUPtr{};
			default:
				CU_Failure( "Unsupported LightingPass::Type" );
				return LightPassUPtr{};
			}
		}
	}

	LightingPass::LightingPass( crg::FrameGraph & graph
		, crg::FramePass const & previousPass
		, RenderDevice const & device
		, castor::Size const & size
		, Scene & scene
		, OpaquePassResult const & gpResult
		, ShadowMapResult const & smDirectionalResult
		, ShadowMapResult const & smPointResult
		, ShadowMapResult const & smSpotResult
		, LightVolumePassResult const & lpvResult
		, LightVolumePassResultArray const & llpvResult
		, Texture const & vctFirstBounce
		, Texture const & vctSecondaryBounce
		, Texture const & depthView
		, SceneUbo const & sceneUbo
		, GpInfoUbo const & gpInfoUbo
		, LpvGridConfigUbo const & lpvConfigUbo
		, LayeredLpvGridConfigUbo const & llpvConfigUbo
		, VoxelizerUbo const & vctConfigUbo )
		: m_graph{ graph }
		, m_previousPass{ previousPass }
		, m_device{ device }
		, m_gpResult{ gpResult }
		, m_smDirectionalResult{ smDirectionalResult }
		, m_smPointResult{ smPointResult }
		, m_smSpotResult{ smSpotResult }
		, m_lpvResult{ lpvResult }
		, m_llpvResult{ llpvResult }
		, m_vctFirstBounce{ vctFirstBounce }
		, m_vctSecondaryBounce{ vctSecondaryBounce }
		, m_depthView{ depthView }
		, m_sceneUbo{ sceneUbo }
		, m_gpInfoUbo{ gpInfoUbo }
		, m_lpvConfigUbo{ lpvConfigUbo }
		, m_llpvConfigUbo{ llpvConfigUbo }
		, m_vctConfigUbo{ vctConfigUbo }
		, m_srcDepth{ depthView }
		, m_size{ size }
		, m_result{ scene.getOwner()->getGraphResourceHandler(), device, size }
		, m_timer{ std::make_shared< RenderPassTimer >( device, cuT( "Opaque" ), cuT( "Lighting pass" ) ) }
	{
		//auto & lightCache = scene.getLightCache();
		//lightCache.initialise();

		//VkImageCopy copy
		//{
		//	{ m_srcDepth->subresourceRange.aspectMask, 0u, 0u, 1u },
		//	VkOffset3D{ 0, 0, 0 },
		//	{ m_srcDepth->subresourceRange.aspectMask, 0u, 0u, 1u },
		//	VkOffset3D{ 0, 0, 0 },
		//	m_srcDepth.image->getDimensions(),
		//};
		//m_blitDepth.commandBuffer->begin();
		//m_blitDepth.commandBuffer->beginDebugBlock(
		//	{
		//		"Deferred - Ligth Depth Blit",
		//		makeFloatArray( m_engine.getNextRainbowColour() ),
		//	} );
		//// Src depth buffer from depth attach to transfer source
		//m_blitDepth.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
		//	, VK_PIPELINE_STAGE_TRANSFER_BIT
		//	, m_srcDepth.makeTransferSource( VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) );
		//// Dst depth buffer from unknown to transfer destination
		//m_blitDepth.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
		//	, VK_PIPELINE_STAGE_TRANSFER_BIT
		//	, m_result[LpTexture::eDepth].getTexture()->getDefaultView().getTargetView().makeTransferDestination( VK_IMAGE_LAYOUT_UNDEFINED ) );
		//// Copy Src to Dst
		//m_blitDepth.commandBuffer->copyImage( copy
		//	, *m_srcDepth.image
		//	, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		//	, m_result[LpTexture::eDepth].getTexture()->getTexture()
		//	, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
		//// Dst depth buffer from transfer destination to depth attach
		//m_blitDepth.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_TRANSFER_BIT
		//	, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		//	, m_result[LpTexture::eDepth].getTexture()->getDefaultView().getTargetView().makeDepthStencilAttachment( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) );
		//// Src depth buffer from transfer source to depth stencil read only
		//m_blitDepth.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_TRANSFER_BIT
		//	, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		//	, m_srcDepth.makeDepthStencilReadOnly( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) );
		//m_blitDepth.commandBuffer->endDebugBlock();
		//m_blitDepth.commandBuffer->end();
	}

	LightingPass::~LightingPass()
	{
		for ( auto & lightPasses : m_lightPasses )
		{
			for ( auto & lightPass : lightPasses )
			{
				if ( lightPass )
				{
					lightPass->cleanup();
					lightPass.reset();
				}
			}
		}
	}

	void LightingPass::update( CpuUpdater & updater )
	{
		m_active.clear();
	}

	void LightingPass::update( GpuUpdater & updater )
	{
		auto & scene = *updater.camera->getScene();
		auto & cache = scene.getLightCache();
		m_voxelConeTracing = updater.voxelConeTracing;

		if ( !cache.isEmpty() )
		{
			doUpdateLightPasses( updater, LightType::eDirectional );
			doUpdateLightPasses( updater, LightType::ePoint );
			doUpdateLightPasses( updater, LightType::eSpot );
		}
	}

	ashes::Semaphore const & LightingPass::render( Scene const & scene
		, Camera const & camera
		, OpaquePassResult const & gp
		, ashes::Semaphore const & toWait )
	{
		auto & cache = scene.getLightCache();
		ashes::Semaphore const * result = &toWait;
		m_active.clear();

		if ( !cache.isEmpty() )
		{
			RenderPassTimerBlock timerBlock{ m_timer->start() };
			using LockType = std::unique_lock< LightCache const >;
			LockType lock{ castor::makeUniqueLock( cache ) };
			auto count = cache.getLightsCount( LightType::eDirectional )
				+ cache.getLightsCount( LightType::ePoint )
				+ cache.getLightsCount( LightType::eSpot );

			if ( timerBlock->getCount() != count )
			{
				timerBlock->updateCount( count );
			}

			//m_device.graphicsQueue->submit( *m_blitDepth.commandBuffer
			//	, *result
			//	, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			//	, *m_blitDepth.semaphore
			//	, nullptr );
			//result = m_blitDepth.semaphore.get();

			uint32_t index = 0;
			result = &doRenderLights( scene
				, camera
				, LightType::eDirectional
				, gp
				, *result
				, index );
			result = &doRenderLights( scene
				, camera
				, LightType::ePoint
				, gp
				, *result
				, index );
			result = &doRenderLights( scene
				, camera
				, LightType::eSpot
				, gp
				, *result
				, index );
		}

		return *result;
	}

	void LightingPass::accept( PipelineVisitorBase & visitor )
	{
		visitor.visit( "Light Diffuse"
			, m_result[LpTexture::eDiffuse].wholeViewId
			, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			, TextureFactors{}.invert( true ) );
		visitor.visit( "Light Specular"
			, m_result[LpTexture::eSpecular].wholeViewId
			, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			, TextureFactors{}.invert( true ) );
		visitor.visit( "Light Indirect Diffuse"
			, m_result[LpTexture::eIndirectDiffuse].wholeViewId
			, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			, TextureFactors{}.invert( true ) );
		visitor.visit( "Light Indirect Specular"
			, m_result[LpTexture::eIndirectSpecular].wholeViewId
			, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			, TextureFactors{}.invert( true ) );

		for ( auto & lightPass : m_active )
		{
			lightPass->accept( visitor );
		}
	}

	void LightingPass::doUpdateLightPasses( GpuUpdater & updater
		, LightType lightType )
	{
		auto & camera = *updater.camera;
		auto & scene = *updater.camera->getScene();
		auto & cache = scene.getLightCache();
		auto previous = &m_previousPass;

		for ( auto & light : cache.getLights( lightType ) )
		{
			if ( light->getLightType() == LightType::eDirectional
				|| camera.isVisible( light->getBoundingBox(), light->getParent()->getDerivedTransformationMatrix() ) )
			{
				auto giType = light->getGlobalIlluminationType();
				auto type = getLightingType( m_device
					, m_voxelConeTracing
					, light->isShadowProducer()
					, giType );
				auto & lightPass = getLightPass( type
					, lightType
					, m_lightPasses );

				if ( !lightPass )
				{
					lightPass = makeLightPass( m_graph
						, previous
						, type
						, lightType
						, m_device
						, cache
						, m_gpResult
						, m_smDirectionalResult
						, m_smPointResult
						, m_smSpotResult
						, m_result
						, m_lpvResult
						, m_llpvResult
						, m_gpInfoUbo
						, m_lpvConfigUbo
						, m_llpvConfigUbo
						, m_vctConfigUbo );
					lightPass->initialise( scene
						, m_gpResult
						, m_sceneUbo
						, *m_timer );
				}
			}
		}
	}

	ashes::Semaphore const & LightingPass::doRenderLights( Scene const & scene
		, Camera const & camera
		, LightType type
		, OpaquePassResult const & gp
		, ashes::Semaphore const & toWait
		, uint32_t & index )
	{
		auto result = &toWait;
		auto & cache = scene.getLightCache();

		if ( cache.getLightsCount( type ) )
		{
			for ( auto & light : cache.getLights( type ) )
			{
				if ( light->getLightType() == LightType::eDirectional
					|| camera.isVisible( light->getBoundingBox(), light->getParent()->getDerivedTransformationMatrix() ) )
				{
					LightPass * pass = doGetLightPass( type
						, light->isShadowProducer() && light->getShadowMap()
						, light->getGlobalIlluminationType() );
					CU_Require( pass != nullptr );
					m_active.insert( pass );
					pass->update( index == 0u
						, camera.getSize()
						, *light
						, camera
						, light->getShadowMap()
						, &m_vctFirstBounce
						, &m_vctSecondaryBounce );
					result = &pass->render( index
						, *result );
					++index;
				}
			}
		}

		return *result;
	}

	LightPass * LightingPass::doGetLightPass( LightType lightType
		, bool shadows
		, GlobalIlluminationType giType )const
	{
		return getLightPass( getLightingType( m_device, m_voxelConeTracing, shadows, giType )
			, lightType
			, m_lightPasses );
	}
}
