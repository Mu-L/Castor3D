#include "Castor3D/Technique/Opaque/LightingPass.hpp"

#include "Castor3D/Technique/Opaque/DirectionalLightPass.hpp"
#include "Castor3D/Technique/Opaque/LightPassShadow.hpp"
#include "Castor3D/Technique/Opaque/OpaquePass.hpp"
#include "Castor3D/Technique/Opaque/PointLightPass.hpp"
#include "Castor3D/Technique/Opaque/SpotLightPass.hpp"

#include "Castor3D/Engine.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/RenderPipeline.hpp"
#include "Castor3D/Render/RenderSystem.hpp"
#include "Castor3D/Scene/Scene.hpp"
#include "Castor3D/Scene/Camera.hpp"
#include "Castor3D/Scene/Light/PointLight.hpp"
#include "Castor3D/Shader/Ubos/MatrixUbo.hpp"
#include "Castor3D/Shader/Ubos/ModelMatrixUbo.hpp"
#include "Castor3D/Technique/RenderTechniquePass.hpp"
#include "Castor3D/Texture/TextureLayout.hpp"
#include "Castor3D/Texture/TextureUnit.hpp"

#include <Ashes/Buffer/VertexBuffer.hpp>
#include <Ashes/RenderPass/FrameBuffer.hpp>
#include <Ashes/RenderPass/FrameBufferAttachment.hpp>
#include <Ashes/Sync/ImageMemoryBarrier.hpp>

#include <ShaderWriter/Source.hpp>

#include "Castor3D/Shader/Shaders/GlslLight.hpp"
#include "Castor3D/Shader/Shaders/GlslShadow.hpp"

using namespace castor;
using namespace castor3d;

namespace castor3d
{
	namespace
	{
		TextureUnit doCreateTexture( Engine & engine
			, Size const & size
			, ashes::Format format
			, bool depth
			, castor::String debugName )
		{
			ashes::ImageUsageFlags usage = depth
				? ashes::ImageUsageFlag::eDepthStencilAttachment | ashes::ImageUsageFlag::eTransferDst | ashes::ImageUsageFlag::eTransferSrc
				: ashes::ImageUsageFlag::eColourAttachment;
			ashes::ImageCreateInfo image{};
			image.arrayLayers = 1u;
			image.extent.width = size.getWidth();
			image.extent.height = size.getHeight();
			image.extent.depth = 1u;
			image.imageType = ashes::TextureType::e2D;
			image.mipLevels = 1u;
			image.samples = ashes::SampleCountFlag::e1;
			image.usage = usage | ashes::ImageUsageFlag::eSampled;
			image.format = format;

			auto texture = std::make_shared< TextureLayout >( *engine.getRenderSystem()
				, image
				, ashes::MemoryPropertyFlag::eDeviceLocal
				, std::move( debugName ) );
			texture->getDefaultImage().initialiseSource();

			TextureUnit result{ engine };
			result.setTexture( texture );
			result.setSampler( engine.getLightsSampler() );
			result.initialise();
			return result;
		}

		TextureUnit doCreateColourTexture( Engine & engine
			, Size const & size )
		{
			return doCreateTexture( engine
				, size
				, ashes::Format::eR16G16B16A16_SFLOAT
				, false
				, cuT( "LightingPass_Colour" ) );
		}

		TextureUnit doCreateDepthTexture( Engine & engine
			, Size const & size )
		{
			return doCreateTexture( engine
				, size
				, getTextureFormat( DsTexture::eDepth )
				, true
				, cuT( "LightingPass_Depth" ) );
		}
	}

	LightingPass::LightingPass( Engine & engine
		, Size const & size
		, Scene const & scene
		, GeometryPassResult const & gpResult
		, OpaquePass & opaque
		, ashes::TextureView const & depthView
		, SceneUbo & sceneUbo
		, GpInfoUbo & gpInfoUbo )
		: m_engine{ engine }
		, m_size{ size }
		, m_depth{ doCreateDepthTexture( engine, size ) }
		, m_diffuse{ doCreateColourTexture( engine, size ) }
		, m_specular{ doCreateColourTexture( engine, size ) }
		, m_timer{ std::make_shared< RenderPassTimer >( engine, cuT( "Opaque" ), cuT( "Lighting pass" ) ) }
		, m_srcDepth{ depthView }
	{
		m_lightPass[size_t( LightType::eDirectional )] = std::make_unique< DirectionalLightPass >( engine
			, m_depth.getTexture()->getDefaultView()
			, m_diffuse.getTexture()->getDefaultView()
			, m_specular.getTexture()->getDefaultView()
			, gpInfoUbo
			, false );
		m_lightPass[size_t( LightType::ePoint )] = std::make_unique< PointLightPass >( engine
			, m_depth.getTexture()->getDefaultView()
			, m_diffuse.getTexture()->getDefaultView()
			, m_specular.getTexture()->getDefaultView()
			, gpInfoUbo
			, false );
		m_lightPass[size_t( LightType::eSpot )] = std::make_unique< SpotLightPass >( engine
			, m_depth.getTexture()->getDefaultView()
			, m_diffuse.getTexture()->getDefaultView()
			, m_specular.getTexture()->getDefaultView()
			, gpInfoUbo
			, false );
		m_lightPassShadow[size_t( LightType::eDirectional )] = std::make_unique< DirectionalLightPassShadow >( engine
			, m_depth.getTexture()->getDefaultView()
			, m_diffuse.getTexture()->getDefaultView()
			, m_specular.getTexture()->getDefaultView()
			, gpInfoUbo );
		m_lightPassShadow[size_t( LightType::ePoint )] = std::make_unique< PointLightPassShadow >( engine
			, m_depth.getTexture()->getDefaultView()
			, m_diffuse.getTexture()->getDefaultView()
			, m_specular.getTexture()->getDefaultView()
			, gpInfoUbo );
		m_lightPassShadow[size_t( LightType::eSpot )] = std::make_unique< SpotLightPassShadow >( engine
			, m_depth.getTexture()->getDefaultView()
			, m_diffuse.getTexture()->getDefaultView()
			, m_specular.getTexture()->getDefaultView()
			, gpInfoUbo );

		for ( auto & lightPass : m_lightPass )
		{
			lightPass->initialise( scene
				, gpResult
				, sceneUbo
				, *m_timer );
		}

		for ( auto & lightPass : m_lightPassShadow )
		{
			lightPass->initialise( scene
				, gpResult
				, sceneUbo
				, *m_timer );
		}

		auto & device = getCurrentDevice( engine );
		ashes::ImageCopy copy
		{
			{ depthView.getSubResourceRange().aspectMask, 0u, 0u, 1u },
			ashes::Offset3D{ 0, 0, 0 },
			{ depthView.getSubResourceRange().aspectMask, 0u, 0u, 1u },
			ashes::Offset3D{ 0, 0, 0 },
			depthView.getTexture().getDimensions(),
		};
		m_blitDepthSemaphore = device.createSemaphore();
		m_blitDepthCommandBuffer = device.getGraphicsCommandPool().createCommandBuffer( true );
		m_blitDepthCommandBuffer->begin();
		// Src depth buffer from depth attach to transfer source
		m_blitDepthCommandBuffer->memoryBarrier( ashes::PipelineStageFlag::eFragmentShader
			, ashes::PipelineStageFlag::eTransfer
			, m_srcDepth.makeTransferSource( ashes::ImageLayout::eUndefined
				, 0u ) );
		// Dst depth buffer from unknown to transfer destination
		m_blitDepthCommandBuffer->memoryBarrier( ashes::PipelineStageFlag::eTransfer
			, ashes::PipelineStageFlag::eTransfer
			, m_depth.getTexture()->getDefaultView().makeTransferDestination( ashes::ImageLayout::eUndefined
				, 0u ) );
		// Copy Src to Dst
		m_blitDepthCommandBuffer->copyImage( copy
			, m_srcDepth.getTexture()
			, ashes::ImageLayout::eTransferSrcOptimal
			, m_depth.getTexture()->getTexture()
			, ashes::ImageLayout::eTransferDstOptimal );
		// Dst depth buffer from transfer destination to depth attach
		m_blitDepthCommandBuffer->memoryBarrier( ashes::PipelineStageFlag::eTransfer
			, ashes::PipelineStageFlag::eEarlyFragmentTests
			, m_depth.getTexture()->getDefaultView().makeDepthStencilAttachment( ashes::ImageLayout::eTransferDstOptimal
				, ashes::AccessFlag::eTransferWrite ) );
		// Src depth buffer from transfer source to depth stencil read only
		m_blitDepthCommandBuffer->memoryBarrier( ashes::PipelineStageFlag::eTransfer
			, ashes::PipelineStageFlag::eFragmentShader
			, m_srcDepth.makeDepthStencilReadOnly( ashes::ImageLayout::eTransferSrcOptimal
				, ashes::AccessFlag::eTransferRead ) );
		m_blitDepthCommandBuffer->end();
	}

	LightingPass::~LightingPass()
	{
		m_blitDepthSemaphore.reset();
		m_blitDepthCommandBuffer.reset();

		for ( auto & lightPass : m_lightPass )
		{
			lightPass->cleanup();
			lightPass.reset();
		}

		for ( auto & lightPass : m_lightPassShadow )
		{
			lightPass->cleanup();
			lightPass.reset();
		}

		m_diffuse.cleanup();
		m_specular.cleanup();
		m_depth.cleanup();
	}

	ashes::Semaphore const & LightingPass::render( Scene const & scene
		, Camera const & camera
		, GeometryPassResult const & gp
		, ashes::Semaphore const & toWait
		, RenderInfo & info )
	{
		auto & cache = scene.getLightCache();
		ashes::Semaphore const * result = &toWait;

		if ( !cache.isEmpty() )
		{
			auto timerBlock = m_timer->start();
			auto lock = makeUniqueLock( cache );
			auto count = cache.getLightsCount( LightType::eDirectional )
				+ cache.getLightsCount( LightType::ePoint )
				+ cache.getLightsCount( LightType::eSpot );

			if ( m_timer->getCount() != count )
			{
				m_timer->updateCount( count );
			}

			auto & device = getCurrentDevice( m_engine );
			device.getGraphicsQueue().submit( *m_blitDepthCommandBuffer
				, *result
				, ashes::PipelineStageFlag::eColourAttachmentOutput
				, *m_blitDepthSemaphore
				, nullptr );
			result = m_blitDepthSemaphore.get();

			uint32_t index = 0;
			result = &doRenderLights( scene
				, camera
				, LightType::eDirectional
				, gp
				, *result
				, index
				, info );
			result = &doRenderLights( scene
				, camera
				, LightType::ePoint
				, gp
				, *result
				, index
				, info );
			result = &doRenderLights( scene
				, camera
				, LightType::eSpot
				, gp
				, *result
				, index
				, info );
		}

		return *result;
	}

	void LightingPass::accept( RenderTechniqueVisitor & visitor )
	{
		m_lightPass[size_t( LightType::eDirectional )]->accept( visitor );
		m_lightPass[size_t( LightType::ePoint )]->accept( visitor );
		m_lightPass[size_t( LightType::eSpot )]->accept( visitor );
		m_lightPassShadow[size_t( LightType::eDirectional )]->accept( visitor );
		m_lightPassShadow[size_t( LightType::ePoint )]->accept( visitor );
		m_lightPassShadow[size_t( LightType::eSpot )]->accept( visitor );
	}

	ashes::Semaphore const & LightingPass::doRenderLights( Scene const & scene
		, Camera const & camera
		, LightType type
		, GeometryPassResult const & gp
		, ashes::Semaphore const & toWait
		, uint32_t & index
		, RenderInfo & info )
	{
		auto result = &toWait;
		auto & cache = scene.getLightCache();

		if ( cache.getLightsCount( type ) )
		{
			auto lightPass = m_lightPass[size_t( type )].get();
			auto lightPassShadow = m_lightPassShadow[size_t( type )].get();

			for ( auto & light : cache.getLights( type ) )
			{
				if ( light->getLightType() == LightType::eDirectional
					|| camera.isVisible( light->getBoundingBox(), light->getParent()->getDerivedTransformationMatrix() ) )
				{
					LightPass * pass = nullptr;

					if ( light->isShadowProducer() && light->getShadowMap() )
					{
						pass = lightPassShadow;
					}
					else
					{
						pass = lightPass;
					}

					pass->update( !index
						, camera.getSize()
						, *light
						, camera
						, light->getShadowMap()
						, light->getShadowMapIndex() );
					result = &pass->render( index
						, *result );

					++index;
					info.m_visibleLightsCount++;
				}

				info.m_totalLightsCount++;
			}
		}

		return *result;
	}
}
